#include "Core/BOOwnership.h"
#include "Core/BOPermissions.h"
#include "Core/BOTransfers.h"
#include "TestHarness.h"

#include <map>

using namespace bo::core;

namespace
{
	ActorState P(const std::string& Id) { return ActorState{Id, false, false}; }

	TeamRegistry TeamOf(std::initializer_list<const char*> Members)
	{
		TeamRegistry T;
		auto It = Members.begin();
		T.Create(*It, "Team");
		for (++It; It != Members.end(); ++It)
		{
			T.InvitePlayer(*Members.begin(), *It, 0);
			T.Accept(*It, 0);
		}
		return T;
	}
}

TEST(RecordsAndStateOf)
{
	OwnershipRegistry O;
	O.SetOwner(1, "alice");
	EXPECT(O.Find(1)->Owner == "alice");
	EXPECT(O.StateOf(1, false).Owner == std::optional<PlayerId>("alice"));
	EXPECT(!O.StateOf(2, false).Owner.has_value());
	EXPECT(O.StateOf(1, true).bExempt);
	EXPECT(O.Remove(1));
	EXPECT(!O.Remove(1));
}

TEST(LockingRules)
{
	OwnershipRegistry O;
	O.SetOwner(1, "alice");
	EXPECT(O.SetLocked(P("alice"), 1, true, false) == LockResult::Disabled);
	EXPECT(O.SetLocked(P("bob"), 1, true, true) == LockResult::NotOwner);
	EXPECT(O.SetLocked(P("alice"), 2, true, true) == LockResult::Unowned);
	EXPECT(O.SetLocked(P("alice"), 1, false, true) == LockResult::NotLocked);
	EXPECT(O.SetLocked(P("alice"), 1, true, true) == LockResult::Ok);
	EXPECT(O.SetLocked(P("alice"), 1, true, true) == LockResult::AlreadyLocked);
	EXPECT(O.SetLocked(ActorState{"root", true, false}, 1, false, true) == LockResult::NotOwner);
	EXPECT(O.SetLocked(ActorState{"root", true, true}, 1, false, true) == LockResult::Ok);
}

TEST(NewOwnerStartsUnlocked)
{
	OwnershipRegistry O;
	O.SetOwner(1, "alice");
	O.SetLocked(P("alice"), 1, true, true);
	O.SetOwner(1, "bob");
	EXPECT(!O.Find(1)->bLocked);
}

TEST(AdminTransferAndUnclaim)
{
	OwnershipRegistry O;
	O.SetOwner(1, "alice");
	O.SetOwner(2, "alice");
	O.SetOwner(3, "bob");
	EXPECT(O.TransferAll("alice", "carol") == 2);
	EXPECT(O.OwnedBy("carol").size() == 2);
	EXPECT(O.OwnedBy("alice").empty());
	EXPECT(O.UnclaimAll("carol") == 2);
	EXPECT(O.Count() == 1);
}

TEST(PlacementFromScratchBelongsToBuilder)
{
	OwnershipRegistry O;
	TeamRegistry T;
	const PlacementResult R = ResolvePlacement(P("alice"), {}, O, T);
	EXPECT(R.bAllowed);
	EXPECT(R.Owner == "alice");
}

TEST(HelperBuildGoesToAttachedOwner)
{
	OwnershipRegistry O;
	TeamRegistry T = TeamOf({"alice", "bob"});
	O.SetOwner(10, "alice");
	const PlacementResult R = ResolvePlacement(P("bob"), {{10, false, Action::BuildOnto}}, O, T);
	EXPECT(R.bAllowed);
	EXPECT(R.Owner == "alice");

	// Trusted helper of a solo owner, connecting a belt.
	TeamRegistry Solo;
	Solo.Trust("dave", "erin");
	O.SetOwner(20, "dave");
	const PlacementResult R2 = ResolvePlacement(P("erin"), {{20, false, Action::Connect}}, O, Solo);
	EXPECT(R2.bAllowed);
	EXPECT(R2.Owner == "dave");
}

TEST(AttachingOnlyToOwnUnownedOrExemptKeepsBuilder)
{
	OwnershipRegistry O;
	TeamRegistry T;
	O.SetOwner(1, "alice");
	O.SetOwner(2, "zed"); // exempt (e.g. HUB) even though recorded
	const PlacementResult R = ResolvePlacement(P("alice"),
		{{1, false, Action::BuildOnto}, {99, false, Action::BuildOnto}, {2, true, Action::Connect}}, O, T);
	EXPECT(R.bAllowed);
	EXPECT(R.Owner == "alice");
}

TEST(StrangerCannotAttach)
{
	OwnershipRegistry O;
	TeamRegistry T;
	O.SetOwner(10, "alice");
	O.SetOwner(11, "eve");
	const PlacementResult R = ResolvePlacement(P("eve"), {{11, false, Action::BuildOnto}, {10, false, Action::Connect}}, O, T);
	EXPECT(!R.bAllowed);
	EXPECT(R.BlockedBy == 10);
	EXPECT(R.Owner.empty());
}

TEST(LockedBuildingBlocksTeammateAttaching)
{
	OwnershipRegistry O;
	TeamRegistry T = TeamOf({"alice", "bob"});
	O.SetOwner(10, "alice");
	O.SetLocked(P("alice"), 10, true, true);
	const PlacementResult R = ResolvePlacement(P("bob"), {{10, false, Action::BuildOnto}}, O, T);
	EXPECT(!R.bAllowed);
	EXPECT(R.Deny == DenyReason::Locked);
}

