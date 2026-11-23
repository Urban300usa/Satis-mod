#include "Core/BOCommands.h"

#include <algorithm>
#include <cctype>

namespace bo::core
{
	namespace
	{
		std::string Lower(std::string S)
		{
			std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
			return S;
		}

		std::string JoinFrom(const std::vector<std::string>& Args, std::size_t Start)
		{
			std::string Out;
			for (std::size_t I = Start; I < Args.size(); ++I)
			{
				if (!Out.empty())
				{
					Out += ' ';
				}
				Out += Args[I];
			}
			return Out;
		}

		const char* RoleName(TeamRole Role)
		{
			switch (Role)
			{
			case TeamRole::Leader: return "leader";
			case TeamRole::Officer: return "officer";
			case TeamRole::Member: return "member";
			}
			return "member";
		}

		void NotifyTeam(CommandOutput& Out, const Team& T, const PlayerId& Except, const std::string& Message)
		{
			for (const PlayerId& P : T.Members)
			{
				if (P != Except)
				{
					Out.Notify.emplace_back(P, Message);
				}
			}
		}

		// Resolves Args[Index] to a player, or writes an error reply.
		std::optional<PlayerId> NeedPlayer(const CommandContext& Ctx, const std::vector<std::string>& Args,
			std::size_t Index, CommandOutput& Out, const char* Usage)
		{
			if (Args.size() <= Index)
			{
				Out.Reply.push_back(std::string("Usage: ") + Usage);
				return std::nullopt;
			}
			const std::string Name = JoinFrom(Args, Index);
			std::optional<PlayerId> Id = Ctx.FindPlayer ? Ctx.FindPlayer(Name) : std::nullopt;
			if (!Id)
			{
				Out.Reply.push_back("No online player matches \"" + Name + "\".");
			}
			return Id;
		}

		std::string Name(const CommandContext& Ctx, const PlayerId& Id)
		{
			return Ctx.NameOf ? Ctx.NameOf(Id) : Id;
		}
	}

	std::string TeamHelp()
	{
		return "Team commands: /team create <name>, invite <player>, accept, decline, kick <player>, "
			"promote <player>, demote <player>, leader <player>, leave, disband, info [team]";
	}

