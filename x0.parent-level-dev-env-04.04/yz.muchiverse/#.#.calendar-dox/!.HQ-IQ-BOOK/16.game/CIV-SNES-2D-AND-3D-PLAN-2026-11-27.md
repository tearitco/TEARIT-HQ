# Civ SNES, 2D and 3D: audit, decision D157, the first playable slice and its build log (2026-11-27)

Owner, 2026-11-27: "the grok agent didnt actually finish the civ game ... make it work like snes 2d civ (with our 3d teropoly style enabled also ofc). teropoly is a good example of a 2d/3d boardgame". Bar: "flawless polished, tested, and tutorial tested before you present to me".
Labels: **RAN** = I ran it this session and saw the output; **READ** = read in a file, not run; **DESIGN** = written here, not built; **BUILT** = built and a named harness case passes; **NOT VERIFIED** = nobody drove it. Paths: `H` = `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`, `BOOK` = `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK`. Quote every path.
This is the ONE doc for the slice: the next agent adds what it learned to section 11 (build log), not to a new file.

## 1. Audit: what exists (and what the "done" marks were worth)

| Thing | What is really there | Evidence |
|---|---|---|
| `H/@.apps/civ-rpgce` (the Grok line) | A petslike skin cloned from dsr-rpg on 2026-10-14. `ops/civ_rpgce.c` is 310 lines of the dsr pet loop; no civ verb exists. `events/` are the dsr island's (`greet_fenn`, `reply_gus`, `phase_dawn`...), `rooms.pdl` still has the dsr island, `state/` holds mutable `pet_blue.txt`, `pet_green.txt`, `relations.txt`, `uid_blue.txt` (ghost-data shape). Its own README says "Fog, huts, and founding a city are not built". The only civ thing is three tinted viking sprites (READ). The owner already called it a false start (2026-11-23, `TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md` section 2). | READ files and README; not launched (a petslike board starts the 3D engine stack beside the owner's running editor) |
| `H/@.apps/civ-txt` | A text 4X plan. Handoff (2026-08-02) says P2 onward (cities, units, movement, AI, combat) is "None of this is started". Predates D92. | READ |
| `H/202.snes-civ` | A standalone freeglut Civ in 1949 lines of C (settler / warrior / scout, 40 x 30 wrap map, 4 civs, greedy AI, fog, shields). Rules are native C and there is no ledger. Useful only as a list of what an MVP Civ has. | READ README, ARCHITECTURE; not run |
| `H/@.apps/tearit-map` + case `tearit_civ` | The seeded 16 x 16 book with ONE walking person, huts, found city, fog, a second seat and a basic agent. `tearit_civ_op` (turn loop, seats, agent) is a correct ledger + RULE rows + event pages shape (D136). `tearit_map_op.c` (1440 lines) keeps most rules as C constants (review `TEARIT-MAP-GROK-WORK-REVIEW-AND-FIX-PLAN-2026-11-25.md`). It has no units list, no city production, no unit types. The owner said it stays the civ slice (guidance section 0): do not extend it. | RAN: case `tearit_civ` this session, `VERDICT PASS passed=100 failed=0` (fresh run through `cpu_gate`, 12.7 s) |
| `H/@.apps/deskopoly` (Teropoly) | The reference for one game, a ledger, a projection to a 2D desk and the 3D view. Its own engine, not generic. | READ the route docs |
| `H/@.apps/rmmc` + `H/@.apps/vox` | The freshest full worked example: ledger op + verbs as event pages (`verbs.pdl` -> `verbgen`) + key driver + projection op + tutorial op + stranger case. | READ; cases `rmmc_*` exist |

**Verdict:** there is no Civ game to finish. The Grok line left a skin of another game, a text plan and an unrelated C program. The only live asset worth keeping is the SHAPE of `tearit_civ_op` (TURN and SEAT rows, an agent that lists legal moves, numbers in `civ.pdl`).

## 2. Decision D157: which build shape (the five questions)

