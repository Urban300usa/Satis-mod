// Team scores, milestone bonuses and per-team AWESOME coupons.
#pragma once

#include "Core/BOTypes.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bo::core
{
	struct TeamScore
	{
		long long Score = 0;           // running total, never goes down
		long long Coupons = 0;         // spendable balance
		long long CouponProgress = 0;  // sink points toward the next coupon
		long long CouponsEarned = 0;   // lifetime, feeds the coupon cost curve
		std::set<std::string> Milestones; // unlocked milestone/tier IDs
	};

	class Scoreboard
	{
	public:
		long long MilestoneBonusPoints = 10000;

		// Points needed for the next coupon, given how many the team has earned.
		// The game side plugs in the game's own coupon curve. Default: flat 1000.
		std::function<long long(long long CouponsEarnedSoFar)> PointsForNextCoupon =
			[](long long) { return 1000LL; };

		// Items fed into the team's own Sink. Returns coupons earned by this call.
		long long AddSinkPoints(TeamId Team, long long Points);

		// True the first time this team unlocks MilestoneId: adds the bonus and
		// the caller announces it. Later calls for the same milestone do nothing.
		bool OnMilestoneUnlocked(TeamId Team, const std::string& MilestoneId);

		bool SpendCoupons(TeamId Team, long long Amount);
		void GrantCoupons(TeamId Team, long long Amount); // admin / refunds

		const TeamScore* Find(TeamId Team) const;
		long long ScoreOf(TeamId Team) const;
		long long CouponsOf(TeamId Team) const;

		// Highest score first; ties by team ID for a stable order.
		std::vector<std::pair<TeamId, long long>> Ranking() const;

		const std::map<TeamId, TeamScore>& Raw() const { return Scores; }
		void Restore(std::map<TeamId, TeamScore> In) { Scores = std::move(In); }

	private:
		std::map<TeamId, TeamScore> Scores;
	};
}
