// Chat command logic for commands that need no game hooks: /team, /trust,
// /untrust, /scores. Game-side command classes parse nothing themselves; they
// build a CommandContext, call these, and deliver the output.
#pragma once

#include "Core/BOScoring.h"
#include "Core/BOTeams.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bo::core
{
	struct CommandContext
	{
		PlayerId Self;
		double Now = 0.0;
		// Resolve a typed name to an online player's ID (case-insensitive;
		// unique prefix allowed). Empty when no single match.
		std::function<std::optional<PlayerId>(const std::string&)> FindPlayer;
		// Display name for an ID (falls back to the ID).
		std::function<std::string(const PlayerId&)> NameOf;
	};

	struct CommandOutput
	{
		std::vector<std::string> Reply;                        // to the sender
		std::vector<std::pair<PlayerId, std::string>> Notify;  // to other players
		std::vector<std::string> Audit;                        // "action\tresult" for the audit log
	};

	CommandOutput RunTeamCommand(const CommandContext& Ctx, const std::vector<std::string>& Args, TeamRegistry& Teams);
	CommandOutput RunTrustCommand(const CommandContext& Ctx, const std::vector<std::string>& Args, TeamRegistry& Teams);
	CommandOutput RunUntrustCommand(const CommandContext& Ctx, const std::vector<std::string>& Args, TeamRegistry& Teams);
	CommandOutput RunScoresCommand(const CommandContext& Ctx, const Scoreboard& Scores, const TeamRegistry& Teams);

	std::string TeamHelp();
}
