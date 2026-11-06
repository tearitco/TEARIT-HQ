# Comparable project: RPGAtlas, and what its generation spine teaches us (research, 2026-11-05)
Labels: **SAID** owner, **READ** a file I opened (named), **OBS** I saw it work, **DOC** my reasoning. The owner: "use RPGAtlas-main as kind of a comparable project ... mirror what they do ... it is a familiar spine ... esp its generation features / algos."
Source: `~/Desktop/🤖️🪤️🏠️/GITS/RPGAtlas-main/` (76 files, 1.7 MB, plain JavaScript, no build step, **GPL-3.0-or-later**). Not run; read only.

## 0. Coverage and licence (do not overclaim)
**Read:** `README.md` (top), `AGENTS.md`, `js/assets.js` lines 1-200 (helpers, first tiles), 640-830 (humans, objects, charsets), 839-945 (faces, enemies), 1104-1123 (enemy canvas), `js/sfx.js` lines 20-150, `js/gl.js` header and shader list, `wiki/Characters-and-Custom-Assets.md`, grep of every random/seed/generate use. 
**Second reading (same day, owner: read and doc anything useful, do not take it if ours is better):** tile drawers (all 57 keys, lines 200-615), the enemy drawer roster, `engine.js` battle command, damage variance, enemy action choice and random encounters, `js/plugins.js` header and API, `js/data.js` traits and `migrateProject`, `tests/traits.test.js`, the head of `tests/heights.test.js`, `package.json`.
**Still not read:** the other eleven enemy drawers line by line, `editor.js` outside the Character Generator hooks, `i18n.js`, `project-io.js`, the Tauri and C# launchers, `modules.test.mjs`.
**Licence:** GPL-3.0. We **mirror ideas and shapes, we do not copy code** (our repo and IP stay unencumbered; our licence reading is parked by the owner but this is the safe default). Reading their code is fine; our implementation is C ops and `.pdl` data written from scratch.

## 1. What they do (READ), and the algorithm behind each
| Feature | How it works | Where |
|---|---|---|
| **Seeded per-asset RNG** | tiny LCG `s = s*1664525 + 1013904223`; each tile gets `rng(id * 7919 + 17)`: same id, same pixels, forever, so a cache is safe | assets.js 19-24, 613-618 |
| **Logical grid, scaled** | sprites are drawn on a **16 x 16 grid in 3 px units** inside 48 x 48, so every shape is chunky and coherent | `drawHuman`, `u = 3` |
| **Tiles = small draw functions** | `defTile(key, name, passable, draw(g, r))`: base fill + `speckle()` (random light/dark 3 px dabs, count and light/dark factors are the knobs) + a few accents | assets.js 142-200 |
| **Humans = params, not pixels** | `{skin, hair, style, shirt, pants, hat}`; style in `[spiky, long, short, hat, bald]`; a 10-row preset table `HUMANS` (hero, mage, cleric, guard, ...) | assets.js 640-660, 799 |
| **Walk cycle** | 3 frames; frame 1 = standing; frames 0 and 2 lift one leg by 1 unit (`lLift`/`rLift`), arm swings by `fwd` in side view; **right = mirror of left** | `drawHuman`, `charFrameCanvas` |
| **Save the recipe** | custom characters are saved as `customChars` (the params only) in the project JSON; sheets are regenerated and cached by `idx_dir_frame` | wiki Characters, `registerHuman` |
| **Randomize** | a button picks each field from its list (`pick(Assets.HAIR_STYLES)`, `pick(SKINS)`): seed-less random over a closed menu | editor.js 3613-3661 |
| **Faces = crop of the sprite** | the face is the standing frame cropped and scaled; a real faceset image overrides it by name | `faceCanvas` |
| **Enemies = (type, colour, size)** | twelve drawer functions `slime(g, s, col)` etc., all in fractions of size `s`, shading via `shade(col, f)` and `mix(col, "#fff", t)`; cache key `type_color_size` | assets.js 845-1123 |
| **Music = seeded random walk** | theme table `{tempo, root, scale[], seed, lead, density, bassEvery, drums}`; per 8th note: bass on a 4-bar progression, melody = random walk on the scale (`move = floor(rng*5)-2`, clamped), mulberry32 RNG per theme | sfx.js 73-135 |
| **Sound effects = primitives** | `tone(freq, dur, wave, vol, slideTo)`, `noise(dur, vol, lowpass)`, `arp([freqs], step)`; each effect is one line (hit = noise + falling square) | sfx.js 20-70 |
| **HD-2D renderer** | per-tile height extrudes terrain into blocks, characters are upright **billboards**, overhead tiles hover one unit; bloom, depth of field, fog, point lights; lights are **event names** (`light #rrggbb radius`) | gl.js header |
| **Conventions in names** | custom tile filenames set behaviour (`.pass`, `.terrain`); events named `light ...` create lights | README, wiki |
| **Process rules** | AGENTS.md: every user-facing change gets a dated entry prepended to `js/patch-notes.js`; tests per subsystem (`heights`, `interpreter`, `textcodes`, `traits`) | AGENTS.md, tests/ |

