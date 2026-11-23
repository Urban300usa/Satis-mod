#include "Core/BOSteals.h"

#include <algorithm>

namespace bo::core
{
	const char* ToString(StealKind Kind)
	{
		return Kind == StealKind::Grab ? "grab" : "hack";
	}

	const char* ToString(StealStartResult Result)
	{
		switch (Result)
		{
		case StealStartResult::Ok: return "OK.";
		case StealStartResult::NotNeeded: return "You can already open that.";
		case StealStartResult::NotStealable: return "You can only steal from another player's storage container.";
		case StealStartResult::CooldownActive: return "That container was hit recently. Try again later.";
		case StealStartResult::ContainerBusy: return "Someone else is already stealing from that container.";
		case StealStartResult::AlreadyStealing: return "You're already stealing from something.";
		case StealStartResult::TooFar: return "You're too far away from that container.";
		}
		return "Unknown result.";
	}

	const char* ToString(CancelReason Reason)
	{
		switch (Reason)
		{
		case CancelReason::None: return "none";
		case CancelReason::ThiefDied: return "the thief died";
		case CancelReason::WalkedAway: return "the thief walked away";
		case CancelReason::ThiefLeft: return "the thief left the server";
		case CancelReason::ContainerGone: return "the container is gone";
		case CancelReason::ServerRestart: return "the server restarted";
		}
		return "unknown";
	}

	StealStartResult StealManager::Start(const PlayerId& Thief, BuildingKey Container, const PlayerId& Owner, StealKind Kind,
		bool bStealable, bool bCanAlreadyOpen, double DistanceMeters, double Now)
	{
		if (!bStealable)
		{
			return StealStartResult::NotStealable;
		}
		if (bCanAlreadyOpen)
		{
			return StealStartResult::NotNeeded;
		}
		if (Active.count(Thief))
		{
			return StealStartResult::AlreadyStealing;
		}
		for (const auto& [Id, S] : Active)
		{
			if (S.Container == Container)
			{
				return StealStartResult::ContainerBusy;
			}
		}
		if (CooldownRemaining(Container, Kind, Now) > 0.0)
		{
			return StealStartResult::CooldownActive;
		}
		if (DistanceMeters < 0.0 || DistanceMeters > Timings.WalkAwayMeters)
		{
			return StealStartResult::TooFar;
		}

		const bool bGrab = Kind == StealKind::Grab;
		ActiveSteal S;
		S.Thief = Thief;
		S.Container = Container;
		S.Owner = Owner;
		S.Kind = Kind;
		S.StartedAt = Now;
		S.EndsAt = Now + (bGrab ? Timings.GrabSeconds : Timings.HackSeconds);
		Active[Thief] = S;
		CooldownUntil[{Container, Kind}] = Now + (bGrab ? Timings.GrabCooldownSeconds : Timings.HackCooldownSeconds);
		return StealStartResult::Ok;
	}

	std::vector<StealEvent> StealManager::Tick(double Now, const std::function<double(const ActiveSteal&)>& DistanceOf)
	{
		std::vector<StealEvent> Events;
		for (auto It = Active.begin(); It != Active.end();)
		{
			ActiveSteal& S = It->second;
			const double Distance = DistanceOf(S);
			CancelReason Cancel = CancelReason::None;
			if (Distance < 0.0)
			{
				Cancel = CancelReason::ContainerGone;
			}
			else if (Distance > Timings.WalkAwayMeters)
			{
				Cancel = CancelReason::WalkedAway;
			}
			if (Cancel != CancelReason::None)
			{
				Events.push_back({StealEventType::Cancelled, S, Cancel});
				It = Active.erase(It);
				continue;
			}

			const double Halfway = S.StartedAt + (S.EndsAt - S.StartedAt) / 2.0;
			if (!S.bAlerted && Now >= Halfway)
			{
				S.bAlerted = true;
				Events.push_back({StealEventType::HalfwayAlert, S, CancelReason::None});
			}
			if (Now >= S.EndsAt)
			{
				Events.push_back({StealEventType::Completed, S, CancelReason::None});
				It = Active.erase(It);
				continue;
			}
			++It;
		}
		return Events;
	}

