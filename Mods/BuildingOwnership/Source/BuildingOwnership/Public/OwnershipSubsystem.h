#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
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

/**
 * Holds all mod state on the server and saves it inside the world save.
 *
 * Step 1 of the build plan: teams/trust persistence plus save/load counters
 * so /owner can prove the subsystem survives a restart. Building ownership,
 * locks, scores, coupons and steal cooldowns are added in later steps.
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
	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY(SaveGame) int32 SchemaVersion = CurrentSchemaVersion;
	UPROPERTY(SaveGame) int32 TimesSaved = 0;
	UPROPERTY(SaveGame) int32 NextTeamId = 1;
	UPROPERTY(SaveGame) TArray<FBOTeamSave> SavedTeams;
	UPROPERTY(SaveGame) TArray<FBOTrustSave> SavedTrust;
	UPROPERTY(SaveGame) TMap<FString, FString> PlayerNames;

	bool bLoadedFromSave = false;
	bo::core::TeamRegistry TeamRegistry;
};
