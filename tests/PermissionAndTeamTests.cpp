// Unit tests for the game-independent rule logic.
#include "Core/BOPermissions.h"
#include "Core/BOTeams.h"

#include "TestHarness.h"

#include <string>
#include <vector>

using namespace bo::core;

namespace
{
	ActorState Player(const std::string& Id) { return ActorState{Id, false, false}; }
	ActorState Admin(const std::string& Id, bool Bypass) { return ActorState{Id, true, Bypass}; }
	BuildingState OwnedBy(const std::string& Owner, bool Locked = false) { return BuildingState{Owner, Locked, false}; }

	bool Allowed(const ActorState& A, const BuildingState& B, Action Act, const TeamRegistry& T)
	{
		return Check(A, B, Act, T).bAllowed;
	}

	const std::vector<Action> FullActions = {Action::Dismantle, Action::Upgrade, Action::Paint};
	const std::vector<Action> AccessActions = {
		Action::ChangeMachine, Action::OpenStorage, Action::UseSwitch, Action::ConfigureSplitter,
		Action::ConfigureStation, Action::DriveVehicle, Action::Connect, Action::BuildOnto, Action::PickFromBelt};
	const std::vector<Action> AnyoneActions = {Action::ViewMachine, Action::UseDoor};
}

// --- Permissions -------------------------------------------------------

TEST(OwnerAndTeammateCanDoEverything)
{
	TeamRegistry T;
	EXPECT(T.Create("alice", "Alpha") == TeamResult::Ok);
	EXPECT(T.InvitePlayer("alice", "bob", 0) == TeamResult::Ok);
	EXPECT(T.Accept("bob", 1) == TeamResult::Ok);

	for (Action A : FullActions)
	{
		EXPECT(Allowed(Player("alice"), OwnedBy("alice"), A, T));
		EXPECT(Allowed(Player("bob"), OwnedBy("alice"), A, T));
	}
	for (Action A : AccessActions)
	{
		EXPECT(Allowed(Player("bob"), OwnedBy("alice"), A, T));
	}
}

TEST(StrangerCanOnlyLookAndUseDoors)
{
	TeamRegistry T;
	for (Action A : FullActions)
	{
		const CheckResult R = Check(Player("eve"), OwnedBy("alice"), A, T);
		EXPECT(!R.bAllowed);
		EXPECT(R.Deny == DenyReason::NotPermitted);
	}
	for (Action A : AccessActions)
	{
		EXPECT(!Allowed(Player("eve"), OwnedBy("alice"), A, T));
	}
	for (Action A : AnyoneActions)
	{
		EXPECT(Allowed(Player("eve"), OwnedBy("alice"), A, T));
	}
}

TEST(StrangerCannotOpenStorage)
{
	TeamRegistry T;
	EXPECT(!Allowed(Player("eve"), OwnedBy("alice"), Action::OpenStorage, T));
	EXPECT(Allowed(Player("eve"), OwnedBy("alice"), Action::ViewMachine, T));
	EXPECT(!Allowed(Player("eve"), OwnedBy("alice"), Action::ChangeMachine, T));
}

TEST(UnownedAndExemptAreOpen)
{
	TeamRegistry T;
	BuildingState Unowned;
	BuildingState Exempt = OwnedBy("alice");
	Exempt.bExempt = true;
	for (Action A : FullActions)
	{
		EXPECT(Allowed(Player("eve"), Unowned, A, T));
		EXPECT(Allowed(Player("eve"), Exempt, A, T));
	}
	EXPECT(Check(Player("eve"), Unowned, Action::Dismantle, T).Allow == AllowReason::ExemptOrUnowned);
}

TEST(LocksShutOutTeammatesAndNonBypassAdmins)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Accept("bob", 1);
	const BuildingState Locked = OwnedBy("alice", true);

	for (Action A : {Action::Dismantle, Action::OpenStorage, Action::UseDoor, Action::ViewMachine})
	{
		EXPECT(Allowed(Player("alice"), Locked, A, T));
		const CheckResult Bob = Check(Player("bob"), Locked, A, T);
		EXPECT(!Bob.bAllowed);
		EXPECT(Bob.Deny == DenyReason::Locked);
		EXPECT(!Allowed(Admin("root", false), Locked, A, T));
		EXPECT(Allowed(Admin("root", true), Locked, A, T));
	}
}

TEST(BypassOnlyCountsForAdmins)
{
	TeamRegistry T;
	ActorState Fake{"eve", false, true}; // bypass flag without admin rights
	EXPECT(!Allowed(Fake, OwnedBy("alice"), Action::Dismantle, T));
	EXPECT(Check(Admin("root", true), OwnedBy("alice"), Action::Dismantle, T).Allow == AllowReason::AdminBypass);
}

TEST(SoloTrustGivesAccessNotFull)
{
	TeamRegistry T;
	EXPECT(T.Trust("alice", "carol") == TeamResult::Ok);
	for (Action A : AccessActions)
	{
		EXPECT(Allowed(Player("carol"), OwnedBy("alice"), A, T));
	}
	for (Action A : FullActions)
	{
		EXPECT(!Allowed(Player("carol"), OwnedBy("alice"), A, T));
	}
	EXPECT(LevelFor("carol", "alice", T) == AccessLevel::Access);
}

