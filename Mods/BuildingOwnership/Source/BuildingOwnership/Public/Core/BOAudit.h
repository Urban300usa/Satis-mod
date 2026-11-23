// Audit log line format: one tab-separated line per event.
#pragma once

#include <string>

namespace bo::core
{
	struct AuditEntry
	{
		std::string Time;     // ISO 8601, supplied by the game side
		std::string Player;   // "Name (id)"
		std::string Action;   // e.g. "deny:dismantle", "lock", "team:kick", "steal:grab:start"
		std::string Building; // building class, or empty
		std::string Location; // "X,Y,Z" in metres, or empty
		std::string Owner;    // "Name (id)", or empty
		std::string Result;   // free text
	};

	// Header line for a new log file.
	std::string AuditHeader();
	// Tabs and newlines inside fields are replaced with spaces.
	std::string FormatAuditLine(const AuditEntry& Entry);
}
