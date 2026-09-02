# ⚔️ TACTICS-TXT — 10×10 GRID TACTICS BATTLER (working name)

> **PURPOSE:** A turn-based tactics battle on a 10×10 grid — think a
> lightweight, comedic-flavored tactics-RPG skirmish (farmers, chefs,
> clowns, lawyers, and warriors all fielding real units side-by-side),
> not a generic fantasy wargame. Built on the same ledger + CHTPM
> architecture as `@.apps/my-chara-txt/`, with its board rendered
> ASCII-first (mutaclysm-style tile registry) and an optional 2D/3D
> GL mirror widget layered on top later, exactly like mutaclysm's own
> optional `gl_mirror`.

**Read `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` first** for the base
CHTPM/`piece.pdl`/ledger architecture, and `@.apps/civ-txt/CIV_TXT_DESIGN.md`
§7 for the automation/decision_mode/supervision layer this game shares
verbatim. Neither is restated in full here.

---

## 📖 1. THE VISION

Two armies face off on a 10×10 grid. Each turn, the active side has a
**shared pool of 5 actions** to spend however it likes across its whole
army (move unit A twice + attack with unit B = 3 of 5 used, 2 left for
someone else) — a real tradeoff between concentrating force and
spreading actions thin. Terrain matters (cover, movement cost,
elevation). Units are drawn from a roster of comedic/varied
**professions** — farmer, chef, clown, lawyer, warrior, and more —
each with genuinely different skills, not reskinned generic classes.

