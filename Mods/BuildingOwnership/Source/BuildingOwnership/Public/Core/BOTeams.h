// Teams, roles, invites and solo-player trust. Plain C++, no game types.
#pragma once

#include "Core/BOTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace bo::core
{
	enum class TeamRole : std::uint8_t
	{
		Member,
		Officer,
		Leader,
	};

	struct Team
	{
		TeamId Id = 0;
		std::string Name;
		PlayerId Leader;
		std::set<PlayerId> Officers;
		std::set<PlayerId> Members; // everyone, leader and officers included
		int ColorIndex = 0;         // index into the team color palette
	};

	struct Invite
	{
		TeamId Team = 0;
		PlayerId InvitedBy;
		double ExpiresAt = 0.0; // seconds, same clock as the "Now" arguments
	};

	enum class TeamResult : std::uint8_t
	{
		Ok,
		AlreadyInTeam,      // player is already in a team with other members
		NotInTeam,
		NameTaken,
		InvalidName,
		NotAllowed,         // caller's role can't do this
		TargetNotInTeam,    // target isn't in the caller's team
		TargetIsSelf,
		NoInvite,
		InviteExpired,
		LeaderMustTransfer, // leader of a team with others must transfer or disband
		NotSolo,            // trust is only for a team of one / no team
		AlreadyTrusted,
		NotTrusted,
	};

	const char* ToString(TeamResult Result);

	// Owns all team state. Time is passed in as seconds so tests can control it.
	class TeamRegistry
	{
	public:
		static constexpr int NumColors = 12;

		double InviteLifetimeSeconds = 300.0;

		// --- Queries ---
		const Team* FindTeam(TeamId Id) const;
		const Team* FindTeamByName(const std::string& Name) const;
		const Team* TeamOf(const PlayerId& Player) const;
		TeamRole RoleOf(const PlayerId& Player) const; // Member if not in a team
		bool SameTeam(const PlayerId& A, const PlayerId& B) const;
		// A player with no team, or in a team of one.
		bool IsSolo(const PlayerId& Player) const;
		// True if Owner is solo and has trusted Player.
		bool IsTrustedBy(const PlayerId& Owner, const PlayerId& Player) const;
		std::vector<PlayerId> TrustListOf(const PlayerId& Owner) const;
		const Invite* PendingInvite(const PlayerId& Player, double Now) const;
		std::vector<const Team*> AllTeams() const;

		// --- Team management ---
		TeamResult Create(const PlayerId& Player, const std::string& Name, TeamId* OutId = nullptr);
		TeamResult InvitePlayer(const PlayerId& By, const PlayerId& Target, double Now);
		TeamResult Accept(const PlayerId& Player, double Now);
		TeamResult Decline(const PlayerId& Player);
		TeamResult Kick(const PlayerId& By, const PlayerId& Target);
		TeamResult Promote(const PlayerId& By, const PlayerId& Target);
		TeamResult Demote(const PlayerId& By, const PlayerId& Target);
		TeamResult TransferLeadership(const PlayerId& By, const PlayerId& Target);
		TeamResult Leave(const PlayerId& Player);
		TeamResult Disband(const PlayerId& By);

		// --- Solo trust ---
		TeamResult Trust(const PlayerId& Owner, const PlayerId& Target);
		TeamResult Untrust(const PlayerId& Owner, const PlayerId& Target);

		// --- Persistence helpers ---
		// Restore state exactly as saved. Rebuilds the player index.
		void Restore(std::vector<Team> InTeams, std::map<PlayerId, std::set<PlayerId>> InTrust, TeamId InNextId);
		const std::map<PlayerId, std::set<PlayerId>>& RawTrust() const { return TrustLists; }
		TeamId NextTeamId() const { return NextId; }

	private:
		Team* MutableTeamOf(const PlayerId& Player);
		void RemoveTeam(TeamId Id);
		void AddMember(Team& T, const PlayerId& Player);
		void OnTeamGrew(Team& T);
		int PickColor() const;

		std::map<TeamId, Team> Teams;
		std::map<PlayerId, TeamId> PlayerTeam;
		std::map<PlayerId, std::set<PlayerId>> TrustLists;
		std::map<PlayerId, Invite> Invites;
		TeamId NextId = 1;
	};
}
