#include "Core/BOPermissions.h"

namespace bo::core
{
	AccessLevel LevelFor(const PlayerId& Actor, const PlayerId& Owner, const TeamRegistry& Teams)
	{
		if (Actor == Owner || Teams.SameTeam(Actor, Owner))
		{
			return AccessLevel::Full;
		}
		if (Teams.IsTrustedBy(Owner, Actor))
		{
			return AccessLevel::Access;
		}
		return AccessLevel::None;
	}

	CheckResult Check(const ActorState& Actor, const BuildingState& Building, Action Act, const TeamRegistry& Teams)
	{
		CheckResult Result;

		// 1. Admin with bypass on.
		if (Actor.bIsAdmin && Actor.bBypass)
		{
			Result.bAllowed = true;
			Result.Granted = AccessLevel::Full;
			Result.Allow = AllowReason::AdminBypass;
			return Result;
		}

		// 2. Exempt or unowned.
		if (Building.bExempt || !Building.Owner.has_value())
		{
			Result.bAllowed = true;
			Result.Granted = AccessLevel::Full;
			Result.Allow = AllowReason::ExemptOrUnowned;
			return Result;
		}

		const PlayerId& Owner = *Building.Owner;

		// 3. Locked: owner only, for every action (doors and viewing included).
		if (Building.bLocked)
		{
			if (Actor.Id == Owner)
			{
				Result.bAllowed = true;
				Result.Granted = AccessLevel::Full;
				Result.Allow = AllowReason::Level;
			}
			else
			{
				Result.Deny = DenyReason::Locked;
			}
			return Result;
		}

		// 4-6. Owner/team, solo trust, everyone else.
		Result.Granted = LevelFor(Actor.Id, Owner, Teams);
		Result.bAllowed = Result.Granted >= RequiredLevel(Act);
		if (Result.bAllowed)
		{
			Result.Allow = AllowReason::Level;
		}
		else
		{
			Result.Deny = DenyReason::NotPermitted;
		}
		return Result;
	}
}
