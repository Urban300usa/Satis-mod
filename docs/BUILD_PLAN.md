# Building Ownership Mod: build plan

> Transcribed from "Building Ownership Mod: build plan for Claude Code" (PDF from Tyler, 2026-11-23).
> **This file supersedes `DESIGN.md`** wherever the two disagree. Open questions go to Tyler; don't guess.

Working name: BuildingOwnership (placeholder, final name must be free on ficsit.app).

## 1. What we are building

A server-side Satisfactory mod for a small friends' dedicated server, played as a team competition. Whoever places a
building owns it. Teams are walled off from each other. Rivals can fight and steal but cannot destroy bases, and teams
score points on a leaderboard.

Constraints:
- C++ on Satisfactory Mod Loader (SML), built from the SML starter project on Windows, packaged with Alpakit for
  Windows and Linux dedicated servers.
- **Server-only for v1.** Players install nothing. No new buildables, assets or UI. Everything is chat commands and
  chat messages.
- Every blocked action gets a chat message saying why, with a 3 s cooldown between deny messages.
- Identify players by Epic or Steam account ID, never by display name.
- All state lives in one server-side subsystem and is saved inside the world save.

Don't rely on any game class or function name written here; they are guesses to search for. Verify them against the
SML docs and game headers, and keep every hook in one file.

## 2. Decisions already made

### Ownership and access
- The player who places a building owns it. Ownership belongs to a player, never a team.
- Buildings placed before the mod was installed are Unowned and behave like vanilla.
- Exempt (never owned, open to all): HUB, Space Elevator, MAM, AWESOME Shop. **The AWESOME Sink is NOT exempt.**
- Access levels:
  - **Full:** the owner and everyone on the owner's team. Dismantle, upgrade, paint, use, connect, build on.
  - **Access:** players trusted by a solo owner via `/trust`. Use, connect, build on. No dismantle, upgrade or paint.
  - **None:** everyone else. **Can look inside machines** but cannot change anything (no recipe or clock changes, no
    power on or off, no moving items in or out). Storage containers and lockers stay fully closed: the server must
    block the open request itself. Doors are open to anyone unless the owner locked them.
- Permission rules (first match wins):
  1. Admin with bypass on: allowed.
  2. Exempt or unowned building: allowed.
  3. Locked building: owner only.
  4. Owner and owner's team: Full.
  5. Players trusted by a solo owner: Access.
  6. Everyone else: None.
- Action levels:
  - **Full:** dismantle (single and mass-select), upgrade-replace, paint.
  - **Access:** change a machine (recipe, clock speed, on or off, move items); storage containers and lockers; power
    and priority switches; smart and programmable splitter settings; train station settings and timetables; driving
    owned vehicles and trains; connecting belts, pipes, power lines, rails or hypertubes; building or snapping onto
    it; picking items off owned conveyor belts (**in v1**).
  - **Anyone unless locked:** doors.

