# rtp-xyz Architecture & Design — RPG Maker MZ Parity, xyz-os Native

Written 2026-07-31, born from a real, sourced discussion — not a
speculative design. Every claim below was either verified by reading
real files this session (`ops/camera_control.c`, `system/prisc+x.c`'s
real opcode set, `&.widgits/event-editor`'s real current status,
`slop-ed-dev_manager.c` from the TPMOS reference tree,
`201.rpg-maker-clone`'s own `ARCHITECTURE.md`/`README.md`) or is a
direct, explicit decision made with the project owner in that same
conversation. See `#.haiku+/30.jul-30-handoff/1.ngn/` for the prior
session's own research this doc builds on
(`1-rtp-rpg-xyz-scaffold.md`, `ngn-vs-rm.txt`,
`battle-plugin-architecture.md`) — read those too, this doc doesn't
repeat their reasoning, it extends and makes concrete decisions on top
of it.

Companion doc: `300.rpg-xyz/dox/rpg-xyz-architecture.md` — same
subject, rpg-xyz's own widget-based variant. Read both; they diverge
only in §2 (GUI architecture) and §7 (save/load).

================================================================================

## 0. Vision — why this isn't just "clone an RPG engine"

Direct instruction from the project owner: **rtp-xyz/rpg-xyz are not
an end in themselves.** Building an RPG-Maker-equivalent is the first
concrete exercise that forces xyz-os's own general capabilities to
mature: dynamic PDL-driven menus, event scripting as real PAL
programs, a general plugin architecture, per-project database-driven
content, multi-window widget composition, file-mediated IPC. "Make an
RPG" is the proving ground; "xyz-os as a general application
development environment" is the actual long-term target — deployment
(§9 appendix) is framed the same way: not RPG Maker's browser/NW.js/
mobile export, but a real **xyzos-style app export** (an executable,
or a house-native "app" package), because this stack is meant to grow
into an environment for building other kinds of applications too, not
just games.

Keep this framing in mind reading every section below: wherever a
design choice could go either "the RPG-specific way" or "the general,
reusable-by-any-future-xyzos-app way," prefer the general one.

================================================================================

## 1. Feature comparison — RPG Maker MZ vs the "ngn" family

Base table verified and written by a prior session
(`ngn-vs-rm.txt`) — reproduced and extended here with this session's
own scope decisions marked.

| Capability | RPG Maker MZ | ngn (xyz-os) | This pass's scope |
|---|---|---|---|
| Tile-based map editor | ✅ multi-layer, autotile | 🟡 mutaclysm: 40×16 terrain+furniture, single layer, no autotile | in scope (§2), multi-layer/autotile explicitly NOT this pass |
| Event system (switches/variables/branches) | ✅ core feature | ❌ zero in mutaclysm; real but disconnected in `201.rpg-maker-clone` | **in scope, §3** — the #1 named gap |
| Database editor (Actors/Classes/Skills/...) | ✅ full GUI | ❌ plain `.txt` registries, hand-edited | **in scope, full category set, §4** (overrides `ngn-vs-rm.txt`'s own "not recommended near-term" — deliberate, explicit choice this pass) |
| Turn-based battle | ✅ core feature | ❌ mutaclysm real-time bump-damage only | **no hardcoded default of any kind** — battle is the first real PAL plugin, §5 |
| Common Events (reusable chains) | ✅ | ❌ | **in scope, §3** |
| Plugin system (JS ecosystem) | ✅ huge | ❌ PAL is an internal VM, not an end-user plugin layer | **in scope, §5** — general-purpose registry, not battle-only |
| Character generator | ✅ built in | ❌ | out of scope this pass, **hook noted, Appendix A** |
| RTP (bundled assets) | ✅ | 🟡 emoji/voxel on-demand pipeline — different, real, not a gap | unchanged |
| Playtest button | ✅ one click | 🟡 `button.sh run` | unchanged, real equivalent already |
| Save/load | ✅ | ✅ mutaclysm-family: real, harness-provable | **rebuild for rtp-xyz, §7** — mutaclysm's own save is per-engine, not per-project/slot |
| Deployment (browser/NW.js/mobile) | ✅ | ❌ | out of scope this pass, **reframed as xyzos-app export, Appendix B** |
| Multi-window live editing | ❌ single app window | ✅ **ngn's own real advantage** | unchanged — rpg-xyz variant demonstrates this, not rtp-xyz |
| Filesystem-as-database | ❌ proprietary JSON | ✅ **ngn's own real advantage** | governing law for every section below |
| Automated real-key-injection test harness | ❌ humans click-test | ✅ **ngn's own real advantage** | governing law, §8 |
| 2D/3D POV switch, 4 camera modes | ❌ does not exist | ✅ **already real**, `ops/camera_control.c` | **ngn's own real advantage**, §6 — zero new work needed |

================================================================================

## 2. GUI architecture (rtp-xyz specific)

**Everything lives in ONE session, ONE process tree, switching via
real CHTPM `<module>` layout tags** — this is rtp-xyz's entire reason
to exist as a variant distinct from rpg-xyz (§2 of rpg-xyz's own doc
covers the opposite, multi-process approach).

### 2a. Reference: `slop-ed-dev_manager.c`, read in full

Found in the TPMOS reference tree (not in this house — e.g.
`~/Downloads/1.TPMOS_c_+rmmp.0100.0110/projects/slop-ed-dev/`), this
is the real, working precedent for "editor tools embedded as CHTPM
`<module>` daemons in the same session as the game." Its real shape,
confirmed by direct read:

- **One persistent daemon per tool** (`slop-ed-dev_manager.c` is the
  main one; the plan names a second, `pal_editor_module`, for the
  event/piece editor specifically).
- **Module ownership via `is_active_layout()`**: the daemon's main
  loop is `while (!g_shutdown) { if (!is_active_layout()) { usleep;
  continue; } ...real work... }` — it checks
  `pieces/display/current_layout.txt` against its OWN known layout
  names every tick, and just idles (no-op sleep loop) whenever a
  DIFFERENT layout currently owns the screen. **This is the concrete
  answer to "how does a module hand back control"** — there is no
  explicit handoff protocol, every module just polls whether it's
  currently the active one.
- **Layout switching** is a file write:
  `transition_to_layout(path)` appends the target layout path to
  `pieces/display/layout_changed.txt` — some other, house-shared
  piece (chtpm_parser_pal.c) watches that file and does the actual
  layout swap.
- **No shared memory, no direct calls, ever** — every single
  interaction (browse a directory, load a game, save a game) is disk
  reads/writes: `gui_state.txt` for what's rendered, `history.txt` for
  input, real `cp -r` shell-outs for save/load (see §7).

### 2b. rtp-xyz's own daemon set

Matching `rpg-xyz-plan.md`'s own file listing, four separate module
binaries, not one monolith (mirrors slop-ed-dev's own real
manager + pal_editor_module split, generalized to four):

| Daemon | Owns layouts | Role |
|---|---|---|
| `manager/rtp_manager.c` | `game.chtpm`, `project_menu.chtpm`, `database.chtpm`, `plugin_manager.chtpm` | the "meta" screens — save/load, project selection, DB editing, plugin config |
| `manager/rtp_map_editor.c` | `map_editor.chtpm` | tile/entity placement |
| `manager/rtp_event_editor.c` | `event_editor.chtpm` | build/edit a piece's exported `.pal` event script (§3) |
| `manager/rtp_palette.c` | `palette.chtpm` | tile/entity brush selection, shared sub-panel usable from map editor |

Each is its own compiled `+x` binary, each independently
`is_active_layout()`-gated, all four can be running simultaneously
(idling except the one whose layout is current) — exactly matching
slop-ed-dev's own real concurrency model, not a new invention.

### 2c. Input model: keyboard-nav only, this pass

Direct instruction: **stay on CHTPM's existing digit-accumulator +
Enter-commit keyboard nav** for every rtp-xyz editor screen this
pass — matches the house-wide standard already named in
`!.xyzos-standards+1.txt` §4 (currently duplicated four times across
mutaclsym/egg-pals/zoo_0000/muchipal-editor, not yet unified — don't
add a fifth divergent implementation, reuse the existing pattern
shape). Tileset/tile picks become digit-jump selections; painting
becomes cursor-move + Enter; no mouse.

**Noted for later, not this pass**: real mouse support has a genuine,
good house precedent already — `wraith-alpha` in TPMOS. Worth a
dedicated design pass on its own, 3rd or 4th iteration once these
keyboard-nav foundations are solid, not bolted on now.

Also explicitly noted: `201.rpg-maker-clone` itself is **a helpful
local reference only**, not the emulation target. Its `ARCHITECTURE.md`
describes an earlier, pure-keyboard-nav design; its current
`README.md`/`UI_EXPECTATION.md` describe a since-evolved mouse-driven
MZ-style editor (tileset click, click-drag paint, WASD camera pan,
F2/F3/F4 modes) — that later evolution is explicitly NOT what we're
copying. When judging "does this feel like real RPG Maker MZ," defer
to the actual real product's known behavior, not this local clone's
own UI, and always bend the answer toward xyz-os's own native idioms
(CHTPM/master-ledger-style file-mediated flow, dynamic PDL menu
methods, the 2D/3D POV system) rather than a literal port.

================================================================================

## 3. Event system

### 3a. The real data shape, per placed event

Mirrors mutaclsym's own existing "one directory per piece" convention
(`monsters/<id>/`, `items/<id>/`) — an event is just another piece
type, not a special case:

```
pieces/world_01/<map_id>/events/<event_id>/
  state.txt      name=..., trigger=action|touch|autorun|parallel,
                 x=.., y=.., sprite=<emoji/glyph>
  piece.pdl      METHOD | on_interact | pal/events/<event_id>.pal
  ../../../../pal/events/<event_id>.pal   <- the real script, see 3b
```

`piece.pdl`'s `METHOD` row is the exact `fo-menu-sys.md` pattern
already proven in this house (fuzz-op's own dispatcher) — a manager
resolves the handler path via `pdl_reader.+x`, no special-cased
per-event C code needed.

### 3b. The script itself is real PAL, run by the real VM — not a
    custom interpreter

`201.rpg-maker-clone`'s own event system (`event.pal`, one command per
line, a C-level `switch` over `CMD_SHOW_TEXT`/`CMD_SET_SWITCH`/
`CMD_IF_SWITCH`/`CMD_TRANSFER`/etc.) is a real, working reference for
the COMMAND VOCABULARY — but its own execution model (a bespoke
interpreter) is explicitly NOT what rtp-xyz should do. Confirmed by
reading `system/prisc+x.c`'s real opcode table this session: PAL is
already a genuine, general, RISC-V-like assembly VM — registers
(`x0..xN`), `addi`, `lw`/`sw` (memory), **`beq`** (real conditional
branch), `j` (unconditional jump), `jalr` (call/return), `halt`,
`ecall` (syscalls: `SYS_OPEN`, `SYS_WRITE_LINE`, `SYS_GET_KV_INT`,
`SYS_SET_KV_INT`) — plus house-specific opcodes like `read_state`,
`compose_frame`, `exec <binary>`. **Every one of RPG Maker's real
event commands maps onto real, already-existing PAL primitives**:

| RPG Maker MZ command | Real PAL mapping |
|---|---|
| Show Text | `ecall "message text"` → `SYS_WRITE_LINE` into `message_log.txt` (same real file mutaclsym's own combat messages already write to) |
| Control Switches | `ecall "switches.pdl" "switch_name"` → `SYS_SET_KV_INT` (new per-project `switches.pdl`, real format borrowed directly from `201.rpg-maker-clone`'s own `SWITCH \| key \| value` rows) |
| Conditional Branch | `read_state`/`ecall SYS_GET_KV_INT` to load the switch into a register, then real `beq` |
| Loop / Break Loop | `j` to a label, exactly like `game_module.pal`'s own existing `loop: ... j loop` |
| Transfer Player | `exec ./ops/+x/event_transfer.+x <map> <x> <y>` — new, small op, reusing `move_player.c`'s own existing transition-tile write logic |
| Common Event call | real `jalr` (subroutine call) into a project-level `pal/common_events/<id>.pal`, real return — not a special mechanism, just a real function call |
| Plugin Command | `exec ./ops/+x/<plugin_op>` or a full app spawn — this IS how "Plugin Command" naturally becomes real, see §5 |
| Comment | PAL's own comment syntax |

This means an exported event script is a **real, inspectable,
hand-editable `.pal` text file**, executed by the exact same VM every
other module in this house already runs through — not a simulation of
RPG Maker, an actual reimplementation of its command set on top of a
real general-purpose VM. This is the single strongest argument for
why "export PAL scripts, associate them with entities via
`piece.pdl`" is the right approach, not just a convenient one.

### 3c. Common Events

Project-level, not per-piece: `pal/common_events/<id>.pal`, callable
by `jalr` from any event's own script (or from a switch-triggered
autorun/parallel check in the main game loop) — same mechanism as
per-piece events, just not anchored to a map tile. In scope this pass
per direct instruction.

### 3d. The editor tool itself

`rtp_event_editor.c` (rtp-xyz) generates/edits the real `.pal` text
above from a keyboard-nav command-list UI (§2c) — not writing custom
bytecode, writing real PAL source, so the result is always inspectable
by hand too (filesystem-as-database law, unbroken).

**Do not build this from scratch blind** — `&.widgits/event-editor`
already exists, with real CHTPM UI chrome (`[x]` digit-accum nav,
multi-digit jump, Commands|Scratch toggle, page cycle, GL text — all
confirmed working) but its own README marks Save/Load/Import/Export/
Edit.pal as `[~]` status-stubs-only, and "real package edit,
event_run, muta auto-spawn" as `[ ]` entirely not done. rpg-xyz's own
variant should finish THIS widget rather than build a second one (see
rpg-xyz-architecture.md §3). rtp-xyz's `rtp_event_editor.c` is a
separate binary (matches §2b's daemon-per-tool shape) but should share
the SAME real package/file shape event-editor already defines
(`ee_export_entity.c`'s own format) so events built in one variant are
at least data-compatible with the other, even though the daemons
themselves aren't shared code.

================================================================================

## 4. Database editor

**Full RPG Maker MZ category set, designed now, per direct
instruction** — overriding `ngn-vs-rm.txt`'s own "not recommended
near-term" (explicit, deliberate choice this pass, not an oversight).

### 4a. Storage: per-project, confirmed decision

Every category lives under each project's own
`pieces/registry/<category>.txt` (or a new project-scoped
`database/<category>.txt` — naming TBD at build time, doesn't change
this design), **seeded from `world_template/`** on new-project
creation, exactly matching how `terrain_types.txt`/`monster_types.txt`
already work per-project today, just extended to the full set. This
matches RPG Maker MZ's own real model (every project ships its own
complete Database) — not a shared, engine-level roster.

### 4b. Category map

| RPG Maker category | xyz-os registry | Status |
|---|---|---|
| Tilesets | `terrain_types.txt` / `furniture_types.txt` | exists, extend fields as needed |
| Items | `item_types.txt` | exists |
| Enemies | `monster_types.txt` | exists, extend with stat-block fields (§4c) that real-time combat ignores but a turn-based battle plugin reads |
| Actors | new `actors.txt` | new — player-character templates; xyz-os is currently single-hero, so v1 may be one real row plus a documented hook for future party members |
| Classes | new `classes.txt` | new — stat-growth curve per level, applied to an Actor |
| Skills | new `skills.txt` | new — named ability + effect, consumed by battle plugins (§5), not by real-time combat |
| Weapons / Armors | new `weapons.txt` / `armors.txt` | new — stat modifiers, referenced by Items |
| States | new `states.txt` | new — status-effect registry (poison, buff, etc.) |
| Troops | new `troops.txt` | new — named enemy-group compositions; a Troop id is exactly what populates a `battle_request.pdl`'s own `TTG|army_p1|...` or `POKEMON|species_id|...` payload (§5) |
| Animations | new `animations.txt` | new, **stylistically reinterpreted**: not a sprite-sheet animation, an emoji/voxel-flash sequence definition matching this house's own real rendering model |
| Common Events | `pal/common_events/` | covered in §3c, not a flat registry — kept here only for completeness of the category list |
| System | `project.pdl` | exists in embryonic form (title, start map/pos) — extend with system-wide settings as needed |

### 4c. Enemy stat-block fields (the real bridge to §5's battle plugins)

`monster_types.txt` today only needs what real-time bump-combat reads
(hp, contact damage). Extending it with `atk`/`def`/`agility`/
`skill_ids`/`xp_reward`/`item_drops` costs nothing for real-time combat
(those fields are simply unread there) but is exactly the payload a
`pokemon_menu` or `ttg_grid` battle plugin needs when a Troop/Enemy row
gets packaged into a `battle_request.pdl` (§5b). Don't design two
separate enemy schemas — one registry, extra fields, read by whichever
consumer cares.

### 4d. Editor UI

A `database.chtpm` layout (owned by `rtp_manager.c`, §2b): a
keyboard-nav category list (§2c's digit-jump pattern) → a scrollable
row list per category → a form view per row using real `<cli_io>`
text fields for each attribute, the exact same pattern already proven
this session in `agy-txt`/`file-menu`'s own rebuilt browsers — no new
UI primitive needed, just a new application of ones that already
work.

================================================================================

## 5. Plugin system — general-purpose, battle as the first real
    demonstration

Direct instruction: **no battle system, of any style, is hardcoded or
default.** Battle is the first real thing registered into a genuinely
general PAL plugin mechanism, not a special case with a plugin-shaped
wrapper bolted on after the fact.

### 5a. The general mechanism

- `projects/<name>/plugins/manifest.pdl` — `PLUGIN | id | enabled |
  param_key | param_value` rows, per project (matches §4a's
  per-project-everything convention).
- `plugin_manager.chtpm` (owned by `rtp_manager.c`) — lists registered
  plugins (keyboard-nav, §2c), Enter toggles enabled/disabled, a
  sub-form edits each plugin's own parameters — the real, direct
  analogue of RPG Maker MZ's own Plugin Manager screen (a list +
  on/off + per-plugin parameter form).
- **Any future "Plugin Command" event command** (§3b's own table) is
  just this same mechanism: `exec`/spawn a real op or app, optionally
  via a file-handoff ticket for anything heavier than a single
  synchronous call. Battle is the proof, not the whole scope.

### 5b. Battle as the first plugin(s) — reusing REAL, already-proven code

`battle-plugin-architecture.md` (prior session, read in full) already
designed this correctly and named two systems that are **not
hypothetical**:

- `205.ttg-tactics/` — grid tactics, real: `src/ttg_core.c`/
  `ttg_input.c`/`ttg_compose.c`/`ttg_loop.c`, compiled `+x` binaries,
  real `data/master_ledger.txt` (append-only audit log, same
  convention as mutaclsym's own — see §8's testing note on
  auditability) + `data/armies/`, three real, already-passing harness
  scripts. Deterministic `dmg = max(1, atk - def)`, `moved`/`acted`
  per-unit action economy, Chebyshev range, regicide win condition —
  a genuinely complete ruleset, not a sketch. **This is the direct
  match for the tactics/roguelike bias already stated as a
  preference** — but per direct instruction, it is registered as ONE
  plugin option, never assumed/hardcoded as the default.
- `203.gb-pokemon/src/battle.c` — turn-based menu battle, real:
  intro→menu→fight/run→win/lose state machine, real Gen-1-style damage
  formula.
- mutaclsym's own `tick_monsters.c` ambient bump-combat — stays
  exactly as-is, explicitly NOT routed through the plugin handoff
  (§5c explains why).

### 5c. The handoff contract (already designed, reused verbatim)

A new tray, `#.desktop/battles/` (sibling to the existing real
`{inbox,events,tiles}/`):

```
#.desktop/battles/<battle_id>.request.pdl   <- overworld writes
#.desktop/battles/<battle_id>.result.pdl    <- battle app writes back
```

`battle_request.pdl`: `META|flavor|realtime|pokemon_menu|ttg_grid`,
return map/x/y, win-switch name, lose-action, plus a flavor-specific
payload section (`POKEMON|species_id|...`, `TTG|army_p0|...`) — a Troop
row from §4b's own registry is exactly what populates this.
`battle_result.pdl`: outcome (win/lose/flee), hp delta, reward rows.
This envelope is intentionally minimal — a handoff ticket, not a
unified combat abstraction. Each battle app's own internals stay as
real and as different as they already are; nothing about `ttg-tactics`
or `gb-pokemon`'s own code needs to change to plug in.

**Build order** (per the existing doc, unchanged): wire `pokemon_menu`
first (simpler state machine, cheaper proof of the whole handoff
mechanism end to end), then `ttg_grid` (handoff already proven by
that point). Ambient real-time combat is explicitly left alone — a
scripted "boss arena" real-time flavor is a real, later possibility,
not a reason to delay the other two.

### 5d. Why real-time combat is the one flavor that stays outside this
    pattern

Ambient monster-steps-toward-hero combat happens every ~16.7ms tick,
inline, zero overhead. Forcing it through a process-spawn + file-poll
handoff for every single step would add real, pointless latency to
something that currently just works. This is a deliberate exception,
stated plainly, not an inconsistency.

================================================================================

## 6. 2D/3D POV camera system — already real, zero new work needed

Worth stating clearly since it's easy to undervalue something that
already works: mutaclsym's `0` key toggles 2D ASCII ↔ 3D GL rendering;
while in 3D, keys `1`–`4` switch `camera_mode` between **first
person, third person, free-roam, and bird's-eye/board-game view** —
all four fully implemented and verified this session
(`ops/camera_control.c` exists; `is_pov_key` includes `'4'` in
`choice.c`; `case 4` exists in `compose_rgb_frame.c`'s render switch).
Full design spec: `dox/pov-cam.md` (this project's own copy).

This is a genuine xyz-os differentiator — RPG Maker MZ has no
equivalent. Since rtp-xyz is a straight copy of mutaclsym (M1, already
done), this feature is already inherited and working; nothing in this
doc's own scope touches it. Mention it in any player-facing docs/UI
hints (§2's HUD footer already does, per `pov-cam.md`'s own §7).

================================================================================

## 7. Save/Load (rtp-xyz specific — the slop-ed-dev pattern)

### 7a. What mutaclsym's own "save" does today, and why it's not enough

mutaclsym has real save/load (`ops/save_game.c`, `world_01_template`
reset for "new game") but it's per-ENGINE, not per-project/slot — one
live world, no named save-game picker, no multi-project browsing.
RPG Maker MZ's real model is per-project saves inside a project the
designer picked from a project list — that's what §7b builds.

### 7b. Real mechanism, directly adapted from `slop-ed-dev_manager.c`

Read in full this session (see §2a). Its own save/load is deliberately
simple — not a serialization format, a **directory copy**:

```c
// save: mkdir -p <dest>, then real cp -r of maps/ + pieces/ +
// project.pdl from the live working project into the save slot
snprintf(cmd, ..., "cp -r '%s/%s/maps' '%s/'", ...);
snprintf(cmd, ..., "cp -r '%s/%s/pieces' '%s/'", ...);
snprintf(cmd, ..., "cp '%s/%s/project.pdl' '%s/'", ...);
```

`rtp_manager.c` reuses this exact shape, adapted to this house's real
path convention (per `rpg-xyz-plan.md`'s own "Decisions" table:
`xyzfs/users/<uuid>/home/projects/` for user projects, not
slop-ed-dev's own `projects/slop-ed-dev/games/<name>/`):

```
save: cp -r pieces/world_01 pieces/registry project.pdl switches.pdl
        pal/events pal/common_events plugins
      -> xyzfs/users/<uuid>/home/projects/rtp-xyz/<save-name>/
load: reverse direction, same file set
```

Everything §3–§5 designed (events, database, plugins) is plain files
under the project root, so this same `cp -r` sweep covers all of it
for free — no special-casing needed per subsystem, exactly the
filesystem-as-database law paying off structurally.

### 7c. UI

A real file-browser screen (`SET_DIR`/`SET_PATH`/autocomplete,
`file_browser.chtpm`-equivalent), same dynamic-button pattern already
proven in `agy-txt`/`file-menu` this session, reskinned for save-slot
names instead of arbitrary files. Owned by `rtp_manager.c` (§2b).

================================================================================

## 8. Testing methodology

Governing law, unchanged from the rest of this house: **real key
injection through real running processes, real frame + real
on-disk-file assertions, never an op-level shortcut treated as proof**
(§36.6 discipline, PITFALL 58 as the concrete cautionary tale — a
"successful" merge that wasn't re-proven via real injection in the
merged context wasn't proven at all).

Concretely, for rtp-xyz, the full loop from
`1-rtp-rpg-xyz-scaffold.md` §5d, unchanged:

1. **Create**: new project seeded from `world_template/`, assert real
   files exist with the right starter content (unique marker string,
   not a stale copy from a previous test run).
2. **Edit**: inject real keys into map editor (place a tile) and event
   editor (create one event, one Show Text command) — assert the
   RENDERED FRAME shows it, not just the state file.
3. **Save**: real menu-triggered save — assert the actual on-disk
   files contain what was placed/typed.
4. **Reload**: kill everything, relaunch fresh, reopen the same
   project — assert the frame matches, proving load isn't a no-op.
5. **Play**: real movement + real action-trigger keys — assert the
   built event actually fires.
6. **(rtp-xyz only) Module handoff**: inject the key/href that
   switches `game.chtpm` → `map_editor.chtpm` mid-session — assert the
   SAME process (check the PID) is now rendering the editor, proving
   `<module>` switching is real, not a silent respawn.

Plus, once §5's battle plugins exist, `battle-plugin-architecture.md`
§5 step 5's own extension: trigger the battle event via real key
injection in the overworld, assert the battle app's own window/frame
actually appears, inject real keys INTO the battle app, assert the
overworld resumes with the right switch/reward applied.

Name new cross-process harnesses `%.harnesses/rtp-xyz-full-loop/`
(matching the existing `%.harnesses/event-editor+desktop/`
convention), not inside a single project's own `test-harn-*/` — this
harness necessarily drives multiple daemons.

================================================================================

## 9. De-theming — the immediate next task after this doc

Per `1-rtp-rpg-xyz-scaffold.md` §5a and direct instruction: de-theme
BEFORE building on top, so rtp-xyz isn't just "mutaclsym with a new
folder name." Full scope, both pieces:

1. **Registries**: swap `terrain_types.txt`/`furniture_types.txt`/
   `item_types.txt`/`monster_types.txt` to generic, intentionally
   non-zombie content (grass/wall/door, stick/potion, a couple of
   generic hostile creatures) — small, ~110 lines total.
2. **Prove it's real — CORRECTED 2026-07-31, verified live**: an
   earlier draft of this doc claimed an `ensure_emoji_asset_ready()`
   auto-regeneration pipeline already wired into
   `ops/compose_rgb_frame.c` — that was wrong, checked against the
   real code. `compose_rgb_frame.c`'s own `load_emoji_voxels(asset_id)`
   only ever READS `pieces/registry/emoji_assets/<asset_id>/
   voxels_16.csv`; nothing in this project generates it. Confirmed by
   `ops/gen_voxels8.c`'s own header comment (a real, present tool, but
   it only DOWNSAMPLES an existing `voxels_16.csv` to `voxels_8.csv`
   for the 3D ray-marcher — it explicitly says the original
   rasterization pipeline that produces `voxels_16.csv` "is not
   present in this project," see this project's own `extrude-emoji.md`)
   and by `system/chtpm_rgb_render.c`'s own real on-demand pipeline
   (`ensure_emoji_asset_generated()`, PITFALL 57) — that one IS real
   and IS auto-generating, but it's keyed by hex UTF-8 CODEPOINT for
   arbitrary TEXT emoji (chat/editor use), a genuinely different
   mechanism from mutaclsym's own asset-id-keyed WORLD-TILE pipeline,
   not a fallback for it. **What actually happens today**: the 2D
   ASCII render path is unaffected (it renders the registry's own
   `glyph`/`unicode` field directly, no voxel asset needed) — verified
   live, launched fresh after this de-theme with the new `slime`/
   `slime_pup` rows, real frame rendered correctly, no crash. The 3D/
   GL path would show `load_emoji_voxels()`'s own documented fail-
   closed fallback (flat color, `glyph_fallback_rgb()`) for the new
   monster types until real `voxels_16.csv` assets exist for them —
   this is a genuine, real, PRE-EXISTING gap (no monster/item/hero
   entity ever gets a real 3D texture in this project at all yet, see
   `extrude-emoji.md`'s own separate bug report: entities render as
   flat quads in 3D mode regardless of theme), not something this
   de-theme pass caused or needs to fix. Correct verification for a
   de-theme: confirm the 2D path renders cleanly (done, live-verified)
   and confirm no crash/hang in 3D mode with a missing asset (fails
   closed by design, confirmed by direct code read) — NOT "voxel art
   regenerates," since nothing regenerates it.
3. **The action-bar itself — CORRECTED 2026-07-31, verified live**:
   an earlier draft of this doc assumed the action-bar was still
   hardcoded C dispatch and needed a fresh `fo-menu-sys.md`-style
   rewrite. Wrong — checked against the real code before starting that
   rewrite. `chtpm_parser_pal.c`'s own `load_dynamic_methods()` is
   ALREADY the real, live, fully-wired mechanism: it reads hero's own
   `piece.pdl` `METHOD` rows, skips reserved names (`move`/`select`/
   `interact`/`stat_decay`/`on_turn_end`) and any underscore-prefixed
   method (the convention for "has its own special panel, not a
   simple one-shot action" — `_craft`/`_examine` use this), and
   auto-generates `<button onClick="KEY:n">` markup into
   `${piece_methods}`, which `game.chtpm` already substitutes
   directly (`${piece_methods}<br/>` sits right after the "Control
   Hero" button). Live-verified: the rendered frame's items 2–6
   (pickup/drop/eat/save/toggle_emoji) exactly match hero's own
   `piece.pdl` METHOD order minus the reserved/underscore-hidden
   rows — this is genuinely data-driven today, not a simulation of
   it. `choice.c`'s own header comment (dated 2026-07-17) documents
   this exact wiring already having been done; an earlier read of
   only PART of that comment (an even older, superseded claim inside
   it) produced the wrong "still hardcoded" belief corrected here.
   **Nothing to build for the core action-bar.** What's real and
   still open: item 7/8/9 (craft/examine/help) are separate static
   `<button onClick="ACTIVATE">` panel-openers in `game.chtpm`, not
   part of the dynamic list (correct, by design — they open
   multi-item sub-panels, not a one-shot action); item 10 ("Info
   (href test)") is a leftover test/demo button from this session's
   own earlier module-split harness verification work, not real game
   content — worth removing or replacing with something real
   (§3's own event-system href navigation, once that exists) when
   `game.chtpm` gets its next real content pass, but not blocking
   anything.

This is genuinely the next concrete task, not a future milestone —
picking this doc back up means starting here.

================================================================================

## 10. Milestones / build order

Revises `rpg-xyz-plan.md`'s own M1–M5 into a concrete, ordered list
incorporating everything above. M1 is real and already done (verified
this session: both `300.rpg-xyz/` and `300.rtp-xyz/` are full
mutaclsym copies, already given the house's own Phase 2 per-screen
module split).

| # | Milestone | Depends on |
|---|---|---|
| M1 | ✅ DONE — copy engine in, Phase 2 module split | — |
| M2 | De-theme (§9) | M1 |
| M3 | ✅ DONE 2026-07-31 — `rtp_manager.c` + real save/load (§7), live-verified end to end (real save under real xyzfs path, real load reverting live hero state) - see save-load-report-j31.txt for the 5 real bugs found+fixed building it (PITFALL 68 is new, house-wide). No automated harness yet (M7's own scope) - this was live/manual verification only. | M2 (a de-themed project is what gets saved) |
| M3.5 | 🎨 **GUI/screen visual redesign** — prioritized OUT OF ORDER, 2026-07-31, direct instruction. Real Editor-vs-Player distinction corrected mid-design (real RPG Maker MZ has two separate top-level experiences - the Editor you build in, and the Player/Playtest window - the first design draft conflated them): new `editor_home.chtpm` becomes the REAL default boot target (a dashboard with a real Playtest action + honest "coming soon" rows for Map Editor/Event Editor/Database/Plugin Manager as those milestones land), `title.chtpm` (New Game/Continue/Options/Back-to-Editor) is reached ONLY via Playtest, plus the in-game Menu overlay redesign and Save/Load visual polish. Full design: `dox/rtp-gui-design.md`. Reason for the reorder: "start visually shaping up" and set momentum for future agents, ahead of the functional-only milestones below. | M3 (save/load, already done, is what the redesigned screens build on) |
| M4 | Event system: piece.pdl METHOD + real exported `.pal` scripts + Common Events (§3) | M2 |
| M5 | Database editor, full category set, per-project (§4) | M2 |
| M6 | Plugin system + battle-as-first-plugin, `pokemon_menu` then `ttg_grid` (§5) | M4 (battle is triggered via an event command), M5 (Troop rows feed the request payload) |
| M7 | Map editor + event editor UI polish; full harness proof of M3–M6 (§8) | M3–M6 |

Then the equivalent build order runs for rpg-xyz (see that doc's own
§10) — de-theme is nearly free there (same registries), the real work
is `&.widgits/project-menu/` and finishing `&.widgits/event-editor`'s
own real save/load/package logic instead of building a parallel one.

================================================================================

## Appendix A: Future-direction notes (not this pass, captured so
    they aren't lost)

### A1. Voxel rendering — future parameter set

Current voxel rendering is fixed: opaque, one emoji texture per face,
grid-snapped position, standard size. Stated future direction (not
designed or built this pass):

- **Transparency/translucency** as a real per-voxel render parameter.
- **Translation** — a voxel offset from its own grid-snapped position
  (not just occupying its cell).
- **Scale** — bigger or smaller than a standard voxel.
- **"Grain"** — a single voxel conceptually subdivided into an 8×8×8
  field of micro-voxels, for finer render detail without changing the
  logical 1-voxel-per-tile game-state model.
- **Two distinct texture-mapping styles**: (a) every face of the voxel
  gets its own copy of the emoji texture (today's model), vs (b) one
  emoji image gets extruded straight through the volume front-to-back —
  only the front/back faces show the "pure" emoji, side faces show
  that image's own stretched-through edge pixels, like pushing a flat
  image through clay.

### A2. Character generator

At minimum, mirror the existing `01.avatar-creation` project's own
real, working customization flow, applied to Actor/piece creation in
the Database editor (§4b) rather than building a separate sprite-part
mixer from scratch. A real hook, not a stub — worth designing once A1
and §4 are both further along, since the render style informs what a
"generated character" even looks like.

## Appendix B: xyzos-app export vision

Not RPG Maker's browser/NW.js/mobile deployment. The real target:
export a finished rtp-xyz/rpg-xyz project as a genuine **xyzos-style
app** — an executable or house-native app package — because this
whole stack (§0) is meant to become a general application development
environment over time, of which "ship a finished RPG" is the first
concrete proof, not the final destination. No concrete mechanism
designed yet; flagged here so it isn't forgotten when export actually
becomes relevant.

================================================================================
*End rtp-xyz-architecture.md*
