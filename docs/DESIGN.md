# Building Ownership Mod — Design Spec (v1)

> **Superseded by [`BUILD_PLAN.md`](BUILD_PLAN.md).** Kept for history; where they disagree, the build plan wins.

Working name: **BuildingOwnership** (placeholder; the mod reference must be unique on ficsit.app).

A server-side Satisfactory mod. Whatever a player places belongs to them. Only the
owner, their team and players they trust can interact with it, and owners can lock
individual buildings even from their own team.

## 1. Scope

- **Server-only for v1.** Players don't need to install anything. Every check runs on
  the dedicated server, and feedback is sent through chat messages.
- **Buildings placed before the mod was installed are unowned** and work like vanilla
  (anyone can do anything with them).
- **Exempt buildings are never owned:** the HUB, Space Elevator, MAM and AWESOME Sink
  (plus the AWESOME Shop), because they drive progress shared by everyone.

## 2. Concepts

| Concept | Definition |
|---|---|
| Player ID | The player's online account ID (Epic/Steam), never their display name. A name cache keeps names for chat messages. |
| Owner | The player who placed the building. Ownership belongs to the player, never the team. |
| Team | A named group. Each player can be in at most one team. Roles: **Leader** (one), **Officers**, **Members**. |
| Trust | Access granted by a team leader (or a solo player) to another player or another team. |
| Lock | A per-building flag set by the owner. Only the owner (and admins with bypass on) can touch it. |
| Admin | A player logged in with the server's admin password, **or** whose player ID is in the config admin list. |
| Bypass | A per-admin toggle (`/bypass`), **off by default**. When on, every check passes. |

## 3. Permission model

### Access levels

| Level | Who | Allowed |
|---|---|---|
| **Full** | Owner; members of the owner's team (same team, any role) | Everything: dismantle, upgrade/replace, paint/customize, use/open, connect, build on |
| **Access** | Players or teams trusted by the owner's team (or by a solo owner) | Use/open, connect, build on. **Not** dismantle, upgrade, or paint |
| **None** | Everyone else | Nothing. Can't open, view inside, connect to, build on or dismantle |

### Evaluation order (first match wins)

1. Actor is an admin **with bypass on** → **allow**.
2. Building is exempt or unowned → **allow** (vanilla behaviour).
3. Building is **locked** → allow only if the actor is the owner; otherwise **deny**.
4. Actor is the owner or on the owner's team → **Full**.
5. Actor is trusted by the owner's team (or by the solo owner), directly or through their own team → **Access**.
6. Otherwise → **None**.

### What each action needs

| Action | Required level |
|---|---|
| Dismantle (single and mass-select) | Full |
| Upgrade/replace (e.g. Mk.3 belt placed over Mk.1) | Full |
| Paint / customize | Full |
| Open machine UI, view inventory, change recipe or clock speed, insert or remove items | Access |
| Storage containers, lockers, Dimensional Depot uploads from that container | Access |
| Power switches, priority power switches | Access |
| Smart/programmable splitter configuration | Access |
| Train station / docking station settings and timetables | Access |
| Drive or use owned vehicles and trains | Access |
| Connect a belt, pipe, power line, rail, or hypertube to it | Access |
| Snap or build anything onto it (foundations, walls, attachments) | Access |
| Doors | Anyone (left open so shared areas can be walked through) |
| Pick items off owned conveyor belts | Access (stretch goal; may not make v1) |

**Power** is covered by the connection rule: attaching a power line to someone's grid needs Access.

### Ownership of new placements

- Whoever places a building owns it, even when they build onto a trusted player's
  foundation.
- **Blueprints:** the placer owns every building inside. The whole placement is refused
  if any part snaps to or connects with a building the placer lacks Access to.

## 4. Teams

