# Piececraft-HQ — the studio: edit maps, script events, play like RPG Maker

**Status:** living vision + implementer briefing (not a build ticket)  
**Date:** 2026-09-08  
**Supersedes as the “where do games get made?” map:** scattered notes in
`piececraft-hq.md` (2026-08-30 board-window progress),
`TILESETS-EVENTS-AND-GAME-CLONES.md` (find-it-later paths),
`CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` (desktop 3D).
Those files stay; this one is the product story **and** the briefing a
lesser model should read before writing Transfer / Shop / Battle / db
consumers or clone skins.

House root: `44.xyz.01.00`.

---

## 0. One sentence

**Two map rooms, one event engine.** Piececraft-HQ is the 2D/3D voxel
board. The **livedesk desktop is also an RPG Maker–shaped map** (pals =
events/characters, tiles = tileset, z layers, transfers between desks,
session save/load). Edit vs Play is **input routing** (Interact), not
hiding chrome. Palettes / db-hq / events-hq / registry already exist;
play still needs real consumers for kv-only cmds.

---

## 1. Product split: Edit vs Play (do not hide chrome)

| | **Edit** (Interact OFF) | **Play** (Interact ON) |
|---|---|---|
| Who | you / an agent | the player (or you testing) |
| Chrome | strip, palettes, db-hq, File/Desk, event editor — **stay visible** | **same chrome stays.** You can still open palettes or db while testing. |
| Input | strip / In toggle / File / Desk / click-to-stamp / click-to-edit event | arrows/WASD/Action go to the map (board **or** desktop). Keys that belong to the game do not steal the strip unless FocusOut disengages. |
| Tiles | palettes brush → drop/stamp | tiles are the world; stamp still *can* work if you click a picker — do not special-case hide |
| Events | markers optional; click cell → events-hq | pages fire (Autorun / Parallel / Action Button / Player Touch / Event Touch) |
| DB | db-hq-pal while paused or not | game **reads the same files**; opening db mid-play is allowed |

**House rule (owner, 2026-09-08):** we do **not** hide chrome in play.
Do not add `play_mode` that `show=`s the toolbar, palettes, or window
chrome off. Optional later: a *thin* overlay flag for collision ghosts
or event balloons only — never the livedesk strip, never the board
window frame.

**Today:** Interact Mode is the play *input* path (WM-managed board +
var-arm). There is no hide-chrome flag and there must not be.

**Do not revive** `run_pchq_board_mode()`. NU Interact + WM-managed is
the contract.

---

## 1b. The desktop IS a map (first-class, not a later 3D toy)

RPG Maker’s map editor is: tiles on layers, events on cells, player
spawn, transfers to other maps, save files. Livedesk already is that
shape:

| RM map concept | Livedesk analog | Notes for implementers |
|---|---|---|
| Map | a **desk** (`xyzfs/.../livedesk/sessions/s1/desks/<name>.pdl`) | `office.pdl` etc. `DESK` rows are placed objects |
| Tileset | palettes + `#.desktop/tiles/` packages + rmmv/cdda/mineclonia pickers | stamp onto desktop **or** pchq chunks |
| Characters / events | **pals** (`pals/<id>/pal.pdl`, `desktop_pos.txt`) | a pal is an RM event: graphic + position + (later) `event_pkg` |
| Z layers | board-viewer z (`chunk_*_zN`, camera `c`/`v`, hero `x`/`z`) **and** desk stacking | RM has A1–A5 + B/C/D/E; we have z files. Do not invent a second z system. |
| Transfer Player | change desk **or** change pchq map/chunk + move hero | same command, two consumers |
| Vehicles / followers | extra pals parented to the player pal | RM boat/ship/airship = pals with `through` + `move_route` |
| Save / Load | session desks + `runtime/ledger.txt` + File menu | RM `SaveN.rvdata2` ≈ snapshot of desks + switches + gold + party |
| Common Events | db-hq CE tab → `common_events/<name>/event_pkg` | autorun/parallel CE still run on the **desktop** in play |
| Map properties | `board_config.txt` / desk header | encounter steps, BGM, tileset id — System tab + per-map kv |

Cursword-as-3D-halo (`CURSWORD-DESKTOP-3D-…`) is camera/controller
chrome on **this same map**, not a different world. Do not block
transfers/shops on that design.

**Implication:** when you implement Transfer Player, Shop, Battle, you
implement them against **map identity** (`map_id` that can be a desk
name **or** a pchq `maps/<id>`), not against “the 3D window only.”

---

## 2. What is already in the room (2026-09-08)

Launch board: **HQ → piececraft-hq** (`livedesk:open-piececraft-hq`) or
Toys → **Piececraft-HQ** (not **Piececraft** / `piececraft-xyz`).

| Piece | Where | Honest status |
|---|---|---|
| Board window | `@.apps/piececraft-hq/pchq-board.xhtpm` + projector + `pchq_board_action.sh` | 🟡 opens, WM-managed, Interact arms from vars; canvas reads live `canvas_raw` |
| 3D/2D view | `&.widgits/board-viewer` POV `1`–`4` | ✅ engine |
| Palettes | `button-pal.sh` cats | 🟡 pickers live; **no drop onto board or desk** |
| Assets | clones in `NNEST-12.00/#.NNEST_ASSETS/` (optional); pointers in `44.xyz.01.00/shared/*-ASSET-SOURCE-LOCATION.pdl` | ✅ C reads shared/; must not hardcode clone paths |
| Sample maps | `pieces/system/maps/{mineclonia_sample,cdda_sample}/` | 🟡 File load copies `map.txt` → chunk |
| Events editor | `&.widgits/events-hq` | ✅ IR → pal → `cmd_N.sh` |
| Common Events | db-hq-pal CE tab | 🟡 in-tab; other tabs are field tiles |
| Event registry | `#.ref/menu/event_commands.registry.pdl` + `mr_world.+x` | 🟡 transfer/shop/battle/fade **kv-only** |
| Event guides | `#.ref/menu/event-guides/` | ✅ authoring; not auto-attached |
| Desk persistence | `desks/<name>.pdl` `DESK` rows | 🟡 pals persist; `#.desktop/tiles/` still vanish |

