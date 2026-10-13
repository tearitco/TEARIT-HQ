# df-hq - SCAFFOLD (2026-10-12)

A carbon copy of dsr-rpg's engine, made so Grok has something to work with. Nothing here is Dwarf Fortress-specific yet: it is dsr-rpg renamed (df-hq / df_hq, binary df_hq.+x, own state folder, own clock, own pc-hq instance df-hq-pchq, own event commands df_hq_verb and df_pet_says).

Plan and open questions: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/CDDA-HQ-AND-DF-HQ-HANDOFF-2026-10-12.md` (sections 3 and 5). Questions channel: co-lab-hai session 1791851915.

Shared on purpose (change when the project needs its own): the cast (DB party `DsrRpgParty`, actors 25-30), the RPG Maker tile sheets and the six dsr-rpg rooms (`rooms.pdl`), the dsr-rpg catalog/voices. Build: `sh ops/build_df_hq.sh`. Window: `sh button.sh run`. Board: `sh board/open_board.sh` (or the Toys entry "df-hq board").
Status: copied and renamed only; see the commit message for what was observed.
