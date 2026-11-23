#include "OwnershipSubsystem.h"

#include "BuildingOwnershipModule.h"
#include "Engine/World.h"
#include "FGPlayerController.h"
#include "GameFramework/PlayerState.h"
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

	TeamRegistry.Restore(std::move(LoadedTeams), std::move(LoadedTrust), static_cast<bo::core::TeamId>(FMath::Max(NextTeamId, 1)));
}