---

## 3. Long-term vision (one engine, many skins)

```
 palettes (rmmv / mineclonia / cdda / tiled / ohr / emoji)
        \  drop / stamp onto DESK or pchq CHUNK (same brush files)
         \                    db-hq (actors, items, troops, CE, terms)
          \                      |
           +--> MAP = livedesk desk  OR  piececraft-hq chunks
          /         |                    (same z, same events)
 events-hq <--------+  click cell in EDIT
          |
          v
     PLAY = Interact ON + event pages + db files play actually reads
     chrome stays
```

POV `1`–`4` is a camera, not a second map format.

**Do not start GTA** until a tileset PDL exists. Civ/Pokemon/CDDA/MC
are **skins** of this loop (see §9).

---

## 4. Next steps (ordered)

Do **not** hide chrome. Do **not** revive `run_pchq_board_mode()`.
Do **not** hardcode `#.NNEST_ASSETS` in C.

### Loop A — stamp

1. Palettes `arm-*` → click-to-stamp on pchq canvas **and** desktop.
   `pchq_place_cell.sh` is the stand-in.
2. Persist into chunk files **and** `DESK` rows
   (`TILE-PLACEMENT-DESK-PERSISTENCE-GAP-2026-08-29.txt`).

### Loop B — attach event

3. Click cell → events-hq `event_pkg` (create if missing).
4. Optionally seed from `event-guides/` (data, not per-game C).

### Loop C — play consumers (this briefing)

5. Play = Interact ON + pages running. Chrome stays.
6. Promote kv cmds: Show Text → Change Gold → **Transfer Player** →
   **Shop** → **Battle** (order below).
7. db-hq field edits must write **play-readable** files, not only
   `#.desktop/db_hq_*.state.txt`. Terms is INI `[Basic Terms]`.

### Loop D — cameras

8. One chunk/desk format; POV stays engine-side.

---

## 5. What “done” looks like for a first playable

A stranger can:

1. HQ → piececraft-hq **or** stamp on the desktop.
2. Palettes → stamp a floor and a door.
3. Click the door → events-hq → Transfer or Show Text → save.
4. Press **In**. Walk. Door transfers (desk or map). Chrome still there.
5. Talk to a pal → shop. Step on a troop region → battle. Gold changes.
6. File save / load restores map, gold, switches, party.

Until stamp + transfer work on hardware, this is a **viewer with File
load**, not a studio.

---

## 6. Implementer briefing — RPG Maker guts a coder may not know

Audience: a competent coder who has **not** lived in RPG Maker VX Ace /
MV. Do **not** invent a new opcode list. The registry
(`event_commands.registry.pdl`) **is** the opcode list. `mr_world.+x`
already writes kv. Your job is **consumers** that change the live
world.

RM mental model (keep this or you will overbuild):

- **Map** = tiles + **events** (each event = graphic + 1..N **pages**).
- **Page** = conditions (switches/variables/self-switch/item/actor) +
  **trigger** + **command list**. Highest matching page wins.
- **Triggers:** Action Button (face + confirm), Player Touch (hero
  walks onto event), Event Touch (event walks onto hero), Autorun
  (blocks player until page ends or page conditions fail), Parallel
  (runs alongside player; must yield).
- **Interpreter:** one stack per map event that is running; Common
  Events have their own. Autorun occupies the map interpreter.
  Nested `call_common_event` pushes a frame. **Wait** and **Show Text**
  pause that interpreter, not the whole engine (Parallel still ticks).
- **Self switches** A–D are **per event instance**, not global. Global
  switches/variables are the db System / event Control Switch/Variable.
- **Player** is not an event, but followers and vehicles behave like
  events with a follow route.

House already has: events-hq pages, IR, `cmd_N.sh`, registry templates,
gold/switch/variable cmds, CE tab, desk save of pals. Missing: the
**interpreter loop in play** and the **three heavy cmds** below.

### 6.1 Transfer Player (`COMMAND transfer_player`)

**Registry today:** `mr_world.+x` `"$ENT" transfer '{map}' '{xy}' ''`
→ kv in `map_state.pdl`. Camera does not move. Desk does not change.

**What RM actually does (order matters — get this wrong and you leak
Autoruns):**

1. **Freeze** the current map interpreter (finish the *current* command
   only; do not run the rest of the page after the fade in a new map
   unless the designer put commands after Transfer — RM **does** run
   them *after* arrival if they exist, but most events Transfer as last
   cmd). Safer house rule: **Transfer is last-on-page** in guides;
   still support leftover cmds on the *new* map’s interpreter, not the
   old.
2. **Fadeout** (`fadeout_screen`) unless fade type is “none”. Registry
   already has fadeout/fadein kv. Consumer: actually dim the canvas /
   desk (tint is enough; do not hide windows).
3. **Unload** old map events (stop Parallel/Autorun). **Keep** party,
   gold, items, global switches/variables, self-switches **keyed by
   (map_id, event_id)** so a door on map A does not clobber door B.
4. **Load** target:
   - If `map` is a pchq sample / chunk id → `load-map` path already
     copies `map.txt` → `chunk_0_0_z0.txt`. Extend: load **all z**
     files if present; load `events.pdl` into live event instances.
   - If `map` is a **desk name** → switch `desks/<name>.pdl` (session
     already has multiple desks). Place hero pal at `xy`.
