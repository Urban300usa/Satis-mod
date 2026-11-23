#include "Core/BOCommands.h"
#include "TestHarness.h"

#include <map>

using namespace bo::core;

namespace
{
	std::map<std::string, std::string> Online = {{"Alice", "id-a"}, {"Bob", "id-b"}, {"Bobby", "id-bb"}, {"Carol", "id-c"}};

	CommandContext Ctx(const std::string& Self, double Now = 0)
	{
		CommandContext C;
		C.Self = Self;
		C.Now = Now;
		C.FindPlayer = [](const std::string& Name) -> std::optional<PlayerId>
		{
			for (const auto& [N, Id] : Online)
				if (N == Name) return Id;
			return std::nullopt;
		};
		C.NameOf = [](const PlayerId& Id)
		{
			for (const auto& [N, I] : Online)
				if (I == Id) return N;
			return Id;
		};
		return C;
	}

	std::vector<std::string> A(std::initializer_list<const char*> L) { return {L.begin(), L.end()}; }

	bool Contains(const std::string& Hay, const std::string& Needle) { return Hay.find(Needle) != std::string::npos; }
}

TEST(TeamCommandFlow)
{
	TeamRegistry T;
	auto O = RunTeamCommand(Ctx("id-a"), A({"create", "Iron", "Works"}), T);
	EXPECT(O.Reply.size() == 1 && Contains(O.Reply[0], "Created team Iron Works"));
	EXPECT(O.Audit.size() == 1);

	O = RunTeamCommand(Ctx("id-a"), A({"invite", "Bob"}), T);
	EXPECT(Contains(O.Reply[0], "Invited Bob"));
	EXPECT(O.Notify.size() == 1 && O.Notify[0].first == "id-b" && Contains(O.Notify[0].second, "/team accept"));

	O = RunTeamCommand(Ctx("id-b"), A({"ACCEPT"}), T);
	EXPECT(Contains(O.Reply[0], "joined Iron Works"));
	EXPECT(O.Notify.size() == 1 && O.Notify[0].first == "id-a");

	O = RunTeamCommand(Ctx("id-a"), A({"promote", "Bob"}), T);
	EXPECT(Contains(O.Reply[0], "officer"));

	O = RunTeamCommand(Ctx("id-a"), A({"info"}), T);
	EXPECT(O.Reply.size() == 3);
	EXPECT(Contains(O.Reply[1], "Alice - leader"));
	EXPECT(Contains(O.Reply[2], "Bob - officer"));

	O = RunTeamCommand(Ctx("id-c"), A({"info", "iron works"}), T);
	EXPECT(Contains(O.Reply[0], "Iron Works (2 members)"));

	O = RunTeamCommand(Ctx("id-a"), A({"leave"}), T);
	EXPECT(Contains(O.Reply[0], "Transfer leadership"));

	O = RunTeamCommand(Ctx("id-a"), A({"kick", "Bob"}), T);
	EXPECT(Contains(O.Reply[0], "Removed Bob"));
	EXPECT(O.Notify.size() == 1 && O.Notify[0].first == "id-b" && Contains(O.Notify[0].second, "buildings stay yours"));
}

TEST(TeamCommandErrors)
{
	TeamRegistry T;
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), {}, T).Reply[0], "Team commands"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"bogus"}), T).Reply[0], "Team commands"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"invite"}), T).Reply[0], "Usage: /team invite"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"invite", "Nobody"}), T).Reply[0], "No online player"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"invite", "Bob"}), T).Reply[0], "not in a team"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"accept"}), T).Reply[0], "no pending invite"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"create"}), T).Reply[0], "Team names"));
	RunTeamCommand(Ctx("id-a"), A({"create", "X"}), T);
	EXPECT(Contains(RunTeamCommand(Ctx("id-a"), A({"create", "Y"}), T).Reply[0], "already in a team"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-c"), A({"info"}), T).Reply[0], "not in a team"));
	EXPECT(Contains(RunTeamCommand(Ctx("id-c"), A({"info", "Nope"}), T).Reply[0], "No team called"));
}

TEST(DisbandNotifiesEveryone)
{
	TeamRegistry T;
	RunTeamCommand(Ctx("id-a"), A({"create", "X"}), T);
	RunTeamCommand(Ctx("id-a"), A({"invite", "Bob"}), T);
	RunTeamCommand(Ctx("id-b"), A({"accept"}), T);
	EXPECT(Contains(RunTeamCommand(Ctx("id-b"), A({"disband"}), T).Reply[0], "role can't"));
	const auto O = RunTeamCommand(Ctx("id-a"), A({"disband"}), T);
	EXPECT(Contains(O.Reply[0], "Disbanded X"));
	EXPECT(O.Notify.size() == 1 && O.Notify[0].first == "id-b");
	EXPECT(T.TeamOf("id-b") == nullptr);
}

TEST(TrustCommands)
{
	TeamRegistry T;
	EXPECT(Contains(RunTrustCommand(Ctx("id-a"), A({"list"}), T).Reply[0], "don't trust anyone"));
	auto O = RunTrustCommand(Ctx("id-a"), A({"Carol"}), T);
	EXPECT(Contains(O.Reply[0], "You now trust Carol"));
	EXPECT(O.Notify.size() == 1 && O.Notify[0].first == "id-c");
	EXPECT(Contains(RunTrustCommand(Ctx("id-a"), A({"list"}), T).Reply[0], "You trust: Carol"));
	EXPECT(Contains(RunUntrustCommand(Ctx("id-a"), A({"Carol"}), T).Reply[0], "no longer trust Carol"));
	EXPECT(Contains(RunUntrustCommand(Ctx("id-a"), A({"Carol"}), T).Reply[0], "don't trust that player"));
	EXPECT(Contains(RunTrustCommand(Ctx("id-a"), {}, T).Reply[0], "Usage"));

	// Players in a bigger team can't trust.
	RunTeamCommand(Ctx("id-a"), A({"create", "X"}), T);
	RunTeamCommand(Ctx("id-a"), A({"invite", "Bob"}), T);
	RunTeamCommand(Ctx("id-b"), A({"accept"}), T);
	EXPECT(Contains(RunTrustCommand(Ctx("id-a"), A({"Carol"}), T).Reply[0], "Only a solo player"));
}

TEST(ScoresCommand)
{
	TeamRegistry T;
	Scoreboard S;
	EXPECT(Contains(RunScoresCommand(Ctx("id-a"), S, T).Reply[0], "No teams yet"));
	TeamId A1 = 0, B1 = 0;
	T.Create("id-a", "Alpha", &A1);
	T.Create("id-b", "Bravo", &B1);
	S.AddSinkPoints(B1, 5000);
	const auto O = RunScoresCommand(Ctx("id-c"), S, T);
	EXPECT(O.Reply.size() == 3);
	EXPECT(O.Reply[1] == "1. Bravo - 5000");
	EXPECT(O.Reply[2] == "2. Alpha - 0");
}
