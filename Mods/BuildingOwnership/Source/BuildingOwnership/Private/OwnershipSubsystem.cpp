#include "OwnershipSubsystem.h"

#include "BuildingOwnershipModule.h"
#include "Command/CommandSender.h"
#include "Core/BOAudit.h"
#include "Engine/World.h"
#include "FGPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/SMLRemoteCallObject.h"
#include "Subsystem/SubsystemActorManager.h"

namespace
{
	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString ToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

AOwnershipSubsystem::AOwnershipSubsystem()
{
	// All rules run on the server only; clients never get this actor.
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

AOwnershipSubsystem* AOwnershipSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	return Manager ? Manager->GetSubsystemActor<AOwnershipSubsystem>() : nullptr;
}

FString AOwnershipSubsystem::GetPlayerId(const AFGPlayerController* Controller)
{
	const APlayerState* State = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
	if (!State)
	{
		return FString();
	}
	// VERIFY on a dedicated server: this should be the Epic (EOS) product user
	// ID or Steam ID, and stay the same across reconnects and name changes.
	const FUniqueNetIdRepl& NetId = State->GetUniqueId();
	return NetId.IsValid() ? NetId->ToString() : FString();
}

double AOwnershipSubsystem::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

AFGPlayerController* AOwnershipSubsystem::FindOnlinePlayer(const FString& PlayerId) const
{
	const UWorld* World = GetWorld();
	if (!World || PlayerId.IsEmpty())
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AFGPlayerController* PC = Cast<AFGPlayerController>(It->Get());
		if (PC && GetPlayerId(PC) == PlayerId)
		{
			return PC;
		}
	}
	return nullptr;
}

bo::core::CommandContext AOwnershipSubsystem::MakeCommandContext(AFGPlayerController* Controller)
{
	bo::core::CommandContext Ctx;
	Ctx.Self = ToStd(GetPlayerId(Controller));
	Ctx.Now = Now();
	if (const APlayerState* State = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr)
	{
		RememberPlayerName(GetPlayerId(Controller), State->GetPlayerName());
	}

	TWeakObjectPtr<AOwnershipSubsystem> WeakThis(this);
	Ctx.FindPlayer = [WeakThis](const std::string& Typed) -> std::optional<bo::core::PlayerId>
	{
		const AOwnershipSubsystem* Self = WeakThis.Get();
		const UWorld* World = Self ? Self->GetWorld() : nullptr;
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return std::nullopt;
		}
		// Exact (case-insensitive) name match wins; otherwise a unique prefix.
		const FString Name = ToF(Typed);
		FString PrefixMatch;
		int32 PrefixCount = 0;
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			const FUniqueNetIdRepl& NetId = PS ? PS->GetUniqueId() : FUniqueNetIdRepl();
			if (!NetId.IsValid())
			{
				continue;
			}
			const FString PlayerName = PS->GetPlayerName();
			if (PlayerName.Equals(Name, ESearchCase::IgnoreCase))
			{
				return ToStd(NetId->ToString());
			}
			if (PlayerName.StartsWith(Name, ESearchCase::IgnoreCase))
			{
				PrefixMatch = NetId->ToString();
				++PrefixCount;
			}
		}
		if (PrefixCount == 1)
		{
			return ToStd(PrefixMatch);
		}
		return std::nullopt;
	};
	Ctx.NameOf = [WeakThis](const bo::core::PlayerId& Id) -> std::string
	{
		const AOwnershipSubsystem* Self = WeakThis.Get();
		return Self ? ToStd(Self->GetPlayerName(ToF(Id))) : Id;
	};
	return Ctx;
}

void AOwnershipSubsystem::Deliver(UCommandSender* Sender, const FString& ActorId, const bo::core::CommandOutput& Output)
{
	for (const std::string& Line : Output.Reply)
	{
		Sender->SendChatMessage(ToF(Line));
	}
	for (const auto& [To, Message] : Output.Notify)
	{
		SendToPlayer(ToF(To), ToF(Message));
	}
	for (const std::string& Entry : Output.Audit)
	{
		FString Action, Result;
		if (!ToF(Entry).Split(TEXT("\t"), &Action, &Result))
		{
			Action = ToF(Entry);
		}
		Audit(ActorId, Action, Result);
	}
}

bool AOwnershipSubsystem::SendToPlayer(const FString& PlayerId, const FString& Message, const FLinearColor& Color)
{
	AFGPlayerController* PC = FindOnlinePlayer(PlayerId);
	// VERIFY: SML's remote call object is how the server sends one player a chat line.
	USMLRemoteCallObject* RCO = PC ? PC->GetRemoteCallObjectOfClass<USMLRemoteCallObject>() : nullptr;
	if (!RCO)
	{
		return false;
	}
	RCO->SendChatMessage(Message, Color);
	return true;
}

