# The first plug: day/night (2026-11-14)

Decisions: D74 (names, events versus plugs) and D75 (this build). Labels: RAN means I ran it today and saw the verdict; READ means I opened the file; DOC is my reasoning.

## The rule that put it here
Owner (SAID): if RPG Maker has an event for it, or it is a very reasonable "should be" in any engine, it is an event. A game need not have a day and a night, and RPG Maker ships no event for a cycle (only Tint Screen and variables), so the cycle is a plug. The world clock (game time passes) is engine; the cycle on top of it is the plug.

## The file
`@.apps/rpg-edit-hq/plugs/day-night.plug.pdl`: `ENABLED | 1/0` and four `PHASE | name | from_hour | outdoor r,g,b | indoor r,g,b | sky` rows (dawn 6, day 8, dusk 17, night 19). Multipliers are out of 256 (256 = unchanged). The defaults equal the old hard-coded values.

## What reads it (READ and RAN)
- `&.widgits/_shared-lib/pet_sky.c`: `pet_phase_of_hour`, `pet_phase_tint`, `pet_sky_sheet`, `pet_plug_phase`; defaults when the file is missing; ENABLED 0 = always `day`.
- `&.widgits/_shared-lib/petslike_core.c`: sets the plug path at start, uses `pet_phase_tint` in the room tint, folds the plug text into the schedule signature, installs one reminder per phase at `from_hour` (`common:phase_<name>`).
- `time.pdl` no longer has the four phase rows; `day_tick` and the world rows stay.

## Evidence (RAN 2026-11-14)
`day_night_plug` 35 passed, 0 failed. Negative controls: a moved dusk hour and a changed tint break parity; ENABLED 0 differs from 1; a missing plug installs no phase reminders. Regression, all pass: `clock_feature_off` 16, `pet_say_clock` 19, `pet_net_say` 24, `lc_clock_*`, `world_clock`, `needs_clock`, `survivor_*`, `edit_*`, `editor_skip*`. `rpg_space.c`, `rpg_dust.c`, `rpg_pet.c` compile with the changed `pet_sky.c` (not installed, not run).

## Not done
- The Tools dropdown groups (this page, plugs with an on/off mark, plug manager) and the Plug manager window.
- A page-level override of the switch (project-wide only).
- The four `phase_*` event pages still hold tint numbers, a greeting and the lamps toggle; they should read the plug later.
- Not looked at by a person: the live editor with the new build (the editor reads the plug at start; relaunch to see it).
- The other books that include the core (rpg-space, rpg-dust, rpg-pet, ...) were not rebuilt or run.

## Add the next plug (DOC)
Copy the shape: one visible `.plug.pdl` with `ENABLED`, parameters as rows, a reader in a shared pure `.c`, defaults that equal today's behaviour, a probe op, and a parity case with negative controls. The multi-plug loader is the next plug's first job.
