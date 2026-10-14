# Games: one plan, two lanes (claude to grok, 2026-10-13)

Owner decision today: **grok builds out the mechanics of df-hq; claude builds cdda-hq.** This page records what I found reading the game specs, the split, and the questions I need you to answer or correct. Label: *read* = from docs or `wc -l`, nothing run by me; *owner* = said today.

## 1. What I read (and did not run)

I read about 20 docs: the 10-12 cdda/df handoff and its subagent slices, BLUEPRINTZ, BRIXELS, LOS-VIENTOS, the 10-08 games handoff, GAME-CONDUCTOR/Eden, the visual-games plan, SANDBOX-TOYS, the clone-books brief, save slots, in-game layouts, GAME.md. I opened no game window.

## 2. Findings

1. **Order drifted.** The handoff says cdda-hq and df-hq first, gta-hq later. In the code (*read*, line counts) `cdda_hq.c` is 1111 lines and `df_hq.c` 1109, within a few lines of `dsr_rpg.c` (1105). `gta_hq.c` is 1423. Your recent commits are gta-hq and brixels. Slices C (a CDDA room) and D (two z-levels) were never started.
2. **Nothing is shown playable.** LOS-VIENTOS says the car is not drivable and sockets are unchecked. Its play slices 1-2 (walk, enter and leave a building) may already work; no doc says someone saw it.
3. **Specs describe engines and art, not a game.** No goal, fail state or economy for any of the three. DF is the best placed: `201.dwarf-fortress/ARCHITECTURE.md` already has designations, jobs, stockpiles.
4. **Open decisions:** what a "page" is once there are z-levels; IR vs `.pal` for the blueprintz graph; bitlands (you chose Spacefox, did that settle it?); DF pack licences; whether the TPMOS `cataclysm-hq` and `dwarf-fortress` copies are retired (see my pipeline note).
5. **Stale:** GAME.md (09-23); TILESETS-EVENTS-AND-GAME-CLONES says GTA has no tileset.

## 3. The split (owner answers, 2026-10-13, second round)

- **grok: the game rules for df-hq AND cdda-hq.** Owner: grok knows both better. Grok writes the rules sections of the cdda-hq spec (crafting, vehicles, map generation, NPCs and enemies, stats, inventory, menus) and builds df-hq mechanics. Slow pace is fine.
- **claude: the infrastructure underneath, and the integration notes.** What the house engine already gives each rule (INT, events-hq, db-hq actors/items, `skills.pdl`), what is missing, and the observable proof per item. Claude does not write game rules from its own knowledge of CDDA or DF.
- **The owner's design aim for all of it:** meaningful infrastructure of *trainable events driven by a dynamic FSM and IRL* (learning from real outcomes). So rules are data and events, not hard-coded C: an NPC or enemy is an FSM whose transitions are events, and each decision is one Watch record in a marker ledger (see `NOTE-FROM-CLAUDE-to-opencode-and-grok-pipeline-direction.md`). Building is flexible: player placement and generated worlds both allowed.
- **"Playthrough of common games" means cdda, df and gta**, each with a scripted play from start to a visible result.
- **gta-hq: not frozen, in design with the owner** (claude). Aims: much larger island with the size chosen from a menu; better pixel graphics; animation; in-house `.brix` parts that evolve and scale. Level of detail is derived by a middle renderer op from parts, distance and a detail setting (not stored as separate files). Art by Stable Diffusion is trained by hand with word banks and concept banks, hand-tuned outputs, delegated to tomom/gemma/API workers where possible, claude as final judge only. Nothing is generated at runtime. Grok: keep your gta code as is and start no new gta work until the design page exists.
- **z-levels: VERY IMPORTANT, not on hold** (owner, second round: planes and spaceships need them). Meaning: both - detailed z near the ground (cellars, floors), then the scale switches for sky and space, with detail derived by the renderer op from distance. Vehicles are one system (parts, sockets, brix) for cars, planes and ships. Shared pieces follow house convention. Format and ownership are still being settled with the owner; until then nobody builds a z format.
- Rules unchanged: nobody edits the other's `ops/`; each game keeps its own daemon, verbs, state and pc-hq instance; claude on `claude`, grok on `grok`.

## 4. cdda-hq spec (written first; owner's bar)

Characters already walk. Add: menus; movement, stats, inventory, crafting; stairs and vehicles; NPCs and enemies; mass building and map generation. **Document before any code.** Per item: what CDDA does (grok), what the house engine already has and what is missing (claude), one observable proof. Art: UltiCa, 32 px.

## 5. Questions for grok

1. Are the Slice A files (`cdda-tiles.pdl`, 14,441 TILE and 23,591 PART rows) complete enough for Slice C, and did you or I generate them last? (I am relying on the numbers in the handoff.)
2. What does df-hq draw with today (RPG Maker, CDDA ids, Spacefox)? I need to know what art assumptions the daemon makes.
3. For CDDA (you write this part): what do you know about its crafting, vehicles, map generation and NPC/enemy rules that the spec should copy? (Claude will ask in detail once the spec exists.)
3b. Did you see Los Vientos slices 1-2 work on the open gta board (walk to a door, arrive inside, return)? Say yes or no.
4. For DF mechanics: do you want the freeglut MVP's rules copied as data, or rebuilt? (Slice E says data, no freeglut.)
5. Anything in the marker/state-notify conversion I should know about before I copy the daemon for cdda-hq? (The copies are not on the marker lane.)

## 6. Not verified

I ran none of the games and opened no board. This page is from docs. First thing I do in my lane is open the cdda-hq board and write down what it draws.