TEST(TeamOfOneCanStillTrust)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	EXPECT(T.IsSolo("alice"));
	EXPECT(T.Trust("alice", "carol") == TeamResult::Ok);
	EXPECT(Allowed(Player("carol"), OwnedBy("alice"), Action::Connect, T));
}

TEST(JoiningABiggerTeamEndsTrust)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.Trust("alice", "carol");
	T.InvitePlayer("alice", "bob", 0);
	EXPECT(T.Accept("bob", 1) == TeamResult::Ok);
	EXPECT(!T.IsSolo("alice"));
	EXPECT(T.TrustListOf("alice").empty());
	EXPECT(!Allowed(Player("carol"), OwnedBy("alice"), Action::Connect, T));
	EXPECT(T.Trust("alice", "carol") == TeamResult::NotSolo);

	// The joiner's own solo trust list ends too.
	TeamRegistry U;
	U.Trust("dave", "carol");
	U.Create("alice", "Alpha");
	U.InvitePlayer("alice", "dave", 0);
	U.Accept("dave", 1);
	EXPECT(U.TrustListOf("dave").empty());
}

TEST(TeamsAreWalledOff)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.Create("zed", "Zulu");
	EXPECT(!Allowed(Player("zed"), OwnedBy("alice"), Action::OpenStorage, T));
	EXPECT(!Allowed(Player("alice"), OwnedBy("zed"), Action::Dismantle, T));
}

TEST(KickedPlayerKeepsBuildingsAndLosesAccess)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Accept("bob", 1);
	EXPECT(Allowed(Player("alice"), OwnedBy("bob"), Action::Dismantle, T));
	EXPECT(T.Kick("alice", "bob") == TeamResult::Ok);
	// Bob still owns his buildings (ownership isn't team-based); access is severed both ways.
	EXPECT(Allowed(Player("bob"), OwnedBy("bob"), Action::Dismantle, T));
	EXPECT(!Allowed(Player("alice"), OwnedBy("bob"), Action::Dismantle, T));
	EXPECT(!Allowed(Player("bob"), OwnedBy("alice"), Action::OpenStorage, T));
}

TEST(RequiredLevelsMatchPlan)
{
	for (Action A : FullActions) EXPECT(RequiredLevel(A) == AccessLevel::Full);
	for (Action A : AccessActions) EXPECT(RequiredLevel(A) == AccessLevel::Access);
	for (Action A : AnyoneActions) EXPECT(RequiredLevel(A) == AccessLevel::None);
}

// --- Teams -------------------------------------------------------------

TEST(CreateValidatesNames)
{
	TeamRegistry T;
	EXPECT(T.Create("alice", "") == TeamResult::InvalidName);
	EXPECT(T.Create("alice", "   ") == TeamResult::InvalidName);
	EXPECT(T.Create("alice", "This name is far too long for a team") == TeamResult::InvalidName);
	EXPECT(T.Create("alice", "Bad<name>") == TeamResult::InvalidName);
	EXPECT(T.Create("alice", " Alpha ") == TeamResult::Ok);
	EXPECT(T.TeamOf("alice")->Name == "Alpha");
	EXPECT(T.Create("bob", "alpha") == TeamResult::NameTaken);
	EXPECT(T.Create("alice", "Beta") == TeamResult::AlreadyInTeam);
}

TEST(OnlyLeaderAndOfficersInvite)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Accept("bob", 0);
	EXPECT(T.InvitePlayer("bob", "carol", 0) == TeamResult::NotAllowed);
	EXPECT(T.Promote("alice", "bob") == TeamResult::Ok);
	EXPECT(T.RoleOf("bob") == TeamRole::Officer);
	EXPECT(T.InvitePlayer("bob", "carol", 0) == TeamResult::Ok);
	EXPECT(T.InvitePlayer("eve", "carol", 0) == TeamResult::NotInTeam);
	EXPECT(T.InvitePlayer("alice", "alice", 0) == TeamResult::TargetIsSelf);
}

TEST(InvitesExpireAfterFiveMinutes)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 100);
	EXPECT(T.PendingInvite("bob", 399) != nullptr);
	EXPECT(T.PendingInvite("bob", 401) == nullptr);
	EXPECT(T.Accept("bob", 401) == TeamResult::InviteExpired);
	EXPECT(T.Accept("bob", 402) == TeamResult::NoInvite);
	EXPECT(T.TeamOf("bob") == nullptr);

	T.InvitePlayer("alice", "bob", 500);
	EXPECT(T.Decline("bob") == TeamResult::Ok);
	EXPECT(T.Accept("bob", 501) == TeamResult::NoInvite);
}

TEST(CannotInviteSomeoneInABiggerTeam)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.Create("zed", "Zulu");
	T.InvitePlayer("zed", "yan", 0);
	T.Accept("yan", 0);
	EXPECT(T.InvitePlayer("alice", "zed", 0) == TeamResult::AlreadyInTeam);
}

