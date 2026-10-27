# RPG-SPACE user / generated asteroids (design + what exists, 2026-10-27)

Owner: "I want users later to be able to create proc-gen asteroids, hopefully one of the first uses of our generate
(image -> 3D / concept bank etc)."

## 1. What exists today (verified, gate `rpg_space_planet_map` / `rpg_dust_planet_map`, 83 PASS each)

* **Every planet has its own asteroids.** The planet-map objects are the `region` rows of `state/nodes.txt` plus a craft's world cell.
  `state/pworld_planet.txt` names the planet they belong to. When the planet below changes (`planet` verb, or the daemon tick) the skin
  saves the outgoing planet's objects to `state/world_<planet>.txt`, then loads the incoming planet's file, or, the first time,
  **generates** 3-5 asteroids from `hash(planet id | seed)`: kind rock / ore / crystal, amount 3-8, 4+ cells from the ship's start page
  and from each other, deterministic, then remembered (so mined-out asteroids stay mined). `space world` runs it by hand; a `WORLD` row
  in `state/events.txt` logs each switch. A craft with a world cell stays with its planet.
* **The seam for made-by-user asteroids.** A node of kind `gen:<name>` takes its shape and colours from
  `<app>/asteroids/<name>/model.txt`: the SAME format as any voxel piece (`dim X Y Z`, then one `x y z r g b` line per voxel; max 16
  cells a side; `gen_vox_max` voxels, default 1024). Missing or bad file -> the plain rock nugget. A generated asteroid keeps its own
  colours (no tile retint) and mines as `rock` (blocks.pdl). The name is `[A-Za-z0-9_-]` only, never a path. A hand-made
  `asteroids/sample_rock/model.txt` is loaded by the gate (shape, voxel count and a colour are checked).
* Everything else about an asteroid already works for a generated one: bottom bar listing (`bar_radius`), collision footprint (the
  model's x / z size), beam reach, minimap box, piloting collision, remnants.

## 2. What the generate pipeline has to produce (the contract)

1. A **concept** (concept bank entry, text or picture): "ice-rimed iron asteroid, cratered".
2. **Image -> 3D** (the house generate chain) -> a mesh or point cloud.
3. **Voxelise** to <= 16 x 16 x 16 cells and <= `gen_vox_max` voxels, palette-quantised (so colours stay readable in the unlit engine and
   the file stays small), the underside ignored (it rests on the planet). Writer: one small C op or pal harness case that outputs
   `model.txt`. Keep it a separate op (house rule: ops are compiled C, pal orchestrates), not a new mode in the skin.
4. Write `asteroids/<name>/model.txt` (+ later `meta.pdl`: label, author, concept id, seed, yields, mass). Nothing else in the skin changes.
5. Place it: a node row `region|<wx>|<wy>|gen:<name>|<amount>` in the planet's world file (or a verb `space asteroid add <name> <wx> <wy>`,
   not written yet).

## 3. Open decisions (the owner's call, none of them started)

* **Where user-made assets live.** Code branches must not carry private user data (see AGENTS.md: `xyzfs/users` is not in code branches).
  A user's generated asteroid probably belongs in their desk data (`user/<id>` data branch), with the app folder holding only shared
  and sample models. The loader would then search the user's folder first.
* **Yields and balance.** A generated asteroid mines as rock today. `meta.pdl` should carry `YIELD | item | n` and a size-based amount
  cap so a huge generated rock cannot mine out an economy (ties into the pet-chain / exchange design).
* **Limits per user and per planet** (the engine takes 10 vehicle pieces: 9 objects + the player's ship slot; the world file holds
  32 nodes). Needs a budget rule and a UI.
* **Player-facing surface.** The same settings / blueprint / db surface the owner wants for the bottom-bar radius (handoff section 0):
  create, name, place, delete, share.
* **Procedural variety without the generator.** The built-in nugget (`sk_nugget`) is a 5-11 voxel random blob. A richer built-in
  proc-gen (noise-based, craters, veins) could ship first and double as a baseline to compare the generated ones against.

## 4. Footprint (house rule: run alongside normal user activity)

A world switch reads and writes at most two small text files and happens only when the planet changes. The voxel pieces are rewritten
only when their text changes. A generated model is read once per piece rebuild; the 1024-voxel cap bounds that. Nothing here runs a
render or a model.