### Teams, solo players and trust
- Each player is in at most one team. A solo player is a team of one: everyone scores through a team.
- Teams are fully walled off from each other. No team trust and no alliances in v1.
- Roles: leader, officers, members.
  - Create a team: any player not in one (they become leader).
  - Invite and kick: leader and officers (officers can't kick the leader or other officers).
  - Promote, demote, transfer leadership, disband: leader.
  - Leave: anyone (a leader must transfer leadership or disband first).
  - Invites expire after 5 minutes.
- `/trust` and `/untrust <player>`, `/trust list`: only a solo player (team of one). Trust is per individual player,
  never a team. Joining a bigger team ends the trust list.
- Joining a team: the player's earlier buildings stay theirs, and the team gets Full over them while the player stays.
- Leaving or being kicked: the player keeps their buildings and takes them. The old team loses access, and the player
  loses access to the team's buildings.
- Pieces someone built onto another owner's building stay with that owner, even if the builder later leaves or is kicked.

### Ownership of new builds
- Helper builds: anything built onto or connected to someone else's building (by a teammate or trusted helper)
  becomes owned by that building's owner. Something started from scratch stays owned by the builder.
- Blueprints: the placer owns every building in it. Refuse the whole placement if any part snaps to or connects with
  a building the placer cannot access.

### Locks
- `/lock` and `/unlock`: any owner, on a team or not, on a building, door or factory. A lock shuts out everyone but the
  owner, teammates included. Admins with bypass can override.

### Transfers
- Player to player: `/give <player>` offers the target building and its connected group. The receiver must accept.
  It moves as one connected group, not piece by piece.
- Admin: `/transfer <player>`, `/transfer all <from> <to>`, `/unclaim`, `/unclaim all <player>`.
- Group size limit is UNDECIDED. Proposed default (implement it; Tyler to confirm): flood fill over belts, pipes,
  rails and snapped foundations, excluding power lines, capped at a configurable maximum. Report the count in chat
  before the transfer is offered.

### Admins
- Admins are players logged in with the server admin password (config toggle, default on) or whose account ID is in
  the config file.
- `/bypass` is a per-admin toggle, off by default. When on, every check passes, locks included.
- `/owner` (anyone): show the target's owner, team and lock state. The target is the building you're looking at, up
  to 50 m.
- Every admin action is written to the audit log.

### Scoring
- Teams score points. A team's score is a running total that never goes down when coupons are spent.
- Milestones and tiers: 10,000 points each. Placeholder, to be tuned after playtesting. Open: whether the bonus
  scales with tier.
- Item deliveries: items fed into a team's own AWESOME Sink score the same points the Sink would pay out. Reuse the
  game's own point values.
- Each team's Sink is owned by the team that built it and follows the normal lock rules. The AWESOME Shop stays
  shared, but coupons are tracked per team so one team cannot spend another's.
- Nothing scores for damaging or removing another team's buildings.
- `/scores` (anyone): every team's current score.
- Server-wide chat announcement when a team reaches a milestone, naming the team and the milestone, e.g.
  "Team Alpha just reached Tier 4".

### PvP and stealing
- Players can damage and kill players on other teams. Friendly fire is off.
- If another player kills you, your inventory drops where you died and anyone can loot it. Any other death
  (creature, fall, anything else) keeps your inventory and drops nothing.
- Players cannot damage or destroy another team's buildings.
- Two kinds of theft against a container the thief cannot normally open. Any player can start one:
  - `/grab` (quick steal): 2 minutes. Takes a random amount of random items from the container, and the thief cannot
    see what else is inside. 1-hour cooldown per container.
  - `/hack` (large steal): 5 minutes. Open access to the container for that time, and the thief can take anything.
    5-hour cooldown per container (assumed per container; confirm with Tyler).
- Both alert the owning team only, and only halfway through the steal (1 minute into `/grab`, 2.5 minutes into
  `/hack`). Rivals get no alert.
- Taking damage does not stop a steal. The thief dying cancels it, and so does walking away. Walk-away distance is
  undecided: implement it as a configurable value, proposed default 10 m.
- The old idea of limiting steals to high-value items, or items the thief's team hasn't unlocked, was dropped.

### Chat colors
- Team colors: each team gets one color, assigned automatically at creation, used for its name in `/scores` and
  announcements.
- Alert colors: each type of server message has its own color, e.g. red for steal alerts and gold for milestone
  announcements.
- Probably only the sender name can be colored from the server. Prototype this (see risks). If text inside a message
  can't be colored without a client mod, put the team or alert name in the sender label and leave the text plain.

### Audit log and config
- Separate log file on the server, rotated daily. Records denied actions, locks and unlocks, team and trust changes,
  bypass toggles, admin actions, steals, and kills that dropped loot. Each entry: time, player, action, building
  type, location, owner, result.
- One config file switches every rule without rebuilding. Defaults:
  - admin player IDs: empty list
  - admin-password logins count as admins: on
  - protect dismantle/upgrade/paint: on
  - protect use/connect/build-on: on
  - protect vehicles: on
  - protect belt pickup: on
  - allow locking: on
  - exempt buildings: HUB, Space Elevator, MAM, AWESOME Shop
  - target range: 50 m
  - invite expiry: 300 s
  - audit log: on
  - deny-message cooldown: 3 s
- Additional keys: PvP on or off, friendly fire, steal durations and cooldowns, walk-away distance, milestone bonus,
  transfer group cap, and the connection types counted in a transfer.

## 3. Architecture
- One server-side subsystem owns all state: building owners and lock flags, teams and roles, trust lists, scores,
  per-team coupon balances, steal cooldowns, and a cache of player names. It is serialized into the world save so it
  survives restarts.
- One permission function, `Check(player, building, action) -> allow | deny(reason)`, implements the six-step order.
  Every hook calls it.
- All hooks are in a single file: place, dismantle, upgrade, paint/customize, use/open (machines, storage, switches,
  splitters, stations, vehicles, doors), connect, build-onto/snap, belt pickup, player damage, player death and
  inventory drop, Sink delivery, Shop purchase, schematic/milestone unlock.
- Chat commands use SML's chat command system.
- Rule logic (permission check, team and trust rules, scoring, steal state machine) is plain C++ with no game types,
  so it can be unit tested without the game. Game hooks are thin adapters around it.

## 4. Build order
Prototype the risky hooks first. Each step ends with something testable on a dedicated server.

1. **Project setup:** SML starter project, empty subsystem saved and loaded, and one chat command (`/owner`) that
   echoes. Confirm Alpakit packages for the Windows and Linux servers.
2. **Ownership recording:** record the owner on place, including blueprints. `/owner` shows it. Unowned and exempt
   handling.
3. **Permission check and first blocking hooks:** dismantle, upgrade, paint. Deny messages with cooldown. Confirm the
   client-prediction glitch (a blocked action briefly looks successful) is tolerable.
4. **Machine and storage blocking:** block machine changes while still allowing viewing. Block the storage open
   request before the screen builds. This is the highest risk (see section 5).
5. **Connection, build-onto and snap hooks** for belts, pipes, power lines, rails, hypertubes, foundations and walls.
   Test each type separately. Helper-build ownership transfer to the attached building's owner. Belt pickup.
6. **Teams:** roles, invites, trust, locks, doors, vehicles.
7. **Transfers:** `/give` with accept, connected-group flood fill, size cap, admin transfer and unclaim.
8. **Admin:** tools, bypass, audit log, config file.
9. **Scoring:** per-team Sinks, score total, milestone hook, `/scores`, announcements, per-team coupons. Tune after
   playtesting.
10. **PvP:** friendly fire off, base damage blocked, kill-only inventory drop.
11. **Steals:** `/grab` and `/hack` state machine, cooldowns, halfway alert, cancel on death or walking away, audit
    entries.
12. **Chat colors.**
13. **Hardening:** game-update patch notes, deny-message spam, save and load edge cases, a server restart mid-steal,
    players leaving during an invite.

## 5. Risks to prototype first, with fallbacks
1. **Blocking machine changes.** Anyone may look, so nothing needs hiding. Confirm the server can reject each of:
   recipe change, clock speed, power on or off, and item moves. If one can't be rejected, report it and ask Tyler
   before inventing a workaround.
2. **Blocking storage access.** The server must stop the open request itself. If the game opens the screen before
   the server can refuse, report it and ask Tyler. A client mod would break the server-only goal.
3. **Per-team coupons.** The base game has one shared pool. Confirm the server can intercept the coupon balance and
   Shop purchases and keep per-team balances.
4. **Death drops.** The base game drops the inventory on every death. Confirm the server can tell who dealt the
   killing blow and cancel the drop unless another player did.
5. **Steal access exception.** `/hack` must open a container the normal path blocks, only for the thief and only for
   the time limit. Confirm that exception can be granted and revoked cleanly.
6. **Chat colors.** Confirm sender-name color. Test whether text inside a message can be colored without a client mod.
7. **Client prediction.** A blocked build or dismantle may briefly look like it worked. The chat message explains it,
   so this is acceptable for v1.
8. **Connections.** Belts, pipes, rails and hypertubes each snap differently, and each needs its own test.
9. **Game updates** can rename hooked functions. Keep hooks in one file and note the game version in the README.

If a risk fails, don't quietly change the design. Stop, write down what you found and what the options are, and ask
Tyler.

## 6. Test checklist
- [ ] The owner and a teammate can dismantle, upgrade, paint and use. A stranger can't, and gets at most one chat
      message per 3 s.
- [ ] A stranger can open a machine screen and look, but can't change the recipe, clock or power, or move items.
- [ ] A stranger can't open a storage container at all.
- [ ] Locked building and locked door: only the owner passes, not teammates. An admin with bypass passes; an admin
      without bypass doesn't.
- [ ] A solo player trusts another player: that player can use, connect and build on, but can't dismantle, upgrade
      or paint. Joining a team ends the trust list.
- [ ] A teammate or trusted helper builds onto an owner's building: the new pieces belong to the owner and stay with
      the owner after the builder leaves or is kicked.
- [ ] Team A and team B can't touch each other's buildings. A kicked player keeps their buildings and takes them.
- [ ] Blueprint placement is refused if any part snaps to a building the placer can't access.
- [ ] `/give` needs acceptance and moves a whole connected group. The cap and the count message work.
- [ ] A team's own Sink scores to that team. Spending coupons never lowers the score. One team can't spend
      another's coupons. The milestone announcement fires once, naming the team and milestone. `/scores` matches.
- [ ] PvP: friendly fire is off. A player killed by a player drops loot; a fall or creature death keeps the
      inventory. Bases can't be damaged.
- [ ] `/grab` and `/hack`: right duration, right cooldown per container, alert to the owning team only at the
      halfway point. Damage doesn't cancel; death and walking away do.
- [ ] Restart the server mid-game and mid-steal: ownership, teams, trust, scores, locks and cooldowns survive.
- [ ] The audit log has an entry for every denied action, lock, team or trust change, bypass toggle, admin action,
      steal and loot kill.

## 7. Still open (ask Tyler, do not guess)
- Final mod name and ID, free on ficsit.app.
- Transfer group cap value, and which connection types count.
- Whether the milestone bonus should scale with tier.
- Whether the 5-hour `/hack` cooldown is per container (assumed), per thief or per team.
- Walk-away cancel distance (proposed 10 m).
- Whether a solo player's team of one is created automatically on first join or by `/team create` (the doc assumes
  `/team create`).

Not in v1: client-side UI, build zones, ownership expiry, per-player building limits, locking whole areas, alliances
or team-to-team trust.
