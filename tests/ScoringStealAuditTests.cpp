#include "Core/BOAudit.h"
#include "Core/BOScoring.h"
#include "Core/BOSteals.h"
#include "TestHarness.h"

#include <set>

using namespace bo::core;

// --- Scoring -------------------------------------------------------------

TEST(SinkPointsScoreAndEarnCoupons)
{
	Scoreboard S;
	EXPECT(S.AddSinkPoints(1, 2500) == 2);
	EXPECT(S.ScoreOf(1) == 2500);
	EXPECT(S.CouponsOf(1) == 2);
	EXPECT(S.AddSinkPoints(1, 500) == 1); // 500 left over + 500
	EXPECT(S.CouponsOf(1) == 3);
	EXPECT(S.AddSinkPoints(1, -50) == 0);
	EXPECT(S.ScoreOf(2) == 0);
}

TEST(SpendingCouponsNeverLowersScore)
{
	Scoreboard S;
	S.AddSinkPoints(1, 3000);
	EXPECT(S.SpendCoupons(1, 2));
	EXPECT(S.CouponsOf(1) == 1);
	EXPECT(S.ScoreOf(1) == 3000);
	EXPECT(!S.SpendCoupons(1, 5));
	EXPECT(!S.SpendCoupons(2, 1)); // team 2 can't spend team 1's coupons
	EXPECT(!S.SpendCoupons(1, -1));
}

TEST(CouponCurveIsPluggable)
{
	Scoreboard S;
	S.PointsForNextCoupon = [](long long Earned) { return 100 * (Earned + 1); }; // 100, 200, 300...
	EXPECT(S.AddSinkPoints(1, 600) == 3);
	EXPECT(S.Find(1)->CouponProgress == 0);
}

TEST(MilestoneBonusOncePerTeam)
{
	Scoreboard S;
	EXPECT(S.OnMilestoneUnlocked(1, "Tier4"));
	EXPECT(!S.OnMilestoneUnlocked(1, "Tier4"));
	EXPECT(S.OnMilestoneUnlocked(2, "Tier4"));
	EXPECT(S.ScoreOf(1) == 10000);
	EXPECT(S.CouponsOf(1) == 0);
}

TEST(RankingOrder)
{
	Scoreboard S;
	S.AddSinkPoints(3, 100);
	S.AddSinkPoints(1, 500);
	S.AddSinkPoints(2, 500);
	const auto R = S.Ranking();
	EXPECT(R.size() == 3);
	EXPECT(R[0].first == 1 && R[1].first == 2 && R[2].first == 3);
}

// --- Steals --------------------------------------------------------------

namespace
{
	auto Near = [](const ActiveSteal&) { return 2.0; };
	auto Far = [](const ActiveSteal&) { return 50.0; };
	auto Gone = [](const ActiveSteal&) { return -1.0; };

	int CountType(const std::vector<StealEvent>& E, StealEventType T)
	{
		int N = 0;
		for (const auto& Ev : E) N += Ev.Type == T;
		return N;
	}
}

TEST(StealStartChecks)
{
	StealManager M;
	EXPECT(M.Start("eve", 1, "alice", StealKind::Grab, false, false, 1, 0) == StealStartResult::NotStealable);
	EXPECT(M.Start("eve", 1, "alice", StealKind::Grab, true, true, 1, 0) == StealStartResult::NotNeeded);
	EXPECT(M.Start("eve", 1, "alice", StealKind::Grab, true, false, 11, 0) == StealStartResult::TooFar);
	EXPECT(M.Start("eve", 1, "alice", StealKind::Grab, true, false, 1, 0) == StealStartResult::Ok);
	EXPECT(M.Start("eve", 2, "alice", StealKind::Grab, true, false, 1, 0) == StealStartResult::AlreadyStealing);
	EXPECT(M.Start("mal", 1, "alice", StealKind::Hack, true, false, 1, 0) == StealStartResult::ContainerBusy);
}

TEST(GrabTimingAlertAndCooldown)
{
	StealManager M;
	M.Start("eve", 1, "alice", StealKind::Grab, true, false, 1, 0);
	EXPECT(M.Tick(59, Near).empty());
	auto E = M.Tick(60, Near);
	EXPECT(E.size() == 1 && E[0].Type == StealEventType::HalfwayAlert && E[0].Steal.Owner == "alice");
	EXPECT(M.Tick(61, Near).empty()); // alert fires once
	E = M.Tick(120, Near);
	EXPECT(CountType(E, StealEventType::Completed) == 1);
	EXPECT(M.ActiveFor("eve") == nullptr);

	// 1-hour cooldown per container, from the start of the steal.
	EXPECT(M.Start("mal", 1, "alice", StealKind::Grab, true, false, 1, 3599) == StealStartResult::CooldownActive);
	EXPECT(M.Start("mal", 1, "alice", StealKind::Grab, true, false, 1, 3600) == StealStartResult::Ok);
}

TEST(GrabAndHackCooldownsAreSeparate)
{
	StealManager M;
	M.Start("eve", 1, "alice", StealKind::Grab, true, false, 1, 0);
	M.Tick(120, Near);
	EXPECT(M.Start("eve", 1, "alice", StealKind::Hack, true, false, 1, 130) == StealStartResult::Ok);
}

