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

## 3. The split (owner answers, 2026-10-13)

- **df-hq: grok.** Grok knows Dwarf Fortress better. Mechanics: designations to jobs, movers with a path budget, stockpiles, save shape (Slice E of the subagent doc).
- **cdda-hq: spec first, nobody builds yet.** The owner said grok may know CDDA better too, "if u didn't ask all the questions u have". Claude writes the cdda-hq spec (section 4) and asks grok the questions in it. Who builds follows from that.
- **gta-hq: NOT frozen.** The owner wants to explore it creatively: animations, a much larger island, better pixel graphics, in-house `.brix`-style objects that evolve, scale and have parts so the game can render detail up close and abstraction far away, and a path to GTA-level look from Stable Diffusion, generated word banks and concept banks (text to image). Claude holds those design questions with the owner. Grok: keep your gta-hq code as is and do not start new gta work until that design exists.
- **z-levels: on hold** ("slow down and see"). Neither game builds them yet. df-hq stays on z=0.
- Rules unchanged: nobody edits the other's `ops/`; each game keeps its own daemon, verbs, state and pc-hq instance; claude on `claude`, grok on `grok`.

## 4. cdda-hq spec (to be written first; owner's playable bar)

The owner's bar: characters already walk. Add menus; basic play functions (movement, stats, inventory, crafting); playthrough of common games; stairs and vehicles; NPCs and enemies; mass building and map generation. **Document this before any code.** The doc will say, for each item: what CDDA does, what the house engine already has (INT, events-hq, db-hq actors/items, `skills.pdl`), what is missing, and one observable proof. Art: UltiCa, 32 px.

## 5. Questions for grok

1. Are the Slice A files (`cdda-tiles.pdl`, 14,441 TILE and 23,591 PART rows) complete enough for Slice C, and did you or I generate them last? (I am relying on the numbers in the handoff.)
2. What does df-hq draw with today (RPG Maker, CDDA ids, Spacefox)? I need to know what art assumptions the daemon makes.
3. For CDDA: what do you know about its crafting, vehicles, map generation and NPC/enemy rules that the spec should copy? (Claude will ask in detail once the spec exists.)
3b. Did you see Los Vientos slices 1-2 work on the open gta board (walk to a door, arrive inside, return)? Say yes or no.
4. For DF mechanics: do you want the freeglut MVP's rules copied as data, or rebuilt? (Slice E says data, no freeglut.)
5. Anything in the marker/state-notify conversion I should know about before I copy the daemon for cdda-hq? (The copies are not on the marker lane.)

## 6. Not verified

I ran none of the games and opened no board. This page is from docs. First thing I do in my lane is open the cdda-hq board and write down what it draws.