	CommandOutput RunTeamCommand(const CommandContext& Ctx, const std::vector<std::string>& Args, TeamRegistry& Teams)
	{
		CommandOutput Out;
		if (Args.empty())
		{
			Out.Reply.push_back(TeamHelp());
			return Out;
		}
		const std::string Sub = Lower(Args[0]);
		const std::string Me = Name(Ctx, Ctx.Self);

		if (Sub == "create")
		{
			const std::string TeamName = JoinFrom(Args, 1);
			const TeamResult R = Teams.Create(Ctx.Self, TeamName);
			if (R == TeamResult::Ok)
			{
				const Team* T = Teams.TeamOf(Ctx.Self);
				Out.Reply.push_back("Created team " + T->Name + ". You're the leader.");
				Out.Audit.push_back("team:create\t" + T->Name);
			}
			else
			{
				Out.Reply.push_back(R == TeamResult::AlreadyInTeam ? "You're already in a team." : ToString(R));
			}
			return Out;
		}

		if (Sub == "invite")
		{
			const auto Target = NeedPlayer(Ctx, Args, 1, Out, "/team invite <player>");
			if (!Target)
			{
				return Out;
			}
			const TeamResult R = Teams.InvitePlayer(Ctx.Self, *Target, Ctx.Now);
			if (R == TeamResult::Ok)
			{
				const Team* T = Teams.TeamOf(Ctx.Self);
				const int Minutes = static_cast<int>(Teams.InviteLifetimeSeconds / 60.0 + 0.5);
				Out.Reply.push_back("Invited " + Name(Ctx, *Target) + " to " + T->Name + ".");
				Out.Notify.emplace_back(*Target, Me + " invited you to team " + T->Name +
					". Type /team accept or /team decline (expires in " + std::to_string(Minutes) + " min).");
				Out.Audit.push_back("team:invite\t" + Name(Ctx, *Target) + " to " + T->Name);
			}
			else
			{
				Out.Reply.push_back(ToString(R));
			}
			return Out;
		}

		if (Sub == "accept")
		{
			const Invite* Inv = Teams.PendingInvite(Ctx.Self, Ctx.Now);
			const TeamId Joining = Inv ? Inv->Team : 0;
			const TeamResult R = Teams.Accept(Ctx.Self, Ctx.Now);
			if (R == TeamResult::Ok)
			{
				const Team* T = Teams.FindTeam(Joining);
				Out.Reply.push_back("You joined " + T->Name + ".");
				NotifyTeam(Out, *T, Ctx.Self, Me + " joined the team.");
				Out.Audit.push_back("team:join\t" + T->Name);
			}
			else
			{
				Out.Reply.push_back(R == TeamResult::AlreadyInTeam ? "Leave your current team first." : ToString(R));
			}
			return Out;
		}

		if (Sub == "decline")
		{
			const Invite* Inv = Teams.PendingInvite(Ctx.Self, Ctx.Now);
			const PlayerId Inviter = Inv ? Inv->InvitedBy : PlayerId();
			const TeamResult R = Teams.Decline(Ctx.Self);
			Out.Reply.push_back(R == TeamResult::Ok ? "Invite declined." : ToString(R));
			if (R == TeamResult::Ok && !Inviter.empty())
			{
				Out.Notify.emplace_back(Inviter, Me + " declined your team invite.");
			}
			return Out;
		}

		if (Sub == "kick" || Sub == "promote" || Sub == "demote" || Sub == "leader")
		{
			const std::string Usage = "/team " + Sub + " <player>";
			const auto Target = NeedPlayer(Ctx, Args, 1, Out, Usage.c_str());
			if (!Target)
			{
				return Out;
			}
			const Team* Before = Teams.TeamOf(Ctx.Self);
			const std::string TeamName = Before ? Before->Name : "";
			TeamResult R = TeamResult::Ok;
			if (Sub == "kick") R = Teams.Kick(Ctx.Self, *Target);
			else if (Sub == "promote") R = Teams.Promote(Ctx.Self, *Target);
			else if (Sub == "demote") R = Teams.Demote(Ctx.Self, *Target);
			else R = Teams.TransferLeadership(Ctx.Self, *Target);

			if (R != TeamResult::Ok)
			{
				Out.Reply.push_back(ToString(R));
				return Out;
			}
			const std::string Them = Name(Ctx, *Target);
			if (Sub == "kick")
			{
				Out.Reply.push_back("Removed " + Them + " from " + TeamName + ".");
				Out.Notify.emplace_back(*Target, "You were removed from " + TeamName + " by " + Me + ". Your buildings stay yours.");
				NotifyTeam(Out, *Teams.TeamOf(Ctx.Self), Ctx.Self, Them + " was removed from the team by " + Me + ".");
			}
			else if (Sub == "promote")
			{
				Out.Reply.push_back(Them + " is now an officer.");
				Out.Notify.emplace_back(*Target, Me + " made you an officer of " + TeamName + ".");
			}
			else if (Sub == "demote")
			{
				Out.Reply.push_back(Them + " is now a member.");
				Out.Notify.emplace_back(*Target, Me + " made you a regular member of " + TeamName + ".");
			}
			else
			{
				Out.Reply.push_back(Them + " now leads " + TeamName + ". You're an officer.");
				NotifyTeam(Out, *Teams.TeamOf(Ctx.Self), Ctx.Self, Them + " is the new leader of " + TeamName + ".");
			}
			Out.Audit.push_back("team:" + Sub + "\t" + Them + " in " + TeamName);
			return Out;
		}

		if (Sub == "leave")
		{
			const Team* Before = Teams.TeamOf(Ctx.Self);
			const std::string TeamName = Before ? Before->Name : "";
			const TeamId Id = Before ? Before->Id : 0;
			const TeamResult R = Teams.Leave(Ctx.Self);
			if (R == TeamResult::Ok)
			{
				Out.Reply.push_back("You left " + TeamName + ". Your buildings stay yours.");
				if (const Team* After = Teams.FindTeam(Id))
				{
					NotifyTeam(Out, *After, Ctx.Self, Me + " left the team.");
				}
				Out.Audit.push_back("team:leave\t" + TeamName);
			}
			else
			{
				Out.Reply.push_back(ToString(R));
			}
			return Out;
		}

		if (Sub == "disband")
		{
			const Team* Before = Teams.TeamOf(Ctx.Self);
			if (!Before)
			{
				Out.Reply.push_back(ToString(TeamResult::NotInTeam));
				return Out;
			}
			const Team Copy = *Before;
			const TeamResult R = Teams.Disband(Ctx.Self);
			if (R == TeamResult::Ok)
			{
				Out.Reply.push_back("Disbanded " + Copy.Name + ".");
				NotifyTeam(Out, Copy, Ctx.Self, Me + " disbanded " + Copy.Name + ". Your buildings stay yours.");
				Out.Audit.push_back("team:disband\t" + Copy.Name);
			}
			else
			{
				Out.Reply.push_back(ToString(R));
			}
			return Out;
		}

		if (Sub == "info")
		{
			const std::string Query = JoinFrom(Args, 1);
			const Team* T = Query.empty() ? Teams.TeamOf(Ctx.Self) : Teams.FindTeamByName(Query);
			if (!T)
			{
				Out.Reply.push_back(Query.empty() ? "You're not in a team. Try /team info <team>." : "No team called \"" + Query + "\".");
				return Out;
			}
			Out.Reply.push_back(T->Name + " (" + std::to_string(T->Members.size()) + " members)");
			std::vector<std::pair<int, std::string>> Rows;
			for (const PlayerId& P : T->Members)
			{
				const TeamRole Role = Teams.RoleOf(P);
				Rows.emplace_back(-static_cast<int>(Role), Name(Ctx, P) + " - " + RoleName(Role));
			}
			std::sort(Rows.begin(), Rows.end());
			for (const auto& Row : Rows)
			{
				Out.Reply.push_back("  " + Row.second);
			}
			return Out;
		}

		Out.Reply.push_back(TeamHelp());
		return Out;
	}

