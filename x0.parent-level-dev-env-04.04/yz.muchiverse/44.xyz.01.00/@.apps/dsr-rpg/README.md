# dsr-rpg

A **carbon copy of `@.apps/rpg-pet`** (made 2026-10-12) that is being turned into a two-city world: six main characters (soldier, chef, clown, king, queen, fairy), chickens / horses / pigs, a 26x16 city plus 16 interiors. Design: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/DSR-RPG-DESIGN.md`. The board window needs its own pc-hq session: `PCHQ-SESSIONS-DESIGN.md` (same folder).

## Status (phase 0)
- Copied: the source only (C program, `.pdl` data, event pages, both windows). Not copied: rpg-pet's docs, runtime state, logs. Everything named `rpg-pet` / `rpg_pet` is renamed `dsr-rpg` / `dsr_rpg` (binary `ops/+x/dsr_rpg.+x`, build `sh ops/build_dsr_rpg.sh`).
- The cast, rooms and numbers are still rpg-pet's (phase 1 swaps the cast; phase 2-3 the maps). Tunables are in `tuning.pdl`, keys in `keybinds.pdl`.
- Event pages use their own registry commands `dsr_rpg_verb` and `dsr_pet_says` (appended to `#.ref/menu/event_commands.registry.pdl`), bound to `dsr_rpg.+x`, never to rpg-pet's program.
- Run the old-style window: `sh @.apps/dsr-rpg/button.sh run` (own daemon, own `state/`, own clock; observed 1% of a core with the window open). The board window (`board/open_board.sh`) still goes through the one shared `piececraft-hq` host package, so do not use it until pc-hq sessions exist: it would replace another game's board.
- rpg-pet's own guide for the shared parts: `@.apps/rpg-pet/HOW-TO-RPG-PET.md`, `TWO-CODEBASES.md` (the two front ends and the daemon; the same structure applies here).

## Rule
This is a real copy (house rule: no header-link split). Do not change rpg-pet to make this work; do not edit rpg-pet's DB actor rows (new characters get new rows).