TEST(HackGrantsTimedAccessWithHalfwayAlert)
{
	StealManager M;
	M.Start("eve", 1, "alice", StealKind::Hack, true, false, 1, 0);
	EXPECT(M.HasHackAccess("eve", 1));
	EXPECT(!M.HasHackAccess("eve", 2));
	EXPECT(!M.HasHackAccess("mal", 1));
	EXPECT(M.Tick(149, Near).empty());
	EXPECT(CountType(M.Tick(150, Near), StealEventType::HalfwayAlert) == 1);
	EXPECT(M.HasHackAccess("eve", 1));
	EXPECT(CountType(M.Tick(300, Near), StealEventType::Completed) == 1);
	EXPECT(!M.HasHackAccess("eve", 1));
	EXPECT(M.CooldownRemaining(1, StealKind::Hack, 300) == 18000 - 300);
}

TEST(DamageDoesNotCancelButDeathAndWalkingAwayDo)
{
	StealManager M;
	M.Start("eve", 1, "alice", StealKind::Hack, true, false, 1, 0);
	// (There is no damage hook into StealManager: taking damage changes nothing.)
	auto E = M.OnPlayerDied("eve");
	EXPECT(E.size() == 1 && E[0].Reason == CancelReason::ThiefDied);
	EXPECT(!M.HasHackAccess("eve", 1));

	M.Start("eve", 2, "alice", StealKind::Grab, true, false, 1, 0);
	E = M.Tick(10, Far);
	EXPECT(E.size() == 1 && E[0].Reason == CancelReason::WalkedAway);

	M.Start("eve", 3, "alice", StealKind::Grab, true, false, 1, 0);
	E = M.Tick(10, Gone);
	EXPECT(E.size() == 1 && E[0].Reason == CancelReason::ContainerGone);

	M.Start("eve", 4, "alice", StealKind::Grab, true, false, 1, 0);
	EXPECT(M.OnPlayerLeft("eve").size() == 1);
	M.Start("eve", 5, "alice", StealKind::Grab, true, false, 1, 0);
	EXPECT(M.OnContainerRemoved(5).size() == 1);
	EXPECT(M.OnPlayerDied("nobody").empty());
}

TEST(CancelledStealStillTriggersCooldown)
{
	StealManager M;
	M.Start("eve", 1, "alice", StealKind::Grab, true, false, 1, 0);
	M.OnPlayerDied("eve");
	EXPECT(M.Start("eve", 1, "alice", StealKind::Grab, true, false, 1, 5) == StealStartResult::CooldownActive);
}

TEST(CooldownsSurviveRestart)
{
	StealManager M;
	M.Start("eve", 1, "alice", StealKind::Hack, true, false, 1, 1000);
	const auto Cancelled = M.CancelAllForRestart();
	EXPECT(Cancelled.size() == 1 && Cancelled[0].Reason == CancelReason::ServerRestart);
	const auto Saved = M.ExportCooldowns(1100);

	StealManager Fresh; // after restart the clock starts again from 0
	Fresh.ImportCooldowns(Saved, 0);
	EXPECT(Fresh.CooldownRemaining(1, StealKind::Hack, 0) == 18000 - 100);
	EXPECT(Fresh.CooldownRemaining(1, StealKind::Grab, 0) == 0);
}

TEST(GrabItemPicks)
{
	std::mt19937 Rng(42);
	const std::vector<ItemStack> Slots = {{"Iron", 100}, {"", 0}, {"Copper", 5}, {"Wire", 1}, {"Screw", 0}};
	std::set<std::size_t> SeenCounts;
	for (int Run = 0; Run < 200; ++Run)
	{
		const auto Picks = PickGrabItems(Slots, 3, Rng);
		EXPECT(!Picks.empty() && Picks.size() <= 3);
		SeenCounts.insert(Picks.size());
		std::set<std::size_t> Used;
		for (const auto& T : Picks)
		{
			EXPECT(Used.insert(T.Slot).second);                 // no slot twice
			EXPECT(T.Slot != 1 && T.Slot != 4);                 // never empty slots
			EXPECT(T.Count >= 1 && T.Count <= Slots[T.Slot].Count);
		}
	}
	EXPECT(SeenCounts.size() == 3); // 1, 2 and 3 stacks all happen
	EXPECT(PickGrabItems({{"", 0}}, 3, Rng).empty());
	EXPECT(PickGrabItems(Slots, 0, Rng).empty());
}

// --- Audit ---------------------------------------------------------------

TEST(AuditLineFormat)
{
	AuditEntry E{"2026-11-23T10:00:00Z", "Eve (id1)", "deny:dismantle", "Constructor", "1,2,3", "Alice (id2)", "not\tpermitted\n"};
	EXPECT(FormatAuditLine(E) == "2026-11-23T10:00:00Z\tEve (id1)\tdeny:dismantle\tConstructor\t1,2,3\tAlice (id2)\tnot permitted ");
	EXPECT(AuditHeader().find("time\tplayer") == 0);
}
