#include "Commands/OwnerCommand.h"

#include "Command/CommandSender.h"
#include "FGPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "OwnershipSubsystem.h"

AOwnerCommand::AOwnerCommand()
{
	CommandName = TEXT("owner");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = true;
	Usage = NSLOCTEXT("BuildingOwnership", "OwnerUsage", "/owner - show who owns the building you're looking at");
}

EExecutionStatus AOwnerCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	AOwnershipSubsystem* Subsystem = AOwnershipSubsystem::Get(this);
	if (!Subsystem)
	{
		Sender->SendChatMessage(TEXT("[Ownership] Subsystem not found. Is the mod installed on the server?"), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	AFGPlayerController* Controller = Sender->GetPlayer();
	const FString PlayerId = AOwnershipSubsystem::GetPlayerId(Controller);
	if (const APlayerState* State = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr)
	{
		Subsystem->RememberPlayerName(PlayerId, State->GetPlayerName());
	}

	Sender->SendChatMessage(FString::Printf(
		TEXT("[Ownership] Running. Your ID: %s. Loaded from save: %s. Times saved: %d. Teams: %d."),
		PlayerId.IsEmpty() ? TEXT("(unknown)") : *PlayerId,
		Subsystem->WasLoadedFromSave() ? TEXT("yes") : TEXT("no"),
		Subsystem->GetTimesSaved(),
		static_cast<int32>(Subsystem->Teams().AllTeams().size())));

	return EExecutionStatus::COMPLETED;
}
