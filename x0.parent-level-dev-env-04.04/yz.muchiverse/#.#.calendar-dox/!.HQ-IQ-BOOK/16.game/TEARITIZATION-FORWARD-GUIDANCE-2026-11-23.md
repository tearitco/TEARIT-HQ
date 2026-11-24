# TEARITIZATION: forward-looking guidance (2026-11-23)

Status: **DESIGN / PROPOSAL. Nothing in this document is built.** Written for the owner and for the next agent. Labels: **RAN** = I ran or saw it and say where; **READ** = I read it in a file, did not run it; **DESIGN** = written, not built; **PROPOSAL** = my recommendation, the owner decides; **NOT FOUND** = I looked and it is not there; **NOT PROVEN** = may be true, no evidence yet.

## 1. What TEARITIZATION means (owner, 2026-11-23)
A "CIV-SNES" style clone (Sid Meier's Civilization as it was on the SNES), made with EVENTS, playable by MULTIPLE AGENTS plus a HUMAN, with WORLD GENERATION, working in BOTH 2D AND 3D. The name is TEARITIZATION: take a known game shape and rebuild it the house way (every rule an op, the ledger the only authority, pages visible in Blueprintz, a harness that can fail).

Two rules on the source game: we copy the SHAPE of the rules (turns, cities, units, tech, fog), never its pictures, sounds or text. Art: placeholders are fine for tutorials; our own art comes later from the diffusion pipeline through hai. The lore of the owner's own franchise is unpublished IP and is not used here.

## 2. What already exists, and what the owner said about it
- **`@.apps/civ-rpgce` is LIKELY A FALSE START (owner, 2026-11-23). Do not build on it.** READ: its 2026-10-14 design `16.game/CIV-RPGCE-DESIGN.md` has useful ideas (settlers, huts that grant one line once per settler, founding a city that leaves a marker, fog kept per unit as a list of seen tiles); but it was a petslike skin whose rules would live in C hooks, which D92 forbids ("no native rules, all ops, forever"). Take the ideas, not the code.
- **`@.apps/civ-txt`** (READ: `CIV_TXT_DESIGN.md`, handoff dated 2026-08-02): an older text 4X plan on the ledger plus CHTPM pattern, "map is an optional separate widget". Not checked by me whether it runs; it predates D92 and the op contract. Same rule: ideas only.
- **The civilization end state** (READ: `16.game/GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md`, owner 2026-10-28): civilizations of hireable and capturable AI NPCs through a DF-style multi-entity interface. That report says NOT FOUND for pedestrians, laws, cops as a system and hiring or capturing an NPC. TEARITIZATION is a much smaller first step toward it: seats (section 5) are the "controller" relation that report proposed.
- **What WORKS now and is the template** (RAN 2026-11-22/23, see NIGHT 62 and the extensive report v1 in XO nov-23): Teario (rules as ops, D92/D93/D106), RTA (Robot Trap Auto: `rta_*_op` ops, `board_city.pdl`, walking by one ledger row per step, D120), the Minecraft-style voxel app `vox` (D105, D112, D115: ledger, terrain, keys, a desk picture), In: ON possession fixed (D117, D118, D119, harnesses `in_session_follow` 19, `token_follow` 13).

## 3. The shape every rule must have (this is the whole method)
Every Teario, RTA and vox rule already has the same five parts (READ in D93, RAN in the cases named):
1. **A ledger row is the only authority** (`NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`). Replay reproduces the state; a hand-edited ledger is refused (vox: exit 14).
2. **One op per rule**, a pure module plus a CLI, with `--describe`.
3. **An event page in Blueprintz calls it** (visible, all fields filled, D109).
4. **Numbers live in RULE or BLOCK rows**, never in C.
5. **A harness case that can fail**, with sabotages ("mutants") that turn it red.
Pictures are projections written by an op (vox and Teario write `cells.txt`, `cells.marker`, `cells.rgba`, `map.png`, `events.txt` into a desk) and are never read back as input.

PROPOSAL: a TEARITIZATION rule that lacks any of the five does not ship. `mech_new_op` (D100) and `game_new_op` (D62) already generate the shell; use them in a scratch house first.

## 4. How a Civ-style turn loop maps onto that shape (DESIGN)
Turns are already manual in vox and RTA: READ in the vox handoff, a verb is one turn, with `TURN | n | wait` rows and derived rows regenerated on replay. So:
- `TURN | n | player` rows; "next turn" is a verb (`civ_op endturn`), the only thing that advances the clock. There is no real-time clock daemon (the clock daemon burn of 2026-11-08 is a warning, see CPU pitfalls 23 to 27).
- **Units, cities, tiles, tech, gold are folds of the ledger**, never files someone edits. Intent rows (`MOVE`, `FOUND`, `RESEARCH`, `BUILD`) are written by verbs after rule checks; derived rows (income, growth, hut result) are regenerated on replay, as vox does for `MINE DROP DAY`.
- **Rules as ops** (each one a case): `rule_move` (cost, terrain, who may enter), `rule_found_city` (not on water, not too near another city), `rule_hut` (the seeded hut result: once per unit, from the seed and the tile, so replay gives the same answer), `rule_growth`, `rule_research`, `rule_income`, `rule_combat` (later, last), `rule_fog_reveal`.
- **Fog is a projection per player**, derived from the ledger by a named verb, safe to delete, regenerated byte for byte (the civ-rpgce idea of a per-unit seen list becomes a per-player fold of `MOVE` and `FOUND` rows; PROPOSAL: per player first, per unit later).
- **Huts, settlers, found-city** come from the civ-rpgce ideas, written as rules-as-ops, not as hooks.

## 5. Multiple agents plus a human (DESIGN)
**Human.** A person plays the way the owner's proof standard says (`!.PRESENTATIONS-THE-ONE-PLACE-...md` section 11): In: ON, possess the piece (cursor onto it, Enter, HUD shows `poss <id>`), then ONLY keys move and act. Mechanics that exist (RAN in NIGHT 62): the desk token is a row of `events.txt` (D101); a GAMEKEY forward writes keys to a relay and a tiny tailer turns each key into one op call, as `vox_keys_op` does with a key table in data (`@.apps/vox/keys.pdl`, D115); In: ON follows the session (D117); the possession cursor follows a moved token (D118); one `events.txt` snapshot per frame (D119). A TEARITIZATION key table is one more `keys.pdl`.

**Agents.** PROPOSAL: an AI player is a hai-style agent. Hai book 23 (hai repo `book/23-agents-sessions-and-personalities-2026-11-23.md`) gives the model (RAN in the hai-cli window): an agent is a self-contained folder with its own corpus, voice, rules and packs, with sessions, and a personality that is a data preset applied at birth. For a civ seat that means a folder `civ_agents/<id>/` with a personality preset (`builder`, `warlike`, `scholar` as RULE rows: weights for expand, research, defend) and a ledger of what it did and what it was graded.

**Seats.** A seat is a ledger row, and "a driver change is a row" (the Teropoly rule in the no-ghost standard; RAN in harness `deskopoly` cases `seats`): `SEAT | civ | human` or `SEAT | civ | agent:<id>`. Changing who plays civ 2 is a new row, never a file edit.

**What an agent does on its turn (the attrition rule, never broken):** a small policy lists the legal moves (an op computes the list), scores them with weights from rows, picks one, and a VALIDATOR op decides whether it is legal and writes the ledger row. A model, if one is used at all, only DESCRIBES in one fixed line ("expand east: city site found"); it never sets a number and never writes a row. This is the same loss function as the hai chat: the harness verdict is the loss (META-STRATEGY section 3). Grades on an agent's turn are human, promotion is human-reviewed, and only harness-verified rows are training data.

**Which model, if any.** First tier: no model at all (weights and a list of legal moves). Optional second tier: the LAN Ollama on the Mac through the `ai_backend.pdl` contract (`17.ai/ENTITY-LLM-ENDPOINTS-HOW-TO-TEST-AND-USE-2026-11-16.md`, `AI-BACKENDS-DISAMBIGUATION.md`). Rules from the house: free models only, never Groq or OpenRouter in a test, never print a key, nothing private in a prompt, and a harness case must not touch the network.

## 6. World generation (DESIGN)
- **A seeded op writes the world** as ledger rows plus a desk projection. Precedents (READ): vox has `terrain.pdl` (TERRAIN boxes, a CONFIG the game never writes; the world is the fold of the ledger over it); RTA has the painter `rta_city_op` driven by `board_city.pdl` with KIND rows; `game_new_op` makes app shells from data templates on one shared binary.
- PROPOSAL: `civ_world_op gen <seed> <w> <h>` writes `WORLD | seed | w | h` and the tile rows (land, water, forest, hills, hut) deterministically from the seed with an integer-only generator (the house avoids floats in rule modules; D93). The same seed must give the same bytes: that is a harness case, with a mutant that changes one tile and must turn it red.
- **Shared base map many players tweak:** the trunk-and-branch standard (`02-architecture/TRUNK-AND-BRANCH-SHARED-DATA-STANDARD-2026-11-16.md`, D87): one base world (trunk), each player's discoveries and edits as branch rows, never a copy per player. Its op `trunk_op` is NOT built (READ in the standard); say so before relying on it.
- Variety (later): biome rows as data, so the tenth map style is a data file.

## 7. 2D and 3D (DESIGN, owner rule "3D must have 2D parity")
One scene description, two projections. The pc-hq board reads the desk (`cells.txt`, `cells.marker`, `map.png`, `events.txt`, `extrusion.pdl`); key 0 toggles the canvas source. Heights come from SHADE rows (event_lift for lifted pieces). The lesson of D119 applies: read the desk once per frame so the cursor and the drawn piece cannot disagree. Do not add a second render path; if 3D lacks something 2D has, fix the shared path.
Risk (NOT PROVEN): a tile game with flat tiles may be unreadable in 3D. Mitigation: heights for terrain, small extruded pieces for units, and a first pictures-only slice (S0) judged by eye before any rule is built.

## 8. The proof standard TEARITIZATION must meet
From `!.PRESENTATIONS-THE-ONE-PLACE-...md` sections 10 and 11: (10) each rule's event page is shown in the REAL Blueprintz window, all fields filled, with the Step ev that fires it and the resulting ledger rows; (11) the playable thing is proved by playing: In: ON, possession, keys only, the HUD position read in each frame, the 2D (key 0) and 3D pictures both changing. A scripted setup is allowed if labelled; a script never replaces the play. A case that passes on ledgers while the screen is empty is a failed test (AGENTS.md). Do not report numbers you did not observe.

## 9. Decisions the owner must make (PROPOSAL, each with my default)
1. **Map scale.** 16x16 first (default), then 40x25 or more. Scale decides the ledger cost below.
2. **Simultaneous or turn-based.** Default: turn-based, hot-seat order by `TURN` row. Simultaneous play needs one writer per ledger (risk 3).
3. **How many AI seats.** Default: 1 human plus 1 agent for the first slice, then up to 3 agents.
4. **Which free model tier, if any.** Default: none for the first five slices (weights and legal-move lists); the LAN Ollama on the Mac only to phrase a one-line description.
5. **What the ledger of a world costs.** Observed once (RAN, scratch, niced, vox handoff): `verify` takes 0.42 s at 1007 rows and 9.9 s at 5035 rows (about 25x time for 5x rows, O(n^2)). A world with many units and turns passes 5000 rows quickly, so a ledger checkpoint or a hash chain is a PREREQUISITE (not built). Decision: build the checkpoint before S1 or cap the first world at about 1000 rows.
6. **Unit art sources.** Placeholder art is fine for now (tutorials); our own art comes later from the diffusion pipeline through hai. Default: flat-colour pieces with a letter glyph for S0 to S4 (the Teario and RTA approach), commission or generate original art later (a generator is allowed if the owner approves its output, "claude may approve into good/ after opening the picture").
7. **Where it lives.** A NEW app (default name `tearit-civ`, owner may rename), generated with `game_new_op` into a scratch house first. Not `civ-rpgce`.
8. **How much of the civilization end state to aim at.** Default: the Civ shape only (cities, units, tech, gold, fog). Laws, cops, hiring and capture are NOT FOUND as designs and wait for the owner's order (the Grok report asked this and it is still open).

## 10. Staged build plan, harness first (PROPOSAL)
Each slice names the check that must be able to FAIL.
- **S0: a 16x16 seeded map.** `civ_world_op gen` plus a desk projection in 2D and 3D. Check: same seed gives identical bytes (mutant: flip one tile, red); the 3D picture is opened and looked at by a person; the desk is regenerated identical after deleting the projection.
- **S1: settler move and found-city as ledger rows via event pages.** `rule_move`, `rule_found_city`, `rule_hut`. Check: replay reproduces state; a hand-added `FOUND` on water is refused; found-city twice on one tile refused; each page shown in Blueprintz.
- **S2: two seats alternating turns (human by keys, one agent).** `SEAT`, `TURN`, `endturn`, a `civ_keys_op` tailer. Check: In: ON, possess, keys only, HUD pos read per frame; a seat swap is a row; an agent move that the validator refuses writes nothing.
- **S3: fog as a per-player projection.** Check: deleted fog regenerates identical; a unit of the other player is not in player A's projection (negative control: it appears when in range).
- **S4: tech and gold.** Rows for `RESEARCH`, `INCOME`; numbers in RULE rows. Check: change one RULE number and the replay verdict changes (the numbers are really data).
- **S5: world generation variety.** Biome rows, several seeds. Check: every seed passes a reachability check (all land reachable from every start; mutant: wall off a start, red).
- **S6: hai-style agent personalities.** Folder per agent, presets as data, grades by a human. Check: two agents with different presets make different first moves on the same seed (and a mutant that ignores the preset turns it red).
Between S1 and S2 add the ledger checkpoint if decision 5 says so.

## 11. Risks
1. **Ledger verify cost** (O(n^2), section 9 decision 5).
2. **Simultaneous writers on one ledger.** The house ledgers are append-only with one writer verb; two agents writing at once risks interleaved rows. Mitigation: one `civ_op` entry point, serialised, turn-based.
3. **Model cost and quota.** Free tier only; a probe once ate a whole day of free calls; never loop to a 429.
5. **3D readability** of a tile game (section 7).
6. **Rules creeping into C.** The Teario precedent (D92): the stopgap hai teach logic is also in C today, labelled with a removal condition. Do not add a second stopgap.
7. **CPU on a weak machine.** One slice, niced; measure the idle cost with a browser open, state it in the commit.

## 12. What is NOT proven
Nothing here is built. NOT PROVEN: that a Civ-shaped rule set fits the five-part rule shape without a hidden native rule; that the 3D view of a tile map is readable; that an agent with weights plays a game worth watching; the cost of a checkpointed ledger; `civ-txt` runs. The next step is the owner's answer to section 9, then S0.

## 13. External references
Repo paths (BOOK = `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/`): `16.game/CIV-RPGCE-DESIGN.md`; `16.game/GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md`; `16.game/MINECRAFT-EVENTS-HANDOFF-FARMING-WATER-MOBS-DAYNIGHT-2026-11-22.md`; `16.game/RTA-BATTERY-CONVERSION-2026-11-21.md`; `16.game/TEARIO-HANDOFF-WHERE-WE-ARE-AND-WHAT-IS-NEXT-2026-11-22.md`; `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D62, D87, D92, D93, D100, D101, D105, D109, D112, D115, D117 to D120); `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`; `02-architecture/TRUNK-AND-BRANCH-SHARED-DATA-STANDARD-2026-11-16.md`; `17.ai/META-STRATEGY-ATTRITION-HARNESS-TOOL-USE-AND-CHEAP-BUILDING-2026-11-22.md`; `17.ai/ENTITY-LLM-ENDPOINTS-HOW-TO-TEST-AND-USE-2026-11-16.md`; `!.PRESENTATIONS-THE-ONE-PLACE-STYLE-TOOLS-AND-HOW-TO-2026-11-18.md`; house `@.apps/vox`, `@.apps/rta`, `@.apps/civ-rpgce` (false start), `@.apps/civ-txt` (older). The hai repo `/home/no/Desktop/github/work/#.hai/harnecient-hai` books 22 and 23. No outside URLs were used.
