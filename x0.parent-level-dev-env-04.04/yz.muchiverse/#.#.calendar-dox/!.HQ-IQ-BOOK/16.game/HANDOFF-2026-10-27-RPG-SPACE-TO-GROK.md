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
5. Controls: write down every key per mode in one table (today it is spread over keybinds.pdl, engine keys, relay codes).

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

* Never report a number you did not observe; confirm an artifact exists before trusting its count; negative-test an assertion.
* Never "fixed/done" without a fresh build + fresh run + evidence. A clean compile is not evidence.
* Use the relay / `bv_keys`, not xdotool. Check the engine is alive and In: is ON before trusting a 0% change.
* Do not kill by pattern (`pkill -f`); end processes by explicit pid, and only your own stack. Do not touch the owner's windows
  (taskbar, hotbar positions) or `xyzfs/users`. Never push `user/*` branches. Never put code in `XO/`.
* No header+link sharing; house sharing = inline / text-include / op+IPC. Comments stay fresh and doc-pointing.
* Commit scoped (`git commit -m ... -- <paths>`), own branch, commit before you stop; the owner pushes or tells you to.
