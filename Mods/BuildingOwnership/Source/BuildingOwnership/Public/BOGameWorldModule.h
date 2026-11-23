#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "BOGameWorldModule.generated.h"

/**
 * Registers the mod's subsystem and chat commands with SML.
 *
 * SML discovers root modules from Blueprint assets, so create a Blueprint
 * subclass of this class in the editor named RootGameWorld_BuildingOwnership
 * (see README). The defaults set here carry over to it.
 */
UCLASS()
class BUILDINGOWNERSHIP_API UBOGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	UBOGameWorldModule();
};