5. **Place party** at `x,y` facing `d` (registry PARAMS are `map,xy`
   only — **add a third field for direction** when you touch the
   registry: `2/4/6/8` RM numpad, or `down/left/right/up`). Default
   retain facing if empty.
6. **Center camera** on party (board-viewer already has hero vs camera
   keys). Do not teleport camera without the hero.
7. **Fadein**. Then start Autorun/Parallel on the **new** map.

**RM gotchas you will hit:**

- **Same-map transfer** (stairs on one map to another cell): do not
  unload events if map_id unchanged; do move the player; **do** re-eval
  page conditions (walking onto a region that flips a switch).
- **Vehicles:** if player is in a boat, Transfer onto land must
  **leave the vehicle** or you trap them. House: if vehicle pal exists,
  Transfer sets `through=0` and unparents.
- **Followers:** snap to player tile, then let move_route catch up.
  Do not Transfer each follower with its own fade.
- **Looping maps / wrapping:** RM optional; we can skip. CDDA overmap
  transfer is a *different* consumer of the same cmd (see §9).
- **Z:** RM maps are one z. We have z layers. Transfer PARAMS should
  allow `x,y,z` (third number optional, default 0). Put z in `xy` as
  `8,8,1` until FIELD3 exists — document it in the template comment.
- **Encounter flash / map BGM:** System + map properties. After
  transfer, play map BGM unless a vehicle BGM is active.

**Files to read/write (do not invent parallel state):**

- `map_state.pdl` (already written by `mr_world`)
- pchq `board_config.txt`, `chunks/chunk_*_z*.txt`, `maps/<id>/events.pdl`
- desk `desks/<name>.pdl` + player pal `desktop_pos.txt`
- `self_switches.txt` already in `mr_world` family — **key by map+event**

**Acceptance:** door on `cdda_sample` with `transfer_player` to
`mineclonia_sample` (or desk `office` → another desk) actually changes
what you see, hero at target xy, fade kv honored, old Autorun stopped.

### 6.2 Shop Processing (`COMMAND shop_processing`)

**Registry today:** goods = comma item ids → `shop_state.pdl`. No UI.

**What RM actually does:**

- Opens a **modal buy/sell** (purchase-only flag exists in Ace; MV
  shop can disable sell). Party gold in the corner. List of goods from
  **Items / Weapons / Armors** db rows, **price** from db (not from the
  event — the event only lists **which ids** are sold). Some RM shops
  pass a custom price override; we can skip override v1.
- Buy: `gold >= price` and inventory not full → `change_gold -price`,
  `change_items +1`. Sell: usually 50% of db price (System term
  “Sell Rate” if you add it; default half).
- Cancel / OK closes; event continues.

**House mapping (reuse, do not write a second inventory):**

- **Gold:** already a cmd / piece. Shop must call the same gold file
  play reads.
- **Items:** db-hq Items tab must emit a play file
  `id | name | price | icon | consumable | …`. Until that tab writes
  play JSON/pdl, shop can read `#.desktop/db_hq_items.state.txt` **only
  if** you also write a canonical `db/Items.pdl` from the same save
  path. **Do not** have shop parse the dashboard `.xhtpm`.
- **UI:** a khtpm toy or overlay list is enough (Show Choices is a
  legal v0: “Buy Potion 50G / Buy Hi-Potion 150G / Sell / Leave”).
  Real shop window is Loop C polish. **Do not** block on a pretty UI.
- **Minecraft crafting / CDDA crafting / Pokemon mart / Civ luxuries**
  are **the same command** with different goods lists and maybe a
  `mode=buy|craft|barter` later. v1 = buy/sell against gold.

**RM gotchas:**

- Shop during Autorun blocks the map; Parallel shops are rare and
  cursed — ignore Parallel shops.
- Empty goods list = sell-only dump shop (RM allows this).
- Items with price 0 are not sold (hidden) unless the event listed
  them — still show if listed.
- Stack vs unique: RM items stack; weapons/armor often unique
  instances. v1: everything stacks. Pokemon held items / MC tools
  with durability come later as extra fields, not a new cmd.

**Acceptance:** pal with `shop_processing` goods=`1,2,3`, gold 100,
buy one, gold and inventory files change, event resumes with Show Text.

### 6.3 Battle Processing (`COMMAND battle_processing`)

**Registry today:** troop id + can_escape → `battle_state.pdl`. No fight.

**What RM actually does (you need this sequence, not a Unity combat
framework):**

1. Optionally **fade + battle-start SE** (System sounds).
2. Switch to **troop** layout: enemies from db Troops → list of Enemies
   rows (hp/mp/atk/skills/drop/gold/exp). Party = Actors in the
   party (not all Actors in db).
3. **Turn loop:** for each battler, AGI order (or ATB if you are insane
   — **do not** do ATB in v1). Command: Fight / Escape / Item / Skill /
   Guard. Skills cost MP, hit formula `a.atk * 4 - b.def * 2` is the
   RM default — copy it; do not invent.
4. **Victory:** gain exp/gold/drops, death of all enemies. **Abort /
   escape:** if `can_escape=1` and luck/agi check. **Defeat:** if
   `can_lose` (registry FIELD2 is only escape today — **add can_lose**
   as FIELD3 or pack `can_escape,can_lose`). RM “If Win / If Escape /
   If Lose” are **event branch commands after Battle Processing**.
   House: after battle, set a variable `last_battle=win|escape|lose`
   so the same page can `conditional_branch` on it. That is how RM
   event pages do post-battle without a special interpreter opcode
   beyond the built-in battle result branches.
5. Return to map; fadein; continue event.

**House mapping:**

- **Troop / Enemy / Actor / Skill** tabs in db-hq must write play
  files. Placeholder field tiles are **not** enough. Minimum viable
  battle: 1 actor (hp/atk/def), 1 enemy, Fight + Escape, gold on win.
