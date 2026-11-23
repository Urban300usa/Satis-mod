#include "BuildingOwnershipModule.h"

#include "Hooks/OwnershipHooks.h"

DEFINE_LOG_CATEGORY(LogBuildingOwnership);

void FBuildingOwnershipModule::StartupModule()
{
	// SML convention: don't install native hooks inside the editor.
#if !WITH_EDITOR
	BuildingOwnership::Hooks::Register();
#endif
	UE_LOG(LogBuildingOwnership, Log, TEXT("BuildingOwnership module started"));
}

void FBuildingOwnershipModule::ShutdownModule()
{
}

IMPLEMENT_GAME_MODULE(FBuildingOwnershipModule, BuildingOwnership);
