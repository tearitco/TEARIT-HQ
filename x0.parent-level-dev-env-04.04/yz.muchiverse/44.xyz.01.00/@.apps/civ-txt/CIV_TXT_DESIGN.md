# 🏛️ CIV-TXT — TEXT-BASED 4X CIVILIZATION GAME

> **PURPOSE:** A full-scope 4X (eXplore/eXpand/eXploit/eXterminate) civilization
> game, built on the same ledger + CHTPM architecture proven in
> `@.apps/my-chara-txt/`, but with a grid-based world simulated on the
> backend that the player is **never required to look at**. Every action
> (found a city, research a tech, invade a rival) is a text command; the
> engine resolves it against the real grid internally and reports the
> outcome in plain text. The map is an **optional separate widget**, not
> a requirement — this game must be fully playable on something as small
> as a text status line (a "beeper"/LCD-class display), with a human just
> steering via commands and periodically reading reports.

**Read `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` first** if you haven't —
this doc assumes you already know the CHTPM nav pattern, `piece.pdl`
METHOD-table dispatch, per-screen PAL modules, and the ledger-driven
state model. Nothing about that base architecture is reinvented here.

---

## 📖 1. THE VISION

Found a civilization, grow cities, research technology, build an army,
and win — by conquest, by score, or by tech — against 1 or more rival
civs (human or AI-controlled). Full 4X scope is the stated goal for v1,
not a stripped-down starter: city founding/growth, a real tech tree,
military units with grid-based movement and combat, and diplomacy
between civs are all in scope from the start. What's **configurable**
(not hardcoded) is scale and depth — see §4, the Setup screen.

**Core design commitment, stated directly by the user and non-negotiable:**
the grid is real (units have real x/y positions, terrain has real
walkability/movement-cost, combat is resolved against real adjacency) —
but the player interacts via **commands and reports**, never by being
required to see or navigate the grid. `INVADE_CITY:rival_city_03` is a
real command; the engine moves whichever of your units it needs to move,
resolves combat tile-by-tile if that setup option is chosen, and returns
"Your army (12 units) took Rivalburg. Losses: 3. Rival forces: annihilated."
— all without the player ever opening a map screen. The map, when opened,
is a separate optional widget (§5) showing the same underlying truth.

---

## 🏗️ 2. REFERENCE SOURCES & PATTERNS

