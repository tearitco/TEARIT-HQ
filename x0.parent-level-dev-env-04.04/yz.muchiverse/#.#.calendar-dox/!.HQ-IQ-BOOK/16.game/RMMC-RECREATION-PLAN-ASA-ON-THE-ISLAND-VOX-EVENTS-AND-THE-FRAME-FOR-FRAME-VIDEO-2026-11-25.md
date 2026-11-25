# RMMC: recreating /home/no/Desktop/github/RMMC.mp4 frame for frame, with Asa on the floating island, driven by house events (PLAN, 2026-11-25)

Status: **PLAN. Written before the build starts, on the owner's order ("record and document in great detail every nuance and how u will achieve with house events etc b4 we start").** Sections 2 and 3 say what is already built and seen; everything from section 6 on is DESIGN until a harness name and a dated row say otherwise. A `[x]` in this file is a claim until a case in section 14 backs it.

Labels: **RAN** = a command was run this session and its output observed. **SAW** = a PNG was opened and read. **READ** = read in a file, not run. **DESIGN** = written, not built. **OPEN** = the owner decides. **RISK** = may fail, and how we find out.

House paths: `H` = `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`; `BOOK` = `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK`. XO media: `/home/no/Desktop/github/work/XO/31.Nov/nov-25/`.

---

## 1. What the owner asked for (his words, 2026-11-25, kept as the acceptance text)

1. "basic minecraft level based on floating sky with asa and terumon but 3x size (room 2 move around) (and no indoor outdoor teleports, so just copy in rpg-edit to BOOK:RMMC; PAGE : RMMC_0001 or w/e".
2. "take possession of harold / asa w/e and make sure they have movement animations" (Harold is not animated like the Minecraft guy).
3. "leave talking to terumon and asa. they will help him, they aren't npc's (creepers/farm-animals/villagers) but we will do npcs later. just focus camera on asa and make sure he can mine walk, jump, place and its all animated, and if we take control of him possessing with xelector, we can make him do the same. we wont act while we act..."
4. "i basically want u 2 drive him to do a frame by frame recreation of /home/no/Desktop/github/work/XO/31.Nov/nov-25/3.rmc-expected".
5. "look at the work grok DID do ... its colors and mini map colors are off ... its rmmc attempt uses flats alot instead of trying 2 mirror the rpg maker levels like we have examples of in many petslike games or the sp levels".
6. Later: "taller mountains, lakes, snow / sand biomes even in the small space"; "make a harness to drive him automatically. dump the frames and do narration as per 'presentations'"; "rpg-space has example of 2d/3d tiling that looks good and makes sense?"
7. Standing rules that apply to all of it: every rule is an op behind an event page (D92); the ledger is the only authority (no ghost data); a harness that can fail, with negative controls; real photographs only; mp3 beside every mp4; do not touch the owner's live editor without OK and always restore by hash; free models only; heavy work through `cpu_gate`; commit only my own paths.

"We wont act while we act" is read as: **one controller at a time**. While a harness (or a human possessing Asa) is driving, no second writer may append rows for Asa. That is a seat rule (section 9.4), not a new mechanism.

---

## 2. The reference video, beat by beat (SAW: a contact sheet of `/home/no/Desktop/github/RMMC.mp4`, one frame every 5.5 s)

Facts of the video (RAN `ffprobe`): 648 by 512, h264 + aac, 93.39 s, 60 fps stream. The picture is a third-person chase view of a Minecraft-style player on a grass field, with a Japanese HUD (SANDBOX, "置くブロック: 土" = block to place, X Y Z coordinates, "描画: DLL (ms)" = renderer and its cost, "動き 60 回 / 秒" = updates per second) and a key legend at the bottom (方向: 歩く = direction: walk, A: ジャンプ = jump, X: こわす = break, Y: 置く = place, B: ブロックを選ぶ = pick block, LB / RB: カメラを回す = turn camera, 右スティック: カメラ, Start: 最初の場所へ = back to the start, Back: 描画を DLL / JS に切り替え = switch renderer).

| # | t (s) | What happens (SAW) | What we need to show it |
|---|---|---|---|
| 1 | 0 | Walk on grass; an oak with a log pillar at the right; terraced grass blocks | walk on grass, an oak (leaves + log), terraces |
| 2 | 5.5 | Walk onto a sand patch; the white placement cursor (a wire square) sits on a block ahead; a dirt block lies on the grass | sand beside grass, a cursor |
| 3 | 11 | A lake with a sand shore; the player walks along the shore | water, sand rim |
| 4 | 16.5 | A dirt block is placed or removed ahead of the player; block to place shows 土 (dirt) | mine and place, dirt |
| 5 | 22 | A one-block-wide trench of dirt walls; the player walks into it | dig a trench, walk in it |
| 6 | 27.5 | Camera zoomed into the player's face (huge pixels: skin, brown hair, a blue eye) | a close camera and a faced head (the rig has no face yet, section 12.3) |
| 7 | 33 | Past the oak, a log pillar | oak, log pillar |
| 8 | 38.5 | The trench again from behind | trench |
| 9 | 44 | Inside the trench: dirt walls fill the view, player walks forward | camera inside a trench |
| 10 | 49.5 | The player places dirt steps; the cursor shows the target | place a dirt block |
| 11 | 55 | "置くブロック: 木の板" (plank) is now selected; snow and a stone pillar behind | pick block, place plank, snow, a stone pillar |
| 12 | 60.5 | A plank placed next to the stone pillar and snow | plank next to a pillar |
| 13 | 66 | Planks seen from above (camera pitched down), another plank placed | camera pitch |
| 14 | 71.5 | The HUD now reads JS (7 updates per second): the other renderer; the player walks | (we do not copy the JS renderer; section 12.5) |
| 15 | 77 | Walk past a dirt block pile | walk |
| 16 | 82.5 | Walk, a cursor at the feet | walk |
| 17 | 88 | Walk, place a plank near the bottom of the frame | place a plank |

