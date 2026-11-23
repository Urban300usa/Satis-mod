#include "Commands/TeamCommands.h"

#include "Command/CommandSender.h"
#include "Core/BOCommands.h"
#include "FGPlayerController.h"
#include "OwnershipSubsystem.h"

namespace
{
	std::vector<std::string> ToStdArgs(const TArray<FString>& Arguments)
	{
		std::vector<std::string> Out;
		Out.reserve(Arguments.Num());
		for (const FString& A : Arguments)
		{
			Out.emplace_back(TCHAR_TO_UTF8(*A));
		}
		return Out;
	}
}

EExecutionStatus ABOCoreCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	AOwnershipSubsystem* Subsystem = AOwnershipSubsystem::Get(this);
	if (!Subsystem)
	{
		Sender->SendChatMessage(TEXT("[Ownership] Subsystem not found. Is the mod installed on the server?"), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	AFGPlayerController* Controller = Sender->GetPlayer();
	const FString PlayerId = AOwnershipSubsystem::GetPlayerId(Controller);
	if (PlayerId.IsEmpty())
	{
		Sender->SendChatMessage(TEXT("[Ownership] Couldn't read your account ID."), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	const bo::core::CommandContext Ctx = Subsystem->MakeCommandContext(Controller);
	const std::vector<std::string> Args = ToStdArgs(Arguments);
	bo::core::CommandOutput Output;
	switch (Kind)
	{
	case EKind::Team: Output = bo::core::RunTeamCommand(Ctx, Args, Subsystem->Teams()); break;
	case EKind::Trust: Output = bo::core::RunTrustCommand(Ctx, Args, Subsystem->Teams()); break;
	case EKind::Untrust: Output = bo::core::RunUntrustCommand(Ctx, Args, Subsystem->Teams()); break;
	case EKind::Scores: Output = bo::core::RunScoresCommand(Ctx, Subsystem->Scores(), Subsystem->Teams()); break;
	}
	Subsystem->Deliver(Sender, PlayerId, Output);
	return EExecutionStatus::COMPLETED;
}

ATeamCommand::ATeamCommand()
{
	Kind = EKind::Team;
	CommandName = TEXT("team");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = true;
	Usage = NSLOCTEXT("BuildingOwnership", "TeamUsage", "/team create|invite|accept|decline|kick|promote|demote|leader|leave|disband|info");
}

ATrustCommand::ATrustCommand()
{
	Kind = EKind::Trust;
	CommandName = TEXT("trust");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = true;
	Usage = NSLOCTEXT("BuildingOwnership", "TrustUsage", "/trust <player> | /trust list");
}

AUntrustCommand::AUntrustCommand()
{
	Kind = EKind::Untrust;
	CommandName = TEXT("untrust");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = true;
	Usage = NSLOCTEXT("BuildingOwnership", "UntrustUsage", "/untrust <player>");
}

AScoresCommand::AScoresCommand()
{
	Kind = EKind::Scores;
	CommandName = TEXT("scores");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = false;
	Usage = NSLOCTEXT("BuildingOwnership", "ScoresUsage", "/scores");
}
