// Every native hook the mod installs lives in this file, so a game update
// only needs patching here. Hooks are thin adapters: they work out the acting
// player, the building and the action, call bo::core::Check via the subsystem,
// and block with a chat message when it denies.
//
// Game class and function names for each step are guesses from the build plan.
// Verify each one against the game headers for the game version in the README
// before hooking it.

#include "Hooks/OwnershipHooks.h"

#include "BuildingOwnershipModule.h"

namespace BuildingOwnership::Hooks
{
	void Register()
	{
		// Step 2:  record owner on place (build gun / hologram construct, incl. blueprints).
		// Step 3:  dismantle (single + mass), upgrade-replace, paint/customize.
		// Step 4:  machine changes (recipe, clock, power, item moves), storage open request.
		// Step 5:  connect + build-onto/snap per type, helper-build ownership, belt pickup.
		// Step 6:  doors, vehicles, switches, splitters, stations.
		// Step 9:  Sink delivery, Shop purchase, schematic/milestone unlock.
		// Step 10: player damage, player death and inventory drop.
		UE_LOG(LogBuildingOwnership, Log, TEXT("Hooks registered (none yet: build step 1)"));
	}
}
