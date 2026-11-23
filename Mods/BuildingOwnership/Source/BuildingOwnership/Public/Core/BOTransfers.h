// /give offers, connected-group search, and admin transfer helpers.
#pragma once

#include "Core/BOOwnership.h"

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace bo::core
{
	struct GroupResult
	{
		std::vector<BuildingKey> Members; // start first, then breadth-first order
		bool bHitCap = false;             // more buildings existed beyond Cap
	};

	// Breadth-first flood fill from Start over Neighbours, keeping only buildings
	// for which Include returns true (e.g. same owner). The game side decides
	// which connection types Neighbours follows (belts, pipes, rails, snapped
	// foundations; not power lines by default). Stops once Cap is exceeded.
	GroupResult FindConnectedGroup(BuildingKey Start,
		const std::function<std::vector<BuildingKey>(BuildingKey)>& Neighbours,
		const std::function<bool(BuildingKey)>& Include,
		std::size_t Cap);

	enum class GiveResult : std::uint8_t
	{
		Ok,
		NotOwner,      // giver doesn't own the target building
		TargetIsSelf,
		GroupTooBig,
		NoOffer,
		OfferExpired,
		NothingLeft,   // giver no longer owns any of the offered buildings
	};

	const char* ToString(GiveResult Result);

	struct GiveOffer
	{
		PlayerId From;
		std::vector<BuildingKey> Buildings;
		double ExpiresAt = 0.0;
	};

	// Pending /give offers, one per receiver (a newer offer replaces an older one).
	class GiveOffers
	{
	public:
		double OfferLifetimeSeconds = 300.0;

		GiveResult Offer(const PlayerId& From, const PlayerId& To, const GroupResult& Group,
			const OwnershipRegistry& Owners, double Now);
		const GiveOffer* Pending(const PlayerId& To, double Now) const;
		// Moves every offered building the giver still owns. OutMoved = count.
		GiveResult Accept(const PlayerId& To, double Now, OwnershipRegistry& Owners, std::size_t* OutMoved = nullptr);
		GiveResult Decline(const PlayerId& To);

	private:
		std::map<PlayerId, GiveOffer> Offers;
	};

	// At most one deny message per player per cooldown window.
	class MessageCooldown
	{
	public:
		explicit MessageCooldown(double InSeconds = 3.0) : Seconds(InSeconds) {}
		bool ShouldSend(const PlayerId& Player, double Now);
		double Seconds;

	private:
		std::map<PlayerId, double> LastSent;
	};
}
