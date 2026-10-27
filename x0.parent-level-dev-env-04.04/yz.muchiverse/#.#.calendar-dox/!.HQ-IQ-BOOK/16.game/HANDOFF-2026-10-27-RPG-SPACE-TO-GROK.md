# Handoff 2026-10-27: rpg-space, from Claude (Sonnet 5.5) to Grok

Owner's brief for you: **push the game forward with game mechanics, especially RPG-Maker-style events, scripting through
blueprints, and controls; and PLAY the game (play, build, survive, manage entities) and write a pros/cons list against
RPG Maker, Unreal, Minecraft, CDDA, Dwarf Fortress, GTA, Pokemon and Palworld.** Paths below are from
`x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/` (house); app = `@.apps/rpg-space`. Quote every path (`&`, `@`).
Branch: commit to YOUR OWN branch (`grok`), never `claude`/`main`, scoped pathspec only (AGENTS.md).

## 0. The owner's raw note, decoded

1. *"when can user decide what radius of items in space / depth (molecules etc) shows up in the bottom tb? ... when we allow
   editing of it in blueprints / settings / db"* = the bottom entity bar lists ships and asteroids near the ship within a radius.
   Today that radius is ONE number in a file: `tuning.pdl` row `TUNE | bar_radius | 48` (world cells, Chebyshev, from the hull
   centre). The owner wants it (and a **depth / scale-ladder** radius: planets -> molecules, see `scale.pdl`) changeable by the
   PLAYER in the game, through an editing surface: a blueprint, a settings page, or a db. Not done. Design it, don't hardcode.
2. *"we should probably have our own db, or at least a pc-hq menu / db soon"* = there is no in-game settings/db surface for
   these knobs. They live in `tuning.pdl` (re-read every ~2 s by the daemon; only POSITIVE values are honoured, 0 = default),
   `life.pdl`, `collision.pdl`, `blocks.pdl`, `hold.pdl` etc. A pc-hq/khtpm menu that edits those rows (and shows what each
   does, from the TUNE text column) is the missing piece. Related memories/docs: wraith-alpha in-memory DB plan (future), marker
   mode (append-only marker files, never mtime).
3. *"ask grok to play"* = see section 5.

## 1. What rpg-space is (30 seconds)

A petslike 3D board game on the shared core `&.widgits/_shared-lib/petslike_core.c` (one core, 11 skins; the skin is
`@.apps/rpg-space/ops/rpg_space.c`). You fly a tile-built sci-fi ship (bridge, quarters, hold, engine room) over a seed-built
planet (Cata Prime etc.), mine asteroids with a beam, craft/build blocks, pets live, die, respawn, chat. 3D is drawn by the
engine `&.widgits/board-viewer` (`bv_render_3d`), 2D is the same room as a tile map (key `0` toggles).
Design books: `RPG-SPACE-BLUEPRINT-SYSTEMS.md`, `RPG-SPACE-ENGINE-PIECES.md`, `RPG-SPACE-HANDOFF-2026-10-23.md`,
`EVENT-VERB-PAGES-DESIGN.md`, `EVENT-AREA-SELECTION-DESIGN.md`, `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md`,
`RPG-SPACE-LIFE-DEATH-RESPAWN.md`. The skill `rpg-space-board-driving` is the manual for driving it (read it first).

## 2. How events and scripting work (your focus)

* **Event pages** = RPG Maker pages: folders `@.apps/rpg-space/events/<name>/` (`helm`, `death`, `respawn`, `collision`,
  `col_hit`, `col_breach`, `destroyed`, `environment`, `env_hit`, `start`, `planet`, `newplanet`, `zoom_in/out`), each with
  `event_pkg/pages/...` (condition + trigger: action / parallel / autorun + commands), `area.pdl` (the voxel AREA the event
  touches), `choice.pdl` ("Show Choices" popup), `master_ledger.txt` (proof a page ran: "Play event page_1 (trigger=...)").
  The pal layer plays the pages; the C skin supplies the VERBS the commands call (`helm on`, `life die @actor`, `mine`,
  `destroy`, `space pilot`, ...). If a page is missing the C fallback runs. So **new mechanic = new verb (C, in
  `rpg_space.c`) + a page that calls it + a `hotspots.pdl` HOT row or a trigger**, not a bespoke code path.
