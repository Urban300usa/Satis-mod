# BuildingOwnership (working name)

A server-only Satisfactory mod for a friends' dedicated server, played as a team competition: whoever
places a building owns it, teams are walled off from each other, rivals can fight and steal but can't
destroy bases, and teams score points on a leaderboard.

- **Design / build plan:** [`docs/BUILD_PLAN.md`](docs/BUILD_PLAN.md) (source of truth; supersedes `docs/DESIGN.md`)
- **Status:** build step 1 of 13 (project setup). No gameplay hooks yet.
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
    OwnershipSubsystem                  server-only state holder, saved inside the world save
    BOGameWorldModule                   registers the subsystem and chat commands with SML
    Commands/OwnerCommand               /owner (step 1: proves the mod is running and saving)
    Hooks/OwnershipHooks                every native hook, in one file (empty in step 1)
tests/                                  unit tests for Core/; no game or engine needed
docs/                                   design spec and build plan
```

## Run the unit tests (any machine with a C++17 compiler)

```
make -C tests
```

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

If any of these fail, note what you saw and the game/SML versions before changing code.

## Things marked VERIFY in the code

These were written without the game headers to hand and must be checked on the first build:

- `UBOGameWorldModule` constructor: `bRootModule`, `ModSubsystems`, `mChatCommands` field names.
- `AChatCommandInstance` fields: `CommandName`, `MinNumberOfArguments`, `bOnlyUsableByPlayer`, `Usage`;
  `UCommandSender::GetPlayer()` / `SendChatMessage()`.
- `AOwnershipSubsystem::GetPlayerId`: that `APlayerState::GetUniqueId()` gives the Epic/Steam ID on a
  dedicated server.
- `IFGSaveInterface` method signatures (`PreSaveGame`, `PostLoadGame`, `ShouldSave`).
- The SML version range in the `.uplugin` (`^3.8.0`).