- **UI:** Show Choices “Fight / Escape” + Show Text for damage is a
  valid v0. Side-view battlers (RMMV `$sv_actors`) are palettes you
  already have — draw later.
- **CDDA melee, Pokemon, Civ combat** are **skins of this cmd** (see
  §9). Do not fork `battle_processing`.

**RM gotchas:**

- **Preemptive / surprise** from map encounter steps — skip for event
  battles v1.
- **Troop events** (RM troops have their own pages: “at turn 3, extra
  slime”). v1 skip; v2 is a nested events-hq pkg on the troop id.
- **Death state** vs HP=0: RM uses States. v1: HP<=0 = dead, remove
  from turn order.
- **Escape from event battle** often disabled (`can_escape=0`) for
  bosses. Honor the flag.
- **Do not** pause the whole livedesk OS. Battle can be a **modal
  khtpm** on top of chrome (chrome stays). Interpreter waits.

**Acceptance:** event `battle_processing` troop=1 can_escape=1, Fight
reduces enemy hp in a file, win writes gold+exp, map event continues.

### 6.4 Remaining db plumbing (play cannot read the dashboard)

db-hq-pal has 15 RMMV tabs. **Only Common Events is a real editor.**
`#.#.calendar-dox` and `#.ref/menu/db-tabs-remaining.txt` are the
ladder. Terms is **INI** (`[Basic Terms]` in `db_hq_terms.state.txt`),
not `TAG|id|name` — a list projector that assumes TAG will show empty
(already happened).

**Rule:** every tab that play needs must **emit a canonical pdl/json
under a stable path** (suggest `44.xyz.01.00/#.desktop/db/play/` or
xyzfs `home/db/`) **on save**, in addition to whatever `state.txt` the
UI uses. Play **never** greps `.xhtpm`.

| Tab | Play needs | RM meaning (short) | v1 columns |
|---|---|---|---|
| Actors | party members | class, slots, start level, battler graphic | id, name, hp, mp, atk, def, face, character |
| Classes | optional v2 | features + learnset | skip v1 (fold stats onto Actor) |
| Skills | battle + field | skill type, mp cost, scope, formula | id, name, mp, formula, scope=1 enemy |
| Items | shop + battle item | consumable, price, effects | id, name, price, effect=recover_hp |
| Weapons / Armors | equip | etype, params | skip v1 or treat as items |
| Enemies | battle | params, actions, drop, gold, exp | id, name, hp, atk, def, gold, exp |
| Troops | battle | list of enemies + coords | id, enemy_ids |
| States | poison etc | skip v1 | — |
| Animations | flash | skip v1 | use `flash_screen` |
| Tilesets | passability | A/B mode, passage, bush, counter | **needed for walk**; can hardcode passage on glyph until tab writes |
| Common Events | already | trigger + switch | keep events-hq pkg |
| System | start party, start map, gold, BGM, boat/ship/airship, terms pointers, battle opts | start_map, start_xy, start_gold, party_actor_ids, title | **do this early** — Transfer’s default dest |
| Terms | vocab | INI sections Basic / Commands / Params / Messages | parse INI; do not TAG-split |
| Types | skill/equip/element | skip v1 | — |

**System tab is the silent blocker.** RM “New Game” reads System:
party, start map, start xy, gold. Without that file, Transfer has
nothing to default to and battle has no party.

**Passability:** RM tileset flags (○/×/☆, 4-dir arrows, bush, ladder,
counter, damage floor). House glyphs in `terrain_legend.txt` can carry
a `pass=0|1` column **now** so you are not blocked on the Tilesets tab.

**Do not** store play state only in `#.desktop/db_hq_*.state.txt`.
That path is a **form buffer**. Duplicate on write.

### 6.5 Interpreter / fade / move route (do not skip)

These are already registry cmds; consumers:

- **Show Text / Show Choices:** must block the interpreter. You already
  have message-ish toys; wire them as the wait point.
- **Fadeout/in, tint, flash, shake:** `screen_state.pdl` — board-viewer
  and desk compositor should **read** that each frame (tint multiply).
- **set_move_route:** RM routes are lists (`Move Down, Wait 10, Turn
  Left, Switch ON A`). v1: parse a tiny DSL in the FIELD2 string.
  Needed for MC gravity? No — gravity is a Parallel CE. Needed for
  NPC patrols and Pokemon wandering: yes.
- **scroll_map:** camera without moving hero (cutscenes). Board already
  distinguishes hero `x/z` vs camera `c/v`.
- **change_menu/save/encounter access:** flags files exist. File menu
  Save should **no-op** when save_access=0 (RM grayed menu).

### 6.6 Save / Load (desktop + board)

RM save blob: map_id, xyz, party, items, gold, switches, variables,
self-switches, screen tint, vehicles, save count.

House already: desk pdl, pal history, ledger, pchq chunks. **Unify
into one snapshot dir** per slot (`saves/slotN/`) that copies:

- current desk name + all `DESK` rows + pal `desktop_pos` + `pal.pdl`
- pchq chunks + `board_config` + `map_state.pdl`
- gold, items, party, switches, variables, self_switches, timer
- System start is **not** overwritten (New Game reads System; Continue
  reads slot)

Load = copy back + Transfer-without-fade to saved map_id/xy. **Do not**
re-run Autoruns that already completed unless their page conditions
still match (self-switch A on a chest must stay ON).

### 6.7 Range overlay (Civ / tactics / AoE) — **priority**, split builtin vs plugin

Tactics games (Civ unit move, Fire Emblem, XCOM, FFT, RM “grid battle”
plugins) all need the same picture: **blue tiles you can walk to, red
tiles you can attack, a path ghost, click only those cells.** Owner:
treat this as a **priority**, not a Civ polish item after shops.

