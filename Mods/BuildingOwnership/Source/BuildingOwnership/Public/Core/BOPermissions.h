// The one permission check every hook calls. Plain C++, no game types.
#pragma once

#include "Core/BOTeams.h"
#include "Core/BOTypes.h"

namespace bo::core
{
	// The access level Actor has on a building, ignoring locks, admin bypass
	// and exemptions. Assumes the building has an owner.
	AccessLevel LevelFor(const PlayerId& Actor, const PlayerId& Owner, const TeamRegistry& Teams);

	// Six-step rule order, first match wins:
	//   1. Admin with bypass on: allowed.
	//   2. Exempt or unowned building: allowed.
	//   3. Locked building: owner only.
	//   4. Owner and owner's team: Full.
	//   5. Players trusted by a solo owner: Access.
	//   6. Everyone else: None.
	// The action is allowed when the granted level meets RequiredLevel(Act).
	CheckResult Check(const ActorState& Actor, const BuildingState& Building, Action Act, const TeamRegistry& Teams);
}
