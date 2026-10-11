# Phymoji engine: review packet for the external reviewer (Qwen)

> Repo copy of the packet written to `XO/13.phymoji-engine/00-REVIEW-PACKET.md` for external review. Reviewer answers are appended in the XO copy; paste them under `## Appended` here when they come back.

Written 2026-10-07 by claude (the house's manager agent) at the owner's request. Copies of design docs and source references live in `design-docs/` and `source-refs/` (copies, so the originals in the house repo are unchanged). **How to answer: add your answers under the `## Appended` heading at the bottom of THIS file (and, for a document-specific point, under an `## Appended (reviewer)` heading at the end of that document's copy). Do not edit the body above; write new text only below the headings. Mark each answer with the question number. Say "cannot verify" instead of guessing.**

## 0. Read-me-first honesty
Everything here is **design only** except: the pal-chain escrow/faucet build and the solar-sandbox step 1 (both committed in a local alpha branch, harnesses passing, **never tested in the GUI**) and the pc-hq pieces named below, which already exist. I have **not run pc-hq live** for this packet; section 1 is a static read of files.

## 1. THE PROBLEM THE OWNER NAMED: pc-hq levels are not synced and not shown loadable from the desktop
Owner's words: "the real problem is that the pc-hq level with 'sun' and chicken etc haven't been synced or shown to be loadable from the desktop yet. That would bridge a lot of gaps. It's a planet already; some of the others in pc-hq as well. How do we treat those?"

### What I found (static read; file paths are in `source-refs/`)
1. **pc-hq already has several "levels" (maps):** `pieces/system/maps/` holds `default`, `mineclonia_sample`, `cdda_sample`, `test_terraces`, `test_walls`. **`maps.pdl` lists only three** (`default`, `mineclonia_sample`, `cdda_sample`), so `test_terraces` and `test_walls` exist on disk but are not in the list (possible drift; unverified whether anything reads that list).
2. **The active level is `mineclonia_sample`** (`board_config.txt`: `active_level` and `active_board` both `mineclonia_sample`), not `default`.
3. **The "sun and chicken" level is `default`:** the sun and moon are real pieces (`pieces/sun_01`, `moon_01`: `entity_type=celestial_body`, positions; the clock daemon `pc_clock_daemon.c` moves them with a shared elliptical orbit around the planet) and `pieces/world_01/animals.txt` holds the chicken (`chicken,14,12,17`). The default map is a 16-line `desk1/map.txt` plus a `game.pdl` with two desks.
4. **The defaults are fixtures only, not wired:** `defaults/README.md` (dated 2026-08-30) says `default-legacy` and `default-pdl` are "real, on-disk fixtures only... Neither is wired into the game's own File/Desk menus yet... wiring real nav rows is step 2, **not started**."
5. **Desktop launch:** the taskbar launches the board through `open_pchq_board.sh` (the standard X11-HQ shape), which **always opens at level 1** (the floor is level 0; owner decision 2026-10-05). The desktop reads a producer-size file `#.desktop/pchq_board_view.txt`. **I found no desktop-side list of pc-hq levels** (nothing that lets the desktop choose `default` vs `mineclonia_sample`); I did not read the whole launcher or the renderer for it.
6. **Three separate "world" notions exist and are not unified:** (a) pc-hq `maps/<level>/desk<N>` (board-game pages), (b) the livedesk **desks** (`xyzfs/users/<uuid>/.../desks/*.pdl` with `DESK|name|path|...` rows), (c) the new solar sandbox's pages (`solar-system`, `solar-earth`, ...) built in a way that follows the `game.pdl` MAP-access / `mr_transfer_desk` pattern. Nobody has shown that a pc-hq level can be opened as a livedesk desk or the reverse.
7. **Animals/celestial bodies are copied code:** `tick_animals` exists in three copies (`pc_clock_daemon.c`, `pc_menu_input.c`, `pc_compose_frame.c`) per the daemon's own comment.

### What this implies (my reading, please challenge it)
The `default` level (sun, moon, chicken, one chunk) is the **only existing planet-like world** and the natural first "planet page" for the solar sandbox. Making it **loadable from the desktop and in sync** (same entities, same clock, same level list) would bridge: the sandbox's `Enter Earth` could land in a real pc-hq level; the Mineclonia/CDDA samples become example "planets"/biomes; the chunk generator and the orbit code already exist.

### Questions (please answer each under Appended)
1. **Level registry:** should there be ONE authoritative list of pc-hq levels (maps.pdl) that the desktop, the launcher and the solar sandbox all read? What breaks if `maps.pdl`, `board_config.txt` and the on-disk folders disagree (as they do for test_terraces/test_walls)?
2. **Desktop load path:** what is the smallest correct way for a livedesk desk row (or taskbar entry) to open a specific pc-hq level, given that `open_pchq_board.sh` always opens level 1 of the active board? (Which files would you change, and which must not be touched: `khtpm_core_render.c` is shared; `board-viewer/ops/bv_menu_input.c` is a collision file.)
3. **Sync:** what does "synced" need to mean here: the same sun/moon/chicken pieces visible in the 3D board, in a desktop 2D tile view, and in the solar-sandbox page? Which file/ledger is the single source of truth for entity positions (pieces state files, the world tick counter, a ledger)?
4. **Unification:** should pc-hq levels, livedesk desks and sandbox pages become ONE concept (a "page" with a type), or stay three, with doors between them? Risks either way?
5. **Treatment of the other levels:** `mineclonia_sample`, `cdda_sample`, `test_*`: treat each as an example planet/biome page, as test fixtures, or retire? How would you mark them (a row in a level registry: `LEVEL | id | kind=planet|sample|test | ...`)?
6. **Duplicated `tick_animals`:** fold into one op, or leave until the event/node design lands? Which is lower risk?
7. **A live verification plan** (the owner dislikes manual test rounds): what evidence would prove "the default level loads from the desktop and shows the sun and chicken"? (State files to read, relay-driven steps, a screenshot op.) The house testing rule is: drive windows through the per-process relay file, not xdotool.

## 2. The larger plan this serves (summaries; full texts in `design-docs/`)
- **`GAME-BUILDING-BLOCKS-AND-TOOLS-DESIGN.md`**: everything is a reusable block (data rows + an event page or small op + a harness + a doc); games assemble blocks; tools are X11-HQ editors/viewers. The house forbids plugin/`.so` loading and header+link sharing; "plugin" means op + event data (+ text-included pure math).
- **`SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md`**: a system page of emoji bodies, each planet its own page, Enter/Leave/Teleport menus through the existing transfer op; planet numbers -> derived values (gravity, orbit, temperature, weather, travel cost) as **swappable event-page nodes linked by `LINK` rows** (owner: "gravity is just a node that can be tuned, swapped out").
- **`REAL-3D-PLANET-EXPLORATION-AND-REALTIME-MODE-DESIGN.md`**: an explorable voxel planet with a Minecraft-like player; **stepped vs real-time clock** (same events, different clock source); assets referenced in place from the pallets (Mineclonia textures, CDDA UltiCa tiles); noise-generated biomes (a new block; existing generators are not real noise).
- **`CANVAS-CRAFT-DESIGN.md`** + recipes: proposal to make it the single recipe source.
- Economy/session designs (`DSR-*`, `PLAY-ECONOMY-*`, `PAL-CHAIN-*`, `GAME-SESSIONS-*`, `MUTACLYSM-AS-EVENTS-*`, `SURVIVAL-GATHERING-*`) are context for how planets, players and goods connect; read only as needed.

## 3. Cross-cutting concerns I want a second opinion on
1. **Cost on a weak machine:** every entity is a process; planets as voxel worlds and real-time ticks are expensive; the raymarch viewer is ~17 fps today and unmeasured on chunked planet data. Is "generate lazily + stepped by default + data rows instead of entities" the right defense?
2. **Physics as swappable event pages** vs a fast compiled engine: where is the line (per-tick gravity reads in real time)?
3. **Three stored-vs-derived rules** (derived values never stored; node wiring and tunables stored): any trap?
4. **Licensing:** Mineclonia/Pixel Perfection textures and CDDA UltiCa tiles (CC-BY-SA 3.0) are used by reference from `#.NNEST_ASSETS`; is reference-in-place enough, or does shipping a game require bundling and attribution?
5. **Identity/ownership of pages:** if a "book" is a planet and planets can contain books, what stops cycles, and how do pointer rows (inode-like) stay valid when a page moves?
6. **Anything in the design docs you think is wrong, contradictory or missing.** Be concrete and cite file and section.

## 4. Known unknowns (I did not check)
How `open_pchq_board.sh` resolves the level; whether the desktop can already open a pc-hq level through some path I did not find; how complete Canvas-Craft's manager is; whether the sandbox's emoji entities render without `sprite.csv`; whether the real `active_session` lookup works for sandbox menu rows.

## Appended
(Reviewer: write below this line. Number your answers to match the questions above.)

