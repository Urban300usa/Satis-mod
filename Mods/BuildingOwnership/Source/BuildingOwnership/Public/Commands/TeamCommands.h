#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "TeamCommands.generated.h"

/**
 * Base for commands whose logic lives in Core/BOCommands: converts arguments,
 * runs the core function against the subsystem, and delivers the output.
 */
UCLASS(Abstract)
class BUILDINGOWNERSHIP_API ABOCoreCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;

protected:
	enum class EKind : uint8 { Team, Trust, Untrust, Scores };
	EKind Kind = EKind::Team;
};

/** /team create|invite|accept|decline|kick|promote|demote|leader|leave|disband|info */
UCLASS()
class BUILDINGOWNERSHIP_API ATeamCommand : public ABOCoreCommand
{
	GENERATED_BODY()
public:
	ATeamCommand();
};

/** /trust <player>, /trust list (solo players only) */
UCLASS()
class BUILDINGOWNERSHIP_API ATrustCommand : public ABOCoreCommand
{
	GENERATED_BODY()
public:
	ATrustCommand();
};

/** /untrust <player> */
UCLASS()
class BUILDINGOWNERSHIP_API AUntrustCommand : public ABOCoreCommand
{
	GENERATED_BODY()
public:
	AUntrustCommand();
};

/** /scores */
UCLASS()
class BUILDINGOWNERSHIP_API AScoresCommand : public ABOCoreCommand
{
	GENERATED_BODY()
public:
	AScoresCommand();
};