	std::vector<StealEvent> StealManager::CancelWhere(const std::function<bool(const ActiveSteal&)>& Pred, CancelReason Reason)
	{
		std::vector<StealEvent> Events;
		for (auto It = Active.begin(); It != Active.end();)
		{
			if (Pred(It->second))
			{
				Events.push_back({StealEventType::Cancelled, It->second, Reason});
				It = Active.erase(It);
			}
			else
			{
				++It;
			}
		}
		return Events;
	}

	std::vector<StealEvent> StealManager::OnPlayerDied(const PlayerId& Player)
	{
		return CancelWhere([&](const ActiveSteal& S) { return S.Thief == Player; }, CancelReason::ThiefDied);
	}

	std::vector<StealEvent> StealManager::OnPlayerLeft(const PlayerId& Player)
	{
		return CancelWhere([&](const ActiveSteal& S) { return S.Thief == Player; }, CancelReason::ThiefLeft);
	}

	std::vector<StealEvent> StealManager::OnContainerRemoved(BuildingKey Container)
	{
		return CancelWhere([&](const ActiveSteal& S) { return S.Container == Container; }, CancelReason::ContainerGone);
	}

	std::vector<StealEvent> StealManager::CancelAllForRestart()
	{
		return CancelWhere([](const ActiveSteal&) { return true; }, CancelReason::ServerRestart);
	}

	bool StealManager::HasHackAccess(const PlayerId& Thief, BuildingKey Container) const
	{
		const auto It = Active.find(Thief);
		return It != Active.end() && It->second.Kind == StealKind::Hack && It->second.Container == Container;
	}

	const ActiveSteal* StealManager::ActiveFor(const PlayerId& Thief) const
	{
		const auto It = Active.find(Thief);
		return It == Active.end() ? nullptr : &It->second;
	}

	double StealManager::CooldownRemaining(BuildingKey Container, StealKind Kind, double Now) const
	{
		const auto It = CooldownUntil.find({Container, Kind});
		return It == CooldownUntil.end() ? 0.0 : std::max(0.0, It->second - Now);
	}

	std::map<std::pair<BuildingKey, StealKind>, double> StealManager::ExportCooldowns(double Now) const
	{
		std::map<std::pair<BuildingKey, StealKind>, double> Out;
		for (const auto& [Key, Until] : CooldownUntil)
		{
			if (Until > Now)
			{
				Out[Key] = Until - Now;
			}
		}
		return Out;
	}

	void StealManager::ImportCooldowns(const std::map<std::pair<BuildingKey, StealKind>, double>& Remaining, double Now)
	{
		CooldownUntil.clear();
		for (const auto& [Key, Seconds] : Remaining)
		{
			if (Seconds > 0.0)
			{
				CooldownUntil[Key] = Now + Seconds;
			}
		}
	}

	std::vector<ItemTake> PickGrabItems(const std::vector<ItemStack>& Slots, int MaxStacks, std::mt19937& Rng)
	{
		std::vector<std::size_t> NonEmpty;
		for (std::size_t I = 0; I < Slots.size(); ++I)
		{
			if (!Slots[I].Item.empty() && Slots[I].Count > 0)
			{
				NonEmpty.push_back(I);
			}
		}
		if (NonEmpty.empty() || MaxStacks <= 0)
		{
			return {};
		}
		std::shuffle(NonEmpty.begin(), NonEmpty.end(), Rng);
		const int Limit = std::min<int>(MaxStacks, static_cast<int>(NonEmpty.size()));
		const int Picks = std::uniform_int_distribution<int>(1, Limit)(Rng);

		std::vector<ItemTake> Out;
		for (int I = 0; I < Picks; ++I)
		{
			const std::size_t Slot = NonEmpty[static_cast<std::size_t>(I)];
			const int Amount = std::uniform_int_distribution<int>(1, Slots[Slot].Count)(Rng);
			Out.push_back({Slot, Amount});
		}
		return Out;
	}
}