Three more facts from Grok's stills in `3.rmc-expected/` (SAW): `3-trench-dirt-walls.jpg` and `4-stone-pillar.jpg` are the same family; `6-pillar-and-snow.jpg` shows that the snow is a white block on top of stone and the pillar is stone, not wood.

**Honest reading of "frame for frame":** the reference runs 60 updates a second on a Windows machine and its renderer is a different program. We cannot reproduce its pixels. What we reproduce is the **sequence of actions and shots** at the same order and approximate time, in our own engine, with our own blocks. Section 11.3 says how many frames and why.

---

## 3. What is already built and SEEN (this session; every row has evidence)

| Piece | Evidence |
|---|---|
| Page `rmmc-0001`: the Centroid island (book centroid, page small, 72 by 52) at 3x = 216 by 156, no door/window/roof events | RAN `rmmc_scale_op` (72x52 -> 216x156, 33,696 cell tokens counted); commit `9af8f7b59` |
| Authored biomes as DATA (`H/@.apps/rmmc/biomes.pdl`): snow-capped stone range (peak H=27 vs plain grass H=3), two sand-rimmed lakes, a sand desert, a beach ring, a dirt trench, trees; 13 Mineclonia block textures | RAN `rmmc_biome_op`; harness `rmmc_biomes` 44 PASS (re-run by me, 44/0), 7 negative controls red |
| The island in the real engine: stone mountain with grass and dirt layers, an oak, terraces, the island floating over the void; the minimap in biome colours (not grey) | SAW `XO/.../5.rmmc-live-proof/2-...`, `3-...`, `5-...` |
| The minimap top-face fix (`bv_cell_top_slot`) | commit `d3f2a5dfc`; harness `minimap_top_face` 9 PASS; SAW the live minimap |
| The box-limb rig (`bv_rig.h`; an event whose charset starts `rig:` is six boxes; pattern 0 idle, 1/2 walk, 3 jump, 4 mine, 5 place; directions 2 S, 4 W, 6 E, 8 N) | commit `d8dc54597`; harness `bv_rig` 35 PASS; SAW all six poses live (`1-asa-poses-idle-walk-jump-mine-place.png`) |
| `event_lift=1` in the island's `cells.txt` header (an event stands ON its column, otherwise it is buried) | commit `7b02ec438`; SAW Asa standing on grass |
| The In: tab engages the board again | commit `ed40d8479`; SAW live: nav 1 + Enter gave `typing=1`, label ON, INT on; second press off |
| The editor was restored after every test | RAN: book `rpg-space` / page `bridge`, `typing=0`, `flag_int=0` |

---

## 4. What is NOT built (the gaps this plan closes)

1. **Asa does nothing by herself.** The pose is a hand-written number in `events.txt`. No verb, no ledger, no walking. (SAW: I wrote `... rig:asa 0 2 <pattern>` by hand for each photo.)
2. **No mine and no place.** The rig only swings an arm. No block is removed or added. A harness must see the column change and the bag change.
3. **No camera that follows Asa.** The engine camera is moved only by keys (`q e c v w a s d 1-4 f ;`); `pan` is relative to the selector cell. A chase camera has to be done as a recorded key sequence per beat (section 11.2) or as a small engine feature (section 12.4, OPEN).
4. **No face on the rig** (`RIG | face` is parsed, not drawn), so beat 6 (the face close-up) cannot match.
5. **The vox foundation does not fit the island as it is** (section 5.2: 64 by 64 projection limit, 32 terrain boxes).
6. **No recorder.** Nothing yet sends a beat, waits for the engine, dumps a frame and assembles a video.
7. **Terumon and NPCs** are out of scope (owner: later). Asa does not talk.
8. Grok's `rmmc-0000` (a 64 by 64 coordinate-painted field, its `rmmc_field_op.c`) stays as it is. It is the wrong shape (section 5.1) and the new work does not build on it.

---

## 5. The design-choice test (AGENTS.md: answer the five questions before building)

### 5.1 Why not extend Grok's `rmmc_field_op` or `game_core`

One authority per fact: its map is magic coordinates (`cell_of`, x>=40 && y>=40 is sand), so the "map" is a function in C. Modular: no. Scalable: a second biome is a new `if`. Long term: the owner said "mirror the rpg maker levels", the rpg-space shape (an authored glyph map + legend + extrusion + cells + atlas, one desk for 2D and 3D, READ `H/@.apps/rpg-space/.../bridge/`). House shape: data. **Rejected; the island is now data (`biomes.pdl`).**

### 5.2 Where Asa's rules live: three options

The house already has the foundation: `H/@.apps/vox/` (D105, D112, D115): ONE append-only ledger; the world is the FOLD of its rows over a seed terrain; verbs `give move mine place eat sleep wait status cell inv verify replay`; numbers in `rules.pdl` / `blocks.pdl` / `terrain.pdl`; event pages `vox_move vox_mine vox_place vox_eat vox_sleep vox_wait`; keys in `keys.pdl`; the `act` verb turns a key into a verb. (READ `H/@.apps/vox/ops/vox_op.c` header and `keys.pdl`.)

READ limits that matter here (these are the nuances that decide the option):