* **Verbs**: `@.apps/rpg-space/verbs.pdl` and `verbs_extra.pdl` list them; run one by hand with `ops/+x/rpg_space.+x <verb> ...`
  (a fresh process; the resident daemon is `rpg_space.+x <house> <app>`, one flock). Verbs ignore `tuning.pdl` (only the
  daemon loads it), so test tuned behaviour through the daemon (real key) not the one-shot verb.
* **Hot spots**: `hotspots.pdl` `HOT | room | x | y | z | label | event` (Enter next to the cell plays the page).
* **Blueprints**: `RPG-SPACE-BLUEPRINT-SYSTEMS.md` + `blocks.pdl` (BLOCK/TOOL/NODE/BUILD rows), `catalog.pdl`, `hold.pdl`,
  `reactions.pdl`, `life.pdl`, `collision.pdl`, `pals.pdl`, `voices.pdl`, `space.pdl` (asteroid / ship seeds). Data first:
  a mechanic that can be a row in a `.pdl` should be.
* **Controls**: `keybinds.pdl` (KEY rows; arrows arrive as 1000-1003, `m` mine 109, `b` build 98, Tab control switch 9,
  `0` render mode 48, 49-52 POV, Esc 27). Engine camera keys (q e r t c v w a s d f) go through the ENGINE key file
  (`&.widgits/board-viewer/tools/+x/bv_keys.+x <session> <text>`), NOT the window relay. Camera + keys need a live engine AND
  `In: ON` (`state/flag_int.txt` = 1; the `int` verb TOGGLES). Esc leaves In: and releases the helm.
* **The gate**: `&.widgits/_shared-lib/harness/play/phases/rpg_space_planet_map.phases.pdl` (compiled to
  `cases/play_rpg_space_planet_map.pdl`), run with a many-args pal VM built from `&.widgits/_shared-lib/system/prisc+x.c`
  (the board-viewer session copy is the OLD VM and silently does nothing). Last result: **58 PASS** (phases p1_seed, p2_beam,
  p3_hull, p4_voxels, p5_pilot). New mechanic = new phase. Verbs: HAS / COUNT / FILEIS / NOFILE / DO / PUT / SAY / INIT.
  Harnesses are pal, not new `.sh`.

## 3. State at handoff (what is solid vs not)

Solid (tested + seen, branch `claude`, pushed through `89092f99e`; later commits local until pushed):
* asteroids = random 5-11 voxel nuggets; the other ship = a **tiled 3D mini-ship** (6x6 layout `SK_SKIFF` of the same tile
  cubes as the player's ship: hex floor, wall ring, monitors + chairs, door gap), all spread over the 48x48 land
  (`space.pdl` WROCK / WCRAFT rows), collision by real footprint, bottom bar lists them by `bar_radius`;
* **piloting the other ship**: at the helm in Direct flight, `space pilot <id>` hands the arrows to that craft
  (`state/pilot.txt`), `space pilot off` returns them; bounds, asteroid and hull collision; gate p5;
* engine minimap: camera dot + view cone; engine now survives its own 270 s idle exit (loop_guard keeps the loops while the
  board window is open); engine records how it died (`pieces/display/.engine_exit.log`, `/tmp/bv_engine_exit.log`);
* `cpu_guard` (proc-mon) ends house processes on a CPU rise: **it ended this board's stack at 01:03 on 2026-10-27 during a
  relaunch.** Launch boards with `nice -n 15`, one at a time, and read `sh '&.hq-apps/proc-mon/guard.sh' status` first.

Committed but NOT verified on a live board: the minimap whole-land / follow-the-camera modes (hud.pdl keys `minimap_margin`,
`minimap_follow`, `minimap_view`, `minimap_max_px`, synced from `tuning.pdl` rows by `sk_hud_sync`). Compiled and key-synced
only; photograph them.

Not done / known gaps (be honest about these in any report):
* the other ship has no in-world console, interior rooms or boarding (the pilot is a verb; the camera does not follow it);
* only the planet map is listed in the bottom bar, not galaxy-grid craft; no gate check for `bar.txt`;
* asteroids/ships draw only in some states (helm direct, or released at altitude 1); planet-surface height vs altitude is
  not calibrated (`obj_z` is measured on the Bridge floor only);
* crystal colour reads beige (model colour clips to white under the warm light); skiff wall lift x1.6 is a guess;
* second ship's crew/cargo are text rows, no mechanics; mining/collision footprints for multi-cell objects only partly done;
* no player-facing settings surface (section 0, items 1-2).

## 4. Suggested work order (cheapest, most central first)

1. **Settings/db surface** for the owner's radius: a khtpm/pc-hq menu (use the generic renderer + a manager, see skill
   `khtpm-house-standards`; no new per-project C in `khtpm_core_render.c`) that edits TUNE rows (`bar_radius`, minimap knobs)
   and a depth/scale radius; write back to `tuning.pdl` or a user-data row; show the TUNE text as help. Ask before inventing a db.