RPG Maker **does not** ship this on the map. Ace/MV give passability +
screen tint. Grid-range is always a **plugin** there (Yanfly, Victor,
VisuStella) because RM combat is a separate scene. **This house is
different:** the map *is* the tactics board (desktop pals, pchq
chunks). So we do **not** copy “it’s only a plugin.” We also do **not**
put Civ math in `khtpm_core_render.c` or `bv_render_3d.c`.

**Verdict (do this split, not a single blob):**

| Piece | Builtin or plugin? | Why |
|---|---|---|
| **Passability** (`pass=0\|1`, 4-dir, z-up/down) | **Builtin map data** | Transfer, walking, CDDA doors, MC gravity, overlay BFS all need it. `terrain_legend.txt` column now. |
| **Cell overlay compositor** | **Builtin, generic** | Board-viewer already blits `rgb_frame_3d_overlay.raw`. Livedesk/pchq must **tint cells from a list file**, same way `tint_screen` tints the whole view. No game name in C. |
| **Reachability BFS** (walk cost, occupancy) | **Builtin map service** (small `+x` / one C helper next to chunks, **not** the khtpm renderer) | Dijkstra/BFS on the grid is engine-shaped (RM already pathfinds internally). Reused by overlay, NPC `move_route` “approach”, CDDA zeds, Pokemon wandering. |
| **Range rules** (min/max, Manhattan vs Chebyshev vs facing, “attack after move”, min-range bows, ZoC) | **Plugin = db + event, not a .so** | Skills/Weapons `range_min,range_max,range_shape`; Actor/unit `move_points,move_cost`. Civ vs FE vs XCOM differ **here only**. |
| **When it shows / what click does** | **Plugin = registry cmd + CE** | `show_range` / `hide_range` / `select_in_range` as events-hq commands. Civ “select unit” CE writes the overlay; RM spell AoE uses the same cmd. |

“Plugin” in this house means **registry command + pdl data + Common
Event**, the RM meaning — **not** a dlopen module and **not** a
`g_is_civ` branch.

**Do not:**

- Draw range inside `khtpm_core_render.c` with a Civ flag.
- Fork a second overlay path for desktop vs pchq (one `range_overlay.pdl`
  both compositors read).
- Wait for a full battle engine — overlay is useful **before** battle
  v0 (move preview, Transfer dest, explosion radius, edit collision
  ghosts).
- Use RM **`tint_screen`** (one multiply on the whole map: dusk, flash,
  fade) as a stand-in for **per-cell** range. That opcode cannot mark
  individual tiles. **This is not calling the existing placer “fake.”**
  The placer is a real click-capture surface (see next subsection).
- Precompute every unit’s range every frame. Recompute **on select**
  and when passability/occupancy changes.

**Existing tile-picker place vs range select — same job, fix and share**

Owner report: armed place from the picker does **not** show a grid, feels
weird, and the hole for the picker window **does not follow** if you
move the picker.

That path is real today:

- Palettes click → `palettes_menu.sh arm-rmmv` → `tp_set_brush_rmmv` +
  detached `tp_arm_placer_rmmv.+x`.
- Placer (`&.widgits/tile-picker/ops/tp_arm_placer_rmmv.c`) maps up to
  **four override_redirect InputOutput windows** covering the **whole
  screen** with ~12% amber (`_NET_WM_WINDOW_OPACITY`), **except** a
  rectangle snapshotted as `picker_x/y/w/h` at arm time. Click →
  `tp_place_desktop_rmmv.+x`. Esc cancels. Mutter/XWayland cannot see
  clicks on bare desktop, so a mapped capture surface was the live
  fix (2026-08-29), not a prototype.

Why it feels like a dumb whole-screen wash (and not a map grid):

1. **No cell grid.** The capture windows are four screen strips, not
   desk/pchq tiles. Nothing reads chunk size or desk cell pitch.
2. **Hole is frozen.** Rect is argv at spawn. Moving the palettes
   window does not `XMoveResizeWindow` the strips. The hole sits where
   the picker **was**.
3. **Rect often never arrives.** `khtpm_core_render.c` puts
   `g_win_x, g_win_y, g_window->w, g_window->h` on the `arm-rmmv`
   action, but `palettes_menu.sh` documents that dispatch **appends
   pkg_dir + house_root**, so `$5–$8` are **paths**, the integer check
   fails, and the placer covers **the entire screen** with no hole.
   That is the “doesn’t show where the original window was” bug.
4. **`g_win_x/y` may be stale** even when integers get through (create
   geom, not live `XGetGeometry` after drag).
5. **arm-pc / arm-cdda** do not necessarily share this placer; emoji
   `tp_arm_placer.+x` is a **root grab** (broken on this Mutter) plus
   board-viewer hit-test. Three place paths, one should remain.

**Yes: integrate place-from-picker with range-select.** Both are
“highlight legal cells, take one click, write a coordinate.” Place is
`kind=place` on **every passable (or all) cells**; tactics range is
`kind=move|attack` on a BFS subset. Same compositor, same
`select_in_range` (or `select_cell`). Do **not** keep a second
full-screen amber OS overlay as the long-term place UI.

Target shape:

- Armed brush writes `range_overlay.pdl` with `kind=place` cells (or
  one `kind=all` + brush kv). Map compositor tints **tiles**, with an
  optional grid stroke on cell edges.
- Click is hit-test on the **map** (desk cell or pchq voxel), not a
  screen-sized X window. Palettes stays a normal window; no hole to
  track.
- If XWayland still cannot click “bare” desk, the capture surface may
  remain as a **hit-test shim only** (InputOnly or 1% opacity), but
  **drawing** (grid, legal cells, brush ghost) lives in the map
  overlay file — not in the shim’s background pixel.