- `DMAX 64`: the desk projection (`project()`, vox_op.c:309) refuses a world wider or deeper than 64 cells (`WX > DMAX || WZ > DMAX` returns 2).
- `MAXX 32`: at most 32 terrain boxes. The island has 17,460 land columns with per-column heights, so it cannot be written as boxes.
- `MAXC 4096`: at most 4096 changed cells and 4096 damage rows. That is plenty for a play session (it holds only mined and placed cells, because the terrain is looked up lazily in `kind_at`).
- `kind_at` (vox_op.c:98) scans changed cells then boxes, returns "air" otherwise: there is exactly one place to teach it a second terrain source.
- vox's coordinate frame: `x`, `y` (VERTICAL), `z` (board row). Our desks: `x`, `y` = row. So `vox z` = desk row and `vox y` = height. This must be written once and tested, it is where a mirrored world comes from.
- The vox event pages carry fixed arguments (`vox_move` is literally `... move ... 'joe' '4,3,3'`), so they are demonstrations, not a play path. The play path needs a variable argument: the `GAME_ARG` pattern of `tear_on_verb` (D123).

| Option | One authority | Modular | Scalable | Long term | House shape | Cost / risk |
|---|---|---|---|---|---|---|
| **A. Extend `vox_op` in place:** a `HEIGHTMAP` terrain source (a CONFIG file of column runs) in `kind_at`, raise `DMAX` to cover a play window, keep one ledger and one set of rules | yes: vox is the one voxel authority | the heightmap reader is one function | a second world is a second CONFIG | the owner's "mine place walk jump" is exactly vox | ledger + data + event pages | edits a shared op (`vox` has its own harness: must stay green); the 64 limit forces a PLAY WINDOW of the island |
| B. A new `rmmc_op` that copies vox's rules | no: two voxel rule sets | no | no | no | op | duplicates D105; every later fix twice |
| C. Lift vox's pure fold into a text-include (`vox_core.h`) used by `vox_op` and `rmmc_op` | yes | best | best | best | text-include (house: pure module, 2+ consumers) | a refactor of a 440-line op before any visible result; do it AFTER A proves the shape |

**Recommendation: A now, C later if a third consumer appears.** The deciding reason: one authority and the smallest change that gives the owner a visible result; A does not close the door on C. Add decision-log row **D137** (this section) before the build.

The PLAY WINDOW (OPEN, my proposal): the island does not need to be playable end to end for the video. A 64 by 64 window `x 130..193`, `y 70..133` (from SAW `4-island-top-down-biomes.png`: it holds the east lake with its sand rim, the dirt trench, the grass plain and the foot of the stone range) covers beats 1 to 5, 7 to 13, 15 to 17. Snow (beat 11, 12) lives on the range further north-west: if the window must show snow the window moves to `x 110..173, y 40..103`. The decision is made by the beat script (section 9.3) not by guessing: the script lists the cells it needs and a harness case fails if one is outside the window.

---

## 6. The architecture in layers (one authority per layer, nothing read back)

```
 biomes.pdl (CONFIG)  --rmmc_scale_op / rmmc_biome_op-->  rmmc-0001 desk  (seed: cells.txt, map.txt, extrusion, atlas)   [exists]
                                    |
                                    v   rmmc_heightmap_op   (new: columns of the SEED desk -> vox HEIGHTMAP config)
 vox/terrain (CONFIG: heightmap)  +  vox rules.pdl / blocks.pdl / keys.pdl (rmmc skin)
                                    |
 beats.pdl (DATA)  --rmmc_beat_op-->  verb pages (events/<verb>/...)  --rmmc_verb-->  vox_op   (APPENDS ledger rows)   [the ONLY authority]
                                    |
                                    v   rmmc_project_op  (fold ledger over seed -> projection; never read back)
 rmmc-0001 desk PROJECTION: cells.txt (changed columns), events.txt (rig:asa row: x y r g b rig:asa 0 dir pattern), cells.marker (GROWS)
                                    |
                                    v   engine (bv_render_3d, bv_rig.h): draws terrain + six boxes, re-reads events every frame
 rmmc_record (harness driver) --keys--> engine camera, --dump--> PNG per step --> ffmpeg --> mp4 + mp3 (Kokoro) + FACELOG row
```

Two desks, on purpose: the SEED desk file set (cells.txt etc. as authored by the biome op) is a CONFIG; the PROJECTION that the engine reads is written to the SAME folder today, so before the build we **split them**: the authored seed is kept as `rmmc-0001/seed/` (CONFIG) and the folder the engine shows is the projection (safe to delete, regenerate byte for byte). Otherwise "mined" cells could be mistaken for the island and the no-ghost-data rule is broken (a projection read back as input). Harness case: delete the projection, regenerate, bytes identical.

---

## 7. The ledger and the verbs (house events)

### 7.1 The ledger

`rmmc/vox_ledger.txt` (one file, append only; path declared in `state.pdl` as LEDGER; the live house keeps vox's `#.desktop/vox_ledger.txt` separate: rmmc must NOT share it, a different path per skin, set by the verb page's environment, D123 `GAME_ARG`/house derivation). Rows are vox's own: `GIVE MOVE DAMAGE MINE DROP PLACE EAT TURN DAY SLEEP SPAWNPOINT`. New row kinds, only if the harness shows they are needed: `FACE | actor | dir` (a turn on the spot has no cell change: without a row the facing is lost on replay) and `JUMP | actor | dz` (or a `MOVE` with `z+1` then `z-1`, DESIGN choice in 7.3). Every mutating verb runs `verify` first, so a hand-edited ledger is refused (exit 14).

### 7.2 Verbs (the manifest `H/@.apps/rmmc/verbs.pdl`, lowercase; each is one event page)

