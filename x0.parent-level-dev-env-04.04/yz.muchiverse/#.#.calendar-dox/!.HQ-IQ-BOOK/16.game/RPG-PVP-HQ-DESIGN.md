# rpg-pvp-hq design - DRAFT 1, 2026-10-20

Status: **draft for the owner. Nothing here is built.** No folder `@.apps/rpg-pvp-hq` exists yet. Labels: SAID = the owner's words; READ = from files, not run; MINE = claude's proposal, needs the owner's yes.

## 1. What the owner wants (SAID, 2026-10-20)
- A 2D/3D shooter in the style of GoldenEye 64, played by AI or human **the same way** (like df-hq, cdda-hq, mcl-hq and the pets).
- Pets live in a **city**: go into buildings, shoot, find guns, reload. **Later:** build their own buildings and fortifications, like Fortnite.
- Question: Doom asset pack, cdda, or some of both?
- Also (SAID): an older GoldenEye project exists in the house, out of house standards, with data and functions worth looking at.

## 2. What exists (READ, not run)

| thing | where | what it gives | what it lacks |
| --- | --- | --- | --- |
| **007-goldeye** | `44.xyz.01.00/007-goldeye+01.00/src/main.c` (1,549 lines, one file, C + freeglut/OpenGL, tracked in git, a compiled binary beside it) | A finished deathmatch loop: 100x100x28 voxel island, 4 biomes, procedural multi-storey buildings with stairs/windows/roofs, 40 gun pickups, 3 weapons + fists, projectile bullets, tank + heli, 3 AI players, frag limit, split-screen | Its own GL window, state only in memory, no ledger, no reload, no sound, AI is direct pursuit. Not khtpm, not events. Not compiled or run by me today |
| **Doom palette + book** | `&.widgits/palettes/sprites/doom/` (64 flats, 64 sprite frames = one monster `BOSS` x 8 rotations x 8 frames); `@.apps/piececraft-hq/pieces/system/maps/doom/` (placeholder levels, 24-wide rooms, walls 3 high); brief `08-roadmap/design-docs/GROK-BRIEF-DOOM-BOOK-2026-10-09.md` | Freedoom (BSD-3) is the chosen source; `ASSETS.pdl` records url + sha256; sprites come with 8 rotations for billboards | The WAD itself is **not on disk** (I searched; `ASSETS.pdl` says it should be `#.NNEST_ASSETS/doom/freedoom1.wad`). Only 1 monster and the floors were extracted. Real-time shooting was left "not decided" in the brief |
| **cdda palette / cdda-hq** | 512-tile palette; `@.apps/cdda-hq`; `RULES-cdda-hq.md` | Top-down city tiles, walls, doors, furniture, guns and ammo icons; a **wound table by body part** (leg_l, torso... max `mhp/6`) and a one-zed turn | Top-down only (one angle per tile); turn-based |
| **gta-hq** | `GTA-HQ-DESIGN.md`, `@.apps/gta-hq` | A city island (Los Vientos), 37 GTA roles already mapped to real CDDA ids, interiors by door events, z-cell format, brixel parts, the three-distance detail op (proposed) | Not confirmed playable by anyone; car not drivable; detail op not built |
| **S1 event ledger** | `S1-EVENT-LEDGER.md`, `_shared-lib/game_events.c` | One ledger `state/events.txt`: `EVENT|ts|actor|verb|target|at`, resolver writes `EVRES`; `give_item take walk dig build craft` (+ `strike` in cdda-hq); `GE_RULE_HOOK` for per-game rules; replay = state | Verbs `walk dig build craft` are `todo:` in the shared code; no `fire`/`reload` |
| **pc-hq 3D board** | piececraft-hq, `bv_render_3d`, camera keys 1-4/0 | Extruded cells + billboard event sprites, POV keys, a GPU raymarch daemon (about 17 fps live when I last measured it, not re-measured today) | Not built for fast shooter input |
| **mcl-hq** | `RULES-mcl-hq.md` | The `build` rule: needs the block, a free land cell, nobody standing there; `BLOCK` rows | Turn-based |

## 3. The art question: Doom pack or cdda? (MINE)

The answer depends on the **view**, not the game. The two packs fit different views:

- **cdda** is a top-down tileset. It is right for the **2D view, the city map, buildings, items, guns and ammo icons**, and gta-hq already maps city roles to it.
- **Doom (Freedoom)** is made for first-person: walls, 8-rotation billboard characters, weapon-in-hand sprites, pickups. It is right for the **3D first-person view** and its sprites are the GoldenEye-style look (flat billboards in a blocky world).
- **Recommendation: both, split by job.** World cells (ground, walls, doors, furniture, loot icons) = cdda ids through gta-hq's role map. First-person extras (weapon sprite in hand, muzzle flash, hit puffs, a few enemy/NPC billboards, wall textures) = Freedoom. **The pets are the actors**, so the characters stay the pets' own sprites; Doom's 8-rotation `BOSS` set is the pattern to copy for pet rotations, not the characters themselves.
- Risk (MINE): the two styles differ in size and palette (cdda cells are 32/48 px, Doom textures 64). Mixed art can look wrong. Proof before committing: one screenshot of a cdda wall, a Freedoom floor and a pet billboard in the same 3D frame. If it clashes, make the first-person pieces with the img-hai word-bank pipeline (`IMG-HAI-DESIGN.md`) instead of importing more.
- Licence: Freedoom is BSD-3. Do **not** use the id Software WADs. cdda tiles are already in the house.

## 4. What to take from 007-goldeye (and what not to)