TEST(HelperPiecesStayWithOwnerAfterBuilderKicked)
{
	OwnershipRegistry O;
	TeamRegistry T = TeamOf({"alice", "bob"});
	O.SetOwner(10, "alice");
	const PlacementResult R = ResolvePlacement(P("bob"), {{10, false, Action::BuildOnto}}, O, T);
	O.SetOwner(11, R.Owner);
	T.Kick("alice", "bob");
	EXPECT(O.Find(11)->Owner == "alice");
	EXPECT(!Check(P("bob"), O.StateOf(11, false), Action::Dismantle, T).bAllowed);
}

TEST(BlueprintOwnershipAndRefusal)
{
	OwnershipRegistry O;
	TeamRegistry T = TeamOf({"alice", "bob"});
	O.SetOwner(10, "alice");
	O.SetOwner(20, "eve");
	// Bob's blueprint touching Alice's foundation: allowed, and Bob owns it all.
	const PlacementResult Ok = ResolveBlueprintPlacement(P("bob"), {{10, false, Action::BuildOnto}}, O, T);
	EXPECT(Ok.bAllowed);
	EXPECT(Ok.Owner == "bob");
	// Touching Eve's building anywhere refuses the whole blueprint.
	const PlacementResult Bad = ResolveBlueprintPlacement(P("bob"),
		{{10, false, Action::BuildOnto}, {20, false, Action::Connect}}, O, T);
	EXPECT(!Bad.bAllowed);
	EXPECT(Bad.BlockedBy == 20);
}

// --- Groups and /give ----------------------------------------------------

namespace
{
	// 1-2-3-4 in a line, 3 also connects to 9 (someone else's), 4 to 5.
	std::map<BuildingKey, std::vector<BuildingKey>> Graph = {
		{1, {2}}, {2, {1, 3}}, {3, {2, 4, 9}}, {4, {3, 5}}, {5, {4}}, {9, {3}}};
	std::vector<BuildingKey> Neighbours(BuildingKey K) { return Graph[K]; }
}

TEST(GroupFloodFillStaysWithinOwner)
{
	OwnershipRegistry O;
	for (BuildingKey K : {1, 2, 3, 4, 5}) O.SetOwner(K, "alice");
	O.SetOwner(9, "eve");
	auto Mine = [&](BuildingKey K) { const auto* R = O.Find(K); return R && R->Owner == "alice"; };
	const GroupResult G = FindConnectedGroup(1, Neighbours, Mine, 100);
	EXPECT(G.Members.size() == 5);
	EXPECT(!G.bHitCap);
	EXPECT(G.Members.front() == 1);
	EXPECT(std::find(G.Members.begin(), G.Members.end(), 9) == G.Members.end());
}

TEST(GroupCap)
{
	auto All = [](BuildingKey) { return true; };
	const GroupResult G = FindConnectedGroup(1, Neighbours, All, 3);
	EXPECT(G.bHitCap);
	EXPECT(G.Members.size() == 3);
	const GroupResult Exact = FindConnectedGroup(1, Neighbours, All, 6);
	EXPECT(!Exact.bHitCap);
	EXPECT(Exact.Members.size() == 6);
	EXPECT(FindConnectedGroup(1, Neighbours, [](BuildingKey) { return false; }, 10).Members.empty());
}

TEST(GiveNeedsAcceptanceAndMovesGroup)
{
	OwnershipRegistry O;
	for (BuildingKey K : {1, 2, 3}) O.SetOwner(K, "alice");
	GiveOffers G;
	GroupResult Group{{1, 2, 3}, false};
	EXPECT(G.Offer("alice", "alice", Group, O, 0) == GiveResult::TargetIsSelf);
	EXPECT(G.Offer("eve", "bob", Group, O, 0) == GiveResult::NotOwner);
	EXPECT(G.Offer("alice", "bob", GroupResult{{1}, true}, O, 0) == GiveResult::GroupTooBig);
	EXPECT(G.Offer("alice", "bob", Group, O, 0) == GiveResult::Ok);
	EXPECT(O.Find(1)->Owner == "alice"); // nothing moves until accepted
	EXPECT(G.Pending("bob", 10) != nullptr);

	O.Remove(3); // dismantled while the offer was pending
	std::size_t Moved = 0;
	EXPECT(G.Accept("bob", 10, O, &Moved) == GiveResult::Ok);
	EXPECT(Moved == 2);
	EXPECT(O.Find(1)->Owner == "bob");
	EXPECT(G.Accept("bob", 11, O) == GiveResult::NoOffer);
}

TEST(GiveOfferExpiresAndDeclines)
{
	OwnershipRegistry O;
	O.SetOwner(1, "alice");
	GiveOffers G;
	G.Offer("alice", "bob", GroupResult{{1}, false}, O, 0);
	EXPECT(G.Pending("bob", 301) == nullptr);
	EXPECT(G.Accept("bob", 301, O) == GiveResult::OfferExpired);
	EXPECT(O.Find(1)->Owner == "alice");
	G.Offer("alice", "bob", GroupResult{{1}, false}, O, 400);
	EXPECT(G.Decline("bob") == GiveResult::Ok);
	EXPECT(G.Decline("bob") == GiveResult::NoOffer);
}

TEST(DenyMessageCooldown)
{
	MessageCooldown C(3.0);
	EXPECT(C.ShouldSend("eve", 0.0));
	EXPECT(!C.ShouldSend("eve", 2.9));
	EXPECT(C.ShouldSend("bob", 2.9));
	EXPECT(C.ShouldSend("eve", 3.0));
}
