#include "Core/BOTransfers.h"

#include <deque>
#include <set>

namespace bo::core
{
	const char* ToString(GiveResult Result)
	{
		switch (Result)
		{
		case GiveResult::Ok: return "OK.";
		case GiveResult::NotOwner: return "You don't own that.";
		case GiveResult::TargetIsSelf: return "You can't give buildings to yourself.";
		case GiveResult::GroupTooBig: return "That connected group is over the transfer limit.";
		case GiveResult::NoOffer: return "You have no pending offer.";
		case GiveResult::OfferExpired: return "That offer has expired.";
		case GiveResult::NothingLeft: return "The giver no longer owns any of those buildings.";
		}
		return "Unknown result.";
	}

	GroupResult FindConnectedGroup(BuildingKey Start,
		const std::function<std::vector<BuildingKey>(BuildingKey)>& Neighbours,
		const std::function<bool(BuildingKey)>& Include,
		std::size_t Cap)
	{
		GroupResult Out;
		if (!Include(Start) || Cap == 0)
		{
			Out.bHitCap = Cap == 0 && Include(Start);
			return Out;
		}
		std::set<BuildingKey> Seen{Start};
		std::deque<BuildingKey> Queue{Start};
		while (!Queue.empty())
		{
			const BuildingKey Current = Queue.front();
			Queue.pop_front();
			if (Out.Members.size() == Cap)
			{
				Out.bHitCap = true;
				break;
			}
			Out.Members.push_back(Current);
			for (BuildingKey Next : Neighbours(Current))
			{
				if (Seen.insert(Next).second && Include(Next))
				{
					Queue.push_back(Next);
				}
			}
		}
		return Out;
	}

	GiveResult GiveOffers::Offer(const PlayerId& From, const PlayerId& To, const GroupResult& Group,
		const OwnershipRegistry& Owners, double Now)
	{
		if (From == To)
		{
			return GiveResult::TargetIsSelf;
		}
		if (Group.bHitCap)
		{
			return GiveResult::GroupTooBig;
		}
		if (Group.Members.empty())
		{
			return GiveResult::NotOwner;
		}
		for (BuildingKey Key : Group.Members)
		{
			const OwnershipRecord* Rec = Owners.Find(Key);
			if (!Rec || Rec->Owner != From)
			{
				return GiveResult::NotOwner;
			}
		}
		Offers[To] = GiveOffer{From, Group.Members, Now + OfferLifetimeSeconds};
		return GiveResult::Ok;
	}

	const GiveOffer* GiveOffers::Pending(const PlayerId& To, double Now) const
	{
		const auto It = Offers.find(To);
		return It == Offers.end() || It->second.ExpiresAt < Now ? nullptr : &It->second;
	}

	GiveResult GiveOffers::Accept(const PlayerId& To, double Now, OwnershipRegistry& Owners, std::size_t* OutMoved)
	{
		const auto It = Offers.find(To);
		if (It == Offers.end())
		{
			return GiveResult::NoOffer;
		}
		const GiveOffer Offer = It->second;
		Offers.erase(It);
		if (Offer.ExpiresAt < Now)
		{
			return GiveResult::OfferExpired;
		}
		std::size_t Moved = 0;
		for (BuildingKey Key : Offer.Buildings)
		{
			const OwnershipRecord* Rec = Owners.Find(Key);
			if (Rec && Rec->Owner == Offer.From)
			{
				Owners.SetOwner(Key, To);
				++Moved;
			}
		}
		if (OutMoved)
		{
			*OutMoved = Moved;
		}
		return Moved ? GiveResult::Ok : GiveResult::NothingLeft;
	}

	GiveResult GiveOffers::Decline(const PlayerId& To)
	{
		return Offers.erase(To) ? GiveResult::Ok : GiveResult::NoOffer;
	}

	bool MessageCooldown::ShouldSend(const PlayerId& Player, double Now)
	{
		const auto It = LastSent.find(Player);
		if (It != LastSent.end() && Now - It->second < Seconds)
		{
			return false;
		}
		LastSent[Player] = Now;
		return true;
	}
}
