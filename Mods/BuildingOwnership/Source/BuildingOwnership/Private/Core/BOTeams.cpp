#include "Core/BOTeams.h"

#include <algorithm>
#include <cctype>

namespace bo::core
{
	namespace
	{
		constexpr std::size_t MaxTeamNameLength = 24;

		std::string Trim(const std::string& S)
		{
			const auto Begin = std::find_if_not(S.begin(), S.end(), [](unsigned char C) { return std::isspace(C); });
			const auto End = std::find_if_not(S.rbegin(), S.rend(), [](unsigned char C) { return std::isspace(C); }).base();
			return Begin < End ? std::string(Begin, End) : std::string();
		}

		std::string Lower(std::string S)
		{
			std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
			return S;
		}

		bool IsValidName(const std::string& Name)
		{
			if (Name.empty() || Name.size() > MaxTeamNameLength)
			{
				return false;
			}
			return std::all_of(Name.begin(), Name.end(), [](unsigned char C)
			{
				return std::isalnum(C) || C == ' ' || C == '-' || C == '_';
			});
		}

		bool CanManageMembers(TeamRole Role)
		{
			return Role == TeamRole::Leader || Role == TeamRole::Officer;
		}
	}

	const char* ToString(TeamResult Result)
	{
		switch (Result)
		{
		case TeamResult::Ok: return "OK.";
		case TeamResult::AlreadyInTeam: return "That player is already in a team.";
		case TeamResult::NotInTeam: return "You are not in a team.";
		case TeamResult::NameTaken: return "That team name is taken.";
		case TeamResult::InvalidName: return "Team names are 1-24 letters, numbers, spaces, - or _.";
		case TeamResult::NotAllowed: return "Your team role can't do that.";
		case TeamResult::TargetNotInTeam: return "That player isn't in your team.";
		case TeamResult::TargetIsSelf: return "You can't do that to yourself.";
		case TeamResult::NoInvite: return "You have no pending invite.";
		case TeamResult::InviteExpired: return "That invite has expired.";
		case TeamResult::LeaderMustTransfer: return "Transfer leadership or disband the team before leaving.";
		case TeamResult::NotSolo: return "Only a solo player (team of one) can manage trust.";
		case TeamResult::AlreadyTrusted: return "You already trust that player.";
		case TeamResult::NotTrusted: return "You don't trust that player.";
		}
		return "Unknown result.";
	}

	// --- Queries ---------------------------------------------------------

	const Team* TeamRegistry::FindTeam(TeamId Id) const
	{
		const auto It = Teams.find(Id);
		return It == Teams.end() ? nullptr : &It->second;
	}

	const Team* TeamRegistry::FindTeamByName(const std::string& Name) const
	{
		const std::string Key = Lower(Trim(Name));
		for (const auto& [Id, T] : Teams)
		{
			if (Lower(T.Name) == Key)
			{
				return &T;
			}
		}
		return nullptr;
	}

	const Team* TeamRegistry::TeamOf(const PlayerId& Player) const
	{
		const auto It = PlayerTeam.find(Player);
		return It == PlayerTeam.end() ? nullptr : FindTeam(It->second);
	}

	TeamRole TeamRegistry::RoleOf(const PlayerId& Player) const
	{
		const Team* T = TeamOf(Player);
		if (!T)
		{
			return TeamRole::Member;
		}
		if (T->Leader == Player)
		{
			return TeamRole::Leader;
		}
		return T->Officers.count(Player) ? TeamRole::Officer : TeamRole::Member;
	}

	bool TeamRegistry::SameTeam(const PlayerId& A, const PlayerId& B) const
	{
		const auto ItA = PlayerTeam.find(A);
		const auto ItB = PlayerTeam.find(B);
		return ItA != PlayerTeam.end() && ItB != PlayerTeam.end() && ItA->second == ItB->second;
	}

	bool TeamRegistry::IsSolo(const PlayerId& Player) const
	{
		const Team* T = TeamOf(Player);
		return !T || T->Members.size() <= 1;
	}

	bool TeamRegistry::IsTrustedBy(const PlayerId& Owner, const PlayerId& Player) const
	{
		if (!IsSolo(Owner))
		{
			return false;
		}
		const auto It = TrustLists.find(Owner);
		return It != TrustLists.end() && It->second.count(Player) > 0;
	}

	std::vector<PlayerId> TeamRegistry::TrustListOf(const PlayerId& Owner) const
	{
		const auto It = TrustLists.find(Owner);
		if (It == TrustLists.end())
		{
			return {};
		}
		return {It->second.begin(), It->second.end()};
	}

