#include "Core/BOScoring.h"

#include <algorithm>

namespace bo::core
{
	long long Scoreboard::AddSinkPoints(TeamId Team, long long Points)
	{
		if (Points <= 0)
		{
			return 0;
		}
		TeamScore& S = Scores[Team];
		S.Score += Points;
		S.CouponProgress += Points;
		long long Earned = 0;
		for (;;)
		{
			const long long Need = std::max(1LL, PointsForNextCoupon(S.CouponsEarned));
			if (S.CouponProgress < Need)
			{
				break;
			}
			S.CouponProgress -= Need;
			++S.CouponsEarned;
			++S.Coupons;
			++Earned;
		}
		return Earned;
	}

	bool Scoreboard::OnMilestoneUnlocked(TeamId Team, const std::string& MilestoneId)
	{
		TeamScore& S = Scores[Team];
		if (!S.Milestones.insert(MilestoneId).second)
		{
			return false;
		}
		S.Score += MilestoneBonusPoints;
		return true;
	}

	bool Scoreboard::SpendCoupons(TeamId Team, long long Amount)
	{
		const auto It = Scores.find(Team);
		if (Amount < 0 || It == Scores.end() || It->second.Coupons < Amount)
		{
			return false;
		}
		It->second.Coupons -= Amount; // score is untouched
		return true;
	}

	void Scoreboard::GrantCoupons(TeamId Team, long long Amount)
	{
		if (Amount > 0)
		{
			Scores[Team].Coupons += Amount;
		}
	}

	const TeamScore* Scoreboard::Find(TeamId Team) const
	{
		const auto It = Scores.find(Team);
		return It == Scores.end() ? nullptr : &It->second;
	}

	long long Scoreboard::ScoreOf(TeamId Team) const
	{
		const TeamScore* S = Find(Team);
		return S ? S->Score : 0;
	}

	long long Scoreboard::CouponsOf(TeamId Team) const
	{
		const TeamScore* S = Find(Team);
		return S ? S->Coupons : 0;
	}

	std::vector<std::pair<TeamId, long long>> Scoreboard::Ranking() const
	{
		std::vector<std::pair<TeamId, long long>> Out;
		for (const auto& [Team, S] : Scores)
		{
			Out.emplace_back(Team, S.Score);
		}
		std::sort(Out.begin(), Out.end(), [](const auto& A, const auto& B)
		{
			return A.second != B.second ? A.second > B.second : A.first < B.first;
		});
		return Out;
	}
}
