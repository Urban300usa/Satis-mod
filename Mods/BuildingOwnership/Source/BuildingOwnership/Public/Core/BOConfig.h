// Every tunable rule, with the defaults from the build plan. The game side
// fills this from the SML config file; tests use the defaults.
#pragma once

#include <string>
#include <vector>

namespace bo::core
{
	struct Config
	{
		// Admins
		std::vector<std::string> AdminPlayerIds;
		bool bAdminPasswordLoginsAreAdmins = true;

		// Protection switches
		bool bProtectDismantleUpgradePaint = true;
		bool bProtectUseConnectBuildOn = true;
		bool bProtectVehicles = true;
		bool bProtectBeltPickup = true;
		bool bAllowLocking = true;
		std::vector<std::string> ExemptBuildingClasses = {"HUB", "SpaceElevator", "MAM", "AwesomeShop"};

		// Commands and messages
		double TargetRangeMeters = 50.0;
		double InviteExpirySeconds = 300.0;
		double GiveOfferExpirySeconds = 300.0; // ASSUMPTION: same as invites
		double DenyMessageCooldownSeconds = 3.0;
		bool bAuditLog = true;

		// Transfers
		std::size_t TransferGroupCap = 500; // UNDECIDED: Tyler to confirm
		bool bTransferCountsBelts = true;
		bool bTransferCountsPipes = true;
		bool bTransferCountsRails = true;
		bool bTransferCountsFoundations = true;
		bool bTransferCountsPowerLines = false;

		// Scoring
		long long MilestoneBonusPoints = 10000; // placeholder, tune after playtesting

		// PvP
		bool bPvP = true;
		bool bFriendlyFire = false;

		// Steals
		double GrabSeconds = 120.0;
		double HackSeconds = 300.0;
		double GrabCooldownSeconds = 3600.0;
		double HackCooldownSeconds = 18000.0; // ASSUMPTION: per container
		double StealWalkAwayMeters = 10.0;    // UNDECIDED: proposed default
		int GrabMaxStacks = 3;                // ASSUMPTION: see BOSteals.h
	};
}
