#include "Core/BOAudit.h"

namespace bo::core
{
	namespace
	{
		std::string Clean(const std::string& S)
		{
			std::string Out = S;
			for (char& C : Out)
			{
				if (C == '\t' || C == '\n' || C == '\r')
				{
					C = ' ';
				}
			}
			return Out;
		}
	}

	std::string AuditHeader()
	{
		return "time\tplayer\taction\tbuilding\tlocation\towner\tresult";
	}

	std::string FormatAuditLine(const AuditEntry& E)
	{
		return Clean(E.Time) + '\t' + Clean(E.Player) + '\t' + Clean(E.Action) + '\t' + Clean(E.Building) + '\t' +
			Clean(E.Location) + '\t' + Clean(E.Owner) + '\t' + Clean(E.Result);
	}
}