## 2. What to mirror (and what we already do) (DOC)
1. **Recipe, not pixels.** Same as our `cast.pdl`; extend to levels: a level is a recipe plus a root. (Already D30.)
2. **A preset table plus a randomiser = our `STYLE` rows plus a seed (D30).** Their closed-menu random is the weak half: ours should be **seeded** (same seed, same character) with optional knobs, which is what the owner asked for.
3. **Per-asset seed from the id** (`id * 7919 + 17`): we derive from the cast id plus a global seed (fixed hash); keep it, and cache by (id, seed, knobs).
4. **A fixed logical grid scaled up** gives a coherent look. Our voxel grid is 24 x 24 x 24 at 2 px per voxel = 48, the same trick. For the 2D pixel pass consider a **coarser logical grid for finishing** (snap shading and outlines to 2 px) so voxel projections read as pixel art.
5. **`speckle(base, count, light, dark)` as the only texture primitive.** Our `tone`/`restyle` use ramps; a seeded speckle with explicit knobs is a cleaner way to make **calm vs rich** ground (our noisy-grass finding): `speckle_count` and the light/dark spread are the knobs. Add as a `GEN` row.
6. **Theme table for audio** (tempo, root, scale, seed, lead, density) is exactly the knob-and-patch shape the owner described. Mirror it for generator themes (`gen_theme`: ramp, hue, sat, levels, speckle) and for the NIGHT/music tools.
7. **Names carry behaviour** (`light #rrggbb 120`): good for our event names and tile catalogs; we already use `.pdl` rows, keep rows as the authority and treat names as sugar.
8. **Billboards for the 3D view.** A voxel render per facing used as an upright billboard is a cheap 3D-board path (no mesh pipeline); our voxel op already renders 4 views and poses.
9. **Tests per subsystem, patch notes per user-facing change.** We have harness cases; consider a short dated "what changed for the user" line per commit (our commit messages already do this).

## 3. Where we should be better (DOC)
- They are runtime and in-browser: every sprite is regenerated each launch. We generate **offline to files** with verification, so the output is inspectable, diffable and approvable (`good/` plus hashes).
- Their art is flat fills with one shade step. Ours adds ink outline, two-tone shading, ramps and the pixel pass; keep that edge.
- Their character params are 6 fields; ours go to equipment, creatures, poses, themes and lore (D30).
- They have **no IP or lore** in the generators; ours should (READ cues, DOC colours).

## 4. Next steps for the generator from this study
1. Add `speckle` knobs to the restyle and tone rows (calm vs rich ground), harness: more speckle gives more colour variance; zero gives none.
2. Write the D30 seed-and-knobs reader with a **theme table** shaped like their `THEMES`.
3. Try one billboard test: a voxel character 4-view strip as a 3D-board billboard.
4. Optional: read their remaining tile drawers (lines 200-615) for water and animated tiles, and `engine.js` battle code for enemy variance; record in this file.