Short-term placer fixes if place must work before the compositor
(optional, do not polish the amber forever):

- Pass picker **xid** (or pid) into the placer; **poll
  `XGetGeometry`/`XTranslateCoordinates`** and resize the four strips
  when the picker moves. That is the actual “window moved” fix.
- Stop stuffing geom on `action=` after house_root; use a state file
  `picker_geom.txt` the projector writes every layout, or pass xid
  only.
- Draw a **cell grid** in the capture windows (or drop drawing there
  and use overlay.pdl even with the shim).
- One placer for rmmv/pc/cdda/emoji.

`tp_range_grid.+x` (AU14 screen-space diamond) was **deleted 2026-09-08**.
Do not revive it. Pet menu Move is a no-op until `select_in_range`.

**`tint_screen` vs the amber placer (wording):** `tint_screen` is an
RM **event command** that dyes the whole playfield one color (night,
damage flash). Using that command as “range overlay” would be the
wrong tool — that is all “don’t fake with whole-screen tint” meant.
The **amber capture** is a different, real, house-specific overlay; it
is the right *click* workaround and the wrong *paint* for grids. Keep
the click lesson; replace the paint with cell tints.

**File contract (implementers — keep this shape):**

```
# range_overlay.pdl  (map-local; empty = hidden)
OVERLAY | on=1 origin=4,7,0 kind=move
CELL    | x=4 y=7 z=0 kind=origin
CELL    | x=5 y=7 z=0 kind=move cost=1
CELL    | x=6 y=7 z=0 kind=move cost=2
CELL    | x=6 y=6 z=0 kind=attack
CELL    | x=5 y=7 z=0 kind=path   # optional walk ghost
```

Kinds to support v1: `origin`, `move`, `attack`, `path`, `aoe`,
`blocked` (optional). Colors are **theme/CSS or one kv map**
(`move=#4a7`, `attack=#c44`) — not hardcoded per clone.

**BFS rules v1 (RM + FE + Civ all survive this):**

- 4-dir default; `shape=diamond` (Manhattan) for move; `shape=square`
  (Chebyshev) optional for king-move / Civ diagonal later.
- Cost: flat 1, or `move_cost` on the glyph (road 1, forest 2, mountain
  impass). Civ **needs** cost; FE too. Pokemon overworld can ignore
  cost.
- Occupancy: allied units block **through** but you may **pass**
  (FE) vs **stop** (Civ). One flag `pass_units=0|1` on the overlay
  request. Do not special-case “Civ.”
- Attack overlay = either (a) cells within `range_min..range_max` of
  **origin** (ranged before move) or (b) union of ranges from every
  **reachable move cell** (move-then-shoot). Request flag
  `attack_from=origin|move_cells`. Civ ranged bombard = origin;
  FE swords = move_cells then adjacent.
- Z: same z only in v1. Stairs/ladders later (CDDA).

**New registry cmds (plugin surface, kv like shop today):**

- `show_range` — PARAMS: `origin`, `move_pts`, `atk_min,atk_max`,
  `shape`, `attack_from`. Writes `range_overlay.pdl` via the BFS
  helper.
- `hide_range` — empty file / `on=0`.
- `select_in_range` — **blocks** the interpreter until the player
  clicks a `CELL` of allowed `kind` (or cancel). Writes
  `selected_tile=x,y,z,kind`. Then the event `move_route` / Transfer
  same-map / `battle_processing`.

That last cmd is the tactics **menu**. RM Show Choices is the v0
stand-in only until this exists; **do not ship Civ with only
choices** if overlay is a priority.

**Who paints:** compositor already has 3D overlay blit. Cell tint
should be **under** pals/units, **over** terrain (RM events sit on
tiles). Desktop map: same — tint desk cells under pals. **Chrome
stays**; this is map paint, not a new window.

**Reuse beyond Civ:**

| Game | Overlay use |
|---|---|
| Civ / tactics | move + attack on unit select |
| RM field | spell AoE, “where does this Transfer land”, shop? no |
| Pokemon | optional move tiles; wild-grass is **not** this |
| CDDA | explosion / fire radius, throw range, stairs preview |
| Minecraft | torch light later; v1 skip. Chest radius? no |
| Edit mode | collision ghosts / event markers **may** use `kind=blocked` — still not hiding chrome |

**Sequence bump (priority vs Loop C):** passability column **before**
or **with** Transfer (walking is Transfer’s cousin). Overlay compositor
+ empty pdl **next** (you can stamp fake CELLs and see tints — that is
the hardware proof). BFS helper + `show_range` **before** Civ End Turn
and **before** pretty battle UI. `select_in_range` before Civ is
playable. Shop/battle v0 can stay parallel; **do not** block overlay
on battle.

**Acceptance:** select a pal, `show_range` with move_pts=3, blue cells
appear on **desktop or pchq**, click a blue cell, pal moves, overlay
clears. Red attack cells optional in the same demo. No renderer
`g_is_*`.

---

## 7. How a lesser model should sequence the work

Do **not** build a combat engine before Transfer. **Do** land range
overlay early (it is not Civ DLC). Suggested PRs:

1. **Play interpreter** that runs `cmd_N.sh` / registry templates in
   order, honors Wait/Text, on the **focused map** (desk or pchq).
2. **System.pdl + Items.pdl + Actors.pdl** writers from db-hq save
   (even if the UI stays field tiles — a “flush to play files” script
   is enough).
3. **Passability** on glyphs + **Transfer consumer** (desk and pchq).
4. **Range overlay compositor** (reads `range_overlay.pdl`; can start
   with stamped CELLs) — **priority**. Same layer as tile-picker
   place; do not keep amber full-screen as the grid.
