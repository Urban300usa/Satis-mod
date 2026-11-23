#include "Core/BOOwnership.h"

#include "Core/BOPermissions.h"

namespace bo::core
{
	const char* ToString(LockResult Result)
	{
		switch (Result)
		{
		case LockResult::Ok: return "OK.";
		case LockResult::Disabled: return "Locking is turned off on this server.";
		case LockResult::Unowned: return "Nobody owns that, so it can't be locked.";
		case LockResult::NotOwner: return "Only the owner can lock or unlock that.";
		case LockResult::AlreadyLocked: return "That's already locked.";
		case LockResult::NotLocked: return "That isn't locked.";
		}
		return "Unknown result.";
	}

	const OwnershipRecord* OwnershipRegistry::Find(BuildingKey Key) const
	{
		const auto It = Records.find(Key);
		return It == Records.end() ? nullptr : &It->second;
	}

	BuildingState OwnershipRegistry::StateOf(BuildingKey Key, bool bExempt) const
	{
		BuildingState State;
		State.bExempt = bExempt;
		if (const OwnershipRecord* Rec = Find(Key))
		{
			State.Owner = Rec->Owner;
			State.bLocked = Rec->bLocked;
		}
		return State;
	}

	void OwnershipRegistry::SetOwner(BuildingKey Key, const PlayerId& Owner)
	{
		Records[Key] = OwnershipRecord{Owner, false};
	}

	bool OwnershipRegistry::Remove(BuildingKey Key)
	{
		return Records.erase(Key) > 0;
	}

	LockResult OwnershipRegistry::SetLocked(const ActorState& Actor, BuildingKey Key, bool bLock, bool bAllowLocking)
	{
		if (!bAllowLocking)
		{
			return LockResult::Disabled;
		}
		const auto It = Records.find(Key);
		if (It == Records.end())
		{
			return LockResult::Unowned;
		}
		const bool bAdminOverride = Actor.bIsAdmin && Actor.bBypass;
		if (It->second.Owner != Actor.Id && !bAdminOverride)
		{
			return LockResult::NotOwner;
		}
		if (It->second.bLocked == bLock)
		{
			return bLock ? LockResult::AlreadyLocked : LockResult::NotLocked;
		}
		It->second.bLocked = bLock;
		return LockResult::Ok;
	}

	std::size_t OwnershipRegistry::TransferAll(const PlayerId& From, const PlayerId& To)
	{
		std::size_t Moved = 0;
		for (auto& [Key, Rec] : Records)
		{
			if (Rec.Owner == From)
			{
				Rec = OwnershipRecord{To, false};
				++Moved;
			}
		}
		return Moved;
	}

	std::size_t OwnershipRegistry::UnclaimAll(const PlayerId& Owner)
	{
		std::size_t Removed = 0;
		for (auto It = Records.begin(); It != Records.end();)
		{
			if (It->second.Owner == Owner)
			{
				It = Records.erase(It);
				++Removed;
			}
			else
			{
				++It;
			}
		}
		return Removed;
	}

	std::vector<BuildingKey> OwnershipRegistry::OwnedBy(const PlayerId& Owner) const
	{
		std::vector<BuildingKey> Out;
		for (const auto& [Key, Rec] : Records)
		{
			if (Rec.Owner == Owner)
			{
				Out.push_back(Key);
			}
		}
		return Out;
	}

	namespace
	{
		// Shared refusal pass: every attachment must allow the builder.
		bool CheckAttachments(const ActorState& Builder, const std::vector<Attachment>& Attachments,
			const OwnershipRegistry& Owners, const TeamRegistry& Teams, PlacementResult& Out)
		{
			for (const Attachment& A : Attachments)
			{
				const CheckResult R = Check(Builder, Owners.StateOf(A.Key, A.bExempt), A.Kind, Teams);
				if (!R.bAllowed)
				{
					Out.bAllowed = false;
					Out.BlockedBy = A.Key;
					Out.Deny = R.Deny;
					return false;
				}
			}
			return true;
		}
	}

	PlacementResult ResolvePlacement(const ActorState& Builder, const std::vector<Attachment>& Attachments,
		const OwnershipRegistry& Owners, const TeamRegistry& Teams)
	{
		PlacementResult Out;
		Out.Owner = Builder.Id;
		if (!CheckAttachments(Builder, Attachments, Owners, Teams, Out))
		{
			Out.Owner.clear();
			return Out;
		}
		for (const Attachment& A : Attachments)
		{
			if (A.bExempt)
			{
				continue;
			}
			const OwnershipRecord* Rec = Owners.Find(A.Key);
			if (Rec && Rec->Owner != Builder.Id)
			{
				Out.Owner = Rec->Owner; // helper build: goes to that building's owner
				break;
			}
		}
		return Out;
	}

	PlacementResult ResolveBlueprintPlacement(const ActorState& Placer, const std::vector<Attachment>& ExternalAttachments,
		const OwnershipRegistry& Owners, const TeamRegistry& Teams)
	{
		PlacementResult Out;
		Out.Owner = Placer.Id;
		if (!CheckAttachments(Placer, ExternalAttachments, Owners, Teams, Out))
		{
			Out.Owner.clear();
		}
		return Out;
	}
}
