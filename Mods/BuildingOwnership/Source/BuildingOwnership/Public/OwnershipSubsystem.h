#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
#include "Core/BOCommands.h"
#include "Core/BOScoring.h"
#include "Core/BOTeams.h"
#include "OwnershipSubsystem.generated.h"

class AFGPlayerController;

/** Saved form of one team. Mirrors bo::core::Team. */
USTRUCT()
struct FBOTeamSave
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) int32 Id = 0;
	UPROPERTY(SaveGame) FString Name;
	UPROPERTY(SaveGame) FString Leader;
	UPROPERTY(SaveGame) TArray<FString> Officers;
	UPROPERTY(SaveGame) TArray<FString> Members;
	UPROPERTY(SaveGame) int32 ColorIndex = 0;
};

/** Saved form of one solo player's trust list. */
USTRUCT()
struct FBOTrustSave
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FString Owner;
	UPROPERTY(SaveGame) TArray<FString> Trusted;
};

/** Saved form of one team's score and coupons. Mirrors bo::core::TeamScore. */
USTRUCT()
struct FBOScoreSave
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) int32 TeamId = 0;
	UPROPERTY(SaveGame) int64 Score = 0;
	UPROPERTY(SaveGame) int64 Coupons = 0;
	UPROPERTY(SaveGame) int64 CouponProgress = 0;
	UPROPERTY(SaveGame) int64 CouponsEarned = 0;
	UPROPERTY(SaveGame) TArray<FString> Milestones;
};

/**
 * Holds all mod state on the server and saves it inside the world save.
 *
 * Saved so far: teams, trust lists, scores/coupons, player names, and
 * save/load counters (so /owner can prove the subsystem survives a restart).
 * Building ownership, locks and steal cooldowns are added with their hooks.
 */
UCLASS()
class BUILDINGOWNERSHIP_API AOwnershipSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AOwnershipSubsystem();

	/** Returns the subsystem for this world, or nullptr (e.g. on clients). */
	static AOwnershipSubsystem* Get(const UObject* WorldContext);

	/** Epic/Steam account ID for a player, or an empty string if unknown. */
	static FString GetPlayerId(const AFGPlayerController* Controller);

	bo::core::TeamRegistry& Teams() { return TeamRegistry; }
	const bo::core::TeamRegistry& Teams() const { return TeamRegistry; }
	bo::core::Scoreboard& Scores() { return Scoreboard; }
	const bo::core::Scoreboard& Scores() const { return Scoreboard; }

	/** Seconds on this server's clock, for invites and offers (not saved). */
	double Now() const;

	/** Context for the core command functions, acting as Controller's player. */
	bo::core::CommandContext MakeCommandContext(AFGPlayerController* Controller);

	/** Sends replies to Sender, notifications to online players, and writes audit lines. */
	void Deliver(class UCommandSender* Sender, const FString& ActorId, const bo::core::CommandOutput& Output);

	/** Chat message to one online player. False if they're offline. */
	bool SendToPlayer(const FString& PlayerId, const FString& Message, const FLinearColor& Color = FLinearColor::White);

	/** Online player's controller by account ID, or nullptr. */
	AFGPlayerController* FindOnlinePlayer(const FString& PlayerId) const;

	/** Appends one line to today's audit log (Saved/BuildingOwnership/Audit-YYYY-MM-DD.tsv). */
	void Audit(const FString& ActorId, const FString& Action, const FString& Result,
		const FString& Building = FString(), const FString& Location = FString(), const FString& OwnerId = FString());

	/** Remember the latest display name for a player ID, for chat messages. */
	void RememberPlayerName(const FString& PlayerId, const FString& DisplayName);
	FString GetPlayerName(const FString& PlayerId) const;

	bool WasLoadedFromSave() const { return bLoadedFromSave; }
	int32 GetTimesSaved() const { return TimesSaved; }

	// IFGSaveInterface
	virtual bool ShouldSave_Implementation() const override { return true; }
	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;

private:
	void WriteSaveData();
	void ReadSaveData();

	/** Bump when the saved layout changes, and handle old versions in ReadSaveData. */
	static constexpr int32 CurrentSchemaVersion = 2; // 2: scores

	UPROPERTY(SaveGame) int32 SchemaVersion = CurrentSchemaVersion;
	UPROPERTY(SaveGame) int32 TimesSaved = 0;
	UPROPERTY(SaveGame) int32 NextTeamId = 1;
	UPROPERTY(SaveGame) TArray<FBOTeamSave> SavedTeams;
	UPROPERTY(SaveGame) TArray<FBOTrustSave> SavedTrust;
	UPROPERTY(SaveGame) TMap<FString, FString> PlayerNames;
	UPROPERTY(SaveGame) TArray<FBOScoreSave> SavedScores;

	bool bLoadedFromSave = false;
	bo::core::TeamRegistry TeamRegistry;
	bo::core::Scoreboard Scoreboard;
};