| Action | Who can do it |
|---|---|
| Create a team | Any player not already in a team (they become Leader) |
| Invite | Leader, Officers |
| Kick | Leader, Officers (officers can't kick the Leader or other Officers) |
| Promote/demote Officers | Leader |
| Manage trust (grant or revoke for players or teams) | **Leader only** |
| Transfer leadership | Leader |
| Disband | Leader |
| Leave | Anyone. A Leader must transfer leadership first or disband |

- A player who leaves or is kicked **keeps their buildings**. The old team loses Full
  access to them, and the player loses access to the team's buildings.
- A solo player (no team) acts as their own Leader for trust purposes.
- Invites expire after 5 minutes (configurable).

## 5. Chat commands (v1)

"Target" means the building the player is looking at (the server traces from the player's view, up to the configured range).

**Ownership**
| Command | Effect |
|---|---|
| `/owner` | Show the target's owner, team, and lock state |
| `/lock` | Lock the target (owner only) |
| `/unlock` | Unlock the target (owner only) |

**Teams**
| Command | Effect |
|---|---|
| `/team create <name>` | Create a team |
| `/team invite <player>` / `/team accept` / `/team decline` | Invites |
| `/team kick <player>` | Kick |
| `/team promote <player>` / `/team demote <player>` | Officers |
| `/team leader <player>` | Transfer leadership |
| `/team leave` / `/team disband` | Leave or disband |
| `/team info [name]` | Members, roles, trust list |

**Trust** (Leader or solo player)
| Command | Effect |
|---|---|
| `/trust <player>` / `/untrust <player>` | Trust a single player |
| `/trust team <name>` / `/untrust team <name>` | Trust a whole team |
| `/trust list` | Show grants |

**Admin**
| Command | Effect |
|---|---|
| `/bypass` | Toggle bypass for yourself |
| `/transfer <player>` | Give the target to a player |
| `/transfer all <fromPlayer> <toPlayer>` | Move all of one player's buildings |
| `/unclaim` / `/unclaim all <player>` | Make the target (or all of a player's buildings) unowned |

## 6. Audit log

Written to a separate log file on the server and rotated daily. Each entry includes
time, player ID, player name, action, building class, location, owner, and result.

Logged events: denied actions, lock/unlock, team changes, trust changes, admin bypass
toggles, and every admin action (transfer/unclaim).

## 7. Config file (server-side, SML mod config)

| Setting | Default |
|---|---|
| `AdminPlayerIds` (list) | empty |
| `UseServerAdminPassword` | true |
| `ProtectDismantle` / `ProtectUpgrade` / `ProtectPaint` | true |
| `ProtectUse` / `ProtectConnect` / `ProtectBuildOn` | true |
| `ProtectVehicles` | true |
| `ProtectBeltItemPickup` | true (if implemented) |
| `AllowLocking` | true |
| `ExemptBuildingClasses` (list) | HUB, Space Elevator, MAM, AWESOME Sink, AWESOME Shop |
| `TargetTraceRange` (m) | 50 |
| `InviteExpirySeconds` | 300 |
| `EnableAuditLog` | true |
| `DenyMessageCooldownSeconds` (anti-spam) | 3 |

## 8. Architecture (implementation notes)

- **`AOwnershipSubsystem`** (SML `AModSubsystem`, spawned on the server only; implements
  `IFGSaveInterface`). It holds all state and is saved inside the world save:
  - `TArray<FOwnershipRecord>` with `{ AFGBuildable* Building (SaveGame); FString OwnerId; bool bLocked; }`.
    A runtime `TMap` is rebuilt on load for fast lookups.
  - `TArray<FTeamRecord>` with `{ FGuid Id; FString Name; FString LeaderId; TArray<FString> Officers, Members; TArray<FString> TrustedPlayers; TArray<FGuid> TrustedTeams; }`
  - `TMap<FString, FString>` mapping player ID to last known name.
  - Records are removed when their building is destroyed.
- **Chat commands** use SML's chat command system (one `AChatCommandInstance` per root command).
- **Hooks** use SML native hooking on server-side code paths. The exact functions need
  verifying against the current game headers during implementation:
  - Placement: build gun construct on the server / hologram construct. Records the owner
    and validates the snap and connection targets.
  - Dismantle: build gun dismantle state server RPCs (single and multi).
  - Upgrade: hologram upgrade/replace path.
  - Use: the buildable's use interface (`OnUse` / `IsUseable`, which receive the character).
  - Paint: customization apply path.
  - Connections: hologram snap or connection checks during the server-side construct.
  - Vehicles: vehicle use/driver entry.

## 9. Risks / things to prototype first

1. **"Can't see inside" without a client mod.** The server can refuse the use action, but
   it isn't yet confirmed that this stops the machine UI and inventory reaching the
   client. Prototype this first. A small client-side part may be needed.
2. **Client prediction.** Blocked builds or dismantles may flicker or appear to
   succeed briefly on the client before the server refuses. This is acceptable for v1
   because the chat message explains it.
3. **Connection detection.** Belts, pipes, rails and hypertubes each snap differently,
   so each hologram type needs testing.
4. **Hook stability.** Game updates can rename or change the hooked functions. Keep
   every hook in one file so updates are easy to patch.

## 10. Out of scope for v1 (future ideas)

- A client UI version: owner nameplates on hover, hiding the dismantle highlight on
  buildings you can't remove, and a team menu.
- Build zones (a protected radius around bases).
- Expiry of ownership for inactive players.
- Per-player build limits.
- `/lock` for an area or for all buildings of a type.
