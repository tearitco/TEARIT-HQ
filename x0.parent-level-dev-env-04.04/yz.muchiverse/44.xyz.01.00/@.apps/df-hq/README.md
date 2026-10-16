# df-hq - SCAFFOLD (2026-10-12)

A carbon copy of dsr-rpg's engine, made so Grok has something to work with. Nothing here is Dwarf Fortress-specific yet: it is dsr-rpg renamed (df-hq / df_hq, binary df_hq.+x, own state folder, own clock, own pc-hq instance df-hq-pchq, own event commands df_hq_verb and df_pet_says).

Plan and open questions: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/CDDA-HQ-AND-DF-HQ-HANDOFF-2026-10-12.md` (sections 3 and 5). Questions channel: co-lab-hai session 1791851915.

Shared on purpose (change when the project needs its own): the cast (DB party `DsrRpgParty`, actors 25-30), the RPG Maker tile sheets and the six dsr-rpg rooms (`rooms.pdl`), the dsr-rpg catalog/voices. Build: `sh ops/build_df_hq.sh`. Window: `sh button.sh run`. Board: `sh board/open_board.sh` (or the Toys entry "df-hq board").
Status: copied and renamed only; see the commit message for what was observed.

## FORTRESS-TODO (2026-10-16, sprint step D1: dig)

Built (claude): rock (`STONE | room | x | y | w | h` rows in `rooms.pdl`, solid until dug), the **Dig** verb (Menu row "Dig: mark the tile in front", or `df_hq.+x dig <x> <y>`), a mark list (`state/jobs.txt`: `DESIGNATE|ts|room|x|y`, `DONE|ts|room|x|y`), and a free pet in the same room that takes the nearest mark, walks to a free tile beside it and digs (`DIG|ts|actor|room|x|y|stone` appended to `state/events.txt`). Both files are append-only and read by byte cursor. A cell that has no free tile beside it (inside a block) stays marked until a neighbour is dug. Pets work only in the room they stand in.
Left empty on purpose, for grok (rules): how long a dig takes, which tool, stone and ore kinds, what drops and where it is hauled (`haul`/`take`/stockpile), labor preferences and skills (who may take the job), mood, and digging down (z: stairs stay a `DOOR` row until a slice writes `current_z`).
Proof: `XO/df-presentation/df-hq-dig-20261016/` (real window, arrow keys and menu taps, state-file checks, old-code control). The running `df_hq.+x` daemon must be restarted to pick up the job logic; the verb and the menu row work as soon as the binary is replaced.
