# DSR (Desk Street Raider) — dev log

Renamed from "Wall Street Raider" to "Desk Street Raider" - a real desk-based
game, not just a name pun.

Desk: `dsr` (new, real — see `sessions/s1/desks/dsr.pdl`).
Roadmap entry: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` §6, §6b.

## The real, deliberate architecture: two independent front doors
1. **Toy** — a real X11-HQ window (nav-numbered `.chtpm` status-screen menu,
   same shape as the original game's own UI), opened via this house's real
   `toy.pdl` convention. Playable with NO desk open at all.
2. **Desk** — the `dsr` desk itself (2 castles/4 banks/8 stores), walkable/
   touchable like `civ-test`. Playable with NO toy window open at all.
3. A real setting on the toy controls whether launching it ALSO auto-opens
   its own desk (defaults on, can be set off).

Both read/write the same real ledger/state - neither is a special case of
the other. See roadmap §6b for the full reasoning (this is meant to be the
proof that the toy/HQ-app convention and the desk/entity convention are
genuinely interchangeable front ends onto one real game, a pattern every
later game with both a spatial AND a quick-check-in presence can copy).

## Status
- Desk created, empty. NO `toy.pdl` yet — deliberately held back until
  there's real launch content behind it (a toy.pdl with nothing real to
  launch would be a dead menu entry, against house convention).
- The core ledger-driven economy mechanic (§6) not started.

## Real system dependencies
- Ledger-driven multi-entity economy (this is its real proving ground)
- Menu/status-screen game UI (nav-driven `.chtpm`, no map view)

## Ideas / synergies (running notes, not commitments)
-

## Open questions
-