	CommandOutput RunTrustCommand(const CommandContext& Ctx, const std::vector<std::string>& Args, TeamRegistry& Teams)
	{
		CommandOutput Out;
		if (Args.empty())
		{
			Out.Reply.push_back("Usage: /trust <player>, /trust list");
			return Out;
		}
		if (Lower(Args[0]) == "list" && Args.size() == 1)
		{
			if (!Teams.IsSolo(Ctx.Self))
			{
				Out.Reply.push_back(ToString(TeamResult::NotSolo));
				return Out;
			}
			const std::vector<PlayerId> List = Teams.TrustListOf(Ctx.Self);
			if (List.empty())
			{
				Out.Reply.push_back("You don't trust anyone.");
				return Out;
			}
			std::string Line = "You trust: ";
			for (std::size_t I = 0; I < List.size(); ++I)
			{
				Line += (I ? ", " : "") + Name(Ctx, List[I]);
			}
			Out.Reply.push_back(Line);
			return Out;
		}

		const auto Target = NeedPlayer(Ctx, Args, 0, Out, "/trust <player>");
		if (!Target)
		{
			return Out;
		}
		const TeamResult R = Teams.Trust(Ctx.Self, *Target);
		if (R == TeamResult::Ok)
		{
			const std::string Them = Name(Ctx, *Target);
			Out.Reply.push_back("You now trust " + Them + ". They can use, connect to and build on your buildings.");
			Out.Notify.emplace_back(*Target, Name(Ctx, Ctx.Self) + " trusted you: you can use, connect to and build on their buildings.");
			Out.Audit.push_back("trust:add\t" + Them);
		}
		else
		{
			Out.Reply.push_back(ToString(R));
		}
		return Out;
	}

	CommandOutput RunUntrustCommand(const CommandContext& Ctx, const std::vector<std::string>& Args, TeamRegistry& Teams)
	{
		CommandOutput Out;
		const auto Target = NeedPlayer(Ctx, Args, 0, Out, "/untrust <player>");
		if (!Target)
		{
			return Out;
		}
		const TeamResult R = Teams.Untrust(Ctx.Self, *Target);
		if (R == TeamResult::Ok)
		{
			const std::string Them = Name(Ctx, *Target);
			Out.Reply.push_back("You no longer trust " + Them + ".");
			Out.Notify.emplace_back(*Target, Name(Ctx, Ctx.Self) + " no longer trusts you.");
			Out.Audit.push_back("trust:remove\t" + Them);
		}
		else
		{
			Out.Reply.push_back(ToString(R));
		}
		return Out;
	}

	CommandOutput RunScoresCommand(const CommandContext&, const Scoreboard& Scores, const TeamRegistry& Teams)
	{
		CommandOutput Out;
		// Every existing team appears, even with 0 points.
		std::vector<std::pair<long long, const Team*>> Rows;
		for (const Team* T : Teams.AllTeams())
		{
			Rows.emplace_back(Scores.ScoreOf(T->Id), T);
		}
		if (Rows.empty())
		{
			Out.Reply.push_back("No teams yet. Make one with /team create <name>.");
			return Out;
		}
		std::sort(Rows.begin(), Rows.end(), [](const auto& A, const auto& B)
		{
			return A.first != B.first ? A.first > B.first : A.second->Id < B.second->Id;
		});
		Out.Reply.push_back("Scores:");
		for (std::size_t I = 0; I < Rows.size(); ++I)
		{
			Out.Reply.push_back(std::to_string(I + 1) + ". " + Rows[I].second->Name + " - " + std::to_string(Rows[I].first));
		}
		return Out;
	}
}