5. **BFS helper + `show_range` / `hide_range` / `select_in_range`**.
6. **Shop v0** (choices + gold + items).
7. **Battle v0** (choices + one enemy hp file) — overlay attack cells
   can target this later.
8. **Save slot** as directory copy.

Guides (`event-guides/`) already list `need=transfer_player` etc.
When a consumer lands, **remove that id from TILE.need** and add it to
TILE.cmds — that is the honest status board.

---

## 8. Pointers

| Need | File |
|---|---|
| Palettes / samples / clone table | `08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md` |
| Board window history | `design-docs/piececraft-hq.md` |
| Interact / focus | `09-appendix/pc-hq-leg-vs-nu-fix.md`, `PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md` |
| Registry / IR | `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`, `event_commands.registry.pdl` |
| db tabs | `#.ref/menu/db-tabs-remaining.txt` |
| Guides schema | `#.ref/menu/event-guides/SCHEMA.pdl` |
| Desktop 3D camera (later) | `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` |

---

## 9. Clone skins — CDDA, Minecraft, Civilization, Pokemon

**One event system. Four data packs.** Do not fork events-hq, do not
fork `mr_world`, do not add `g_is_cdda`. Each clone is: a **palette
category**, **event-guide sheets**, **sample map**, **db rows**
(items/enemies/actors), and **which existing cmds you fill in**.

Shared chunks (reuse everywhere):

| Chunk | Why every clone uses it |
|---|---|
| events-hq IR / pages / triggers | doors, signs, NPCs, wild encounters, end-turn buttons |
| registry cmds | transfer, shop, battle, gold, switch, variable, text, choices, move_route, fade, CE call |
| palettes + guides | TILE.tex + trigger + cmds; seed `event_pkg` on stamp |
| desk `DESK` + session save | overmap / world / town persistence |
| pchq chunks + z + POV 1–4 | local map (CDDA reality bubble, MC chunk, Civ city view, Poke route) |
| gold / items / party files | currency, inventory, squad |
| Common Events | clocks, hunger, wild-grass, gravity, end-of-turn |
| board-viewer passability + hero | walking; z for MC/CDDA stairs |

### 9.1 CDDA-like (Cataclysm)

**Feel:** top-down survival, time, loot, doors, stairs, traps, simple
combat, overmap travel.

**Already:** `cdda` picker (UltiCa), `event-guides/cdda/{terrain,furniture,traps,items}.pdl`,
`cdda_sample`, door example pkg.

| CDDA thing | House reuse | Implement notes (RM-savvy) |
|---|---|---|
| Reality bubble map | pchq chunk + z | One RM map = one bubble. Do not simulate the whole overmap in the interpreter. |
| Overmap travel | **Transfer Player** between maps/desks | Stairs already `need=transfer_player`. Portal trap same. Desk-per-overmap-tile is valid v0. |
| Closed/open door | Control Switch + **page graphic change** | RM door = two pages (page1 closed graphic + Action → switch A; page2 open + through). Example `examples/cdda_door`. Do **not** write door C. |
| Stairs | Transfer + z | `xy` includes z. Fade none for in-building stairs. |
| Traps | Player Touch + `change_hp` / Battle | Beartrap: touch → hp; landmine: Battle or hp+switch. `tr_portal`: Transfer. |
| Loot / examine | Action + `change_items` / Shop sell-only | Crate = shop with goods=loot table **or** CE that rolls a variable and gives item. |
| Craft / anvil | `shop_processing` as craft | Goods = recipes; “price” = ingredient check (v1: gold as stand-in; v2: consume items). Guide already `need=shop_processing`. |
| Sleep / time | CE Parallel + `control_timer` | Timer already kv. CE: every N ticks hunger variable++. Bed: Action → wait + timer jump. |
| Zeds | Troop on **Player Touch** region **or** Parallel CE encounter | RM random encounter = System encounter steps. CDDA: CE checks “zombie density” variable, then `battle_processing`. |
| Melee | Battle v0 | One actor, one enemy, Fight/Escape. Skills later = bite/bash as Skills db. |
| Light / night | `tint_screen` | CE at timer thresholds. |
| Save | §6.6 | CDDA save is just RM save + extra variables (hunger, hour). |

**Do not build:** full CDDA JSON mods, vehicle physics, 3D overmap.
**Do build:** door pages, stair Transfer, loot Action, trap Touch,
battle v0, timer CE.

### 9.2 Minecraft-like (Mineclonia tiles)

**Feel:** place/break, inventory, chests, doors, furnace, gravity,
day/night, craft.

**Already:** `piececraft` picker, `event-guides/mineclonia/{mcl_core,interact}.pdl`,
`mineclonia_sample`, chest example pkg.

| MC thing | House reuse | Implement notes |
|---|---|---|
| Block place/break | palettes stamp **in play** + `change_items` | RM has no “break tile” opcode. House: play-mode stamp/erase **is** the MC verb. Optionally event on break (coal → give item). |
| Chunks / height | pchq z layers | Each z file is a slice. Transfer `x,y,z` for ladders (`mcl_core` ladder). Water `need=transfer` was wrong — water is `change_hp` + passability, not map change. |
| Chest | Action + Shop or `select_item`/`change_items` | Guide: `need=shop_processing`. Chest inventory = shop with price 0 (deposit/withdraw). Better v1: CE + item counts in variables named `chest_<map>_<id>_<item>`. |
| Door | same as CDDA two-page switch | `interact.pdl` door_wood. Room change = Transfer if the door is a portal; else just through-flag. |
| Furnace / craft table | Shop as recipe list **or** CE | Crafting table guide already `call_common_event`. Put recipes in CE, not C. |
| Bed | Transfer (set spawn in System) + timer | RM “inn” is Show Text + recover HP + gold. MC bed = set System start_xy + skip timer. |
| Gravity / falling | Parallel CE **or** move_route Down while cell below empty | Do not put gravity in the renderer. CE: if passability below hero, move_route down. |
| TNT | Action / Touch → `change_hp` AoE + erase cells | Switch + wait + flash + shake (cmds exist) then script erases glyphs. |
| Mobs | Battle or map events with move_route | Creepers = event with Parallel move_route toward player + Touch battle. |
| Day/night | `tint_screen` + timer CE | Same as CDDA. |
| Inventory hotbar | Items db + gold unused | MC has no gold; use items only. Shop still works as chest/craft. |

