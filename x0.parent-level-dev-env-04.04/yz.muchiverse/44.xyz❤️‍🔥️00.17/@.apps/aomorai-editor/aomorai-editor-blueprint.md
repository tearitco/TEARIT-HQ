# 🗺️ aomorai-editor-blueprint.md — Design & Architecture

**Status: LIVING DOCUMENT.** Written 2026-08-04, direct instruction —
reference, refine, and reuse this throughout the build. Emoji-heavy on
purpose, for human skimming. This is the RPG Maker MV/MZ-inspired game
engine + editor built on top of piececraft-xyz's own real skeleton
(3D/phymoji raymarcher, board-viewer widget, prisc+x/pal VM,
master-ledger) — we are **not** rebuilding that skeleton, we're
building an **editor + engine layer on top of it**. 🏗️

---

## 📖 Table of Contents

1. [🎯 The Real Mental Model](#1--the-real-mental-model)
1.5. [🔍 Real Existing Precedent — file-menu/map-picker/tile-picker](#15--real-existing-precedent--file-menumap-pickertile-picker)
2. [📜 Master Ledger — Drilling Down (the part we need to get right)](#2--master-ledger--drilling-down-the-part-we-need-to-get-right)
2.5. [🕐 The real game-clock file (not hardcoded, never guessed)](#25--the-real-game-clock-file-not-hardcoded-never-guessed)
3. [🎬 Event Editor — Buttons AND Script, Both Real](#3--event-editor--buttons-and-script-both-real)
4. [🧱 Tiles/Phymoji — 2D Now, Voxel Later](#4--tilesphymoji--2d-now-voxel-later)
5. [🗂️ What's an "Atlas" (Project Bundle)](#5-️-whats-an-atlas-project-bundle)
6. [🪟 Widget/Window Plan](#6--widgetwindow-plan)
7. [📋 View-Editor's Own Menu (Data-Driven, Extensible)](#7--view-editors-own-menu-data-driven-extensible)
8. [🗃️ The DB (Actors/Classes/Skills/Items/Common Events)](#8-️-the-db-actorsclassesskillsitemscommon-events)
9. [🔌 Plugins](#9--plugins)
10. [▶️⏸️🔁 Play/Pause/Reset + Step Back/Forward](#10-️️-playpausereset--step-backforward)
11. [🗺️ Real Build Order](#11-️-real-build-order)
12. [❓ Open Questions (not yet decided)](#12--open-questions-not-yet-decided)

---

## 1. 🎯 The Real Mental Model

Direct quote, the load-bearing sentence of this whole doc: **"the
engine should start just like piececraft for now... later widgets will
allow saving or loading of atlases (game-projects) within the
'illusion' of the game editor app, but its really just piececraft with
more widgets."**

So, concretely:
- The real skeleton (chunk gen, hero/xelector, phymoji rendering,
  board-viewer, prisc+x/pal, the clock daemon) **stays exactly as-is**.
  Nothing here proposes changing that engine.
- "Atlas Editor" is a real **additive layer**: new widgets, new menu
  options (`View Editor`, already wired up as a real, honest stub),
  new data files (maps/db/events) that the EXISTING engine reads the
  same way it already reads `pieces/world_01/state.txt` etc.
- A "project" (an "Atlas") is just a real, namespaced BUNDLE of the
  same kinds of files piececraft already has ONE of — maps, events,
  DB, saves — so you can have MANY atlases, switch between them, but
  the underlying engine never needs to know anything new about
  "atlases" as a concept, only about "which files am I reading right
  now." 🎭

---

## 1.5. 🔍 Real Existing Precedent — file-menu/map-picker/tile-picker

**Real, important finding, checked directly (2026-08-04) before writing
any further design below**: three widgets matching exactly what this
doc needs already exist, at `&.widgits/file-menu`, `&.widgits/
map-picker`, `&.widgits/tile-picker` — real, previously built, real
harness-proven against `mutaclysm` (not stubs, not abandoned). This
section replaces guesswork in §4/§6/§12 below with real findings.

### 1.5a. file-menu — the strongest reuse candidate

Real, already built and **harness-proven** to do exactly what §5/§6
need: it's genuinely **focus-adaptive**, and when focused on a
`kind=game_world` project it does real whole-bundle `NEW_GAME`/
`SAVE_GAME`/`SAVE_GAME_AS`/`LOAD_GAME` against a real save-slot
directory tree (`saves/<slot>/world_01/...`) - a real, disk-verified
round-trip (seed demo → save → mutate → load → restored), proven via
`%.harnesses/file-menu+mutaclysm/`. It has real CHTPM multi-screen UI
(browser/save/load/peers layouts), real `ledger_append`/`ledger_peers`
participation (the ONLY one of the three that does), its own real
`button.sh`/`pal/`/`system/`. **Real, honest gap**: the actual live GL
window has never been human-verified clicking through it - all "done"
proof is file-based harness proof, not a verified live screenshot.

**Real decision for aomorai-editor**: reuse file-menu's own real
FOCUS-ADAPTIVE PATTERN (adapt save/load verbs + browse root by the
focused project's own declared `kind`) for atlas bundles (§5) - add a
new `kind=atlas_project` branch to its own real dispatch, same real
shape as its existing `kind=game_world` branch, rather than building a
separate file panel from scratch.

### 1.5b. map-picker — real, proven, but headless (no UI yet)

Real, working, harness-proven ops (`mp_list_maps.c`/`mp_switch_map.c`)
that list maps under a live project and switch the active one
(teleporting hero+xelector) - but genuinely **no GL/CHTPM chrome at
all**, no `pal/`, not even a `run`/`run-widget` button.sh verb. It's a
real backend pair, not a launchable widget program (unlike file-menu).

**Real decision for aomorai-editor**: reuse the real DATA-FORMAT
assumption (maps live as real subdirectories, one real `map.txt` +
`state.txt` per map) and the real "switch = write new focus + move
hero/xelector" logic, but the actual foldable-tree UI (§7's own real
Maps panel) needs to be built fresh - map-picker gives us proven
backend logic to call into, not a UI to embed.

### 1.5c. tile-picker — real, proven, but data model needs widening

Real, working, harness-proven ops (`tp_set_brush.c`/`tp_place.c`/
`tp_place_desktop.c`) for painting a glyph onto a map cell, plus a real
"desktop tile stamp" pattern (`tp_place_desktop.c` writes a real,
persisted `#.desktop/tiles/<name>/glyph.txt` + `meta.pdl` - a genuinely
reusable "tile palette entry" shape). **Real, load-bearing limitation,
confirmed by reading the code directly, not assumed**: the glyph is
a single ASCII `char` (32-126) end to end - `SET_BRUSH:%c`/
`PLACE_TILE:...:%c` cannot carry a real multi-byte UTF-8 emoji
codepoint at all. Its own real harness report even says so plainly:
"v1 ASCII glyphs only."

**Real decision for aomorai-editor**: reuse the real command-bus
shape (`SET_BRUSH:<glyph>`, `PLACE_TILE:<map_id>:<x>:<y>:<glyph>`) and
the real desktop-stamp persistence pattern, but the `<glyph>` field
needs a real, direct widening from `char` to a real UTF-8 string
before it can carry actual emoji tiles - flagged as real, necessary
rework in the build order (§11), not an oversight to discover later.

### 1.5d. A real convention mismatch to resolve on purpose

**Real, important finding**: board-viewer's own real integration model
(the widget reads the HOST's real project root DIRECTLY, via
`bv_state.txt`'s own `focused_project_root` field) is a genuinely
DIFFERENT real pattern from what file-menu/map-picker/tile-picker
already use (a real `focus.txt` file written by `fm_set_focus.+x`,
read by every op, plus a one-way inbox cmd-bus). Both are real, proven
patterns in this house - they are simply not the SAME pattern. Real,
open question, flagged again in §12: does view-editor's own new
widgets adopt the `focus.txt` convention (to stay consistent with the
three we're reusing), or does file-menu/map-picker/tile-picker need a
real adapter to also understand board-viewer's own
`focused_project_root` convention? Not decided here - a real,
load-bearing integration decision for whoever builds §11 step 2.

---

## 2. 📜 Master Ledger — Drilling Down (the part we need to get right)

**Real, honest correction of my own earlier confusion**: "the ledger
is the real source of truth" does **NOT** mean replaying the entire
history every tick — that was never the right design, and you were
right to push back on it. Here's the real, correct model, same one
every event-sourced system (this is a real, established pattern,
"event sourcing" is its real industry name) actually uses:

### 2a. The real split: LIVE STATE vs. LEDGER

```
LIVE STATE (materialized)          LEDGER (the real audit trail)
────────────────────────           ──────────────────────────────
pieces/hero_01/state.txt      ←──  every real change to it is ALSO
pieces/world_01/state.txt     ←──  appended as one real ledger line
pieces/<map>/entities.txt     ←──  (exactly like today already does
                                    for tick/move/wander)
```

**Real key insight**: the LIVE FILES are already the "materialized
view" — reading `hero_01/state.txt` for the hero's current HP is
already O(1), zero replay, exactly like today. The ledger is a real,
PARALLEL log of every change, for three real purposes:
1. **Audit** — a real, human-readable "what happened and when."
2. **Step-back debugging** — reconstruct an EARLIER state by real
   replay, but only ever on-demand (when you press `<<`), never on
   every normal tick.
3. **Real crash recovery / consistency check** — if live state and
   ledger ever disagree, the ledger is the tie-breaker (it's
   append-only, harder to corrupt than an in-place-rewritten file).

### 2b. Why "replay everything" is wrong, and what real engines do instead

A REAL, honest problem: if `<<` (step back) needed to replay from
literal tick 0 every single time, a game played for 10,000 ticks would
take 10,000x longer to step back once than to play forward once — a
real, unacceptable cost that GROWS FOREVER the longer you play.

**The real fix, same one real databases/game-engines with event
sourcing use: periodic SNAPSHOTS.**

```
Ledger:     [tick 0] [tick 1] [tick 2] ... [tick 500] ... [tick 1000] ... [tick 1050] (now)
Snapshots:      📸                              📸                📸
            (full state          (full state           (full state
             at tick 0)           at tick 500)          at tick 1000)
```

Real rule: every N real ticks (tune to taste — maybe every 100 ticks,
or every real in-game hour), write a real, full **snapshot** of every
piece of live state (a real directory copy or a real zip, matching
this house's own `x0.moke` compression precedent). To step back to
tick 1030, you don't replay from 0 — you load the CLOSEST real
snapshot at-or-before it (tick 1000) and replay only the real ~30
entries between there and tick 1030. Real, bounded cost — at most N
entries replayed, EVER, regardless of how long the game has been
running.

### 2c. Real, concrete answer for THIS project

- **Normal forward play**: zero replay, ever — a tick applies ONE real
  change to live state (exactly like today), and appends ONE real
  ledger line. No different from what already exists.
- **Step back (`<<`)**: load nearest snapshot ≤ target tick, replay
  forward from there. Bounded, real, cheap.
- **Step forward (`>>`) past "now"**: not really "replay" at all —
  it's just normal forward play (advance_tick + friends), same as
  always.
- **Snapshot cadence**: real, tunable config value (`snapshot_every_n_
  ticks` in a real config file, matching this project's own
  established "put it in a config file" convention from piececraft).

This is the real, complete answer to "is everything the ledger" — YES,
conceptually every real change is ledgered, but the ENGINE never pays
an unbounded replay cost for it. 🎯

---

## 2.5 🕐 The real game-clock file (not hardcoded, never guessed)

Direct question 2026-08-04: "is there a file that keeps the timedate? it shouldn't be hardcoded/in code, it should be read from that file after the daemon writes it."

**Yes — and this is already the real, existing convention, not something new to build.**

| Field | Where | Written by | Read by |
|---|---|---|---|
| `game_time_epoch_sec` / `game_time_epoch_ms` | `pieces/world_01/state.txt` | **Sole writer**: `ops/pc_clock_daemon.c` (advances it every real tick, per `autotick_speed`). **Initial value only**: `ops/pc_generate_chunk.c`, at world creation. | This project's own `ops/pc_compose_frame.c` (for its own view/UI clock display), and **cross-project**: `&.widgits/board-viewer/ops/bv_render_3d.c`'s `compute_sun_light_level()` — reads this exact file via `focused_project_root`, computes real sun-angle/sky-color from it, no hardcoded day/night. |

**No push notification exists, and none is needed** — this is the same poll-based convention every renderer in this house already uses (`chtpm_rgb_render`, `bv_render_3d.c`'s own idle loop, etc.): each reader just re-reads the file fresh on its own render cycle. A daemon "notifying" the game or ledger isn't a separate mechanism here — advancing the file itself, on disk, IS the notification; anything that polls it (every renderer already does) sees the change on its very next cycle, no event bus required.

**Real fix this session (2026-08-04):** `pc_generate_chunk.c` originally defaulted every brand-new world's epoch to `1767225600` (exactly midnight UTC) — real-world consequence: `bv_render_3d.c`'s own `compute_sun_light_level()` placed the sun below the horizon from a fresh world's very first frame, board-viewer/aomorai-editor's own view rendered dark by default. Fixed by adding `+43200` (12h) to that default, so new worlds now start at noon. **This only affects newly-generated worlds** — an already-existing save's own `pieces/world_01/state.txt` keeps whatever epoch it already had (deliberately not silently rewritten, since that's live save data, not a template) unless someone explicitly asks for that specific save to be nudged to daytime (done once, by direct request, for this project's own live `world_01` save this same session — a real, one-time save edit, not a code default).

---

## 3. 🎬 Event Editor — Buttons AND Script, Both Real

Direct quote: **"rpg maker lets users pick from buttons, or lets them
write javascript... it should all compile to a .pal script."**

Real, concrete design, directly matching RPG Maker MV/MZ's own real
precedent:

### 3a. The real command list (primary authoring mode)

A real, ordered list of real commands, picked from a real menu (NOT
hand-typed), same real UX as RPG Maker's own event editor:

| RPG Maker command | Aomorai-editor real equivalent | Compiles to |
|---|---|---|
| Show Text | `SHOW_TEXT "..."` | real `.pal` string display |
| Show Choices | `SHOW_CHOICES [...]` | real `.pal` branch on real player input |
| Control Variables | `SET_VAR name, expr` | real `.pal` register/variable op |
| Conditional Branch | `IF cond THEN ... ELSE ...` | real `.pal` `beq`/`bne` branch |
| Move Route | `MOVE entity, [steps]` | real `.pal` position-write sequence |
| Common Event Call | `CALL_COMMON name` | real `.pal` subroutine jump |
| **Script** (RPG Maker's own real escape hatch) | `RAW_PAL "..."` | the literal real `.pal` text, unmodified |

**Real, important design point**: `RAW_PAL` is the SAME real escape
hatch RPG Maker's own "Script..." command already provides (write raw
JavaScript when the button-list can't express what you want) — matches
their own real, proven precedent exactly, not an invented gap-filler.

### 3b. Real compile pipeline

```
Human-authored event (a real, ordered list of the commands above,
stored in a real, simple line-based or JSON-ish file per event)
        │
        ▼
Real "event compiler" op (a new, real house-convention .c op -
ev_compile_event.c or similar) - walks the command list, emits real
.pal instructions for each real command (a real, direct 1:1 or 1:N
mapping per command type, same real spirit as this house's own prisc+x
op-dispatch)
        │
        ▼
Real .pal script, saved alongside the event definition - THIS is what
actually runs when the game runs, exactly like every other real .pal
module already does (main_module.pal etc.)
```

**Real, honest scope note**: the FULL command-to-.pal compiler
(Conditional Branch → real `beq`, Move Route → a real sequence of
position writes, etc.) is genuinely the single biggest real chunk of
NEW work in this whole doc — real, not hand-waved, flagged in the
build order (§11).

---

## 4. 🧱 Tiles/Phymoji — 2D Now, Voxel Later

Direct quote: **"the picker screen will just have 2d tiles for now...
leave room to add more methods to the menu... by pulling all methods
from a .txt or .pdl file."**

Real, concrete design, now grounded in §1.5c's own real findings (a
real, existing `tile-picker` widget already does most of this against
mutaclysm, harness-proven):
- **v1 tile picker**: reuse tile-picker's own real command shape
  (`SET_BRUSH:<glyph>` / `PLACE_TILE:<map_id>:<x>:<y>:<glyph>`) and its
  real desktop-stamp persistence pattern (`tp_place_desktop.c`'s own
  `#.desktop/tiles/<name>/glyph.txt` + `meta.pdl` shape) - but with the
  real, necessary widening from a single ASCII `char` to a real UTF-8
  string, so an actual emoji tile can travel through the same real
  bus. A real grid of emoji tiles (same real terrain-legend-style
  glyph→emoji mapping piececraft-xyz already has), click to select,
  click on the 2D map view to place. No 3D voxel editing yet.
- **Real, deliberate extensibility**: the picker's own available
  METHODS (2D Tiles, and later "3D Voxel/Obj Picker", "Phymoji
  Picker," etc.) are read from a real `piece.pdl`-style file, EXACT
  SAME real convention `pc_menu_input.c`'s own `load_menu_items()`
  already uses for piececraft's own screens. Adding a new picker mode
  later means adding a real `METHOD` row to a real `.pdl` file — zero
  new C code for the MENU itself (the new mode's own real
  implementation is separate work, but discovering/offering it in the
  UI is pure data).
- **Later, real, flagged-not-built**: right-click a placed phymoji
  while the game is PAUSED → real per-voxel size/shape editing (your
  own real idea, direct quote: "let player edit size of the phymoji as
  it appears in 3d mode... and 2d mode if it is meant to take up
  multiple tiles like a big building") + that phymoji's own real event
  editor (§3). Real, separate, later pass — flagged here so the DATA
  MODEL (§5's own real atlas bundle format) reserves real space for
  per-instance voxel-size overrides now, even before the UI to edit
  them exists.

---

## 5. 🗂️ What's an "Atlas" (Project Bundle)

Real, proposed directory layout — one real folder per atlas, all the
real pieces a "project" bundles per your own direct list ("maps,
events, common events, db and in game save files"):

```
pieces/atlases/<atlas_name>/
    atlas_meta.txt          # real name, created-date, engine version
    maps/
        <map_id>/
            map_data.txt    # real tile grid (same real glyph format piececraft's chunks already use)
            phymoji_entities.txt   # real placed objects (same real format already built)
            events/
                <event_id>.txt      # real command-list source (§3a)
                <event_id>.pal      # real COMPILED output (§3b)
    common_events/
        <event_id>.txt / .pal      # same real shape as map-local events, global scope
    db/
        actors.txt
        classes.txt
        skills.txt
        items.txt
        # (§8 for the real per-table format)
    plugins/
        <plugin_name>/              # real, user-added .c ops / .pal scripts (§9)
    saves/
        save_01/
            world_01_state.txt      # real snapshot of live state (§2's own real snapshot mechanism)
            ledger_slice.txt        # real ledger entries since that snapshot
    data/
        master_ledger.txt           # real, full ledger for THIS atlas (matches piececraft's own real file, now per-atlas-scoped)
```

**Real, open question, not decided here**: does "loading an atlas"
mean the live engine's own real `pieces/world_01/`, `pieces/hero_01/`
etc. get SYMLINKED to point INTO the chosen atlas's own `maps/<map_id>/`
(matches this house's own real "symlink specific files" convention
already used for session isolation), or does the engine's own code
need to learn to read `pieces/atlases/<current>/...` paths directly?
Real, load-bearing decision, flagged for a real answer before the
file-menu integration (§6) gets built.

---

## 6. 🪟 Widget/Window Plan

| Window | Real status | Notes |
|---|---|---|
| Board-viewer (2D/3D game view) | ✅ Unchanged, reused as-is | Your own instruction: "leave as is" |
| **View-Editor** (new) | 🆕 To build | Real, separate GL window, own pal loop, same real launch pattern `OPEN_BOARD_WIDGET` already uses (already wired as a real, honest stub this session) |
| File-menu widget | ✅ **Real, confirmed reusable** (§1.5a) | Already built + harness-proven doing exactly this shape of job for mutaclysm - add a real `kind=atlas_project` branch to its existing focus-adaptive dispatch, don't rebuild a file panel |
| Map-picker (ops) | ✅ Real backend, 🆕 needs a real UI | Real, proven list/switch logic (§1.5b) - the Maps panel's own real UI (§7) still needs building, but calls into this |
| Tile-picker (ops) | ✅ Real backend, ⚠️ needs glyph widening | Real, proven brush/place logic (§1.5c) - real, necessary rework: `char` → UTF-8 string, before it can carry actual emoji tiles |

**Real, proposed View-Editor internal layout**: a real left-side panel
(the menu list from §7) + a real main content pane that changes based
on which menu item is selected (Maps → a real foldable tree + the
board-viewer's OWN real 2D view embedded/mirrored; Tiles → the real
picker grid; DB → real spreadsheet-like tables; Plugins → a real list
+ enable/disable toggle).

---

## 7. 📋 View-Editor's Own Menu (Data-Driven, Extensible)

Direct instruction: real menu items pulled from a real `.txt`/`.pdl`
file, matching `pc_menu_input.c`'s own real `load_menu_items()`
convention EXACTLY (same real METHOD-row format):

```
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | view_editor
METHOD       | 📁 File Menu (Save/Load Atlas)     | OPEN_FILE_MENU
METHOD       | 🗺️ Maps                            | SHOW_MAPS_PANEL
METHOD       | 🧱 Tiles                            | SHOW_TILES_PANEL
METHOD       | 🗃️ Database                        | SHOW_DB_PANEL
METHOD       | 🔌 Plugins                          | SHOW_PLUGINS_PANEL
METHOD       | ▶️ Play                             | EDITOR_PLAY
METHOD       | ⏸️ Pause                            | EDITOR_PAUSE
METHOD       | 🔁 Reset                            | EDITOR_RESET
```

Real, exact same benefit already proven in §4: a LATER real addition
(step-back `⏪`/step-forward `⏩`, or a new "3D Voxel Picker" method)
is a real, new LINE in this file, not new C dispatch code for the menu
itself (the new command's own real HANDLER is separate work, same
real split piececraft's own `keybinds.txt`/`arrow_config.txt` already
demonstrated this session).

---

## 8. 🗃️ The DB (Actors/Classes/Skills/Items/Common Events)

Real, direct RPG Maker MV/MZ equivalents, real proposed flat-file
format (same real pipe/kv convention already used everywhere in this
house, not a new format invented for this):

```
pieces/atlases/<atlas>/db/actors.txt:
    id|name|class_id|initial_level|hp|mp|atk|def|...

pieces/atlases/<atlas>/db/classes.txt:
    id|name|exp_curve_params|skill_learn_list|...

pieces/atlases/<atlas>/db/skills.txt:
    id|name|cost|effect_script_ref|...   # effect_script_ref points at a real common_event or RAW_PAL snippet (§3)

pieces/atlases/<atlas>/db/items.txt:
    id|name|type|effect_script_ref|price|...
```

**Real, honest scope note**: THIS doc doesn't lock the exact real
column set per table yet (RPG Maker's own real schemas are large and
genuinely game-specific) — flagged as real, later refinement once the
editor's own real DB panel is actually being built, not blocking
everything else in this doc.

---

## 9. 🔌 Plugins

Direct instruction: "external ops/pal scripts that user wants to use
in game, including the ones we include." Real, proposed model:

- A real plugin = a real directory under `pieces/atlases/<atlas>/
  plugins/<plugin_name>/`, containing its own real `.c` op(s) and/or
  `.pal` script(s), plus a real `plugin_meta.txt` (name, version,
  enabled/disabled).
- Real "included by us" plugins ship as real, house-built examples in
  a real shared registry (e.g. `&.widgits/aomorai-editor-plugins/`) that
  a project can real-copy in via the Plugins panel, same real
  copy-not-symlink convention board-viewer's own build.sh already
  uses for shared binaries.
- Real, open question (§12): do plugins get a REAL, sandboxed API
  surface (a defined set of things they're allowed to read/write), or
  full real filesystem access like every other op in this house? Real,
  load-bearing security-shaped decision, not decided here.

---

## 10. ▶️⏸️🔁 Play/Pause/Reset + Step Back/Forward

- **▶️ Play**: real, same as piececraft's own `autotick`/daemon model
  already built - the world ticks forward on its own.
- **⏸️ Pause**: real `autotick_enabled=0` (already exists!) - PLUS, per
  your own real instruction, pause is also the real GATE for phymoji
  voxel-size editing and per-entity event-editor access (§4) - real,
  direct reuse of a flag that already exists, not a new concept.
- **🔁 Reset**: real, honest question for later (§12) - reset to atlas
  default state, or reset to the last real snapshot (§2)?
- **⏪ Step back / ⏩ Step forward**: real, later work per your own
  instruction ("later will have..."), built directly on §2's own real
  snapshot+replay model - flagged, not in the v1 build order (§11).

---

## 11. 🗺️ Real Build Order

1. ✅ **Done this session**: `View Editor` menu entry, real honest stub
   handler; real precedent research (§1.5) confirming file-menu/
   map-picker/tile-picker as real, working reuse candidates.
2. Real convention-mismatch decision (§1.5d/§12.7) - resolve BEFORE
   step 3, since it decides how every widget below actually talks to
   its host.
3. Real View-Editor widget skeleton - own GL window, own pal loop,
   real data-driven menu (§7) reading a real `view_editor` piece.pdl,
   launched the same real way `OPEN_BOARD_WIDGET` already launches
   board-viewer.
4. Real Maps panel - real UI built on top of map-picker's own already-
   proven `mp_list_maps`/`mp_switch_map` ops (§1.5b) - foldable tree
   (real directory listing), clicking a map focuses board-viewer on
   it. "Add new map" real form (name, size).
5. Real Tiles panel - real UI built on top of tile-picker's own ops,
   AFTER the real glyph-widening rework (§1.5c/§12.8) - 2D emoji
   picker grid + real click-to-place.
6. Real atlas bundle format (§5) fully wired - real `kind=atlas_
   project` branch added to file-menu's own existing focus-adaptive
   dispatch (§1.5a), not a new file panel built from scratch.
7. Real event editor - command-list UI (§3a) + real compiler (§3b) -
   THE single biggest real chunk of GENUINELY NEW work in this whole
   doc (no existing widget covers this).
8. Real DB panel (§8).
9. Real Plugins panel (§9).
10. Real snapshot mechanism (§2) + step back/forward (§10).
11. Real per-voxel phymoji size/shape editing (§4's own later pass).

---

## 12. ❓ Open Questions (not yet decided)

Real, direct flags raised throughout this doc, collected here so
nothing gets silently assumed:

1. **Atlas-loading mechanism** (§5): symlink-swap vs. path-aware engine
   code?
2. ~~File-menu widget reuse~~ — **ANSWERED (§1.5a)**: yes, real and
   confirmed reusable, harness-proven against mutaclysm already.
3. **DB table schemas** (§8): exact real columns per actors/classes/
   skills/items - deferred until that panel is actually being built.
4. **Plugin sandboxing** (§9): real, defined API surface vs. full
   filesystem access?
5. **Reset semantics** (§10): atlas default vs. last snapshot?
6. **Snapshot cadence** (§2c): every N ticks - what's N? Tune once
   real gameplay pacing is felt, not guessed now.
7. **NEW (§1.5d) - convention mismatch**: board-viewer's own
   `focused_project_root`-direct-read model vs. file-menu/map-picker/
   tile-picker's own `focus.txt` + inbox-cmd-bus model - which does
   view-editor's own new widgets adopt, or does a real adapter bridge
   both? Real, load-bearing, not decided here.
8. **NEW (§1.5c)**: tile-picker's real glyph-widening (char → UTF-8
   string) - who owns this rework, tile-picker itself or a real,
   separate aomorai-editor-side fork of it?

Nothing in this doc is built beyond §11 item 1 - written per direct
instruction to record real intentions, ask real questions, and give us
a real, shared reference before any more of it gets coded. 🚀