**Two play modes, chosen at a pre-battle setup screen, per direct instruction:**
- **Classic** — fixed "staff" preset armies. Quick, no meta-progression, pure positioning/tactics. This is v1's buildable core.
- **Collection** — players field units from their own persistent, leveled, skill/item-customized collection (RPG-Maker-esque: pieces have levels, skills, equipped items, allocated within a points cap), or from the same staff defaults freely reallocated. This is real, designed scope for v1 — **not deferred** — but built as a genuinely separate module from the core engine (§2's modularity principle), so it never entangles with core combat/movement logic.

---

## 🏗️ 2. REFERENCE SOURCES & PATTERNS

| Source | Pattern we reuse |
|---|---|
| **@.apps/my-chara-txt/** | CHTPM nav, `piece.pdl` METHOD dispatch, per-screen PAL module loop, ledger-driven state |
| **101.mutaclsym🧟‍♂️️+18.01** | The whole grid-game toolkit: `terrain_types.txt` registry (glyph/id/name/walkable/move_cost — reused directly for this board's terrain), `compose_frame.c`'s ASCII grid-drawing shape, the xlector/interact-mode free-roam cursor (adapted here as the **tile-targeting cursor** for choosing a move/attack destination — see §5), the optional GL/RGB mirror (`ops/compose_rgb_frame.c` + `system/gl_mirror.c`) as the eventual 2D/3D board widget, and the bidirectional-melee combat model (`move_player.c`'s attack-instead-of-move check, `tick_monsters.c`'s per-instance stepping) as the base for real unit-vs-unit combat here |
| **@.apps/civ-txt/CIV_TXT_DESIGN.md** §7 | The automation supervision layer (manual/semi/full) + independent risk (1-10) / compute-tier (preset→llm) dials — shared verbatim, not reinvented per-project |
| **014.wsr-pal💸️📌️$2** (`ops/corp_decide.c`) | The underlying `decision_mode` chassis every automation tier above ultimately branches on |

**Modularity is a hard requirement here, per direct instruction ("both, but keep modular as possible, no monoliths"):** the core tactics engine (grid, terrain, action-pool turn structure, combat resolution) is built as one self-contained module that knows nothing about WHERE an army roster came from. Classic mode and Collection mode are both just different **army-source modules** that hand the engine a resolved roster (a list of unit instances with stats) — see §6's directory layout for how this separation is enforced physically (different directories, not different code paths inside the same op).

---

## 🌍 3. THE WORLD (DATA MODEL)

**Read `@.apps/PORTABLE_ENTITY_ARCHITECTURE.md` before building P2's real `pieces/battle_01/units/<unit_id>/` movement work.** This section's `units/<unit_id>/` nesting already has the right instinct, but that doc adds the real target shape: a `piece.pdl` per unit (real per-unit actions, board-viewer's own future selector/possession hookup can show them), a small universal cross-game schema shared with civ-txt's own units/cities, and inventory-as-nested-directory instead of a flat `equipped_item_ids` list. Real, decided house direction (`❤️‍🔥️!.world_architecture+1=rusindol.txt`), not yet applied here - target shape for P2, not a change to current P1 code.

### Registry pieces (global, static)

- `pieces/registry/terrain_types.txt` — `glyph|id|name|walkable|move_cost|cover_bonus` (grass, mud, wall, water, high-ground, etc. — `cover_bonus` is new vs. mutaclysm's own terrain registry, a flat defense bonus for units standing on that tile)
- `pieces/registry/professions.txt` — `id|name|base_hp|base_attack|base_defense|move_range|skill_ids` (farmer, chef, clown, lawyer, warrior, ...; `skill_ids` is a comma-list into the skills registry)
- `pieces/registry/skills.txt` — `id|name|action_cost|range|effect_type|effect_value` (e.g. `clown_pie_throw|Pie Throw|1|3|damage|8`, `chef_heal_meal|Home-Cooked Meal|1|1|heal|15`, `lawyer_injunction|Injunction|2|4|stun|1turn`) — every skill costs some number of the shared 5-action pool, not always 1
- `pieces/registry/item_types.txt` — equippable items for Collection mode (stat modifiers, not required for Classic mode at all)

### World/instance pieces (live, per-battle)

- `pieces/battle_01/board/tile_<x>_<y>/state.txt` — only for non-default tiles (terrain override, occupied) — sparse, same principle as civ-txt's world
- `pieces/battle_01/units/<unit_id>/state.txt` — `profession_id`, `pos_x`, `pos_y`, `hp`, `owner_side` (1 or 2), `skills` (resolved list, may differ from profession default in Collection mode)

### Persistent Collection data (separate from any single battle, per the modularity principle)

- `pieces/collection/<player_id>/pieces/<piece_id>/state.txt` — `profession_id`, `level`, `xp`, `allocated_skill_ids`, `equipped_item_ids`, `stat_points_spent` (checked against a cap, per direct instruction "having a certain cap") — this directory tree is the actual persistent meta-game, untouched by any single battle's own `pieces/battle_01/` tree. A battle only ever reads a **snapshot copy** of the pieces it selected into `pieces/battle_01/units/`, never mutates the collection directly mid-fight (post-battle XP/level-up is a deliberate separate step, see §8 P8).

### `config.txt` — per-battle setup

```
battle_id=tactics-001
mode=classic|collection          # SELECTABLE AT SETUP
side_1_supervision=manual|semi|full
side_2_supervision=manual|semi|full
side_1_risk=5                    # 1-10, only consulted if not manual
side_1_compute_tier=1            # 0-3 (preset/weighted/rl/llm), only if not manual
side_2_risk=5
side_2_compute_tier=1
turn=1
active_side=1
actions_remaining_this_turn=5
game_state=playing
```

### `master_ledger.txt`

```
timestamp|turn|side|action_type|details
2026-08-02T14:00:00|1|1|move|unit_farmer_01:from_2_2:to_2_4:cost:2
2026-08-02T14:00:05|1|1|attack|unit_warrior_02:target:unit_clown_05:damage:12:cost:1
2026-08-02T14:00:10|1|1|skill|unit_chef_03:skill:chef_heal_meal:target:unit_warrior_02:heal:15:cost:1
2026-08-02T14:05:00|2|2|end_turn|actions_used:4
```

---

## 🎮 4. ACTION POOL — SHARED 5 PER TURN (per direct instruction)

Confirmed: **not** 5 actions per unit — a single shared pool of 5 per turn, spent across the whole army however the player (or automation) chooses. This is the central tactical tension of the whole game and is enforced by one field, `actions_remaining_this_turn`, decremented by whatever a chosen action's registry-defined `action_cost` is (basic Move = 1 per tile per mutaclysm-style move_cost, basic Attack = 1, most skills = 1, a few heavier skills = 2+, per `skills.txt`'s own `action_cost` field). Turn ends either explicitly (an End Turn METHOD row) or automatically when `actions_remaining_this_turn` hits 0.

**Action types, per the profession-driven design (confirmed via context, not a literal menu pick — flag if wrong):**
- **Move** — step toward a target tile, cost = terrain `move_cost` summed along the path, capped by the unit's own `move_range`
- **Attack** — basic melee/ranged damage vs. an adjacent-or-in-range enemy, cost 1
- **Skill** — profession-specific ability from `skills.txt` (heal, pie-throw, injunction/stun, etc.), cost per-skill
- **Defend** — brace in place, gain a temporary defense bonus until your next turn, cost 1

---

## 🖥️ 5. THE INTERFACE — BOARD WIDGET SEPARATE FROM MANAGEMENT WIDGETS

Same widget-separation principle as civ-txt §5, applied here:

| Screen (widget) | Shows | Requires the board? |
|---|---|---|
| `setup.chtpm` | Mode select (Classic/Collection), army allocation (Collection only), supervision/risk/compute dials per side | No |
| `main.chtpm` | Turn number, whose turn, actions remaining, army HP summary | No |
| `roster.chtpm` | Your units list as `${piece_methods}` rows (dynamic piece.pdl, same pattern as my-chara-txt's `farm.chtpm`) — select a unit to see its available actions | No |
| `board.chtpm` (**the visual widget**) | The actual 10×10 ASCII grid, units rendered as glyphs, terrain rendered per registry | Yes — this IS the board |

**Tile targeting without requiring the board widget:** picking a move/attack destination from `roster.chtpm` is done by a short relative/named reference (e.g. "move toward nearest enemy," "attack: unit_clown_05," a numbered list of valid in-range targets/tiles resolved server-side and shown as `${piece_methods}` rows) — not by moving a cursor around a grid you have to be looking at. **If** the board widget IS open, it additionally supports mutaclysm's own xlector-style free-roam cursor (`'i'` toggles interact/targeting mode, arrow keys move a cursor, Enter confirms) as a richer, optional input method for players who do want to look at the board — both input paths resolve to the exact same underlying `MOVE:<unit>:<x>:<y>` / `ATTACK:<unit>:<target_unit>` commands, so the engine never knows or cares which one was used.

**GL/3D board widget (deferred, not v1):** mutaclysm's `gl_mirror.c` + `compose_rgb_frame.c` pattern is the proven precedent for an optional real-window visual mirror of the same ASCII truth. Named here as the eventual "2D/3D board" the user referenced, explicitly deferred past v1's ASCII board (§8's build order).

---

## ⚔️ 6. COMBAT & MOVEMENT (ported from mutaclysm, adapted for turn/action-pool structure)

- **Movement**: `move_unit.c` sums `move_cost` along a path (straight-line or simple pathing, not full A* for v1 — a real open question, see §9), checks the unit's own `move_range` isn't exceeded, checks the destination tile is `walkable` and unoccupied, decrements `actions_remaining_this_turn` by the path cost.
- **Attack/Skill resolution**: `resolve_attack.c` (basic) / `resolve_skill.c` (profession abilities) apply `attack - (defense + terrain cover_bonus + defend_bonus_if_active)` style damage math — the exact formula is real new design work (no direct "tactics-RPG damage formula" precedent exists in this house yet, unlike civ-txt's lucky reuse of mutaclysm's melee model wholesale) but the SHAPE (stat vs. stat, terrain modifies the roll) mirrors mutaclysm's own `HERO_ATTACK_DAMAGE` vs. monster HP subtraction.
- **Death**: a unit at 0 HP is removed from `pieces/battle_01/units/` (same delete-the-piece-directory pattern as mutaclysm's `eat.c`/killed-monster convention) — in Collection mode, this does **not** delete the persistent collection piece, only the battle-instance snapshot (§3's snapshot-not-mutate principle).
- **Win condition**: one side has zero units remaining, OR a turn/action limit is hit (configurable, mirrors civ-txt's own score/turn-limit optionality) — a real open question for v1, see §9.

---

## 🤖 7. AUTOMATION — SHARED VERBATIM WITH CIV-TXT

No new design here — this game uses the exact same three-part automation model as `@.apps/civ-txt/CIV_TXT_DESIGN.md` §7:
- **Supervision** (`manual`/`semi`/`full`) per side, independent per side (so you could play Manual vs. a Full-automated opponent, or watch two automated sides play each other in Full/Full for testing)
- **Risk** (1-10) tuning the `weighted` tier's scoring
- **Compute tier** (0-3: preset/weighted/rl/llm) selecting the actual decision mechanism, per the real `decision_mode` chassis in `corp_decide.c`

`tactics_ai_decide.c` is this game's own decision-mode-branching op, modeled directly on `civ_ai_decide.c`/`corp_decide.c` — swap found-city/research/invade for move/attack/skill/defend decisions. Same park-and-wait mechanic for Manual, same pause-after-one-turn mechanic for Semi, same free-run mechanic for Full.

---

## ⚙️ 8. OPS

| Op | Purpose |
|---|---|
| `tactics_menu_input.c` | METHOD-table dispatcher for whichever screen is current — same shape as `mychara_menu_input.c` |
| `tactics_compose_frame.c` | Per-screen render, including the ASCII board draw when `board.chtpm` is active (ported from mutaclysm's `compose_frame.c`'s tile-grid loop) |
| `move_unit.c` | Movement + path-cost validation |
| `resolve_attack.c` / `resolve_skill.c` | Combat/ability math |
| `tactics_ai_decide.c` | decision_mode-branching automation brain, modeled on `corp_decide.c` |
| `setup_battle.c` | Reads mode (Classic/Collection) + army selections, materializes `pieces/battle_01/units/` — this is the ONE op allowed to read from `pieces/collection/` (the modularity boundary — see §2) |
| `apply_battle_results.c` | Post-battle only (P8): reads battle outcome, applies XP/level-ups back into `pieces/collection/<player_id>/pieces/` — the ONLY other op allowed to touch the collection tree, and only after a battle fully ends |

**The modularity boundary, stated concretely:** every op EXCEPT `setup_battle.c` and `apply_battle_results.c` only ever reads/writes `pieces/battle_01/units/` — never `pieces/collection/` directly. This is what makes Classic and Collection genuinely separate modules rather than an if-branch buried inside combat code: the combat/movement/action-pool engine has zero awareness that Collection mode even exists.

---

## 📁 9. DIRECTORY LAYOUT

```
tactics-txt/
├── TACTICS_TXT_DESIGN.md
├── button.sh
├── scripts/build.sh
├── default_op.txt
├── system/                       ← copied from my-chara-txt
├── ops/
│   ├── tactics_menu_input.c  tactics_compose_frame.c
│   ├── move_unit.c  resolve_attack.c  resolve_skill.c
│   ├── tactics_ai_decide.c
│   ├── setup_battle.c              ← ONLY op that reads pieces/collection/
│   ├── apply_battle_results.c      ← ONLY other op that touches pieces/collection/
│   └── +x/
├── pal/
│   ├── setup_module.pal  main_module.pal  roster_module.pal  board_module.pal
├── pieces/
│   ├── chtpm/layouts/
│   │   ├── setup.chtpm  main.chtpm  roster.chtpm  board.chtpm
│   ├── registry/
│   │   ├── terrain_types.txt  professions.txt  skills.txt  item_types.txt
│   ├── system/config.txt
│   ├── battle_01/
│   │   ├── board/tile_<x>_<y>/state.txt     ← sparse, non-default tiles only
│   │   └── units/<unit_id>/state.txt
│   ├── collection/<player_id>/pieces/<piece_id>/state.txt   ← persistent, cross-battle
│   ├── display/  keyboard/  apps/player_app/
└── data/
    └── master_ledger.txt
```

---

## 🚀 10. BUILD ORDER

| Phase | What we build | Verified by |
|---|---|---|
| **P1** | Skeleton: 10×10 board (empty terrain, no cover yet), `setup.chtpm` (Classic mode only for now), `main.chtpm`, two fixed 3-unit staff armies | Real `button.sh run`: setup → battle starts, main shows turn/actions |
| **P2** | Movement: `move_unit.c`, `roster.chtpm` unit selection + move-target list, real path-cost | Move a unit, actions_remaining decrements correctly, ledger entry present |
| **P3** | Terrain registry with real `move_cost`/`cover_bonus`, a hand-authored 10×10 layout using it | Moving through mud costs more actions; standing on high-ground measurably reduces incoming damage |
| **P4** | Basic Attack: `resolve_attack.c`, unit death/removal | A unit dies in a deterministic number of hits, directory removed, ledger records it |
| **P5** | `board.chtpm` ASCII widget + xlector-style targeting cursor (optional input path alongside roster-list targeting) | Board renders real state; both targeting paths produce identical MOVE/ATTACK commands |
| **P6** | Professions + skills registry (a first real roster: farmer, chef, clown, lawyer, warrior, each with ≥1 real skill), `resolve_skill.c` | Each profession's signature skill works end-to-end (chef heals, clown deals ranged damage, lawyer stuns) |
| **P7** | Automation: `tactics_ai_decide.c`, supervision/risk/compute dials on `setup.chtpm`, Semi-mode pause/continue verified live | A Full-automated battle runs to completion unattended; a Semi-automated one correctly pauses each turn |
| **P8** | Collection mode: `pieces/collection/` persistent pieces, `setup_battle.c`'s collection-read path, level/skill/item allocation with a real cap, `apply_battle_results.c` post-battle XP | A leveled, customized piece from a player's collection fights correctly; post-battle XP persists across a second battle |
| **P9** | Optional GL/RGB 2D/3D board mirror, ported from mutaclysm's `gl_mirror.c`/`compose_rgb_frame.c` | Visual mirror matches ASCII board exactly; confirmed not required for any prior phase |
| **P10** | Full replay/audit tooling | Replay reproduces final battle state exactly from the ledger |

---

## 🔗 11. CROSS-REFERENCES & ECOSYSTEM

- **@.apps/my-chara-txt/** — architectural parent (CHTPM, ledger, `piece.pdl`).
- **@.apps/civ-txt/** — sibling design, shares the automation/decision_mode/supervision layer verbatim (§7 here, §7 there — same doctrine, not duplicated).
- **101.mutaclsym🧟‍♂️️+18.01** — source of the grid/terrain-registry pattern, ASCII compose_frame shape, xlector cursor, optional GL/RGB mirror, and the base melee-combat model this game's own combat is adapted from.
- **014.wsr-pal💸️📌️+2** — source of the underlying `decision_mode` chassis (`corp_decide.c`).
- Future: a shared piece/collection format across tactics-txt and any other game in this house that wants "bring your own customized character" — named in mutaclysm's own `platform-vision.txt` as a far-term cross-game trading vision. Not scoped here, but `pieces/collection/`'s schema should be designed with half an eye toward that (plain pipe-delimited fields, no game-specific serialization tricks) even though no cross-game bridge is being built yet.

---

## 🤔 12. OPEN QUESTIONS / NOT YET DECIDED

1. **Pathing for movement**: straight-line/simple cost-summing (named as the v1 assumption in §6) vs. real A* pathfinding around obstacles — confirm simple is acceptable for v1, or if obstacle-avoidance pathing needs to be real from P2.
2. **Win condition specifics**: "all enemy units dead" is the obvious default — should there also be an optional turn-limit/objective-based win (matching civ-txt's own configurability philosophy), or is elimination-only fine for v1?
3. **Collection mode's stat/skill point cap** — direct instruction confirms a cap exists ("having a certain cap") but not its actual number or formula (flat points pool? per-level budget? item slots separate from skill slots?) — needs a concrete number before P8.
4. **Profession roster size for v1 P6** — five example professions are named in this doc (farmer/chef/clown/lawyer/warrior) as illustrative; confirm this is the actual starting roster or if it's a placeholder for a different initial set.
5. **Board size beyond 10×10**: is 10×10 fixed forever, or should the engine treat board dimensions as data (like civ-txt's configurable map size) even if v1 only ever uses one fixed 10×10 layout?
6. **Simultaneous multi-battle**: does a player ever run more than one `battle_01`-style instance at once (e.g. several Full-automated battles running in the background simultaneously), or is it always exactly one active battle at a time? Affects whether `battle_01` should really be `battle_<id>` from the start.

---

## 🏁 13. TL;DR

- 10×10 grid, terrain matters, shared pool of 5 actions per turn across your whole army — real tactical tradeoffs, not per-unit action economy.
- Units are profession-flavored (farmer/chef/clown/lawyer/warrior/...), each with real distinct skills — not generic fantasy classes.
- Two modes: **Classic** (fixed staff armies, v1 core) and **Collection** (persistent leveled/customized pieces, real v1 scope, but built as a genuinely separate module via a hard read/write boundary — only `setup_battle.c`/`apply_battle_results.c` ever touch `pieces/collection/`).
- ASCII board first (mutaclysm-style registry + rendering), optional GL/3D mirror widget later, board widget always separable from text management widgets (roster/main/setup never require it).
- Same automation/decision_mode/supervision layer as civ-txt, shared doctrine not duplicated code.
