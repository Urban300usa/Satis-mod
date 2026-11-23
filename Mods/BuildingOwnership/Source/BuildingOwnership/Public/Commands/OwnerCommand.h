#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "OwnerCommand.generated.h"

/**
 * /owner
 * Step 1: proves the mod and subsystem are running and survive save/load.
 * Step 2 replaces this with: show the owner, team and lock state of the
 * building the player is looking at (up to 50 m).
 */
UCLASS()
class BUILDINGOWNERSHIP_API AOwnerCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	AOwnerCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