Forks: (A) extend the `civ-rpgce` petslike skin; (B) generate a ledger game with `game_new_op` and add a Civ MODE to the shared `game_core`; (C) extend `tearit-map`; (D) a NEW app on the rmmc / vox pattern (a ledger op, verbs as event pages, a projection op, a key driver, a tutorial op, its own harness cases).

1. **One authority per fact.** (A) keeps unit and pet positions in mutable `state/*.txt` (the ghost shape the no-ghost rule forbids). (B) one ledger, good. (C) one ledger, good, but its op is a 1440-line shared hard-coded rule file owned by another line. (D) one append-only ledger; the world is the FOLD of the `NEW` row's seed over RULE rows; fog, the picture, the side panel are declared projections. **D wins or ties B.**
2. **Modular.** (B) needs a Civ mode inside `game_core.c`: that is exactly the native-rule shape D92 forbids (Teario's stomp). (A) puts rules in a C hook. (C) edits another agent's op. (D) rules are verbs of a civ op with numbers in `rules.pdl` RULE rows, no number in C, no edit to any shared engine file. **D.**
3. **Scalable (the 10th rule / the 3rd civ).** (D): a rule is one verb + one `verbs.pdl` row + one registry COMMAND row + one case; a civ is a `SEAT` row and weights in data; a second map is a second seed. **D.**
4. **Long term.** The roadmap is rules as ops behind pages, agents that list legal moves and a validator that writes the row (attrition, `META-STRATEGY...`), and the civilization end state. (D) is that shape and the same one rmmc already proved. **D.**
5. **Cost and risk of D.** More new code than (B) (an op, a projection op, a driver, a tutorial op, about 4 files of C) and a new ledger replay that must stay cheap (D156: the replay cost is linear, 4.7 microseconds a row; a slice of a few hundred rows is far below the tripwire). We copy the rmmc driver SHAPE (`step_ms`, relay tail, feed projection), not its code, because the rmmc driver is bound to vox verbs.

**Chosen: D, a new app `civ-snes`** (the guidance doc named `tearit-civ`; that name is already the harness case of the old book's turn loop, so it is avoided). Rejected: A (ghost data, D92), B (D92), C (owner: it stays the civ slice, another agent's op). **Stopgaps kept (each with its removal condition):** the first slice has the human on the keys and one rival agent on `agent:basic` weights, no model at all (removal: when the S6 personality slice starts); art is flat colour tiles and letter pieces from the existing Teropoly tile atlas (removal: the diffusion art pipeline, owner approval).

## 3. The FIRST PLAYABLE SLICE (all of this and nothing else)

SNES Civilization feel: a top-down tile map, a side panel, a turn counter, a clear "what to do next" line.

1. **A map** 20 x 14 tiles from a seed: `grass`, `forest`, `hills`, `mountain`, `water`, integer hash only (the `tearit_map_op` precedent). Mountains cost 3 moves, hills and forest 2, grass 1, water is impassable for land units (the `202.snes-civ` numbers). The start settlers are placed by a rule (largest land, far apart), not by hand.
2. **Two civs.** `civ1` (human, blue) and `civ2` (rival, red, `agent:basic`). Each starts with one `settler` and one `warrior`.
3. **Settler founds a city** on land that is not within `citygap` of another city (verb `found`; the settler is consumed; a city row is written, size 1).
4. **A city builds a unit.** The city picks `BUILD warrior` or `BUILD settler`; each `TURN` it adds `city_shields` plus a terrain bonus; at the cost (RULE `cost_warrior`, `cost_settler`) a new unit appears beside the city and the stock resets (rows, not a stored counter).
5. **Unit movement** with move points (`mp` per unit per turn, terrain cost, the last step may overspend as in the reference), no stacking, no combat.
6. **End turn** (`TURN | n`, the only thing that advances time; it refreshes move points, runs production and then the rival agent's turn).
7. **The rival does something:** `agent:basic` lists its legal moves (move toward unexplored land, found a city when legal, set a build), scores them with `aiw_*` weights from data, and the validator op writes the row. By turn 10 on a fixed seed the rival has founded at least one city (that is a harness check).
8. **Fog of war per civ:** a tile is seen when a unit or city of that civ is within `sight` (RULE) now or ever (a fold of the rows). Unseen tiles are drawn as black in the human's view; the rival's units are drawn only on tiles the human sees. Fog is a PROJECTION.
9. **A side panel** (the editor's left bar, the rmmc `nick_line / stat_line / stat_line2` shape): turn number and year, the selected unit or city, its move points or shields, and ONE "next" line (`press ] to pick a unit`, `B: found a city here`, `Space: end turn`...).
10. **2D and 3D, one scene:** the projection op writes ONE desk (`cells.txt`, `map.txt`, `events.txt`, `extrusion.pdl`, atlas) from the fold; the pc-hq board draws it flat (2D) and extruded (3D, hills and mountains have height, units are small raised pieces). Key `p` toggles 2D/3D (D152). No second render path.
11. **Tutorial lesson 1** (data `tutorial.pdl`, graded from the ledger like rmmc): pick the settler, walk two tiles, found a city, set production to warrior, end the turn. Proven by a case in the style of `rmmc_stranger`.

**State and ghost-data tests (shipped with the slice):** replay reproduces the fold; a hand-edited result is refused; a stray file beside the ledger changes nothing; the audit finds a stray file; deleted projections regenerate identical bytes.

## 4. NOT in the slice (so nobody thinks it is)

Combat and capture; tech, research, gold, taxes; huts (the old design had them: a `rule_hut` page is the first thing after the slice); wonders, diplomacy, government, pollution; ships and ocean travel; stacking; more than two civs; multi-turn goto orders; save slots; sound; our own art (placeholders only); a model-driven agent; the LyfeDesk top-bar cell (rmmc rule: prove it in the editor first); simultaneous play; large maps (the desk page limit is 64 x 64 cells).

## 5. Data model DESIGN (ledger, rules, state)

Files in `H/@.apps/civ-snes/` (names are the plan; section 11 records what was actually built):

| File | Kind | Content |
|---|---|---|
| `rules.pdl` | CONFIG | `RULE` rows: `map_w 20`, `map_h 14`, `sea`, `hill`, `mount`, `forest` thresholds, `cost_grass 1`, `cost_forest 2`, `cost_hills 2`, `cost_mountain 3`, `mp_settler 1`, `mp_warrior 1`, `citygap 3`, `city_shields 1`, `cost_warrior 3`, `cost_settler 4`, `sight 1`, `aiw_found`, `aiw_explore`, `aiw_build`. Their hash is in the `NEW` row: editing a number after a game began refuses that ledger (the game_core / vox rule). |
| `ledger.txt` | LEDGER | rows: `NEW \| seed \| rules_hash`, `UNIT \| id \| civ \| kind \| x \| y` (start units), `SEAT \| civ \| who`, `MOVE \| id \| x0 \| y0 \| x1 \| y1 \| cost`, `FOUND \| unit \| city \| x \| y`, `BUILD \| city \| kind`, `TURN \| n` followed by the DERIVED rows the rules wrote that turn: `PROD \| city \| stock`, `BORN \| unit \| civ \| kind \| x \| y`, `AGENT \| civ \| verb`. Replay re-runs each rule and refuses a row whose recorded result differs. |
| `state.pdl` | CONFIG | declares every file; the `audit` verb fails on an undeclared file |
| `verbs.pdl` | CONFIG | the manifest read by `verbgen`: `new select move found build endturn seat` (each `page=yes`) |
| `keys.pdl` | CONFIG | `KEYVERB` rows (arrows = move, `b` found, `[ ]`...); never binds `y u i o p` (camera keys, D152) |
| `tutorial.pdl` | CONFIG | lesson 1 as `LESSON` / `STEP` rows; graded from the ledger |
| desk folder in `civ-snes-pchq/.../maps/civ-snes/board/` | PROJECTION | `cells.txt`, `map.txt`, `events.txt`, `extrusion.pdl`, `cells.marker`; regenerates from the fold |
| `play/result.txt`, `play/sel.txt` | PROJECTION / PREFERENCE | last verb result; the selected unit (a preference: it changes no outcome) |

The selected unit is a PREFERENCE (a UI cursor), exactly as `play/pick.txt` is in rmmc. Everything that changes an outcome is a row.

**Rules as ops (the registry rows and pages):** one op `civ_op` whose verbs are the rules (D105 shape: "a rule is a verb of the op"): `move` (cost, terrain, mp, no stacking), `found` (land, citygap, settler), `build` (a legal kind), `endturn` (refresh mp, production, births, then the agent), `agent` (legal list, weights, validator), `fog` (the seen set, a pure query). Each has a registry `COMMAND` row (`civ_move`, `civ_found`, `civ_build`, `civ_endturn`) and an event page made by `verbgen`; the window's key driver plays the PAGE (and says "(direct)" when Blueprintz is paused, as rmmc does). One harness case per verb with hand-computed numbers and a sabotage per clause (STANDARD-RULES-AS-OPS sections 13 to 15).

## 6. 2D and 3D (DESIGN, the Teropoly lesson)

Teropoly proves one desk is read by both views: the game writes ONE description and the board draws it flat or extruded; the numbers are baked into the floor texture so the 3D view costs nothing per frame. `civ-snes` does the same: the projection op writes the desk (`BOARD`-like page of `block` cells per tile), terrain from the tile atlas, `extrusion.pdl` gives hills and mountains height, units and cities are `events.txt` rows (pieces). Fog changes the cells (a black tile), so 2D and 3D show the same fog. Parity check (case): the desk written for 2D is byte-identical to the one the 3D view reads (one folder); the photograph shows both. Risk: a flat tile game may be unreadable in 3D (guidance section 7): the first photograph decides, before any rule is polished.

## 7. Keys (DESIGN)

Camera keys are Y U I O P everywhere (D152): `y` first person, `u` third person, `i` free roam, `o` bird's eye, `p` 2D/3D. Digits belong to the game (pick a build item `1`..`3`). Arrows move the selected unit, `]` select the next unit (the possess key of the editor), `b` found a city, `Space` end turn, `Home` nothing. The driver reads the editor's relay file by byte cursor (not xdotool), `step_ms` in `keys.pdl`.

## 8. Proof plan

Cases (pal + `.pdl` + C ops; no Python, no new `.sh`): `civ_snes_rules` (each rule alone with a negative control and a sabotage per clause), `civ_snes_ledger` (replay, forged-row refusal, ghost, audit, projection regeneration), `civ_snes_agent` (the rival founds a city by turn N; a mutant with weights zeroed fails), `civ_snes_stranger` (a person who only reads the bar finishes lesson 1). Then the real window: on debil in Xephyr (the owner's editor is running here and is his) a photograph in 2D and one in 3D, opened and looked at.

## 9. Runtime files and cost

The ledger of the slice is bounded by play (a game of 100 turns is well under 3000 rows, below the D156 tripwire of about 12000). `play/result.txt` is one line, rewritten. The desk folder is rewritten only when a row is appended. A row for each in `RUNTIME-FILE-GROWTH-STANDARD.md` is added when the writers exist. The key driver is one process registered like `rmmc_driver`; idle and active cost are stated in its commit.

## 10. Owner decisions needed (defaults chosen so work can go on)

1. **Name:** `civ-snes` (the old `tearit-civ` name collides with a harness case). Say if you want another.
2. **Map size** 20 x 14 for the slice (the SNES view was about 15 x 13 tiles on screen). Bigger later.
3. **Art:** placeholders now; when do we commission or generate our own?
4. **Huts, combat, tech: which comes after the slice?** Default: huts, then combat, then tech.
5. **Tutorial on the LyfeDesk bar:** only after you approve the editor version (the rmmc rule).

## 11. Build log (append here; date and evidence per line)

- 2026-11-27 design written; audit above is READ except the one RAN row (`tearit_civ` 100/0). Decision row D157 added to the decision log.
