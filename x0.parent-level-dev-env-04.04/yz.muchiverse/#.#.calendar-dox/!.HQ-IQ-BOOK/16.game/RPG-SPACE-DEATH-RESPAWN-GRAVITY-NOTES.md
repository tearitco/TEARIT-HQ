# rpg-space: death, respawn, drops, gravity, sky drops (owner notes 2026-10-24, NOT built)

Status: **notes only**. Nothing here exists in code. Written down the moment the owner said it, with my reading of it and the questions it opens. Do not treat the proposals as decided.

## 1. The owner's words (kept)

> they need to die and respawn. often. it's a game.
> so? so they lose their stuff. it drops near their death place. so if they die of hunger they will do this.
> let's look at gravity soon. so if the airship breaks it can fall to the nearest planet or float in space.
> drop in from the sky like Fortnite. 2D will only see landing.
> their spawn / safe place.

## 2. What it means, as I read it

1. **Death is normal and frequent.** A character can die (hunger is the named cause; others later: damage, falling, suffocation in space).
2. **Respawn at a safe place.** Each character (or the party) has a spawn / safe place. The ship's bridge is the obvious default; a planet could have its own.
3. **Death costs the inventory.** The dead character's items drop as items on the ground **near where they died** (not at the spawn point), so a corpse run recovers them.
4. **Gravity.** A broken airship is no longer held at its hover height: it **falls to the nearest planet** or **floats in space** (no planet in reach). So the ship's position needs a physical state (flying / drifting / falling / landed), not only a grid cell as in the helm today.
5. **Dropping in from the sky** (Fortnite-style parachute): a character or the ship arrives from above onto a planet page. The 3D view can show the whole descent; the **2D view only shows the landing** (the cell where they touch down).

## 3. How it connects to what exists

- Hunger / needs: the pets games already track pets' state (`petslike_core.c` pets, `pet_*.txt`); a death rule would read those. Not checked in detail.
- Items on the ground: furniture / placed items exist (`placed`, `npl`, `pl[]` in the core, "in this room (click to pick up)" in the left bar); a drop could be a placed item at the death cell with a flag. Not checked whether placed items can carry an owner id.
- Gravity: the ship hovers a fixed 8 levels above the planet (sunk planet surface, second solid run) and the helm moves a hull box on the space desk. `ops/dust_orbit.c` already does orbit physics (two suns, Kepler period, 27 checks). Falling needs a vertical position for the ship, which the desk model does not have yet (the ship's rooms are fixed desks with the planet as backdrop).
- Sky drop: the engine already draws the planet below the hull; a descent is a camera and entity move along z over the planet's desk, ending at a cell. 2D = only the landing cell (the 2D scene has no height).
- Events: death, respawn, drop and fall are natural **event pages** (RPG Maker style, with an affected area, see `EVENT-AREA-SELECTION-DESIGN.md`): e.g. `death` (trigger: needs reach 0), `respawn` (trigger: after death), `hull_break` (affects the ship's hull area, then gravity takes over).

## 4. Questions before any build

1. Who dies: every character (pets included) or only the player's? Does the party respawn together?
2. What kills: hunger only at first, or also damage, falling, vacuum?
3. Respawn place: one per character, one for the party, or the nearest safe place (bridge, a bed, a planet base)? Does death cost anything beyond the items (time, a stat)?
4. Drops: all items, or a part? How long do they stay? Can other characters (and pets) take them?
5. Gravity: what breaks the airship (damage, no fuel, a hull event)? Falling to "the nearest planet" by galaxy-grid distance (cheap, uses the helm grid) or by real orbit physics (`dust_orbit`)? What happens on impact (damage, crash site, crew)?
6. Sky drop: who can do it (anyone from the ship, any time, or only from a ship above a planet)? Where do they land (chosen cell, random, wind)? Does the 2D view jump straight to the landing cell?

## 5. Suggested order (proposal)

1. Needs + death as an event page that only drops items and respawns (no gravity): proves the drop rule and the safe place.
2. A vertical state for the ship (hover / drift / fall / landed) on the space desk with the `fall to nearest planet` rule on the galaxy grid.
3. Sky drop as a move along z with a landing cell, 2D shows only the landing.
4. Real orbit-based drift and fall (`dust_orbit`) once 2 is shown to feel right.