## 5. Second reading: verdicts (adopt, adapt or skip) (DOC)
Rule from the owner: take it only if it is better than what we have. "Ours" below is what I verified in this repo today; where I did not check, it says so.
| Their idea (READ) | Verdict | Why |
|---|---|---|
| **Traits** (`classes[].traits = [{type: param/special/element/skill/state/equip, key, value}]`; `traitRate` multiplies the percents, `traitSum` adds; `canEquip` reads `equip` traits) | **ADAPT for armor and weapons** | It is a small, data-only way to let any piece (class, gear, state) carry stat modifiers that combine by one fixed rule. Our `PARTSET` equipment pieces (D30) can carry `TRAIT` rows the same way, so a helmet is a model **and** its stats in one row. First check our own actor and item stats in `&.widgits/db-hq/data/actors.pdl` and the petslike core so we extend the house model, not add a second one. |
| **`migrateProject` with `meta.version`** (idempotent upgrades, tests for old files) | **ADAPT, later** | Our recipe files (`cast.pdl`, future `PARTSET`/`STYLE`) will outlive the code that reads them. A `# version` row plus a migrate verb with a harness case is cheap insurance once D30 lands; not needed today. |
| **Tile roster** (57 keys incl. cactus, deadtree, snowtree, pine, waterlily, cobweb, lava_rock, crystals, statue, pillar, bookshelf, torch, crate, flowerpot, checkered, swamp, ice, snow) | **ADOPT as a checklist** | A cheap list of object and ground types a level kit needs. Cross-check against `Outside_B/C`, `Dungeon_*` before generating new object recipes (voxel items thread). |
| Tiles as one whole-cell draw each, no autotile | **SKIP** | Ours is better: the 48-shape floor table (A2) and per-block ramps give real edges and joins. |
| **`TERRAIN_KEYS`** (terrain vs decoration, drives their Auto layer) | **SKIP** | Our catalogs already tag `kind floor/wall`; equivalent. |
| **Enemy roster** (slime, bat, orc, ghost, golem, wasp, wolf, shroom, skeleton, imp, crystal, serpent) | **ADAPT two body plans** | Our creature kinds cover quad, blob, winged, plant, jelly. Missing and useful: **serpent** (a chain of boxes that tapers and sways, poses move segments) and **undead biped / ghost** (humanoid with hollow parts, floating hem). Add as `PARTSET` body plans in D30, not as code. |
| `enemyCanvas(type, colour, size)` cache by key | **SKIP** | Same shape as ours (cast id + seed + knobs) once D30 lands. |
| **Weighted enemy action table** (`actions: [{skillId, weight}]`, roll against the total) | **SKIP for now** | Fine for a simple game; our attrition model is FSM/GOAP and richer. Remember it as the fallback "dumb AI" row format. |
| Random encounters: counter `>= rate * (0.7 + rand * 0.6)` | **NOTE only** | A one-line jittered step counter; use it if a new level needs encounters. |
| Damage variance `0.85 + rand * 0.3` | **SKIP** | Standard; no value in writing it down twice. |
| **Plugin bridge** (`atlas.onMapLoad/onUpdate/onRender`, `registerCommand`, `startBattle`) | **SKIP** | Our extension points are ops, event pages and `.pdl` rows; no in-process script engine wanted. Their hook names are a useful list of the places a mod would want. |
| **Procedural SFX** (`tone(freq, dur, wave, vol, slideTo)`, `noise(dur, vol, lowpass)`, `arp([...])`; each effect one line) | **ADAPT as a future thread** | A C op that writes a WAV from `SFX | name | wave | freq | slide | dur | vol` rows gives deterministic, versioned, harness-testable game sounds, with the same knob feel. Check what the house already has first (pulse short sounds, TTS) before building. |
| **Music themes** (tempo, root, scale, seed, lead, density, bass pattern, drums; seeded random walk) | **ADAPT, optional** | Same knob-and-patch shape as D30. A `MUSIC | theme | ...` row set rendered offline to WAV (mp3s stay out of the repo, per the house rule). Lower priority than characters. |
| **`light #rrggbb radius` event names** | **NOTE** | Their sugar for lights. Our rows already carry named fields; keep names as sugar only. |
| **Message text codes** (`\i[n]`, `\c[n]`, BBCode) | **SKIP** | Not generation; check house text rules before any adoption. |
| **Auto layer** in the editor (sorts terrain and decoration) | **NOTE** | Could become a `generator_level_op` row type later (place by catalog kind). |
| **Height mode** (per-tile elevation extruded into blocks) | **SKIP** | Our 3D board has its own height model; their value for us is the billboard trick (section 2). |
| **Exports** (standalone .exe, .html, Tauri wrapper), **i18n** | **SKIP** | Not part of the generator spine. |
| **Tests** are tiny Node scripts that load the real files in a VM and assert | **SKIP** | Our pal harness with negatives is stronger (a check must be seen failing). |
| `AGENTS.md` patch-notes rule | **SKIP** | Our commit messages already carry the user-visible changes. |
**Net:** from the second reading we keep three things (traits for armor, versioned recipe files, two body plans) and two optional future threads (SFX and music as seeded offline renderers). Everything else is either already in the house or worse than what we have.
