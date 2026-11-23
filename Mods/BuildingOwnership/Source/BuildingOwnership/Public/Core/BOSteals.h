// /grab and /hack: timers, cooldowns, halfway alerts, cancellation, and the
// temporary access exception /hack grants.
//
// ASSUMPTIONS (flagged for Tyler):
//  - A container's cooldown starts when a steal STARTS, whatever the outcome,
//    so cancelling and restarting can't be used to dodge it.
//  - /hack gives open access for its whole 5 minutes, starting immediately.
//  - A container can only have one steal running at a time; a thief can only
//    run one steal at a time.
//  - Steals in progress are cancelled by a server restart (cooldowns survive).
//  - /grab takes 1..GrabMaxStacks random non-empty slots, and a random
//    1..stack-size amount from each.
#pragma once

#include "Core/BOOwnership.h"
#include "Core/BOTypes.h"

#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace bo::core
{
	enum class StealKind : std::uint8_t { Grab, Hack };

	const char* ToString(StealKind Kind);

	enum class StealStartResult : std::uint8_t
	{
		Ok,
		NotNeeded,      // the thief can already open it
		NotStealable,   // unowned, exempt, or not a storage container
		CooldownActive,
		ContainerBusy,  // someone else is stealing from it
		AlreadyStealing,
		TooFar,
	};

	const char* ToString(StealStartResult Result);

	enum class StealEventType : std::uint8_t
	{
		HalfwayAlert, // tell the owning team (only them)
		Completed,    // Grab: take items now. Hack: access window closed.
		Cancelled,
	};

	enum class CancelReason : std::uint8_t
	{
		None,
		ThiefDied,
		WalkedAway,
		ThiefLeft,       // disconnected
		ContainerGone,   // destroyed or dismantled
		ServerRestart,
	};

	const char* ToString(CancelReason Reason);

	struct ActiveSteal
	{
		PlayerId Thief;
		BuildingKey Container = 0;
		PlayerId Owner; // owner when the steal started
		StealKind Kind = StealKind::Grab;
		double StartedAt = 0.0;
		double EndsAt = 0.0;
		bool bAlerted = false;
	};

	struct StealEvent
	{
		StealEventType Type = StealEventType::Cancelled;
		ActiveSteal Steal;
		CancelReason Reason = CancelReason::None;
	};

	struct StealTimings
	{
		double GrabSeconds = 120.0;
		double HackSeconds = 300.0;
		double GrabCooldownSeconds = 3600.0;
		double HackCooldownSeconds = 18000.0;
		double WalkAwayMeters = 10.0;
	};

	class StealManager
	{
	public:
		StealTimings Timings;

		// Caller has already established the target is an owned storage container
		// (bStealable) and whether the thief could open it normally (bCanAlreadyOpen).
		StealStartResult Start(const PlayerId& Thief, BuildingKey Container, const PlayerId& Owner, StealKind Kind,
			bool bStealable, bool bCanAlreadyOpen, double DistanceMeters, double Now);

		// Advance time. DistanceOf returns the thief's distance from the container
		// in metres, or a negative number if the container no longer exists.
		std::vector<StealEvent> Tick(double Now, const std::function<double(const ActiveSteal&)>& DistanceOf);

		// Taking damage doesn't cancel; these do.
		std::vector<StealEvent> OnPlayerDied(const PlayerId& Player);
		std::vector<StealEvent> OnPlayerLeft(const PlayerId& Player);
		std::vector<StealEvent> OnContainerRemoved(BuildingKey Container);

		// The /hack exception: true while Thief has an active hack on Container.
		bool HasHackAccess(const PlayerId& Thief, BuildingKey Container) const;

		const ActiveSteal* ActiveFor(const PlayerId& Thief) const;
		double CooldownRemaining(BuildingKey Container, StealKind Kind, double Now) const;

		// Persistence: cooldowns as seconds remaining. Active steals are not
		// saved; CancelAllForRestart reports them so the thief can be told.
		std::vector<StealEvent> CancelAllForRestart();
		std::map<std::pair<BuildingKey, StealKind>, double> ExportCooldowns(double Now) const;
		void ImportCooldowns(const std::map<std::pair<BuildingKey, StealKind>, double>& Remaining, double Now);

	private:
		std::vector<StealEvent> CancelWhere(const std::function<bool(const ActiveSteal&)>& Pred, CancelReason Reason);

		std::map<PlayerId, ActiveSteal> Active; // by thief
		std::map<std::pair<BuildingKey, StealKind>, double> CooldownUntil;
	};

	// --- /grab item selection ------------------------------------------------

	struct ItemStack
	{
		std::string Item; // item class name; empty = empty slot
		int Count = 0;
	};

	struct ItemTake
	{
		std::size_t Slot = 0;
		int Count = 0;
	};

	std::vector<ItemTake> PickGrabItems(const std::vector<ItemStack>& Slots, int MaxStacks, std::mt19937& Rng);
}