2. **In-world console for the skiff** (hot spot + event page calling `space pilot`), then boarding, then crew mechanics.
3. **Event-page coverage**: move hard-coded behaviour into pages/verbs; add pages for mining, building, trading, hail.
4. Survival loop: hunger/air/temperature exist as stubs (`environment.pdl`, `life.pdl`); make one full loop (need -> resource
   -> craft -> consequence) playable end to end.
5. **Back-merge life/death/health bars to the other pets games and keep the rpg-dust copy in step (section 4b).**
6. Controls: write down every key per mode in one table (today it is spread over keybinds.pdl, engine keys, relay codes).

## 3b. UPDATE (end of 2026-10-27, branch `claude` pushed through 54a801f68): what changed since sections 3 to 4b were written

* **rpg-dust is now a copy of rpg-space.** `rpg_dust.c` = `rpg_space.c` with the names swapped, plus the data files dust lacked (collision / environment /
  hold / hotspots / lights / space .pdl, `pchq-overlay/`, helm / collision / col_* / environment / env_hit event pages, the board layout). Dust had no data
  of its own that space lacked. Re-sync method: `sed 's/rpg-space/rpg-dust/g; s/rpg_space/rpg_dust/g; s/!space/!dust/g'` on the skin, then `build_rpg_dust.sh`.
  Gate `play_rpg_dust_planet_map.pal` = the same phases as rpg-space. **Both gates: 83 PASS** (p1 seed ... p7 worlds). The old dust life gate is retired.
  Dust keeps its own `life.pdl`, scale, pals, party and planet data. Dust's board button look is the owner's own settings change, not a bug.
* **Ambient world** (`world_ambient` row): with the helm off and a planet below, the land entity stays written (altitude 1), so the planet shows in every
  ship room, not only the Bridge. Objects (asteroids, skiff) still draw on the **Bridge desk only**.
* **Every planet has its own asteroids**: `sk_world_switch` saves the outgoing planet's objects to `state/world_<planet>.txt`, loads the incoming file or
  generates 3 to 5 from `hash(planet id, seed)` on the first visit; `space world` runs it by hand; the skiff stays with its planet (Cata Prime).
* **User / generated asteroids seam**: node kind `gen:<name>` loads `<app>/asteroids/<name>/model.txt` (voxel piece format, 16 cells a side,
  `gen_vox_max` 1024). Design + the contract for the generate pipeline (concept bank -> image -> 3D -> voxelise -> model.txt) and the open decisions:
  `RPG-SPACE-USER-ASTEROIDS-DESIGN.md`. Nothing generates models yet; no verb adds one; no yields / per-user storage.
* **CPU**: our programs must run alongside normal user activity (AGENTS.md rule, `hq-cpu-safety.md` section 9). A footprint bug of mine (unthrottled
  world check + a land rewrite every 1.5 s) was found and fixed; the daemon now writes `state/tick_stats.txt` (`cpu_permille`, ticks, uptime) every 10 s.
  Reference: rpg-dust at rest, 18 permille. **Read it before and after any change to a tick.** The harness cannot run a daemon, so no gate catches this.
  Other games' daemons do not report yet. The cpu_guard alert from 2026-10-27 01:03 is still waiting for the owner's review.
* **Piloting the other ship** (`space pilot <id>`) and the **minimap knobs** (`minimap_margin` / `follow` / `view` / `max_px`, synced from `tuning.pdl` to the
  board's `hud.pdl` by `sk_hud_sync`) exist; the minimap modes were compiled and key-synced but **never photographed**; the pilot works by verb (gate p5),
  not by an in-world console.
* **Life layer**: shared `petslike_life.c`; rpg-space and rpg-dust ON; nine other games wired but OFF (compile-only). In rpg-dust it now also has drops
  and `take`, because dust has the whole space skin. See 4b.
* **Next, in this order** (suggested): (1) put objects on every ship room's desk or explain why not; (2) the in-world console for the skiff + boarding;
  (3) the settings / db surface (bar radius, minimap, world_ambient, asteroid budgets); (4) gates + a live look for the nine OFF life games before enabling
  any; (5) the generate pipeline writer for `asteroids/<name>/model.txt`; (6) add `sk_tick_stats` to the other daemons.
* **Process lessons from today**: do not end a board's stack by pattern or sweep (the permission layer refused it; close the window instead, or end your own
  single daemon pid); a window count from `pidof` needs the full process name; a `cd` in a chained command can land in the wrong folder and make a
  commit message claim something the commit does not contain (it happened once: see the correction commit after 5c8d3660b).

