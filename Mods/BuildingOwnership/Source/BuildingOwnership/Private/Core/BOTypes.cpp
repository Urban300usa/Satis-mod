#include "Core/BOTypes.h"

namespace bo::core
{
	AccessLevel RequiredLevel(Action Act)
	{
		switch (Act)
		{
		case Action::Dismantle:
		case Action::Upgrade:
		case Action::Paint:
			return AccessLevel::Full;

		case Action::ChangeMachine:
		case Action::OpenStorage:
		case Action::UseSwitch:
		case Action::ConfigureSplitter:
		case Action::ConfigureStation:
		case Action::DriveVehicle:
		case Action::Connect:
		case Action::BuildOnto:
		case Action::PickFromBelt:
			return AccessLevel::Access;

		case Action::ViewMachine:
		case Action::UseDoor:
			return AccessLevel::None;
		}
		return AccessLevel::Full; // unknown actions fail closed
	}

	const char* ToString(Action Act)
	{
		switch (Act)
		{
		case Action::Dismantle: return "dismantle";
		case Action::Upgrade: return "upgrade";
		case Action::Paint: return "paint";
		case Action::ChangeMachine: return "change machine";
		case Action::OpenStorage: return "open storage";
		case Action::UseSwitch: return "use switch";
		case Action::ConfigureSplitter: return "configure splitter";
		case Action::ConfigureStation: return "configure station";
		case Action::DriveVehicle: return "drive vehicle";
		case Action::Connect: return "connect";
		case Action::BuildOnto: return "build onto";
		case Action::PickFromBelt: return "pick from belt";
		case Action::ViewMachine: return "view machine";
		case Action::UseDoor: return "use door";
		}
		return "unknown";
	}

	const char* ToString(AccessLevel Level)
	{
		switch (Level)
		{
		case AccessLevel::None: return "None";
		case AccessLevel::Access: return "Access";
		case AccessLevel::Full: return "Full";
		}
		return "unknown";
	}
}
