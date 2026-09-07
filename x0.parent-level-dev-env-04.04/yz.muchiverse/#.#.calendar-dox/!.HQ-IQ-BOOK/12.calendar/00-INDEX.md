# 12 — Calendar

Dated, day-scoped work log: what we're doing/2do on a given day, one
directory per date (`YYYY-MM-DD/`). This is where a `11.brainstorm/`
doc's ideas land once they're actually picked up as real, scheduled
work — the "today's real todo list" layer, distinct from:
- `11.brainstorm/` — pre-decision idea/option exploration, not yet
  scheduled.
- `09-appendix/`'s own handoff/progress docs — cross-session state and
  in-progress feature tracking, not strictly day-indexed.
- `04-bugs/BUG-LOG.md` — the append-only bug tracker.

Each day's directory can hold more than one file if useful (e.g. a
`2do.md` plus a `notes.md`), but should always have at least a short
"what happened / what's next" entry so a future session can scan
`12.calendar/` chronologically and get a real timeline without reading
every chat log.

## Contents

- `2026-09-05/` — taskbar dock keyboard-focus regression (fixed +
  documented in `03-pitfalls/`), swatch-picker data-driven color list,
  dock label/badge contrast fixes; font-size/UI-scale brainstorm
  started, not yet scheduled.
- `2026-09-07/` — pc-hq board: clicking File/Desk trapped the user in
  Interact Mode (auto-engaged, never released) → arrows forwarded to the
  game, local nav frozen on the File item. Fixed in `pchq_board_action.sh`
  (engage-if-off + restore); `09-appendix/pc-hq-bugs.md` Bug 5.
- `2026-09-06/` — periodic-picker Down-arrow bug fixed by reusing the
  existing scroll path (pitfall #14 + skill update); `main` unified to
  both agents' work (`git push origin opencode:main`, FF to
  `4290b4a0`) + git-for-newbies walkthrough; cross-platform (Win/Mac)
  handoff-friendliness concern captured — docs still live under
  `07-install-and-ship/windows-mac/`, `khtpm_core_render.c` still has
  no Windows twin (CROSS-PLATFORM-PENDING #1), today's change is
  port-positive (removed bespoke code, routed through OS-free shared
  helpers).
