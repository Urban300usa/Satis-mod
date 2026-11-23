// Building ownership records, locks, placement ownership and transfers.
// Plain C++: buildings are opaque keys the game side maps to actors.
#pragma once

#include "Core/BOTeams.h"
#include "Core/BOTypes.h"

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace bo::core
{
	using BuildingKey = std::uint64_t;

	struct OwnershipRecord
	{
		PlayerId Owner;
		bool bLocked = false;
	};

	enum class LockResult : std::uint8_t
	{
		Ok,
		Disabled,      // locking switched off in config
		Unowned,       // unowned or exempt buildings can't be locked
		NotOwner,
		AlreadyLocked,
		NotLocked,
	};

	const char* ToString(LockResult Result);

	class OwnershipRegistry
	{
	public:
		const OwnershipRecord* Find(BuildingKey Key) const;
		// The state Check() needs. Exemption comes from the building's class.
		BuildingState StateOf(BuildingKey Key, bool bExempt) const;

		// Sets the owner. A new owner always starts unlocked.
		void SetOwner(BuildingKey Key, const PlayerId& Owner);
		// Building destroyed, or unclaimed by an admin.
		bool Remove(BuildingKey Key);

		// Owner may lock/unlock; an admin with bypass may too.
		LockResult SetLocked(const ActorState& Actor, BuildingKey Key, bool bLock, bool bAllowLocking);

		std::size_t TransferAll(const PlayerId& From, const PlayerId& To);
		std::size_t UnclaimAll(const PlayerId& Owner);
		std::vector<BuildingKey> OwnedBy(const PlayerId& Owner) const;
		std::size_t Count() const { return Records.size(); }

		const std::map<BuildingKey, OwnershipRecord>& Raw() const { return Records; }
		void Restore(std::map<BuildingKey, OwnershipRecord> InRecords) { Records = std::move(InRecords); }

	private:
		std::map<BuildingKey, OwnershipRecord> Records;
	};

	// --- Placement ---------------------------------------------------------

	// One existing building the new piece snaps onto or connects with.
	struct Attachment
	{
		BuildingKey Key = 0;
		bool bExempt = false;
		Action Kind = Action::BuildOnto; // BuildOnto or Connect
	};

	struct PlacementResult
	{
		bool bAllowed = true;
		PlayerId Owner;            // who will own the new piece(s)
		BuildingKey BlockedBy = 0; // first attachment that refused, when !bAllowed
		DenyReason Deny = DenyReason::None;
	};

	// A single build. Refused if any attachment refuses the builder. Otherwise
	// the helper-build rule applies: attached to someone else's building (in the
	// order given; put the primary snap target first) means that building's
	// owner owns the new piece. Attached only to your own, unowned or exempt
	// buildings, or to nothing, means the builder owns it.
	PlacementResult ResolvePlacement(const ActorState& Builder, const std::vector<Attachment>& Attachments,
		const OwnershipRegistry& Owners, const TeamRegistry& Teams);

	// A blueprint. External attachments only (pieces inside the blueprint don't
	// count). Refused if any part touches a building the placer can't access;
	// otherwise the placer owns everything in it.
	PlacementResult ResolveBlueprintPlacement(const ActorState& Placer, const std::vector<Attachment>& ExternalAttachments,
		const OwnershipRegistry& Owners, const TeamRegistry& Teams);
}