**Reuse with CDDA:** door pages, Transfer, shop-as-container, timer CE,
tint, battle for mobs. **Unique:** play-mode stamp/break, z gravity CE.

### 9.3 Civilization-like

**Feel:** grid terrain, cities, gold per turn, simple combat, end turn.
Not a full 4X sim.

| Civ thing | House reuse | Implement notes |
|---|---|---|
| World map | desktop **or** pchq 2D POV | One cell = one tile. Palettes terrain glyphs (piececraft / emoji / civ-txt). **Desktop as map** is the right Civ board — pals = units/cities. |
| Cities | pal with Autorun/Parallel CE | City pal: graphic = city; CE each **End Turn** adds gold (Change Gold). |
| Units | pals with move_route + Action | RM events that the **player** “possesses” is awkward. v0: one unit = the hero; other units = events you Action to “select” (switch) then Transfer-in-place as that pal. v1 later: party = army list in Actors. |
| End turn | Common Event called from a **desk pal** “Next Turn” or Show Choices | CE: gold income, city growth variable, AI move_route, encounter check. |
| Combat | `battle_processing` | Troop = the other unit’s enemy row. Win → erase loser event (self-switch + graphic none). |
| **Move / attack range overlay** | **§6.7 builtin compositor + BFS; rules in db** | **Priority.** Unit Action → `show_range` (move blue, attack red) → `select_in_range` → `move_route` or Battle. Not a Civ-only plugin; not renderer C. |
| Diplomacy / tech | switches + variables + Show Choices | “Open borders” = switch. Tech tree = variables; CE gates units. **Do not** build a tech UI; a list of choices is RM-correct. |
| Fog | skip v1 **or** tint cells | Not a new engine. |
| Save | desk snapshot | Civ save **is** the desktop save. |

**Reuse:** gold, CE, pals-as-events, battle, switches. **Unique:** End
Turn CE as the clock (instead of CDDA timer). **Do not** start until
Loop A stamp works on the **desktop** (Civ is a desk game more than a
pchq voxel game).

### 9.4 Pokemon-like

**Feel:** overworld + grass encounters + party of 6 + turn battle + mart
+ PC box + gym doors.

| Poke thing | House reuse | Implement notes |
|---|---|---|
| Routes / towns | maps + **Transfer** on doors/ledges | Classic RM. Ledges = Transfer same-map + 1 tile hop (move_route). Doors = Transfer other map. |
| Player sprite | Actors character graphic / rmmv characters picker | Followers = party pals optional (RM followers). |
| Wild grass | Parallel CE **or** Player Touch on grass tiles | RM encounter steps live in System. House: CE on step count variable → `battle_processing` with random troop from a table (variable). **This is the RM random encounter system.** Do not write a Pokemon engine. |
| Trainers | event page: Player Touch in line of sight | RM trainer = Autorun after `set_move_route` toward player + Show Text + Battle + self-switch A (won’t retrigger). LOS v0: skip; use Touch. |
| Party of 6 | Actors db + party list in System | Battle uses first living Actor; Items “Pokeball” later. |
| Types / STAB | skip v1 **or** Skills element field | v1 Fight = tackle formula. Types are db Types tab (skip until battle v0 works). |
| Mart | **shop_processing** | Goods = pokeballs/potions from Items. |
| PC / storage | Shop-as-container **or** CE moving items | Same as MC chest. |
| Gym / badge | switches + Transfer | Badge = switch; gym door page condition = switch. |
| Evolution | CE after battle if exp variable ≥ n | Change Actor graphic (db) — can wait. |
| Legendaries | map event, `can_escape=0` | Standard RM boss. |

**Reuse with RM demo:** Transfer, Shop, Battle, Text, Choices, self-switch,
CE, System start map. Pokemon **is** the RM template with grass CE.
**Do not** invent a second battle cmd for “pokemon battle.”

### 9.5 What not to share / not to build

- **No per-clone renderer flags.** Skins are data.
- **No GTA** until a tileset PDL exists (cars/peds pack).
- **Do not** hide chrome to “feel more like the game.”
- **Do not** implement DF, Kenney, or new palettes before Loop C
  consumers — guides already outrun play.

### 9.6 Suggested first demo per skin (honest, small)

| Skin | 5-minute demo once consumers exist |
|---|---|
| RM | Town desk + door Transfer + mart Shop + one Battle troop |
| CDDA | `cdda_sample` door page + stairs Transfer + trap hp + loot Action |
| MC | stamp dirt, chest shop, door page, ladder z Transfer |
| Civ | desktop grid, city pal CE gold/turn, one unit Battle |
| Pokemon | route map, grass CE → Battle, door Transfer, mart Shop |

---

## 10. Non-goals

- Hiding livedesk / board chrome in play.
- New renderer `g_is_pchq` / bringing back `run_pchq_board_mode`.
- Hardcoded asset paths in C.
- Playable GTA before a tileset PDL.
- Using `event.commands.remaining.txt` as truth.
- Forking the interpreter per clone.
- Range overlay as `g_is_civ` / khtpm renderer branch, or as a dlopen
  plugin. Split is §6.7: builtin tint+BFS, plugin = registry+db.
