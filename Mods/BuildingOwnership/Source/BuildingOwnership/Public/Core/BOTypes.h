// Plain C++ rule types. No Unreal or game types here, so this code can be unit
// tested outside the game (see /tests). Game code converts to and from these.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace bo::core
{
	// Epic or Steam account ID. Never a display name.
	using PlayerId = std::string;
	using TeamId = std::uint32_t;

	enum class AccessLevel : std::uint8_t
	{
		None = 0,   // Strangers: may look inside machines, nothing else.
		Access = 1, // Players trusted by a solo owner: use, connect, build on.
		Full = 2,   // Owner and owner's team: everything.
	};

	enum class Action : std::uint8_t
	{
		// Full
		Dismantle,
		Upgrade,
		Paint,
		// Access
		ChangeMachine,     // recipe, clock speed, on/off, move items
		OpenStorage,       // storage containers and lockers
		UseSwitch,         // power and priority switches
		ConfigureSplitter, // smart and programmable splitters
		ConfigureStation,  // train station settings and timetables
		DriveVehicle,      // owned vehicles and trains
		Connect,           // belts, pipes, power lines, rails, hypertubes
		BuildOnto,         // snapping or building onto it
		PickFromBelt,      // taking items off an owned conveyor
		// Anyone (unless locked)
		ViewMachine,
		UseDoor,
	};

	const char* ToString(Action Act);
	const char* ToString(AccessLevel Level);

	// The level an action needs, before locks are considered.
	AccessLevel RequiredLevel(Action Act);

	// What the permission check needs to know about the building.
	struct BuildingState
	{
		std::optional<PlayerId> Owner; // empty = unowned (pre-mod) building
		bool bLocked = false;
		bool bExempt = false; // HUB, Space Elevator, MAM, AWESOME Shop
	};

	// What the permission check needs to know about the acting player.
	struct ActorState
	{
		PlayerId Id;
		bool bIsAdmin = false;
		bool bBypass = false; // only honoured when bIsAdmin
	};

	enum class DenyReason : std::uint8_t
	{
		None,
		Locked,       // building is locked by its owner
		NotPermitted, // actor's level is below what the action needs
	};

	enum class AllowReason : std::uint8_t
	{
		None,
		AdminBypass,
		ExemptOrUnowned,
		Level, // actor's access level was high enough
	};

	struct CheckResult
	{
		bool bAllowed = false;
		AccessLevel Granted = AccessLevel::None;
		AllowReason Allow = AllowReason::None;
		DenyReason Deny = DenyReason::None;
	};
}