| Verb | Args | Does | Refusal (exit, nothing written) |
|---|---|---|---|
| `spawn` | cell | first MOVE of the actor (vox: first move is a spawn); GIVE tools and blocks with a stated reason (`why`: "scratch seed for the recording") | in the solid, out of the window (11) |
| `walk` | n e s w | MOVE one cell, ground-follow: the vox move is x y z, our walk resolves the target z from the heightmap (the column top + 1, at most RULE `step_up` higher) | into a wall higher than a step (18), into water deeper than 1 (18, see 7.5), out of bounds (11) |
| `face` | n e s w | FACE row (no turn) | unknown direction |
| `jump` | - | a hop: MOVE up then down in one atomic write (both rows or none) | no ground under the feet (15) |
| `mine` | dir (the cell in front, or `down`) | DAMAGE rows until `progress >= hardness` then MINE + DROP; tool from the bag | reach (12), air (13), tool power (17), bag full (21) |
| `place` | item, dir | PLACE row (removes one item); support rule on | item missing (16), no support (15), occupied (13) |
| `pick` | item | selects the block to place (the HUD "置くブロック"): a PREFERENCE row (it changes no outcome), kept in `play/layout.txt`-style preference, not in the ledger | unknown item |
| `reset` | - | "Start: 最初の場所へ": MOVE back to the SPAWNPOINT | none |

Each page is generated by `H/&.widgits/events-hq/ops/+x/verbgen.+x gen` from the manifest and verified by `verbgen check` (the tear-on and tearit-map shape, commits `0886102a7` and the D123 work: pages `events/<verb>/event_pkg/pages/page_1/{event.ir.pdl,condition.pdl,cmd_1.sh}`, registry `COMMAND rmmc_verb` in `H/#.ref/menu/event_commands.registry.pdl`, engine `ops/rmmc_verb.c` which reads `GAME_ARG`, derives the house, runs the op, writes `play/result.txt`, exits 2 on an unknown verb or a bad argument). Played by `bash H/&.widgits/events-hq/ops/play_event.sh <pkg> <house> ui` with `GAME_ARG` and `MUCHI_TARGET_ENT`.

**Blueprintz pause (nuance, RAN earlier this session):** `H/#.desktop/blueprintz_debug.txt` reads `paused=1`: that is the OWNER's debugger switch, it only QUEUES page runs. Rules: the harness sets `BLUEPRINTZ_DEBUG_FILE` to a scratch file (so pages run in a test); the live driver skips the page and plays the verb directly with a visible message when the pause is on (tear-on did exactly this); I never clear the pause. The recorder (section 11) uses the direct path and says so in its log, or the owner clears the pause for the recording: OPEN.

### 7.3 The pose is a projection, with an expiry (the one place time enters)

The engine has no timer for poses: it draws what `events.txt` says, every frame. So:

- `rmmc_project_op` writes the pose from the LAST ledger row by a data table `H/@.apps/rmmc/pose.pdl` (`POSE | last_row | pattern`): `MOVE` ground: 1 then 2 alternating per step (the legs swing is `(frame + (pattern==2)) & 1` at a 140 ms step of the wall clock, `bv_rig.h`); `MOVE` with up: 3; `DAMAGE`/`MINE`: 4; `PLACE`: 5; anything older than `idle_after_ms`: 0.
- The ledger has no clock, so **expiry is the driver's job**, not a ledger row: the recorder or the live driver re-projects with `--idle` after the beat's seconds, writing pattern 0. This is a projection write (safe, derived), never an input.
- Direction is the last move delta (2 S, 4 W, 6 E, 8 N), or the last FACE row.

### 7.4 Keys and possession (the human path; "if we take control of him possessing with xelector")

READ: the desk token is a row of `events.txt` (D101); a possessed piece forwards keys; `vox_keys_op` turns a key into one op call from `keys.pdl` (arrows 1000-1003, `l` mine, `p` place, `u`/`j` up/down, `k` eat, `b` wait, `n` sleep), and `vox_keys_op keybinds` writes the window's GAMEKEY rows. The rmmc skin gets its own `keys.pdl` with `space` jump, `x` mine (Minecraft X), `y` place, `b` pick, arrows walk, `q`/`e` camera (the engine already owns `q e`). The human recipe is the one in the presentations standard section 11: **In: ON, put the cursor on Asa, Enter (HUD shows `poss <id>`), then ONLY keys**. NOT PROVEN for a rig actor: possession was proven for pieces (`hero_01`) and for the vox token; whether the xelector can possess an `events.txt` rig row is a case to run (gate G6), and if it cannot, the human path is `tab` into the token like vox does (D101).

### 7.5 Rules as numbers (nothing in C)