void AOwnershipSubsystem::Audit(const FString& ActorId, const FString& Action, const FString& Result,
	const FString& Building, const FString& Location, const FString& OwnerId)
{
	const FDateTime UtcNow = FDateTime::UtcNow();
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BuildingOwnership"));
	const FString File = FPaths::Combine(Dir, FString::Printf(TEXT("Audit-%s.tsv"), *UtcNow.ToString(TEXT("%Y-%m-%d"))));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*Dir);
	if (!PlatformFile.FileExists(*File))
	{
		FFileHelper::SaveStringToFile(ToF(bo::core::AuditHeader()) + LINE_TERMINATOR, *File);
	}

	auto Who = [this](const FString& Id) { return Id.IsEmpty() ? FString() : FString::Printf(TEXT("%s (%s)"), *GetPlayerName(Id), *Id); };
	bo::core::AuditEntry Entry;
	Entry.Time = ToStd(UtcNow.ToIso8601());
	Entry.Player = ToStd(Who(ActorId));
	Entry.Action = ToStd(Action);
	Entry.Building = ToStd(Building);
	Entry.Location = ToStd(Location);
	Entry.Owner = ToStd(Who(OwnerId));
	Entry.Result = ToStd(Result);
	FFileHelper::SaveStringToFile(ToF(bo::core::FormatAuditLine(Entry)) + LINE_TERMINATOR, *File,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void AOwnershipSubsystem::RememberPlayerName(const FString& PlayerId, const FString& DisplayName)
{
	if (!PlayerId.IsEmpty() && !DisplayName.IsEmpty())
	{
		PlayerNames.Add(PlayerId, DisplayName);
	}
}

FString AOwnershipSubsystem::GetPlayerName(const FString& PlayerId) const
{
	const FString* Name = PlayerNames.Find(PlayerId);
	return Name ? *Name : PlayerId;
}

void AOwnershipSubsystem::PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	++TimesSaved;
	WriteSaveData();
}

void AOwnershipSubsystem::PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	bLoadedFromSave = true;
	ReadSaveData();
	UE_LOG(LogBuildingOwnership, Log, TEXT("Loaded: schema %d, %d teams, %d trust lists, saved %d times"),
		SchemaVersion, SavedTeams.Num(), SavedTrust.Num(), TimesSaved);
}

void AOwnershipSubsystem::WriteSaveData()
{
	SchemaVersion = CurrentSchemaVersion;
	NextTeamId = static_cast<int32>(TeamRegistry.NextTeamId());

	SavedTeams.Reset();
	for (const bo::core::Team* T : TeamRegistry.AllTeams())
	{
		FBOTeamSave& S = SavedTeams.AddDefaulted_GetRef();
		S.Id = static_cast<int32>(T->Id);
		S.Name = ToF(T->Name);
		S.Leader = ToF(T->Leader);
		S.ColorIndex = T->ColorIndex;
		for (const auto& P : T->Officers) S.Officers.Add(ToF(P));
		for (const auto& P : T->Members) S.Members.Add(ToF(P));
	}

	SavedScores.Reset();
	for (const auto& [Team, Score] : Scoreboard.Raw())
	{
		FBOScoreSave& S = SavedScores.AddDefaulted_GetRef();
		S.TeamId = static_cast<int32>(Team);
		S.Score = Score.Score;
		S.Coupons = Score.Coupons;
		S.CouponProgress = Score.CouponProgress;
		S.CouponsEarned = Score.CouponsEarned;
		for (const std::string& M : Score.Milestones) S.Milestones.Add(ToF(M));
	}

	SavedTrust.Reset();
	for (const auto& [Owner, Trusted] : TeamRegistry.RawTrust())
	{
		FBOTrustSave& S = SavedTrust.AddDefaulted_GetRef();
		S.Owner = ToF(Owner);
		for (const auto& P : Trusted) S.Trusted.Add(ToF(P));
	}
}

void AOwnershipSubsystem::ReadSaveData()
{
	std::vector<bo::core::Team> LoadedTeams;
	LoadedTeams.reserve(SavedTeams.Num());
	for (const FBOTeamSave& S : SavedTeams)
	{
		bo::core::Team T;
		T.Id = static_cast<bo::core::TeamId>(S.Id);
		T.Name = ToStd(S.Name);
		T.Leader = ToStd(S.Leader);
		T.ColorIndex = S.ColorIndex;
		for (const FString& P : S.Officers) T.Officers.insert(ToStd(P));
		for (const FString& P : S.Members) T.Members.insert(ToStd(P));
		LoadedTeams.push_back(std::move(T));
	}

	std::map<bo::core::PlayerId, std::set<bo::core::PlayerId>> LoadedTrust;
	for (const FBOTrustSave& S : SavedTrust)
	{
		auto& Set = LoadedTrust[ToStd(S.Owner)];
		for (const FString& P : S.Trusted) Set.insert(ToStd(P));
	}

	std::map<bo::core::TeamId, bo::core::TeamScore> LoadedScores;
	for (const FBOScoreSave& S : SavedScores) // empty in schema 1 saves
	{
		bo::core::TeamScore& Score = LoadedScores[static_cast<bo::core::TeamId>(S.TeamId)];
		Score.Score = S.Score;
		Score.Coupons = S.Coupons;
		Score.CouponProgress = S.CouponProgress;
		Score.CouponsEarned = S.CouponsEarned;
		for (const FString& M : S.Milestones) Score.Milestones.insert(ToStd(M));
	}
	Scoreboard.Restore(std::move(LoadedScores));

	TeamRegistry.Restore(std::move(LoadedTeams), std::move(LoadedTrust), static_cast<bo::core::TeamId>(FMath::Max(NextTeamId, 1)));
}
