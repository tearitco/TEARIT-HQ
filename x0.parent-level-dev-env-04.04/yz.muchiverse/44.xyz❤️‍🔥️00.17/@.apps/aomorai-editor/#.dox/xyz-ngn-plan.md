# 🌞 xyz-ngn-plan.md — piececraft-xyz's Day/Night & Lighting Engine

**Status: PLANNING ONLY, nothing below is built yet.** Written
2026-08-04, direct instruction, right after the real game clock
(`game_time_epoch_sec`, `autotick_enabled`, the `]` toggle) landed —
this doc is the "what's next" for turning that clock into an actual
sun, sky, and lighting system. Emoji-heavy on purpose, for human
skimming. 🎉

---

## 📖 Table of Contents

1. [🎯 What We Already Have](#1--what-we-already-have)
2. [☀️ Step 1 — The Sun (and Moon, and Mars) as Real Orbiting Bodies](#2-️-step-1--the-sun-and-moon-and-mars-as-real-orbiting-bodies)
3. [🌗 Step 2 — Day/Night Sky Color](#3--step-2--daynight-sky-color)
4. [💡 Step 3 — Real Directional Lighting](#4--step-3--real-directional-lighting)
5. [🌑 Step 4 — Shadows (the hard part)](#5--step-4--shadows-the-hard-part)
6. [🕯️ Step 5 — Point Lights (torches, later)](#6-️-step-5--point-lights-torches-later)
7. [🎮 What Unreal/Modern Engines Do (and what we can actually steal)](#7--what-unrealmodern-engines-do-and-what-we-can-actually-steal)
8. [⚡ Performance Budget](#8--performance-budget)
9. [🗺️ Real Build Order](#9-️-real-build-order)

---

## 1. 🎯 What We Already Have

The load-bearing piece is already real and working: **`game_time_
epoch_sec`** in `pieces/world_01/state.txt`, a real whole-seconds
game clock that advances on its own when `autotick_enabled=1`, at a
real configurable rate (`autotick_speed`: `cent`/`sec`/`min`/`hour`/
`day`, cycled with the real `[` key — `]` toggles autotick on/off).
Everything below is about making that NUMBER visually mean something —
right now it only drives the `${game_datetime}` text readout and the
chicken's own wander tick. 🐔⏰

The real next question every step below answers: **given
`game_time_epoch_sec`, what does the SKY look like right now?**

---

## 2. ☀️ Step 1 — The Sun (and Moon, and Mars) as Real Orbiting Bodies

**Real, direct instruction, expanded 2026-08-04**: not a simple
circular day/night arc — a real **elliptical ("long oval") orbit**
where the sun sits CLOSE to the planet during the day (bright, real
light reaches the ground) and swings far away at night (too distant
for its light to matter) — plus a real **moon** and a real **mars**,
each on their own real orbit, each rotating "mathematically every
tick." This is explicitly the FIRST real step toward a bigger,
later space-travel scope ("more planets, suns, systems, get it?") —
the math below is written to generalize to N bodies from day one, not
just these three. 🪐

### 2a. Real orbital math (one shared function, N real bodies)

A real ellipse, not a circle — same real orbital-mechanics shape every
solar system actually has (Kepler's own real first law: orbits are
ellipses, not circles). Each body gets its own real, independent set
of these parameters:

```
body_angle = (game_time_epoch_sec / body_period_seconds) * 2π   // real, own period per body
// Real ellipse in a body-local orbital plane (semi-major axis a, semi-minor axis b, b < a for a real "long oval"):
local_x = cos(body_angle) * body_orbit_a
local_z = sin(body_angle) * body_orbit_b   // b < a is what makes it "long" (stretched), not circular
distance_from_planet = sqrt(local_x² + local_z²)   // real, changes continuously - THIS is what makes the sun feel "close" or "far"
```

Real, per-body table (tune all of these to taste, this is the real
STRUCTURE, not final numbers):

| Body | `period_seconds` (one real full orbit) | `orbit_a` (far) | `orbit_b` (near) | Real light role |
|---|---|---|---|---|
| ☀️ Sun | e.g. 1200 (one real day/night cycle) | large | small | Real light source - see §2b |
| 🌕 Moon | a real different, LONGER period (its own real day/night, out of sync with the sun on purpose) | large | small | Dim, real secondary light at night when the sun is far |
| 🔴 Mars | a real, even longer/different period again | large | small | Visual only for now (a real, later-game destination, not a light source yet) |

**Why elliptical instead of circular, stated plainly**: a circular
orbit keeps the sun at a CONSTANT distance, so "distance-based
brightness" would do nothing - the whole real point of the "long oval"
shape is that `distance_from_planet` genuinely swings between "close
enough to light the ground" and "too far to matter," giving day/night
a real PHYSICAL cause instead of a hand-picked on/off angle threshold.

### 2b. Real brightness from real distance (not just a fixed angle check)

```
sun_light_level = clamp(1.0 - (sun_distance_from_planet / SUN_DARK_DISTANCE), 0.0, 1.0)
```

At `sun_distance <= 0` (perihelion, closest approach): `sun_light_
level = 1.0`, full real daylight. At `sun_distance >= SUN_DARK_
DISTANCE`: `sun_light_level = 0.0`, real night, sun's own real light
contributes nothing. This directly REPLACES the simpler "sun_y > 0"
day/night check from the original version of this doc - same real
role (feeds Steps 3/4 below), now driven by real orbital distance
instead of a flat angle threshold, matching the real instruction
("orbit far away so light doesn't reach the planet") exactly.

### 2c. Rendering each body

Same real phymoji-style approach for all three (one shared function,
different source emoji/color per body - ☀️/🌕/🔴 or real generated
voxel discs): position = `planet_center + (local_x, height_offset,
local_z)` from §2a, rendered far past normal draw distance, always
along that real vector from the camera - exactly the same real
"render a bright object way out past the world" approach the original
single-sun version of this doc already planned, just now driven by
real elliptical position instead of a simple circular angle.

### 2d. The real space-travel throughline

This is explicitly written as a REAL, generalizable "N orbiting
bodies" system, not three hardcoded special cases - `body_angle`/
`body_orbit_a`/`body_orbit_b`/`period_seconds` are the same real four
numbers for ANY future body (a second star, a fourth planet, a whole
new system). When the later "more planets, suns, systems" scope
arrives, the real ask is "add a row to the table," not "rewrite the
math" - that generality is a deliberate real design decision made
NOW, in this doc, for exactly that reason. 🚀🌌

---

## 3. 🌗 Step 2 — Day/Night Sky Color

`clear_sky()` in `bv_render_3d.c` currently fills the whole frame with
one fixed blue (135,180,220). Real fix: make it a function of
`sun_light_level` (§2b - real, distance-based, not a fixed angle
threshold) - interpolate between real, tuned color stops:

```
sun_light_level >  0.6  →  bright day blue   (135,180,220)
sun_light_level ≈  0.3  →  sunset orange/pink (255,150,90)
sun_light_level <  0.1  →  night navy         (10,15,40)   // moon (§2a) can lighten this slightly when it's near
```

Real, cheap linear interpolation between whichever two stops
`sun_light_level` falls between. This alone gives a real, visible
day/night cycle even before any ground lighting changes — the sky
itself tells the story, and because it's driven by the sun's own real
ELLIPTICAL distance (not a simple angle), the transition timing
naturally isn't perfectly symmetric - sunrise/sunset can genuinely
take different real amounts of time depending on the orbit shape,
matching how a real ellipse actually behaves. 🌅🌃

---

## 4. 💡 Step 3 — Real Directional Lighting

Right now every voxel face gets a flat, FIXED darken factor
(`if (best_face != 3) r * 3/4`) — same brightness at noon and
midnight. Real fix: make that darken factor a function of
`sun_light_level` too (same real value already computed for the sky,
§2b):

```
ambient_floor = 0.15   // never fully black
light_level = clamp(sun_light_level, ambient_floor, 1.0)
final_color = base_color * light_level * (face == top_face ? 1.0 : 0.75)
```

This is a REAL, if simplified, directional light — every face still
gets its existing flat per-face shading (matches the engine's own
established look), just globally scaled by how high the sun is. Cheap:
one multiply per pixel, no new geometry, no new render pass. 🔆

---

## 5. 🌑 Step 4 — Shadows, Unreal's Real Trick (adapted, not copied)

**Decided 2026-08-04: THIS is the real approach we're building**, not
a real full per-pixel shadow ray. Real, honest naive version first
(what most engines do NOT actually do at runtime): cast a real second
ray from every solid hit point toward the sun, walk the DDA, check if
anything blocks it before reaching infinity - real, correct, and real
expensive (roughly doubles the per-pixel raymarch cost, since it's a
SECOND full walk through the terrain grid for every single pixel that
hit something).

**Unreal's own real trick (Cascaded Shadow Maps) is why nobody actually
does the naive version**: before rendering the real camera view, it
renders the scene's own depth from the LIGHT's own point of view into
a texture (a real "shadow map" - multiple resolutions stacked for
near/far, hence "cascaded"). Then for every real camera pixel, it
projects into that light-space texture and does a CHEAP DEPTH
COMPARISON - "is something between me and the light closer than I am?"
- instead of a real geometric ray test. It turns an expensive
per-pixel question into a cheap texture lookup, paid for ONCE per
frame (building the map) instead of once per PIXEL (a real ray).

**Our own real, cheap equivalent - no GPU depth buffer needed**: this
project's terrain is already, functionally, a real heightmap (one
solid-ground height per (x,y) column - see `mc-speed-algos.md`'s own
real `col_top`/`col_bottom` empty-space-skip data, ALREADY computed
every frame for a completely different reason). That per-column height
array **already IS a real shadow map** for a heightfield world - we
don't need to build a separate one.

Real algorithm:
```
for each real solid ground pixel at (col, row, height):
    step from (col, row) toward the sun's own real (x,z) direction,
    one column at a time (a real, cheap 2D line walk - NOT a full 3D
    DDA, no y-axis stepping needed for a flat-ground shadow test)
    at each stepped-through column, real lookup: col_top[stepped_col][stepped_row]
    if that column's own real height is TALLER than a straight line
    from our own height to the sun's own real height at that distance
    would be, we're shadowed - real, cheap, no ray, just array lookups
    real early-exit: stop once we've walked far enough that even a
    max-height column couldn't shadow us anymore (same real "distance-
    aware max_steps" bound this file's own terrain DDA already uses)
```

**Why this is real, honestly cheap**: no second 3D raymarch, no new
geometry pass, no new texture - just a real 2D line walk through data
that's ALREADY sitting in memory for the empty-space-skip optimization,
reusing it as a de facto shadow map. The real cost is proportional to
shadow-ray LENGTH in 2D columns, not a full 3D DDA - genuinely cheap
enough to run per-pixel, unlike the naive version above.

**Real limitation, stated honestly**: this only shadows the FLAT
TERRAIN from other flat terrain (a hill blocking sun from a valley) -
it does NOT (yet) account for phymoji entities (trees) casting real
shadows, since those aren't part of the column-height array at all.
Real, honest follow-up if wanted: trees could contribute a real,
approximate "occupied height" to their own column too, extending the
same cheap lookup rather than needing a real second system.

**Real, honest recommendation: skip this for v1.** Directional
lighting (Step 3) alone gets 80% of the visual value at near-zero
cost. Only build real shadow rays if the flat-lit look genuinely
bothers you after seeing Steps 1-3 live — and even then, start with a
CHEAP approximation (e.g., only cast shadow rays for phymoji entities,
not full terrain) before the expensive full version.

---

## 6. 🕯️ Step 5 — Point Lights (torches, later)

Real, separate, LATER feature: a light source with a real position
(not "infinitely far away" like the sun) whose brightness falls off
with distance (`1 / distance²`, the real physically-based falloff
formula every modern engine uses, including Unreal's own). Needed for
torches/lava/anything indoors where the sun can't reach. Flagged here
only so Step 3's lighting math is written in a way that's easy to
EXTEND later (a real `total_light = sun_contribution + sum(point_
light_contributions)`), not because it's in scope now.

---

## 7. 🎮 What Unreal/Modern Engines Do (and what we can actually steal)

Real, honest translation of big-engine concepts into things that fit
THIS project's own real constraints (CPU raymarch, no GPU shaders, no
real-time global illumination budget):

| Unreal concept | What it actually does | What we can realistically steal |
|---|---|---|
| **Directional Light** | One real "sun" light, infinite distance, parallel rays | ✅ Exactly Step 3 above — this is the SAME real technique, just without a GPU |
| **Sky Atmosphere** | Physically-based sky color from sun angle | ✅ Step 2's color-stop interpolation is a real, simplified version of this exact idea |
| **Lumen (real-time GI)** | Light bouncing off surfaces onto other surfaces | ❌ Skip - needs a GPU compute budget we don't have. Our real "ambient floor" clamp (Step 3) is the honest low-cost substitute |
| **Cascaded Shadow Maps** | Pre-rendered shadow depth textures from the light's POV | ❌ Skip - a real shadow-map RENDER PASS doesn't fit our per-pixel-raymarch architecture. Real shadow RAYS (Step 4) are our own real equivalent, just slower per-pixel instead of pre-baked |
| **Physically-based falloff (1/d²)** | Real point-light distance attenuation | ✅ Cheap, steal it directly for Step 5 point lights |
| **Time-of-day Blueprint** | A designer-facing curve editor for sun angle → color | ❌ Skip the EDITOR, keep the CONCEPT — our color-stop table in Step 2 is the same idea, just a hardcoded table instead of a visual curve |

**The real, honest summary**: we can steal Unreal's own MATH (sun
vectors, color interpolation, inverse-square falloff) cheaply — we
can't steal its real-time GPU techniques (Lumen, shadow maps) at all,
our own real per-pixel CPU raymarch needs its own, simpler answers to
those same problems (an ambient floor instead of GI, real shadow rays
instead of shadow maps, and even those only if Step 3 alone isn't
enough).

---

## 8. ⚡ Performance Budget

Real, honest cost estimate per step, relative to `mc-speed-algos.md`'s
own already-established real numbers (~0.03s/frame current baseline):

| Step | Real added cost |
|---|---|
| 1-2 (sun position + sky color) | ~0, pure math, no new rays |
| 3 (directional lighting) | ~0, one multiply per already-shaded pixel |
| 4 (real shadow rays) | Roughly DOUBLES per-pixel cost (a real second raymarch) - real, honest, not hand-waved |
| 5 (point lights) | Scales with light count × pixel count - fine for a handful of torches, needs the SAME real coarse-box-first pattern phymoji entities already use if it ever grows |

**Real recommendation: ship Steps 1-3 first, live-test the actual
frame time, THEN decide if Step 4 is worth its real cost** - matches
this house's own established "test everything headlessly/directly
before claiming success" pattern, not a guess made in this doc.

---

## 9. 🗺️ Real Build Order

1. Sun position math (Step 1) - pure math, testable headlessly via a
   Python script computing `sun_y` for a few `game_time_epoch_sec`
   values before touching any C.
2. Sky color interpolation (Step 2) - `clear_sky()`, real, small,
   visually obvious the moment it's live.
3. Directional lighting (Step 3) - the existing per-face darken
   factor, made real time-of-day-aware.
4. Live-test 1-3 together, measure real frame time, THEN decide on
   Step 4 (shadows) - don't build it speculatively.
5. Point lights (Step 5) - later, only once there's an actual light
   SOURCE to place (a torch item/block doesn't exist yet either).

Nothing above is built yet - written per direct instruction to record
real intentions before any of it gets coded, same real pattern
`phymoji.md` already established for this project. 🚀