Take as **numbers and algorithms** (read from `main.c`, not tested here), re-written as a C op in house style:
- Weapon table: fists 14 dmg, 0.30 s, range 2.5 / pistol 20 dmg, 0.18 s, speed 55 / auto 11 dmg, 0.07 s, speed 60 / shotgun 5 pellets x 14 dmg, 0.50 s, spread 0.15; pickup ammo 28 / 48 / 16. HP 100, 150 for player 1. Bullet life about 1.4 s.
- Terrain/biome generator (`gen_terrain`, heightmap noise, 4 biomes, water border), `place_building` (stairs, windows, roofs), `collide_player` / `snap_feet` (walk-on-voxel collision), a seeded LCG for replay.
- AI shape: pick nearest enemy, face, close to 4 cells, strafe, pick up guns when unarmed, sometimes board a vehicle.
- The frag-limit loop and respawn-at-random-spawn.

Do **not** take: the freeglut window, global mutable state, the frame loop owning the rules, the no-reload design, direct-pursuit AI as the final AI.

## 5. The shape (MINE)

One rule makes AI and human "the same": **both only ever write INTENT rows.**

```
state/intents.txt   INTENT|ts|actor|move|dx,dy | aim|yaw,pitch | fire | reload | use | equip|<slot> | build|<piece>,<x>,<y>
state/events.txt    EVENT|...|fire|<target>|... then EVRES|...|result     (S1 format; only things that matter)
state/world.txt     snapshot of positions, hp, ammo, facing (rewritten each tick, NOT a ledger)
```

- A human's keys (via the relay, like the other windows) become INTENT rows. An AI actor's decision-maker reads `world.txt` and writes INTENT rows. The match daemon cannot tell them apart. This also makes a **replay** possible: seed + intents = the whole match.
- The match daemon is a C op with a fixed tick (start at 10 Hz sim; the view interpolates). It reads intents by size-growth marker (not mtime), resolves them with the 007 numbers, and appends only meaningful events (`FIRE HIT KILL PICKUP RELOAD DOOR BUILD`). **Positions are not ledger rows** (grok, 2026-10-17: never `STEP` in the log) - they live in `world.txt`.
- Hitscan for pistol/auto (cheap, deterministic); projectile only for slow things (rockets, grenades) later.
- Hit location from the cdda-hq wound table (head/torso/legs): this is the GoldenEye headshot feel and it already has numbers.
- Reload is a verb with a time cost; magazines are items (`give_item`/`take`), so `inventory` replay gives ammo.
- The AI follows the house rule: an FSM whose transitions are events, every decision appended as (actor, state, action, outcome), a human reviews proposals, no auto-promotion (`GAMES-DECISIONS-2026-10-13.md` section 7).
- City = a gta-hq/z-cell world with generated blocks, doors into interiors, stairs to floors (`Z-CELL-FORMAT.md`).
- Rendering: top-down 2D board first (cheap, proves the rules), then the pc-hq 3D POV (billboards for actors). The 3D view is a projection of `world.txt`; it never owns state.

## 6. Order, each step ends with something the owner can see (MINE)

| step | what | proof |
| --- | --- | --- |
| P0 | this page agreed; make `@.apps/rpg-pvp-hq` from the cdda-hq pattern | folder + button.sh opens an empty city window |
| P1 | city map generated from a seed (blocks, roads, 6 buildings with doors, interiors) on the 2D board; one pet walks in and out | PNG of the city, PNG inside a building, ledger DOOR rows |
| P2 | guns: loot on the floor, pick up, equip, fire (hitscan), ammo, reload. `fire`/`reload`/`equip` added through `GE_RULE_HOOK` | scripted run: inventory before/after, EVRES rows, a refused fire on empty magazine, negative test |
| P3 | two actors: hit, hp, death, respawn, frag limit; a deterministic duel replays to the same result | harness case (pal) + replay equality check |
| P4 | AI actor v1 from the 007 behaviour (seek, shoot, reload, loot) as an FSM, decisions logged | 100-tick bot vs bot, decision ledger lines |
| P5 | 3D first-person view on the pc-hq POV with billboards and a hand weapon sprite | PNG pair 2D / 3D of the same tick; the art test from section 3 |
| P6 | human + AI in one match; 2-4 panes like 007's split-screen | screenshot of the match |
| P7 | learning: the AI's weights tuned from the decision ledger, owner reviews the proposals | review file, one line per proposal |
| P8 | **build** (Fortnite): `build` verb with the mcl-hq rule (material, free cell, nobody there) plus a build time and material cost; wall / floor / ramp pieces; harvesting by `dig`/mine | PNG of a fortification, ledger BUILD rows, a refused build on an occupied cell |
| P9 | many humans on several machines | not planned; the cross-machine plan is paper only |

## 7. Questions for the owner
1. First playable: **top-down 2D with real-time ticks** (my recommendation, cheapest proof of the rules) or go straight to 3D first-person?
2. Is a pet that dies gone for good, or does it respawn (007 respawns)? Do pets keep guns between matches?
3. Do the Freedoom WAD downloads go on the usual path `#.NNEST_ASSETS/doom/` (outside git)? Should I fetch it, or will you?
4. Fortnite-style building under fire needs build speed and materials. OK to leave the first slice with only guns and doors, and add building at P8?

## 8. Not verified
007-goldeye was read, not compiled or run (numbers above are from the source). The Freedoom WAD is not on disk. The 3D frame rate was last measured on an earlier day. The art-clash risk is a guess until the section 3 screenshot exists. No code, folder or ledger for rpg-pvp-hq has been created.