## 4b. BACKWARD MERGE to the other pets games, and the rpg-dust copy (owner 2026-10-27)

**STATUS (updated after the work, commits 11d37fd82, d98f08920, abccd2783):** the generic life layer IS lifted and offered to all ten other skins. Details:

* **Shared file** `&.widgits/_shared-lib/petslike_life.c` (text-include, `pl_*` names): hp, hunger, death, respawn, life bars, the `life` verb. Hooks the skin may define: `PL_DROPS_BEGIN/END`, `PL_DROP`, `PL_DROPS_CLEAR`, `PL_DROP_COUNT`, `PL_LIFE_VERB_EXTRA`, `PL_LIFE_VERB_KEEPS_NOSLOT` (all default to nothing). `LIFE | enabled | 0` in a game's `life.pdl` makes the tick, the bar state and the UI return at once.
* **rpg-space**: refactored onto it with no behaviour change (gate 58 -> 68 PASS; new phase `p6_life`). It keeps its remnant drops and `life take` behind the hooks.
* **rpg-dust**: ON (`enabled 1`), verified: gate `play_rpg_dust_life.pal` 13 PASS (no items lost, no drops file), and a live board showed the green life bar next to "1 Ember". No drops, no `take`, no air or collision in dust yet.
* **Nine more games** (cdda-hq, civ-rpgce, df-hq, dsr-rpg, gta-hq, mcl-hq, rpg-edit-hq, rpg-pet, tcg-rpgce): wired and **OFF**. Each has the five seams in its skin, a `life.pdl` (`enabled 0`), and its board's bottom-bar item split into the four hp variants plus `.hpb-*` css. Only compile-checked (all 11 skins, `gcc -std=gnu11 -fsyntax-only -I.`); **no gate and no live board for these nine.**
* **To turn a game on**: set `enabled 1` AND set `respawn_room / respawn_x / respawn_y` for that game (the copied values are rpg-space's `bridge 7,5`), write a gate like `rpg_dust_life.phases.pdl`, run it, launch the board and look at the bar. Check the game's `events/death` and `events/respawn` pages (rpg-dust has them; the C fallback runs when a page is missing, but the page route was NOT tested in dust).
* **Still to merge**: drops + `take`, mining/build/hold, air (`environment.pdl`), collision (flight-specific, probably stays in space). Do them as shared text-includes with hooks, same method.
* **Lessons**: `-fsyntax-only` did not catch a missing symbol until the include was added to a second game; always compile AND run a gate for the second consumer. A shared file must not name the first skin's globals (`sk_*`).

The original plan text follows (kept for the reasoning):

The owner expects the "normal things every pets game should have" to exist in all of them: **death / respawn and life bars first**,
then whatever else turns out generic. Today they exist ONLY in the rpg-space skin. Counts from a text search of the skin files
(`grep -c`, not a behaviour test): `life_` appears 71 times in `rpg_space.c` and **0 times** in each of the other ten skins
(`rpg_pet`, `cdda_hq`, `gta_hq`, `df_hq`, `rpg_dust`, `civ_rpgce`, `dsr_rpg`, `tcg_rpgce`, `rpg_edit_hq`, `mcl_hq`); `respawn`
13 vs 0. The core (`petslike_core.c`) only has the hooks: `PET_BAR_HP` (the 6th field of `bar.txt`, drives the hp slots in
`pchq_board_projector`) and `PET_BAR_EXTRA` (new, default none), a 64-slot `tune()` table, and generic drops/hold helpers.

| rpg-space feature | Lives in | Generic? | Back-merge plan |
|---|---|---|---|
| life state (hp, hunger, dead, deaths, inv), `life` verb, `life.pdl`, event pages `death` / `respawn`, DEATH/RESPAWN rows | skin `rpg_space.c` + `events/death`, `events/respawn` | YES (every game with pets) | lift into a shared text-include (house rule: pure + 2+ consumers = text-include, never header+link), per-game `life.pdl`, **off by default** per game, enabled game by game |
| health bar in the bottom bar (`bar.txt` field 6 ok/low/ko -> projector `ent_N_hp_*`, `.hpb-*` css) | core hook + projector + each board's xhtpm slots | YES | each board's `pchq-board.xhtpm` needs the four hp slot variants and css; check every board, not only rpg-space |
| bottom bar extra rows (`PET_BAR_EXTRA`), `bar_radius` | core hook + skin | generic hook, game-specific rows | already in core (default does nothing); each game writes its own rows |
| collision / destruction (`collision.pdl`, `col_*` pages), mining nodes, build (`blocks.pdl`, keys `m` `b`), hold, remnants | skin | partly (mine/build/hold are survival-generic; collision is flight-specific) | decide per game; start with mine/build/hold behind `blocks.pdl` rows |
| voxel pieces (asteroids, tiled ships), direct flight, helm, `space pilot` | skin | NO (space only) | stays in rpg-space |
| tuning rows + `sk_hud_sync` minimap knobs | skin + `tuning.pdl` | generic idea | a shared "sync tuning to hud.pdl" helper later |

Method (owner's standing rules): rehearse in a scratch/staging worktree, additive only, all ten skins must still compile, and the
**per-game verb-script diff gate** must show every old behaviour unchanged before a game gets the new layer (see memory
`petslike-core-framework`; design `RPG-DUST-DESIGN.md` R8). Do the lift once in the shared file, then turn it on per game.
Footprint (section 6): the shared layer must cost nothing when a game has not enabled it.

**The rpg-dust copy** is the owner's short-term goal and comparison point: get rpg-space to a defined level, then copy it onto
`@.apps/rpg-dust` so any lost feature can be diffed against it without digging through git. rpg-dust's skin is 478 lines against
rpg-space's 1,177 and it has 8 event folders against rpg-space's 14, i.e. it is far behind. Steps: (1) finish the generic life /
health-bar lift above, since dust needs them too; (2) port the rpg-space gate (`rpg_space_planet_map` phases, then the other
rpg-space gates) to a dust copy and make it pass there; (3) keep the per-game diff gate green for the other skins; (4) record
what dust does NOT take (space-only: helm, direct flight, voxel ships) so the comparison stays honest. dust-specific design:
`RPG-DUST-DESIGN.md` (scale ladder, molecules).

## 5. Your play-test assignment (owner: "ask grok to play")

Play it like a person (launch with `sh '@.apps/rpg-space/button.sh' run`; drive with real keys / the per-pid relay / `bv_keys`,
xdotool last; read the skill). Try: **play** (fly, land, talk to pets), **build** (`b`), **survive** (air, hunger, hazards,
death and respawn), **entity management** (pets, crew, hold, trading). For each, record evidence the owner can see (a PNG, a
state file), not a claim. Then write `RPG-SPACE-PLAYTEST-GROK-2026-10-xx.md` here with:

| Game | Where rpg-space is better | Where it is worse / missing |
|---|---|---|
| RPG Maker | (events, tiles) | ... |
| Unreal | ... | ... |
| Minecraft | ... | ... |
| CDDA | ... | ... |
| Dwarf Fortress | ... | ... |
| GTA | ... | ... |
| Pokemon | ... | ... |
| Palworld | ... | ... |

Compare mechanics you actually exercised, not the marketing of the other games. Mark "not tried" where you did not play it.

## 6. House rules that bit us (short)

* **Our programs run alongside normal user activity (owner 2026-10-27).** The owner has Chrome/Firefox and more open while the
  game runs, on a weak CPU. Every feature needs a footprint note (idle and active, measured with a browser running), heavy
  things launch niced and one at a time, idle must be near zero, and a `cpu_guard` kill is a finding to fix, not noise. See
  `03-pitfalls/hq-cpu-safety.md` section 9. When you write a design or a report, say what it costs.
* Never report a number you did not observe; confirm an artifact exists before trusting its count; negative-test an assertion.
* Never "fixed/done" without a fresh build + fresh run + evidence. A clean compile is not evidence.
* Use the relay / `bv_keys`, not xdotool. Check the engine is alive and In: is ON before trusting a 0% change.
* Do not kill by pattern (`pkill -f`); end processes by explicit pid, and only your own stack. Do not touch the owner's windows
  (taskbar, hotbar positions) or `xyzfs/users`. Never push `user/*` branches. Never put code in `XO/`.
* No header+link sharing; house sharing = inline / text-include / op+IPC. Comments stay fresh and doc-pointing.
* Commit scoped (`git commit -m ... -- <paths>`), own branch, commit before you stop; the owner pushes or tells you to.
