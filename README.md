# BuildingOwnership (working name)

A server-only Satisfactory mod for a friends' dedicated server, played as a team competition: whoever
places a building owns it, teams are walled off from each other, rivals can fight and steal but can't
destroy bases, and teams score points on a leaderboard.

- **Design / build plan:** [`docs/BUILD_PLAN.md`](docs/BUILD_PLAN.md) (source of truth; supersedes `docs/DESIGN.md`)
- **Status:** build step 1 of 13 written (project setup), plus the game-independent rule logic for
  steps 2-11 and the hook-free chat commands (`/team`, `/trust`, `/untrust`, `/scores`). No gameplay hooks yet.
  Nothing on the game side has been compiled yet.
- **Target game version:** _fill in when first built and tested_ (record the game build number here; hooks
  break when the game updates)

## Repo layout

```
Mods/BuildingOwnership/                 the SML mod plugin; copy into the starter project's Mods/ folder
  BuildingOwnership.uplugin             "RequiredOnRemote": false, so players don't need to install it
  Source/BuildingOwnership/
    Public|Private/Core/                game-independent rule logic: plain C++, unit tested in tests/
      BOTypes        access levels, actions, building/actor state
      BOPermissions  Check(): the six-step permission order every hook calls
      BOTeams        teams, roles, invites, solo trust, team colors
      BOOwnership    owner records, locks, helper-build and blueprint placement rules, admin transfer/unclaim
      BOTransfers    connected-group flood fill with cap, /give offers, deny-message cooldown
      BOScoring      team scores, milestone bonuses, per-team coupons, ranking
      BOSteals       /grab and /hack timers, halfway alerts, cancels, cooldowns, hack access, grab item picks
      BOAudit        audit log line format
      BOCommands     /team, /trust, /untrust, /scores logic (replies, notifications, audit lines)
      BOConfig       every tunable rule with the plan's defaults
    OwnershipSubsystem                  server-only state holder, saved inside the world save; per-player
                                        chat messages; daily audit log file
    BOGameWorldModule                   registers the subsystem and chat commands with SML
    Commands/OwnerCommand               /owner (step 1: proves the mod is running and saving)
    Commands/TeamCommands               /team, /trust, /untrust, /scores (thin wrappers over Core/BOCommands)
    Hooks/OwnershipHooks                every native hook, in one file (empty so far)
tests/                                  unit tests for Core/; no game or engine needed
docs/                                   design spec and build plan
```

## Run the unit tests (any machine with a C++17 compiler)

```
make -C tests
```

59 tests covering the plan's test checklist wherever it doesn't need the game: permissions, locks,
bypass, teams and roles, invites, trust, helper builds, blueprints, /give, transfer caps, scoring,
coupons, milestones, steals (timing, alerts, cancels, cooldowns, restart), and the chat command text.

## Build the mod (Windows)

1. Set up the SML starter project and its custom Unreal Engine by following the
   [SML getting-started docs](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/index.html).
2. Copy `Mods/BuildingOwnership` into the starter project's `Mods/` folder.
3. Regenerate project files (right-click `FactoryGame.uproject` > Generate Visual Studio project files),
   then build the `Development Editor` target in Visual Studio.
4. Open the editor. In the BuildingOwnership content folder, create a Blueprint class whose parent is
   `BOGameWorldModule` and name it **`RootGameWorld_BuildingOwnership`**. Open it and check that
   *Root Module* is ticked and that *Mod Subsystems* lists `OwnershipSubsystem` and *Chat Commands* lists
   `OwnerCommand` (the C++ constructor sets these; if the fields are empty or named differently in your
   SML version, set them in the Blueprint instead).
5. In Alpakit, select BuildingOwnership and package for **Windows**, **WindowsServer** and **LinuxServer**.

## Step 1 test on a dedicated server

1. Install the packaged mod on the server (and nothing on clients). Start the server and join.
2. Type `/owner`. Expected: `[Ownership] Running. Your ID: <id>. Loaded from save: no. Times saved: 0. Teams: 0.`
   Check that `<id>` is a stable account ID (same after reconnecting), not your display name.
3. Save the game, restart the server, rejoin, type `/owner` again. Expected: `Loaded from save: yes`
   and `Times saved` is at least 1.
4. Check the server log for `LogBuildingOwnership` lines: module started, hooks registered, and a
   `Loaded:` line after restart.

5. With two players: `/team create Alpha`, `/team invite <name>`, `/team accept` on the other player,
   `/team info`, `/team promote <name>`, `/scores`. Restart the server and check `/team info` still shows
   the team. A solo player can `/trust <name>` and `/trust list`.
6. Check `Saved/BuildingOwnership/Audit-<date>.tsv` on the server has team lines.

If any of these fail, note what you saw and the game/SML versions before changing code.

## Decisions made in code that need Tyler's confirmation

The plan left these open or didn't cover them. Each is one setting or a few lines to change.

| Topic | What the code does now | Where |
|---|---|---|
| Transfer group cap | 500 buildings; belts, pipes, rails, foundations count; power lines don't | `BOConfig.h` |
| `/hack` cooldown scope | Per container | `BOConfig.h` |
| Walk-away distance | 10 m; also the maximum distance to start a steal | `BOConfig.h`, `BOSteals.cpp` |
| When a steal cooldown starts | When the steal starts, even if it's cancelled (stops cancel-and-retry) | `BOSteals.h` |
| `/hack` access window | Open access from the moment it starts, for 5 minutes | `BOSteals.h` |
| `/grab` amount | 1-3 random non-empty stacks, a random 1..stack-size from each | `BOSteals.h` |
| One steal at a time | A thief runs one steal; a container has one thief at a time | `BOSteals.cpp` |
| Steal during restart | In-progress steals are cancelled; cooldowns survive | `BOSteals.h` |
| Helper build touching two owners | Goes to the owner of the first other-owned building it attaches to (the snap target) | `BOOwnership.h` |
| Lock/unlock by admin | Admin with bypass can unlock someone else's building | `BOOwnership.cpp` |
| `/give` offer expiry | 5 minutes, same as invites | `BOConfig.h` |
| Team of one | Created only by `/team create`; a solo player in a team of one can still be invited (their solo team dissolves on accept) | `BOTeams.cpp` |
| Leadership transfer | The old leader becomes an officer | `BOTeams.cpp` |
| Team names | 1-24 letters, numbers, spaces, `-` or `_`; unique ignoring case | `BOTeams.cpp` |
| Milestone bonus | Flat 10,000 per milestone, once per team | `BOConfig.h` |

## Things marked VERIFY in the code

These were written without the game headers to hand and must be checked on the first build:

- `UBOGameWorldModule` constructor: `bRootModule`, `ModSubsystems`, `mChatCommands` field names.
- `AChatCommandInstance` fields: `CommandName`, `MinNumberOfArguments`, `bOnlyUsableByPlayer`, `Usage`;
  `UCommandSender::GetPlayer()` / `SendChatMessage()`.
- `AOwnershipSubsystem::GetPlayerId`: that `APlayerState::GetUniqueId()` gives the Epic/Steam ID on a
  dedicated server.
- `IFGSaveInterface` method signatures (`PreSaveGame`, `PostLoadGame`, `ShouldSave`).
- The SML version range in the `.uplugin` (`^3.8.0`).
- `AOwnershipSubsystem::SendToPlayer`: `USMLRemoteCallObject::SendChatMessage` via
  `GetRemoteCallObjectOfClass` for messaging a single player.
