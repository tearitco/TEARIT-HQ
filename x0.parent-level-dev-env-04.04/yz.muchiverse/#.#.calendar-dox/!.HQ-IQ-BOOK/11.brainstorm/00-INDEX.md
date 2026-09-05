# 11 — Brainstorm

Raw planning docs for a feature BEFORE it's scoped and scheduled.
This is the "thinking out loud on paper" stage — options considered,
open questions, current-state findings — not yet a committed plan.

**Organized by date, same convention as `12.calendar/`**: one
`YYYY-MM-DD/` directory per day, holding that day's brainstorm docs.
A brainstorm doc doesn't get "finished" and moved on a later date —
it stays filed under the day it was started; a later day's follow-up
thinking on the same topic gets its own dated doc (or a note inside it)
that cross-references back, rather than editing history in place.

Once a brainstorm here solidifies into real, scheduled work, split it:
- The **decision** (what we're actually building, in what order) goes
  into `12.calendar/` under the day it's picked up, as that day's
  2do/log entry.
- A genuinely large feature's own detailed design doc still belongs in
  `08-roadmap/design-docs/` (the house's existing convention for that)
  — a brainstorm doc here can graduate into one of those once it's
  real, rather than growing in place indefinitely.

A brainstorm doc is allowed to be wrong, incomplete, or list options
we don't end up taking — that's the point of doing it here first.

## Contents

- `2026-09-05/FONT-SIZE-AND-UI-SCALE-BRAINSTORM.md` — user-adjustable
  font size in Settings, and what "surrounding sizes" (buttons, window
  chrome, row heights) would need to scale with it.
- `2026-09-05/PDL-READER-AND-FILE-EXPLORER-WIDGET.md` — a document
  reader ("pdl-reader") for PDL-indexed readable docs (PDF-like
  zoom/page nav), which needs a not-yet-built "file explorer widget"
  shared component; the same widget should also back real save-as/load
  flows house-wide (currently missing/broken there). Includes the
  root-caused `102.agy-txt` legacy-launcher bug found while
  researching this, and the tpmos `agy-text-editor` reference UX this
  session captured live for a future `toys` text-editor refactor.