`H/@.apps/rmmc/vox/rules.pdl` (copy vox's, then change): `world_*_max` (63 for the window), `reach 4`, `move_max 1`, `step_up 1`, `fall_max`, `turns_*`, hunger OFF (`hunger_per_turn 0`: the video has no hunger; a rule value, not a code branch), `place_needs_support 1`. `blocks.pdl`: `BLOCK | grass | tool hand/shovel | hardness | drops dirt`, `dirt`, `stone` (pick), `sand`, `snow`, `log` (axe), `plank` (placeable), `leaves`, `water` (not solid; walking into deep water refused for now); PICTURE rows map each block to the atlas tile that `rmmc_biome_op` already built (14 tiles, slot numbers in `rmmc-0001/legend.txt`): the projection must use those slots, not vox's flat colours. Hardness values chosen so a dirt block takes ONE swing (the video digs fast) and stone takes more.

---

## 8. Data files the build adds (all CONFIG or DATA; the ops hold no number)

| File | Kind | Content |
|---|---|---|
| `H/@.apps/rmmc/vox/terrain.heightmap` | CONFIG (generated) | `COL | x | z | ground_y | kind_top | kind_below | water_level`, from the seed desk (heightmap of the window) |
| `H/@.apps/rmmc/vox/{rules,blocks,keys}.pdl` | CONFIG | 7.4, 7.5 |
| `H/@.apps/rmmc/pose.pdl` | CONFIG | 7.3 |
| `H/@.apps/rmmc/verbs.pdl` + `events/<verb>/` | CONFIG (generated) | 7.2 |
| `H/@.apps/rmmc/beats.pdl` | CONFIG | section 9.3 |
| `H/@.apps/rmmc/state.pdl` | declaration | LEDGER = the rmmc vox ledger; PROJECTION = the desk folder, `play/result.txt`; PREFERENCE = the picked block and the layout; CONFIG = the files above (the `audit` verb must pass: an undeclared file fails) |
| `rmmc-0001/seed/` | CONFIG | the authored island (moved from the desk folder, section 6) |

---

## 9. The beats, the recorder, and how "we won't act while we act" is kept

### 9.1 One controller (a seat row)

`SEAT | asa | harness` or `SEAT | asa | human`: a row in the ledger (the Teropoly rule: a driver change is a row). The beat op refuses to append while the seat is `human` (exit 24, nothing written) and the live driver refuses while it is `harness`. Switching is a verb (`seat <who>`).

### 9.2 What a beat is

`H/@.apps/rmmc/beats.pdl`: `BEAT | n | seconds | verb | arg | camera | note`. One row per action in the order of section 2, e.g. `BEAT | 4 | 1.2 | mine | e | wwss | dirt block ahead (ref 16.5 s)`. `camera` is a key string for the engine key file. `rmmc_beat_op <house> <beats.pdl> next|all|<n>` plays the next row through the verb page (direct op as the visible fallback), prints the row and the seconds to wait, exit 0, or the refusal code. The case `beats-all-play` fails if ANY beat is refused, if a beat needs a cell outside the window, or if a beat mines or places something the bag does not hold (a missing GIVE).

### 9.3 Choosing coordinates from the island (not from guesses)

The op `rmmc_beat_op check` resolves every beat's cells against `legend.txt`: beat 3 must stand on a cell whose neighbour is `water`, beat 5 must start next to the trench glyph, beat 11/12 next to a stone cell, beat 2 on a `sand` cell. If the island lacks a feature the check says which beat and why, and the fix goes into `biomes.pdl` (data), not into the beat.

### 9.4 The recorder (a harness driver, not a script in the repo)

A C op `rmmc_record_op` (so no shell) does, per beat row:
1. play the beat (verb page or fallback) and project;
2. send the camera keys with `bv_keys.+x <session> <keys>`, then the nudge key `~` (the engine only draws on a frame request; RAN: the first frame after a desk change needs it);
3. wait for the engine: poll the frame marker / `cells.marker` size and the frame file mtime? **no, never mtime**: poll the frame counter file the engine writes (`.bv_dispatch_3d_ms` changes) up to a timeout;
4. grab frames with `dump_frame_png_op.+x <window id> out.png` (RAN: 0.12 s per grab, 1365 by 934 window, board area 951 by 773 at window offset 208,107), N frames per beat at the pose steps (section 11.3);
5. write one `rec_log.txt` row per frame: beat, step, seconds, file, md5. The video assembly reads the log, never the folder listing.

Window id: found by size with `xwininfo -root -tree` (khtpm windows have no X name, house note; RAN: `0xc00003`, 1365x934).

---

## 10. The camera (the nuance the owner will notice first)

RAN and SAW today: the engine camera has NO follow mode. Keys: `q`/`e` yaw -10/+10 (two keys per 20 degrees: LB/RB of the reference), `c`/`v` camera level, `w a s d` pan, `r`/`t` pitch (clamped -80..-25, SAW pitch behaviour in the pchq-3d skill), `1`..`4` POV presets, `0` toggles 2D, `f` resets, `;` snaps. Pan is relative to the SELECTOR cell (`pos 8,8` here), so to look at cell (140,100) I sent `d` x132 and `s` x92 (RAN). A mode key (`2`) RESETS the pan to its preset (SAW: after `2` the camera sat in empty sky at cell 8,8).

Therefore a chase camera is, in this plan, **a per-beat recorded key sequence**, computed by the recorder from Asa's cell: `d`/`a` and `s`/`w` presses = (asa cell - camera cell) after each walk, then the beat's own yaw keys. The recorder keeps the camera cell as its own state, read back from `cam_pan_x/z` in `bv_state.txt` (RAN: those values change as keys arrive), and corrects.

The cheaper and cleaner fix is an engine feature: **`cam_follow=<piece id|event name>` in `arrow_config.txt`** (the way `default_yaw` was added, D96): the camera pans to the named event each frame. That is a small engine change, in `bv_render_3d.c`/`bv_menu_input.c`, which already carry other agents' uncommitted hunks: stage by hunk (done twice today). **OPEN (owner): build `cam_follow` (about 40 lines, harness case reads `bv_state`) or keep the recorded key sequence.** My recommendation: build it, because a key sequence drifts (a refused walk leaves the camera ahead of Asa) and the drift is exactly what makes a recording look amateur.

Other camera beats: beat 6 (face close-up) = POV `1` (first person) turned around is NOT a face view: we need the camera 1.5 blocks in front of the head: `c` x? plus pitch 6, yaw 0 relative to Asa: tried in the recorder (gate G8); if the engine cannot put the camera that close, beat 6 is shown as a crop of a high-resolution frame (a nearest-neighbour zoom of the head, like the reference's own pixel look). Beat 13 (from above) = `3` top-down (RAN: mode 3 sets pitch -90).

---

## 11. Recording, frame count and the video

### 11.1 Environment protocol (the owner's editor)

The live editor is the owner's. One relaunch was done today with his OK (RAN) and the editor was restored. Each recording session: record book/page/state by hash to `/home/no/tear-live-backups/<date>-<label>/` first (`MD5SUMS`), run, restore, verify, and say the state of In:/INT. `cpu_gate` before anything heavy; the engine idles out after 270 s without a frame request (not a crash): the recorder nudges. No recording while the owner is typing in the window: ask. A scratch window instead of the live editor is the alternative the owner declined today (OPEN again for the long recording).

### 11.2 Why not screen-record

A live screen recorder would capture the owner's whole desktop and the editor chrome, and the beat timing would depend on the machine load (RISK: the guard kills at heat). Stepped capture is deterministic and cheap (0.12 s per grab).

### 11.3 Frames: how many, how timed (DESIGN, to be measured at gate G9)

The reference is 93.4 s. Our video keeps the SAME BEAT ORDER and duration: 17 beats of about 5.5 s. Per beat, the recorder produces a short burst: the 140 ms pose step of the rig means a walk shows two poses 140 ms apart: capture at **8 frames per second of video time** during motion (a frame every 125 ms of beat time, matching one rig step), 2 frames per second while idle. Estimated 93.4 s x about 6 average = about 560 PNGs, about 80 s of grab time plus engine wait (RISK: each frame needs the engine to re-render after the ledger/projection change: the earlier photos used 3.5 s sleeps, which would be 33 minutes; the recorder polls the engine's frame counter instead, expected under 0.5 s per frame: measure it at G9 and report the real number, do not assume it). The mp4 is assembled at 12 fps (so it runs at about the reference's length) and the file's duration is checked with `ffprobe` against the sum of the beat seconds.

### 11.4 Crop and size

The board area is 951 by 773 inside the 1365 by 934 window; the reference is 648 by 512 (1.266:1, ours 1.230:1). Crop to 951 by 751 then scale to 648 by 512 (nearest neighbour for block pixels), no editor chrome, no `pid` HUD line (the engine HUD text at top-left is turned off by the board's HUD option if one exists, OPEN: otherwise the crop starts below it? it is inside the board area: we blank it in the crop or accept it as our own "HUD" with the position in Japanese like the reference: the nicest answer is to supply our own HUD rows via `game_ui.txt`).

### 11.5 Narration and the presentation standard

Read `BOOK/!.PRESENTATIONS-THE-ONE-PLACE-STYLE-TOOLS-AND-HOW-TO-2026-11-18.md` again before assembling. Rules to apply (READ in earlier sessions this week): one scratch script per video; manifest rows "Japanese subtitle ## English voice" (voice and subtitles in different languages, never the same; make a pair: owner rule); real frames only; Kokoro narration on debil (heavy work offloaded; `cpu_gate` + `nice`); `make_presentation_video.py` run through `cpu_gate`; `ffprobe` + `volumedetect` before shipping; **mp4 AND mp3 together** into `XO/31.Nov/nov-25/6.rmmc-recreation/`; a FACELOG row in the presentations standard (faces rotate: next face after People1); the NIGHT note names the video. The narration script lists the 17 beats in the reference's order with what the viewer sees, no claims the footage does not show.

---

## 12. Nuances and traps (each one from a real observation or a real read)

1. **An event is buried without `event_lift=1`.** (SAW, fixed.) Any new page built by `rmmc_biome_op` carries the header; the projection op must keep it.
2. **`cells.marker` size, never mtime** (house rule; the engine reloads a desk only when the marker's SIZE changes). The projection grows it by `(old % 4000) + 1` bytes on every write. A projection that does not grow it shows nothing (RAN earlier today with the tear-on marker).
3. **The rig has no face** (`RIG | face` parsed, not drawn) and **no skin textures on limbs**: beat 6 cannot match until a face is drawn: a small addition to `bv_rig.h` (eyes/hair as extra thin boxes on the front of the head, no texture), OPEN, after the first full recording.
4. **128 boxes per scene** (`BV_GPU_MAX_BOX`) is shared by terrain boxes and rigs: one Asa is fine (6 boxes); Terumon would add 6 each: still fine for a handful.
5. **The JS renderer beat (14) is not copied.** Our engine is one renderer; the beat shows a slower-update moment by a lower frame rate (capture at 2 fps for that beat) with a caption, not by faking a different renderer.
6. **Water:** the island's lake cells are sunk below grass with a sand rim; vox treats water as non-solid. Rule: walking into water deeper than 1 is refused, shallow edge allowed (needs the `water_level` column in the heightmap).
7. **The 3D board has no inverse-view of "mine ahead".** The cursor (the white wire square in the reference) is the engine's xelector box; to show the target the recorder moves the xelector to the target cell with its own keys (`z`/`x` level, arrows need In:): NOT PROVEN, OPEN; fallback: draw the target with a second event row `cursor` (a thin box) projected by `rmmc_project_op` from the last mine/place target.
8. **Pose swing at 140 ms is wall-clock.** A frame dumped 3 ms after another can show the same step: that is correct; the recorder sleeps 140 ms between the two walk frames of a step.
9. **The projection of mined cells is a column edit, the engine draws columns.** Mining a block under a column top lowers the column by one; mining a side block of a cliff (a wall of dirt in a trench) needs the cell format to carry two runs (`floor,wall,depth` has one wall slot): trench walls are done by lowering the floor of the trench cell, which is what the reference shows (a one-wide dug channel). Overhangs/tunnels are NOT supported (RISK; stated, not hidden).
10. **No Python, no new `.sh`** (house): the recorder, beat op, heightmap op, projection op, verb wrapper are C; assembly is `ffmpeg`/`ffprobe` (external programs are fine), the video script is the one scratch script per video the presentations standard allows.
11. **`git`:** commit only my paths, by pathspec; `bv_render_3d.c` has another agent's uncommitted hunks: stage mine by hunk (the method used today: `git diff -U0`, build a patch of my hunks, `git apply --cached --unidiff-zero`). No push without the owner's word.
12. **Blueprintz photo of the new pages** is possible only when the owner clears the pause; until then the proof of the pages is `verbgen check` + the harness case + their files.

---

## 13. Steps and gates (each gate is a proof that can fail; I do not start step n+1 before gate n)

| Gate | Step | Proof (harness case or observation) |
|---|---|---|
| G0 | this document, D137, index row; the owner reads and OKs the open decisions below | the owner's word |
| G1 | `vox` HEIGHTMAP terrain + raised limits; vox keeps its own harness green; a new case `vox_heightmap`: same ledger over the same heightmap twice = identical bytes; a column's top follows the file; a cell outside the window refuses (11) | vox harness count before/after, new case, 2 mutations red |
| G2 | `rmmc_heightmap_op`: seed desk -> `terrain.heightmap`; case: every land column of the window appears once, heights equal the desk's, water cells flagged | case, mutation: one column changed -> red |
| G3 | `rmmc_project_op` + the seed/projection split; case: delete projection -> regenerate identical; marker grows each write; pose per `pose.pdl`; `event_lift=1` kept | case, mutations |
| G4 | verbs + pages (`verbgen check` PASS) + `rmmc_verb`; refusals write nothing | case |
| G5 | `beats.pdl` + `rmmc_beat_op check/all`: every beat resolves against the legend and plays without refusal | case; negative: a beat on air refuses |
| G6 | possession proof: can the xelector (or the token tab) possess the rig actor and move it with keys? | one live proof, SAW + `bv_state` rows; if not: the token path of D101 |
| G7 | `cam_follow` (if the owner OKs) with a case reading `bv_state` | case + SAW |
| G8 | live rehearsal of beats 1 to 5 in the editor: PNG per beat, I open each and compare to the reference frames side by side | SAW, honest list of mismatches |
| G9 | measure real seconds per frame; decide the frame schedule; full recording | `rec_log.txt` with counts; `ffprobe` duration vs the sum of the beats |
| G10 | narration, video, mp3 beside it, FACELOG row, NIGHT note, XO copy; editor restored by hash | files in XO nov-25, `MD5SUMS` verified |

## 14. Harness cases that will exist (names fixed now so they can be grepped later)

`vox_heightmap`, `rmmc_heightmap`, `rmmc_project`, `rmmc_verbs`, `rmmc_beats`, `rmmc_pose`, `rmmc_seat` (a second controller is refused), `rmmc_audit` (no undeclared file), plus the existing `rmmc_biomes` (44), `bv_rig` (35), `minimap_top_face` (9).

## 15. Open decisions for the owner (I will not guess these)

1. **Play window** (section 5.2): a 64 by 64 window of the island (x 130..193, y 70..133 by default) or the vox limit raised further so the whole island is playable (a bigger change to `vox_op`).
2. **`cam_follow` engine feature** (section 10): build it, or recorded key sequences.
3. **Blueprintz pause** during the recording (section 7.2): keep it (direct path with a visible message) or clear it for the recording.
4. **Face on the rig** (section 12.3): draw a simple face now so beat 6 can match, or after the first recording.
5. **Terumon cameo:** none in this video (the owner said later). Say if one should stand in the background.
6. **Long recording on the live editor** or a scratch window (CPU).
7. **Publishing:** the video goes to XO only; the repo gets the plan, ops and cases. No push without the owner.

## External references

- Reference video: `/home/no/Desktop/github/RMMC.mp4`; stills: `XO/31.Nov/nov-25/3.rmc-expected/`.
- Seen today: `XO/31.Nov/nov-25/5.rmmc-live-proof/` (poses strip, island third person, mountain layers, top-down biomes, minimap).
- Code: `H/@.apps/rmmc/` (biomes.pdl, ops/rmmc_scale_op.c, ops/rmmc_biome_op.c), `H/@.apps/vox/` (the foundation), `H/&.widgits/board-viewer/ops/bv_rig.h`, `H/&.widgits/board-viewer/ops/bv_render_3d.c`, `H/@.apps/piececraft-hq/ops/pchq_board_action.sh`, `H/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x`, `H/&.widgits/board-viewer/tools/+x/bv_keys.+x`.
- Docs: `BOOK/16.game/BOX-LIMB-RIG-FOR-WALKING-CHARACTERS-2026-11-25.md`, `BOOK/16.game/EVENT-VERB-PAGES-DESIGN.md`, `BOOK/16.game/TEARIT-MAP-GROK-WORK-REVIEW-AND-FIX-PLAN-2026-11-25.md`, `BOOK/16.game/TEAR-IT-RANCH-ARENA-TEAR-OFF-TEAR-ON-HOW-TO-USE-2026-11-24.md` (the verb-page pattern, D123), `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D92, D105, D112, D115, D123, D96), `BOOK/!.PRESENTATIONS-THE-ONE-PLACE-STYLE-TOOLS-AND-HOW-TO-2026-11-18.md`.
- Skills: `pchq-3d-board-driving` (camera keys, traps), `rpg-space-board-driving` (launch and photograph).

---

## 16. UPDATE 2026-11-25 (later): owner answers, two findings that change the plan, and the first slice

**Owner answers (his words):** "u can raise limit. look at world. search code for chase camera or 3rd person mode or w/e if none we will make a plan. lets get something going and see where we are from the [start]. most important is asa as mc character (and any other rmmv sprite able to use same converter) and walking animations etc as presentation. later breaking placing. and driving from 'human' as well as ai."

- Open decision 1 (window): **raise the limit**, do not cut a window (vox `DMAX`, terrain boxes become a heightmap, see G1). The whole island is the world; if a limit makes that impossible the worker reports the measured limit, it does not silently cut.
- Open decision 2 (camera): **resolved by reading code, no engine feature needed.** RAN/READ: camera mode 2 IS a third-person chase with a spring arm (`bv_render_3d.c` build_camera mode 2, `tp_collide`, `tp_*` framing keys in the camera config) and it anchors on the xelector (`g_xelector_present`, line ~3774). The xelector FOLLOWS a possessed game-driven token each frame: `desk_token_cell` (bv_render_3d.c:1202, D118) matches `possessed_id` against the name field of a row of the shown page's `events.txt`. So **possessing the `rig:asa` row makes the camera chase Asa.** `cam_follow` is dropped.
- Open decision 4 (face): later; first the converter (below) gives hair, sleeves and shoes, a face needs textured boxes.
- Priority order (owner): (1) Asa as the Minecraft-style character, **with a converter any RPG Maker sprite can use**; (2) walking animations as a presentation; (3) later break and place; (4) driving by a human and by AI through the same verbs.

**The human path already exists and is the one to reuse (READ, D97, D118, Teario P2):** a game-driven token is a row of `events.txt`; the engine's Enter on its cell sets `possessed_id`; keys listed as `GAMEKEY` rows in the book's `keybinds.pdl` (arrows 1000-1003, `jump` = 32) are then NOT the cursor's: the engine appends them to `<host>/../state/interact_relay.txt` as `<key> <ms>`, and the game's driver turns each into a verb (the tear-on / tearit driver shape). The AI path calls the SAME verbs (`rmmc_beat_op` or an agent policy). One verb set, two controllers, the seat row of section 9.1 says who may write.

**First slice (this is what the workers build next; everything else waits):**

- **Slice A, the converter and the character.** `rig_from_charset_op <charset.png> <char_index 0..7> <out rig_<name>.pdl>`: reads the FRONT standing frame (3 by 4 frames of 48 by 48 per character, 4 by 2 characters per sheet, the RPG Maker MV layout; names starting `$` are single characters) and samples regions (hair = top rows of the head that are not skin, face/skin, torso/chest, sleeves, trousers, shoes) into RIG colour rows; the rig gets the extra thin parts that make it read as a Minecraft player (hair cap on the head, sleeves, shoes), still axis-aligned boxes. Works for ANY sheet in `#.NNEST_ASSETS/rmmv-www-img/characters/` and `#.NNEST_ASSETS/tsots-characters/` (Asa and Terumon are looked up by name there). Proof: a harness case on a synthetic 144 by 192 test sheet with known colours (the op must return those colours), plus the real sheets listed, plus SAW photos in the live engine.
- **Slice B, Asa walks on the island by verbs.** G1 to G5 of section 13, minimum set: `spawn walk face jump reset` plus the projection with the pose rule; `keybinds.pdl` GAMEKEY rows for the human; the same verbs from `rmmc_beat_op`. Break and place stay in the manifest but are validated later (priority 3).
- **Slice C, the presentation** (me, live): possess Asa, chase camera in mode 2, walk the four directions on grass, sand, along the lake, jump, with Asa and three more converted RPG Maker sprites in a line-up; photos first, then the video per the presentations standard.

## Backup and the alpha page (2026-11-25, owner: "is there a way to back this one up by making a copy of the map like the other, because it's event based? then we can build on the iterate alpha")
Yes: the world is the FOLD of one ledger over a seed, so a copy is files, not code. What exists now (RAN, listing and sha256 checked):
- **Stable baseline (do not build on it):** git tag `rmmc-stable-2026-11-25` (commit 281b31af8, local, not pushed) plus `/home/no/tear-live-backups/2026-11-25-claude/rmmc-stable/` (`rmmc-stable-2026-11-25.tar.gz` of the maps folder, `vox/`, the rule files, the editor keybinds and the game ledger, with `.sha256`; and a copy of the UNCOMMITTED working `bv_render_3d.c` / `bv_rig.h`, which carry the rig branch and other agents' hunks that are not in any commit). The ledger is GITIGNORED (`state/games/rmmc/game/ledger.txt`): only the tarball keeps it.
- **Older page backup:** `page_2` (label `rmmc-0001-backup-v1`) was already a page copy backup; it predates the tree, skin and tutorial work.
- **Alpha page to iterate on:** page `rmmc-alpha` (label `rmmc-alpha-iterate`, a `cp -a` of `rmmc-0001` including its `seed/`, `desk_id` renamed in both `extrusion.pdl` files, registered as `desk_4` in `maps/rmmc/game.pdl`) and its own ledger `state/games/rmmc/game/ledger_alpha.txt` (a copy of the live ledger). To run a tool on alpha, give it the three overrides that rmmc ops already read: `RMMC_DESK=<maps/rmmc/rmmc-alpha>`, `RMMC_SEED=<that>/seed`, `RMMC_LEDGER=<.../ledger_alpha.txt>` (`rmmc_driver`, `rmmc_verb`; `rmmc_biome_op <biomes.pdl> <alpha seed dir>`, `rmmc_project_op project <alpha seed> <alpha desk> <alpha ledger> ...`). NOT verified yet: opening the page in the editor and playing it; the book-level data (`@.apps/rmmc/vox/rules.pdl`, `blocks.pdl`, `terrain.heightmap`, `biomes.pdl`) is shared by both pages, so a rule experiment on alpha changes the live page too until the vox data dir is also overridable (the driver builds `<book>/vox` itself: open item).
- **Not the right tool:** `desk_copy_op` copies livedesk PALS (dsr -> dsr-test), not a map page; `/home/no/staging/alpha` is a git worktree on `claude-alpha` from 2026-10-07 (610 MB, a month stale): keep it for engine C experiments after a refresh, not for data iteration.