TEST(AcceptingFromATeamOfOneDissolvesIt)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.Create("bob", "Bravo");
	T.InvitePlayer("alice", "bob", 0);
	EXPECT(T.Accept("bob", 0) == TeamResult::Ok);
	EXPECT(T.FindTeamByName("Bravo") == nullptr);
	EXPECT(T.TeamOf("bob")->Name == "Alpha");
}

TEST(OfficersCannotKickLeaderOrOfficers)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	for (const char* P : {"bob", "carol", "dave"})
	{
		T.InvitePlayer("alice", P, 0);
		T.Accept(P, 0);
	}
	T.Promote("alice", "bob");
	T.Promote("alice", "carol");
	EXPECT(T.Kick("bob", "alice") == TeamResult::NotAllowed);
	EXPECT(T.Kick("bob", "carol") == TeamResult::NotAllowed);
	EXPECT(T.Kick("dave", "bob") == TeamResult::NotAllowed);
	EXPECT(T.Kick("bob", "dave") == TeamResult::Ok);
	EXPECT(T.Kick("alice", "carol") == TeamResult::Ok);
	EXPECT(T.Kick("alice", "alice") == TeamResult::TargetIsSelf);
	EXPECT(T.Kick("alice", "eve") == TeamResult::TargetNotInTeam);
}

TEST(OnlyLeaderPromotesTransfersDisbands)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Accept("bob", 0);
	T.InvitePlayer("alice", "carol", 0);
	T.Accept("carol", 0);
	T.Promote("alice", "bob");
	EXPECT(T.Promote("bob", "carol") == TeamResult::NotAllowed);
	EXPECT(T.Demote("bob", "bob") == TeamResult::NotAllowed);
	EXPECT(T.Disband("bob") == TeamResult::NotAllowed);
	EXPECT(T.TransferLeadership("bob", "carol") == TeamResult::NotAllowed);

	EXPECT(T.TransferLeadership("alice", "carol") == TeamResult::Ok);
	EXPECT(T.RoleOf("carol") == TeamRole::Leader);
	EXPECT(T.RoleOf("alice") == TeamRole::Officer);
	EXPECT(T.Demote("carol", "alice") == TeamResult::Ok);
	EXPECT(T.RoleOf("alice") == TeamRole::Member);

	EXPECT(T.Disband("carol") == TeamResult::Ok);
	EXPECT(T.TeamOf("alice") == nullptr);
	EXPECT(T.TeamOf("bob") == nullptr);
}

TEST(LeaderMustTransferBeforeLeaving)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Accept("bob", 0);
	EXPECT(T.Leave("alice") == TeamResult::LeaderMustTransfer);
	EXPECT(T.Leave("bob") == TeamResult::Ok);
	EXPECT(T.Leave("alice") == TeamResult::Ok); // now a team of one: leaving dissolves it
	EXPECT(T.FindTeamByName("Alpha") == nullptr);
	EXPECT(T.Leave("alice") == TeamResult::NotInTeam);
}

TEST(DisbandCancelsInvitesToThatTeam)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Disband("alice");
	EXPECT(T.PendingInvite("bob", 1) == nullptr);
	EXPECT(T.Accept("bob", 1) == TeamResult::NoInvite);
}

TEST(TrustRules)
{
	TeamRegistry T;
	EXPECT(T.Trust("alice", "alice") == TeamResult::TargetIsSelf);
	EXPECT(T.Trust("alice", "bob") == TeamResult::Ok);
	EXPECT(T.Trust("alice", "bob") == TeamResult::AlreadyTrusted);
	EXPECT(T.Untrust("alice", "carol") == TeamResult::NotTrusted);
	EXPECT(T.Untrust("alice", "bob") == TeamResult::Ok);
	EXPECT(!T.IsTrustedBy("alice", "bob"));
}

TEST(TeamColorsAreUniqueAndReused)
{
	TeamRegistry T;
	T.Create("a", "A");
	T.Create("b", "B");
	T.Create("c", "C");
	EXPECT(T.TeamOf("a")->ColorIndex == 0);
	EXPECT(T.TeamOf("b")->ColorIndex == 1);
	EXPECT(T.TeamOf("c")->ColorIndex == 2);
	T.Disband("b");
	T.Create("d", "D");
	EXPECT(T.TeamOf("d")->ColorIndex == 1);
}

TEST(RestoreRoundTrip)
{
	TeamRegistry T;
	T.Create("alice", "Alpha");
	T.InvitePlayer("alice", "bob", 0);
	T.Accept("bob", 0);
	T.Promote("alice", "bob");
	T.Trust("solo", "helper");

	std::vector<Team> Saved;
	for (const Team* Tm : T.AllTeams()) Saved.push_back(*Tm);

	TeamRegistry R;
	R.Restore(Saved, T.RawTrust(), T.NextTeamId());
	EXPECT(R.SameTeam("alice", "bob"));
	EXPECT(R.RoleOf("bob") == TeamRole::Officer);
	EXPECT(R.IsTrustedBy("solo", "helper"));
	TeamId NewId = 0;
	R.Create("zed", "Zulu", &NewId);
	EXPECT(NewId == T.NextTeamId());
}
