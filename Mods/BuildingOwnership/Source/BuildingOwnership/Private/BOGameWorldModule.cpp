#include "BOGameWorldModule.h"

#include "OwnershipSubsystem.h"
#include "Commands/OwnerCommand.h"
#include "Commands/TeamCommands.h"

UBOGameWorldModule::UBOGameWorldModule()
{
	// VERIFY against the SML version you build with: these field names come
	// from UModModule / UGameWorldModule and have been stable through SML 3.x.
	bRootModule = true;
	ModSubsystems.Add(AOwnershipSubsystem::StaticClass());
	mChatCommands.Add(AOwnerCommand::StaticClass());
	mChatCommands.Add(ATeamCommand::StaticClass());
	mChatCommands.Add(ATrustCommand::StaticClass());
	mChatCommands.Add(AUntrustCommand::StaticClass());
	mChatCommands.Add(AScoresCommand::StaticClass());
}
