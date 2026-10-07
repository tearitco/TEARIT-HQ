# Dwarf Fortress — dev log

The connective-tissue game: player-managed avatar delegates work to other
entities via a real quest/task-board, generalizing Monster Rancher's own
single-avatar tamagotchi-care into Civ-scale multi-entity management. Also
the intended natural home for testing farming mechanics + z-level/3D digging
(back-and-forth with piececraft-hq).

Desk: `dwarf-fortress-test` (new, real, live — see
`sessions/s1/desks/dwarf-fortress-test.pdl`).
Roadmap entry: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` §3.13, §4.4, §7, §8.

## Status
Desk created, empty — tinkering/synergy-scouting stage, nothing built yet.

## Real system dependencies
- Quest/task-board delegation (new, this is its proving ground)
- Calendar/time-tick engine (needed from day one)
- Existing desk/entity/event trigger layer (a posted task = another real
  event an entity's own trigger can pick up and act on)

## Ideas / synergies (running notes, not commitments)
-

## Open questions
-