| Source | Pattern we reuse |
|--------|------------------|
| **@.apps/my-chara-txt/** | CHTPM nav screens, `piece.pdl` METHOD dispatch, per-screen PAL module loop, ledger-driven state, the exact P1-P8 build-order shape |
| **101.mutaclsym🧟‍♂️️+18.01** | Grid/world architecture: registry pieces (global static definitions, e.g. `terrain_types.txt`) vs. world/map instance pieces (live mutable state as real nested directories) — see its `dox/01-cdda-architecture.md` §2. Also its optional GL/RGB mirror pattern (`ops/compose_rgb_frame.c` + `system/gl_mirror.c`) for an eventual visual map widget, and its xlector/interact-mode free-roam cursor for "look at any tile" if a map widget is ever built |
| **014.wsr-pal💸️📌️+2** (`ops/corp_decide.c`) | The **decision_mode chassis** (preset/weighted/rl/llm/human) — the real, proven automation-tier mechanism this game's AI civs and the automation-supervision layer are both built on. See §7. |
| **%.harnesses/xo-human.md** | The full write-up of decision_mode as an "exo harness" a human can step into/out of mid-game. Read this before building any AI-civ decision logic. |
| **@.apps/TSC_ELO/** | Master ledger (append-only event log) convention, reused unchanged |

**Key architectural principle carried from my-chara-txt, pushed further here per direct instruction:** widget-based, not monolithic. The map view, the city-management readout, the tech-tree readout, and the diplomacy readout are each their **own separate CHTPM screen/widget**, independently reachable and independently useful — you should be able to check city status without ever touching the map widget. This is not a new pattern (CHTPM screens are already this shape) but is called out explicitly here because civ-txt's scope makes the temptation to build one giant "world screen" much stronger than it was for my-chara-txt.

---

## 🌍 3. THE WORLD (DATA MODEL)

**Read `@.apps/PORTABLE_ENTITY_ARCHITECTURE.md` before building any of P2's real cities/units.** This section's own `civs/<civ_id>/cities/<city_id>/`/`units/<unit_id>/` nesting already has the right instinct (per-entity directories), but that doc adds the real target shape for P2+: a `piece.pdl` per city/unit (so board-viewer's own future selector/possession hookup can show real per-entity actions), a small universal cross-game schema, and inventory/vehicle-as-nested-directory conventions - all real, decided house direction (`❤️‍🔥️!.world_architecture+1=rusindol.txt`), not yet applied here. Not a retroactive change to current P1 code - the target shape for when P2 actually starts.

### Two-tier registry/instance split (ported directly from mutaclysm §2)

**Registry pieces** (global, static, under `pieces/registry/`):
- `terrain_types.txt` — `glyph|id|name|walkable|move_cost` (plains, hills, forest, mountain, water, etc.)
- `unit_types.txt` — `id|name|attack|defense|move_range|upkeep|req_tech`
- `building_types.txt` — `id|name|effect|cost|req_tech`
- `tech_tree.txt` — `id|name|cost|prereq_ids|unlocks` (a real dependency graph, pipe-delimited: `prereq_ids` is a comma-joined list of other tech ids)
- `civ_presets.txt` — starting-bonus flavor per civ (optional; a "Civilization Trait" like +growth or +military, not required for v1)

**World/instance pieces** (live, mutable, physically nested directories, per mutaclysm's own convention):
- `pieces/world_01/tile_<x>_<y>/` — only materialized for tiles that matter (owned/visible/occupied), not all W×H tiles pre-created — a sparse world, same spirit as mutaclysm's per-map directories but sparser since civ-txt's grid can be large
- `pieces/world_01/civs/<civ_id>/state.txt` — the civ's own top-level state: `treasury`, `current_tech`, `researched_techs` (comma list), `decision_mode`, `risk_level`, `compute_tier` (see §7)
- `pieces/world_01/civs/<civ_id>/cities/<city_id>/state.txt` — `pos_x`, `pos_y`, `population`, `production_queue`, `owner_civ_id`
- `pieces/world_01/civs/<civ_id>/units/<unit_id>/state.txt` — `unit_type`, `pos_x`, `pos_y`, `hp`, `moves_remaining_this_turn`

### `config.txt` — game-level setup, written once at New Game and never changed mid-game

```
game_id=civ-txt-001
turn=1
turn_order_index=0          # whose sequential turn it is right now (§6)
victory_condition=conquest|score_turnlimit|tech_score   # SELECTABLE AT SETUP, see §4
turn_limit=200              # only consulted if victory_condition includes score
map_width=20
map_height=20
num_civs=2                  # you + N-1 AI, SELECTABLE AT SETUP
combat_resolution=abstract|per_unit   # SELECTABLE AT SETUP, see §4
game_state=playing
```

### `master_ledger.txt` — append-only, same convention as my-chara-txt

```
timestamp|turn|civ_id|action_type|details
2026-08-02T10:00:00|1|player|found_city|city_01:pos_5_5
2026-08-02T10:00:05|1|player|research|tech:bronze_working
2026-08-02T10:05:00|2|rival_civ_1|move_unit|unit_04:pos_6_6
2026-08-02T10:10:00|3|player|invade_city|target:rival_city_03:result:captured:losses:3
2026-08-02T10:10:01|3|player|diplomacy|target:rival_civ_1:action:declare_war
```

---

## 🎮 4. THE SETUP / NEW GAME SCREEN — WHERE SCOPE BECOMES CONFIGURATION

Per direct instruction, none of the following are hardcoded for v1 — they are **player-selectable at game creation**, written into `config.txt`, and every downstream op (`victory_check.c`, `resolve_combat.c`, the AI civ count/loop) branches on these fields rather than assuming one fixed ruleset:

| Setup option | Choices | What it changes downstream |
|---|---|---|
| **Victory condition** | Conquest / Score+TurnLimit / Tech+Score | `victory_check.c` (called every turn-cycle) checks different win predicates |
| **Map size & civ count** | e.g. Small (2 civs, 20×20) / Medium (3-4 civs, 40×40) | World generation (`generate_world.c`, modeled on mutaclysm's `generate_map.c` seeded-PRNG approach) and the sequential-turn loop's civ count |
| **Combat resolution depth** | Abstract (army-strength weighted roll) / Per-unit (real grid skirmish, mutaclysm-melee-style) | `resolve_combat.c` branches its entire implementation on this flag — see §6 |

This is a `new_game.chtpm` screen (its own widget, reached before `main.chtpm` exists) with a handful of `piece.pdl` METHOD rows (`SET_VICTORY:conquest`, `SET_MAP_SIZE:small`, `SET_COMBAT:abstract`, `CONFIRM_START`), same METHOD-table dispatch pattern as everything else in this house — no new UI mechanism required.

---

## 🖥️ 5. THE INTERFACE — SEPARATE WIDGETS, NOT ONE WORLD SCREEN

Following the widget-separation principle from §2, civ-txt's screens are grouped by concern, each independently useful:

| Screen (widget) | Shows | Requires the map? |
|---|---|---|
| `new_game.chtpm` | Setup options (§4) | No |
| `main.chtpm` | Turn number, whose turn, civ summary (treasury/tech/city count), top-level action menu | No |
| `cities.chtpm` | List of your cities, each a `${piece_methods}` row (production queue, population) — dynamic piece.pdl like my-chara-txt's `farm.chtpm` | No |
| `tech.chtpm` | Tech tree state: researched / in-progress / available-next, pick next research | No |
| `military.chtpm` | Your units list, `INVADE_CITY:<target>` / `MOVE_ARMY:<target_city_or_region>` commands — **region/city-name targeting, not tile-by-tile clicking** | No |
| `diplomacy.chtpm` | Other known civs, their disposition (peace/war/unknown), declare-war/propose-peace actions | No |
| `automation.chtpm` | The supervision-tier + risk/compute dials (§7) | No |
| `map.chtpm` (**optional, separate widget**) | The actual ASCII grid (mutaclysm-style tile rendering). **Never required to reach or use any of the above** — but not view-only either: per direct instruction, this widget can also **view AND control** the game (move units, found cities, invade — the same underlying `civ_menu_input.c` commands as the text widgets, just issued via a map cursor instead of a named-target list). Not this session's build focus, but the architecture must not preclude it. | Yes — this IS the map |

`INVADE_CITY:<target_city_id>` (from `military.chtpm`) is the concrete example of the "grid-backed but text-fronted" principle: the player picks a target by **name/id**, not by coordinate or map click. Internally, `resolve_combat.c` looks up both armies' real grid positions, does real pathing/adjacency math (reusing mutaclysm's `terrain_walkable()`-style checks), and resolves the fight — but none of that internal grid math is ever surfaced as a required interaction.

**Architectural hook for map-as-controller (not built now, must not be precluded):** because every game-changing command (`FOUND_CITY`, `MOVE_ARMY`, `INVADE_CITY`, `SET_RESEARCH`, ...) is dispatched through the same single `civ_menu_input.c` METHOD-table entry point regardless of which widget it came from (per `xyzos-standards`' shared-dispatcher convention, same as my-chara-txt's own `mychara_menu_input.c`), `map.chtpm` gaining its own xlector-style free-roam cursor (mutaclysm's proven pattern) later is purely a NEW INPUT PATH into the exact same dispatcher — never a fork of game logic. This is why the "text-first, map optional" design and the "map can fully control the game" requirement are not in tension: they're two input surfaces over one command layer.

---

## 🎯 6. TURN STRUCTURE — SEQUENTIAL PER-CIV (per direct instruction)

Unlike my-chara-txt's simultaneous "queue actions → End Turn → everything resolves together," civ-txt uses **classic sequential per-civ turns**:

```
1. Player's civ takes its full turn (any number of actions: found city,
   set research, move units, invade, diplomacy) → player ends turn
   explicitly (an "End Turn" METHOD row, same as my-chara-txt)
2. turn_order_index advances. AI civ 1's FULL turn resolves via its
   own decision_mode (§7) — this can be instant (preset/weighted) or
   take a real LLM round-trip if compute_tier=llm (rare, opt-in, see
   xo-human.md's speed doctrine — never the default per-civ tier)
3. ...repeat for every AI civ...
4. Once every civ has acted, `turn` increments, per-turn ticks apply
   (city growth, unit healing, tech progress), victory_check.c runs
5. Back to step 1
```

`ops/civ_turn_dispatch.c` (the sequential-turn equivalent of `mychara_menu_input.c`) reads `turn_order_index`, determines whose turn it is, and either waits for real player input (player's own turn) or calls `ops/civ_ai_decide.c` (§7) and auto-resolves (AI civs' turns) before advancing the index.

**Combat resolution, per the setup-selectable flag (§4):**
- `abstract`: `resolve_combat.c` sums each side's unit `attack`/`defense` stats into one strength number, does one weighted RNG roll (a direct structural cousin of `mychara_menu_input.c`'s own MINE roll), applies proportional losses to both sides.
- `per_unit`: `resolve_combat.c` runs a real multi-round grid skirmish — each unit is a real piece with its own hp, adjacency determines who can hit whom, ported directly from mutaclysm's own bidirectional-melee model (`move_player.c`'s attack-instead-of-move check, `tick_monsters.c`'s per-instance stepping) — genuinely reused logic, not reinvented.

---

## 🤖 7. AUTOMATION — THE SUPERVISION LAYER + decision_mode CHASSIS

This section applies **identically to my-chara-txt and tactics-txt** — it's a house-wide pattern, not civ-txt-specific, and my-chara-txt is being retrofitted with it (see the addendum in `MY_CHARA_TXT_DESIGN.md`).

**Two independent dials, per direct instruction ("maybe both, for development/testing/experimenting, as separate risk & compute"):**

| Dial | Range | What it controls |
|---|---|---|
| **Risk** | 1 (conservative) – 10 (aggressive) | Tunes the `weighted` decision tier's own scoring formula — how much health/treasury/units it's willing to spend chasing a bigger payoff. A pure numeric bias, consulted by `civ_ai_decide.c`'s scoring function the same way `corp_decide.c`'s `fundamental_value()` is tuned by `risk_bias` in wsr-pal. |
| **Compute tier** | Maps directly onto the real, proven `decision_mode` chassis from `014.wsr-pal💸️📌️+2/ops/corp_decide.c`: `0`=preset, `1`=weighted, `2`=rl (stub, falls back to weighted), `3`=llm | Which actual decision mechanism runs. Per `xo-human.md`'s hard speed rule: `llm` (3) must **never** be the default/routine tier across many AI civs — reserve it for rare, genuinely judgment-shaped calls (a risky invasion, not routine tech-picking), never the per-turn workhorse for more than one or two entities at a time. |

**On top of these two, a third, separate SUPERVISION dial — the "manual/semi/full" the user asked for, confirmed exactly as described:**

| Supervision level | Behavior |
|---|---|
| **Manual** | `decision_mode=4` (human) on your own civ — you act through the real UI every turn, exactly like today's my-chara-txt. No automation at all. |
| **Semi** | Automation (whatever compute_tier/risk is set) decides and executes ONE full turn, then **pauses** — parks and waits for you to explicitly continue before it takes the next turn. Lets you review/override between every single turn without babysitting each individual decision. |
| **Full** | Automation runs turns back-to-back, fully unattended (until a turn-count cap, a victory condition, or an explicit stop command) — you can check in on read-only state (any of the text widgets from §5) at any time, without your input being required for the game to keep advancing. |

Mechanically, Semi and Full are **not** new decision_mode values — they're a supervision wrapper read by `civ_turn_dispatch.c` from a separate `supervision_mode` field (`manual`/`semi`/`full`) in the civ's own `state.txt`, orthogonal to `decision_mode`. Manual forces `decision_mode=4` regardless of what's configured; Semi and Full both use whatever `decision_mode`/`risk_level` are set, differing only in whether the turn loop halts for confirmation after each turn (`semi`) or free-runs (`full`).

**Concrete mechanism, ported directly from `corp_decide.c`'s mode 4 "park and wait":** `civ_turn_dispatch.c` checks `supervision_mode` before calling `civ_ai_decide.c`. If `manual`, it behaves exactly like `decision_mode=4` always does — parks, waits for a real queued player action. If `semi`, after resolving one full automated turn it writes a `paused_for_confirmation=1` flag and halts the turn loop until a `CONTINUE` command clears it. If `full`, it never sets that flag and just keeps advancing.

**Check-in without the map — direct instruction, clarified:** "no GUI" was a misstatement corrected to **"no map."** The point isn't a headless/no-UI game — it's that the map/board should be its own separate widget (§5's `map.chtpm`), decoupled from the text-based management/readout widgets, so a player running Full automation can check `main.chtpm`/`cities.chtpm`/`military.chtpm` for a plain-text status readout without ever needing the map widget open at all. This is the same widget-separation principle as §2, just stated as a hard requirement here because it's the whole point of the automation tier existing.

---

## ⚙️ 8. OPS (new, on top of my-chara-txt's own reusable shape)

| Op | Purpose |
|---|---|
| `civ_turn_dispatch.c` | Sequential per-civ turn dispatcher — the civ-txt analog of `mychara_menu_input.c`, but drives WHOSE turn it is, not just what a single player's keypress means |
| `civ_menu_input.c` | Within a civ's own turn, the same METHOD-table dispatcher shape as `mychara_menu_input.c` — handles `FOUND_CITY`, `SET_RESEARCH:<tech_id>`, `MOVE_ARMY:<target>`, `INVADE_CITY:<target>`, `DECLARE_WAR:<civ_id>`, etc. |
| `civ_compose_frame.c` | Renders whichever civ-txt screen/widget is current — same per-screen-branch shape as `mychara_compose_frame.c` |
| `civ_ai_decide.c` | The decision_mode-branching automation brain (§7) — modeled directly on `corp_decide.c`, swap buy/sell/hold for found-city/research/invade/diplomacy decisions |
| `generate_world.c` | One-shot seeded-PRNG world generation at New Game — modeled directly on mutaclysm's `ops/generate_map.c` |
| `resolve_combat.c` | Branches on `combat_resolution` (§6) — abstract army-strength roll, or per-unit grid skirmish reusing mutaclysm's melee model |
| `victory_check.c` | Branches on `victory_condition` (§4) — checked once per full turn-cycle (after every civ has acted) |
| `tech_progress.c` | Per-civ-turn tech accumulation, checks `tech_tree.txt` prereqs, flips a tech from in-progress to researched when its cost is met |

---

## 📁 9. DIRECTORY LAYOUT

```
civ-txt/
├── CIV_TXT_DESIGN.md
├── button.sh
├── scripts/build.sh
├── default_op.txt
├── system/                      ← copied from my-chara-txt's own system/ (prisc+x, chtpm_parser_pal, etc.)
├── ops/
│   ├── civ_turn_dispatch.c  civ_menu_input.c  civ_compose_frame.c
│   ├── civ_ai_decide.c
│   ├── generate_world.c  resolve_combat.c  victory_check.c  tech_progress.c
│   └── +x/
├── pal/
│   ├── main_module.pal  cities_module.pal  tech_module.pal
│   ├── military_module.pal  diplomacy_module.pal  automation_module.pal
│   └── map_module.pal          ← the one OPTIONAL widget's own module
├── pieces/
│   ├── chtpm/layouts/
│   │   ├── new_game.chtpm  main.chtpm  cities.chtpm  tech.chtpm
│   │   ├── military.chtpm  diplomacy.chtpm  automation.chtpm
│   │   └── map.chtpm       ← optional widget, never required
│   ├── registry/
│   │   ├── terrain_types.txt  unit_types.txt  building_types.txt
│   │   └── tech_tree.txt
│   ├── system/config.txt
│   ├── world_01/
│   │   ├── civs/<civ_id>/state.txt
│   │   ├── civs/<civ_id>/cities/<city_id>/state.txt
│   │   ├── civs/<civ_id>/units/<unit_id>/state.txt
│   │   └── tile_<x>_<y>/       ← sparse, only materialized tiles
│   ├── display/  keyboard/  apps/player_app/
└── data/
    └── master_ledger.txt
```

---

## 🚀 10. BUILD ORDER

| Phase | What we build | Verified by |
|---|---|---|
| **P1** | Skeleton: `new_game.chtpm` (setup options write config.txt), `main.chtpm`, world generation (single civ, tiny map, no AI yet) | Real `button.sh run`: setup screen works, main screen shows turn 1 |
| **P2** | Found city, basic production queue, `cities.chtpm` | Found a city, see it in the cities widget, ledger entry present |
| **P3** | Tech tree: `tech.chtpm`, `tech_progress.c`, a real small tech graph (5-10 techs) | Research completes, unlocks reflected in unit/building availability |
| **P4** | Units + movement + `military.chtpm` (text-targeted, no map needed) | Move a unit toward a named target, real grid pathing under the hood |
| **P5** | Second civ (AI, `decision_mode=preset` only for now), sequential turn dispatch | Two civs alternate turns correctly, AI civ takes trivial preset actions |
| **P6** | Combat: both `abstract` and `per_unit` resolution paths, `INVADE_CITY` | A city changes hands, ledger records losses correctly under both settings |
| **P7** | Diplomacy: war/peace state, `diplomacy.chtpm` | Declare war, AI civ reacts (even if reaction is a preset stub initially) |
| **P8** | `victory_check.c` for all three conditions, `automation.chtpm` (supervision + risk + compute dials) | Game correctly ends under each victory condition; Semi-mode pause/continue verified live |
| **P9** | `civ_ai_decide.c` upgraded to real `weighted` scoring (not just preset stubs) for AI civs | AI civ makes non-trivial found-city/research/invade choices under `weighted` |
| **P10** | Optional `map.chtpm` widget (ASCII grid, mutaclysm-style) | Map widget renders real world state; confirmed NOT required for any prior phase's functionality |
| **P11** | Full replay/audit tooling (ledger dump vs. state cross-check), matching my-chara-txt's own P7/P8 | Replay reproduces final state exactly |

---

## 🔗 11. CROSS-REFERENCES & ECOSYSTEM

- **@.apps/my-chara-txt/** — the architectural parent. Ledger, CHTPM nav, `piece.pdl` dispatch, and now the automation/decision_mode layer are all directly reused, not reinvented.
- **@.apps/tactics-txt/** — sibling design (`TACTICS_TXT_DESIGN.md`), shares the exact same automation/decision_mode/supervision layer (§7 here is verbatim-shared doctrine, not duplicated logic).
- **101.mutaclsym🧟‍♂️️+18.01** — source of the grid/registry-vs-instance pattern, the terrain registry shape, the optional GL/RGB mirror precedent for the eventual map widget, and the per-unit combat model for `combat_resolution=per_unit`.
- **014.wsr-pal💸️📌️+2** — source of the `decision_mode` chassis itself (`corp_decide.c`), reused essentially unmodified in concept.
- **@.apps/genesis-txt/** — the true multiplayer-human-players version of this whole family; civ-txt's AI civs are single-player analogs of what genesis-txt will eventually let real human players occupy instead.

---

## 🤔 12. OPEN QUESTIONS / NOT YET DECIDED

1. **Diplomacy depth for v1**: binary war/peace is assumed above — should trade agreements, alliances, or tribute be in v1 or deferred?
2. **AI civ personality/flavor**: should different AI civs have different `risk_level`/`compute_tier` presets out of the box (an "aggressive AI" vs. "builder AI" feel), or should all AI civs start identically configured and only diverge via player-set automation dials?
3. **Tech tree size for v1**: a real but small tree (5-10 techs, per P3) is assumed — confirm this is enough for a first playable pass, or should v1's tree be bigger?
4. **Sparse-tile materialization threshold**: exactly when does a tile get its own `pieces/world_01/tile_<x>_<y>/` directory — only when owned/occupied, or also when merely "explored/seen"? Matters once fog-of-war is considered (not scoped above, a real future feature).
5. **Cross-game portability**: `platform-vision.txt` (mutaclysm's sibling doc) names a far-future cross-game item/pet trading vision — is civ-txt ever meant to participate in that, or does it stay fully self-contained?

---

## 🏁 13. TL;DR

- Full 4X scope from the start: cities, tech, military, diplomacy — but every dimension is **setup-configurable**, not hardcoded (victory condition, map/civ scale, combat resolution depth).
- Real grid simulation on the backend; **text commands and reports on the frontend** — the map is one optional widget among many, never a requirement.
- Sequential per-civ turns (not simultaneous like my-chara-txt).
- Same ledger + CHTPM architecture as my-chara-txt, same `decision_mode` chassis as wsr-pal's `corp_decide.c` for AI civs.
- New: a **supervision layer** (manual/semi/full) orthogonal to decision_mode, plus independent risk (1-10) and compute-tier (preset→llm) dials — built once here, shared verbatim by tactics-txt, and retrofitted into my-chara-txt.
- Widget-separation is a hard requirement here, not just a preference: you must be able to run this entire game via text-only screens with the map widget never opened.