	const Invite* TeamRegistry::PendingInvite(const PlayerId& Player, double Now) const
	{
		const auto It = Invites.find(Player);
		if (It == Invites.end() || It->second.ExpiresAt < Now || !FindTeam(It->second.Team))
		{
			return nullptr;
		}
		return &It->second;
	}

	std::vector<const Team*> TeamRegistry::AllTeams() const
	{
		std::vector<const Team*> Out;
		Out.reserve(Teams.size());
		for (const auto& [Id, T] : Teams)
		{
			Out.push_back(&T);
		}
		return Out;
	}

	// --- Team management -------------------------------------------------

	TeamResult TeamRegistry::Create(const PlayerId& Player, const std::string& RawName, TeamId* OutId)
	{
		if (TeamOf(Player))
		{
			return TeamResult::AlreadyInTeam;
		}
		const std::string Name = Trim(RawName);
		if (!IsValidName(Name))
		{
			return TeamResult::InvalidName;
		}
		if (FindTeamByName(Name))
		{
			return TeamResult::NameTaken;
		}

		Team T;
		T.Id = NextId++;
		T.Name = Name;
		T.Leader = Player;
		T.ColorIndex = PickColor();
		const TeamId Id = T.Id;
		Teams.emplace(Id, std::move(T));
		AddMember(Teams.at(Id), Player);
		Invites.erase(Player);
		if (OutId)
		{
			*OutId = Id;
		}
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::InvitePlayer(const PlayerId& By, const PlayerId& Target, double Now)
	{
		const Team* T = TeamOf(By);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (!CanManageMembers(RoleOf(By)))
		{
			return TeamResult::NotAllowed;
		}
		if (By == Target)
		{
			return TeamResult::TargetIsSelf;
		}
		// A player alone in a team of one may still be invited; they leave it on accept.
		if (!IsSolo(Target) || SameTeam(By, Target))
		{
			return TeamResult::AlreadyInTeam;
		}
		Invites[Target] = Invite{T->Id, By, Now + InviteLifetimeSeconds};
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::Accept(const PlayerId& Player, double Now)
	{
		const auto It = Invites.find(Player);
		if (It == Invites.end() || !FindTeam(It->second.Team))
		{
			Invites.erase(Player);
			return TeamResult::NoInvite;
		}
		if (It->second.ExpiresAt < Now)
		{
			Invites.erase(It);
			return TeamResult::InviteExpired;
		}
		if (!IsSolo(Player))
		{
			return TeamResult::AlreadyInTeam;
		}

		const TeamId Target = It->second.Team;
		Invites.erase(It);

		// Leaving a team of one dissolves it.
		if (const Team* Own = TeamOf(Player))
		{
			RemoveTeam(Own->Id);
		}

		Team& T = Teams.at(Target);
		AddMember(T, Player);
		OnTeamGrew(T);
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::Decline(const PlayerId& Player)
	{
		return Invites.erase(Player) ? TeamResult::Ok : TeamResult::NoInvite;
	}

	TeamResult TeamRegistry::Kick(const PlayerId& By, const PlayerId& Target)
	{
		Team* T = MutableTeamOf(By);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (By == Target)
		{
			return TeamResult::TargetIsSelf;
		}
		if (!SameTeam(By, Target))
		{
			return TeamResult::TargetNotInTeam;
		}
		const TeamRole ByRole = RoleOf(By);
		const TeamRole TargetRole = RoleOf(Target);
		if (!CanManageMembers(ByRole) || (ByRole == TeamRole::Officer && TargetRole != TeamRole::Member))
		{
			return TeamResult::NotAllowed;
		}
		T->Members.erase(Target);
		T->Officers.erase(Target);
		PlayerTeam.erase(Target);
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::Promote(const PlayerId& By, const PlayerId& Target)
	{
		Team* T = MutableTeamOf(By);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (RoleOf(By) != TeamRole::Leader)
		{
			return TeamResult::NotAllowed;
		}
		if (By == Target)
		{
			return TeamResult::TargetIsSelf;
		}
		if (!SameTeam(By, Target))
		{
			return TeamResult::TargetNotInTeam;
		}
		T->Officers.insert(Target);
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::Demote(const PlayerId& By, const PlayerId& Target)
	{
		Team* T = MutableTeamOf(By);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (RoleOf(By) != TeamRole::Leader)
		{
			return TeamResult::NotAllowed;
		}
		if (By == Target)
		{
			return TeamResult::TargetIsSelf;
		}
		if (!SameTeam(By, Target))
		{
			return TeamResult::TargetNotInTeam;
		}
		T->Officers.erase(Target);
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::TransferLeadership(const PlayerId& By, const PlayerId& Target)
	{
		Team* T = MutableTeamOf(By);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (RoleOf(By) != TeamRole::Leader)
		{
			return TeamResult::NotAllowed;
		}
		if (By == Target)
		{
			return TeamResult::TargetIsSelf;
		}
		if (!SameTeam(By, Target))
		{
			return TeamResult::TargetNotInTeam;
		}
		T->Leader = Target;
		T->Officers.erase(Target);
		T->Officers.insert(By); // old leader stays on as an officer
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::Leave(const PlayerId& Player)
	{
		Team* T = MutableTeamOf(Player);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (T->Leader == Player)
		{
			if (T->Members.size() > 1)
			{
				return TeamResult::LeaderMustTransfer;
			}
			RemoveTeam(T->Id);
			return TeamResult::Ok;
		}
		T->Members.erase(Player);
		T->Officers.erase(Player);
		PlayerTeam.erase(Player);
		return TeamResult::Ok;
	}

	TeamResult TeamRegistry::Disband(const PlayerId& By)
	{
		const Team* T = TeamOf(By);
		if (!T)
		{
			return TeamResult::NotInTeam;
		}
		if (T->Leader != By)
		{
			return TeamResult::NotAllowed;
		}
		RemoveTeam(T->Id);
		return TeamResult::Ok;
	}

	// --- Solo trust ------------------------------------------------------

	TeamResult TeamRegistry::Trust(const PlayerId& Owner, const PlayerId& Target)
	{
		if (!IsSolo(Owner))
		{
			return TeamResult::NotSolo;
		}
		if (Owner == Target)
		{
			return TeamResult::TargetIsSelf;
		}
		return TrustLists[Owner].insert(Target).second ? TeamResult::Ok : TeamResult::AlreadyTrusted;
	}

	TeamResult TeamRegistry::Untrust(const PlayerId& Owner, const PlayerId& Target)
	{
		if (!IsSolo(Owner))
		{
			return TeamResult::NotSolo;
		}
		const auto It = TrustLists.find(Owner);
		if (It == TrustLists.end() || It->second.erase(Target) == 0)
		{
			return TeamResult::NotTrusted;
		}
		if (It->second.empty())
		{
			TrustLists.erase(It);
		}
		return TeamResult::Ok;
	}

	// --- Persistence -----------------------------------------------------

	void TeamRegistry::Restore(std::vector<Team> InTeams, std::map<PlayerId, std::set<PlayerId>> InTrust, TeamId InNextId)
	{
		Teams.clear();
		PlayerTeam.clear();
		Invites.clear(); // invites are short-lived and not saved
		TrustLists = std::move(InTrust);
		NextId = InNextId;
		for (Team& T : InTeams)
		{
			NextId = std::max(NextId, T.Id + 1);
			for (const PlayerId& P : T.Members)
			{
				PlayerTeam[P] = T.Id;
			}
			const TeamId Id = T.Id;
			Teams.emplace(Id, std::move(T));
		}
	}

	// --- Private ---------------------------------------------------------

	Team* TeamRegistry::MutableTeamOf(const PlayerId& Player)
	{
		const auto It = PlayerTeam.find(Player);
		if (It == PlayerTeam.end())
		{
			return nullptr;
		}
		const auto TeamIt = Teams.find(It->second);
		return TeamIt == Teams.end() ? nullptr : &TeamIt->second;
	}

	void TeamRegistry::RemoveTeam(TeamId Id)
	{
		const auto It = Teams.find(Id);
		if (It == Teams.end())
		{
			return;
		}
		for (const PlayerId& P : It->second.Members)
		{
			PlayerTeam.erase(P);
		}
		for (auto InvIt = Invites.begin(); InvIt != Invites.end();)
		{
			InvIt = InvIt->second.Team == Id ? Invites.erase(InvIt) : std::next(InvIt);
		}
		Teams.erase(It);
	}

	void TeamRegistry::AddMember(Team& T, const PlayerId& Player)
	{
		T.Members.insert(Player);
		PlayerTeam[Player] = T.Id;
	}

	void TeamRegistry::OnTeamGrew(Team& T)
	{
		// Joining a bigger team ends every member's solo trust list.
		if (T.Members.size() > 1)
		{
			for (const PlayerId& P : T.Members)
			{
				TrustLists.erase(P);
			}
		}
	}

	int TeamRegistry::PickColor() const
	{
		std::set<int> Used;
		for (const auto& [Id, T] : Teams)
		{
			Used.insert(T.ColorIndex);
		}
		for (int I = 0; I < NumColors; ++I)
		{
			if (!Used.count(I))
			{
				return I;
			}
		}
		return static_cast<int>(NextId % NumColors);
	}
}
