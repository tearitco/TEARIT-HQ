# Q006 — one generic board layout for grave, ghost roster, server and phone windows

| field | value |
|---|---|
| status | open — after the overlay/nav work in the pc-hq layouts plan is settled |
| tier | manager (claude) or outside-agent with layout experience |
| size | M |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | none to start; look-and-feel review by the owner at the end |

## Mission (one sentence)

Build `board.chtpm` (+ css) once, driven by a small `board.pdl` data-source file, so the grave, the ghost roster, the server and the phone windows are the same layout with different data.

## Why it matters

All four windows are a list of rows, a detail/history pane and actions (design `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3d). One layout means one fix
fixes all of them, the owner edits it once in the layout studio, and no per-window C gets written (house rule: zero new per-project renderer code).

## Read first

1. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3d
2. `#.#.calendar-dox/!.HQ-IQ-BOOK/18.pc-hq/IN-GAME-LAYOUTS-PLAN.md` and `08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md` (the layout, overlay and nav machinery; what is built vs planned)
3. The house standards skill `khtpm-house-standards` (read in full before touching the renderer; do not add a `g_is_<project>` branch; edit the shared-lib draw core, not the ops copy)
4. `@.apps/layout-studio/sandbox/` (a working layout with `<overlay src>`, `vars="..."`, `<repeat>`)

## Do

1. A `board.chtpm` with: a header row (title + minimize/close chrome), a scrolling row list (`<repeat count="${n_rows}" bind="row">`), a detail pane (tail of a file), an action row. Every interactive element nav-numbered.
2. `board.pdl` (`SOURCE | ...`, `ROW | label=... id=...`, `DETAIL | file=...`, `ACTION | label=... cmd=...`) read by a small generic op that writes the vars file the layout reads; no per-window C.
3. Prove it with two sources: the quest board (`^.grave/quests/INDEX.md` data) and a roster (`^.ghost/roster/*/ghost.pdl`).

## Acceptance

- [ ] Both windows render from the same `board.chtpm`; a frame dump of each (paste paths) shows rows, detail and actions.
- [ ] Every row/button has a nav number; a typed number + Enter and a real click both activate it (relay AND real key).
- [ ] Changing one css rule in `board.css` changes both windows.
- [ ] No new renderer branch; build warnings none.

## Rules

Everything in `^.grave/README.md`. Test windows are launched by you; do not reset the owner's desktop.

## Log

## Result
