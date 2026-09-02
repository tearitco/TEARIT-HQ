# 09 — Appendix

- `HANDOFF-2026-09-02.md` — **read this FIRST if resuming cold after
  2026-09-02.** New working directory, what shipped today (path
  migration both phases, the full book rebuild, a friend's
  network-browser branch review, 3 new architecture/security docs),
  and an in-progress, not-yet-verified login-menu label fix with exact
  next steps to finish it.
- `CROSS-REFERENCE-INDEX.md` — slim "topic X → chapter/file Y" map.
- `S1_HOUSE_PATH_MIGRATION.md` — the 2026-09-01 house-path migration
  record (both phases done). Moved verbatim.
- `CALENDAR-LOG-ARCHIVE.md` — the historical daily log/todo/progress
  archive, moved (near-)verbatim from `1.^V-hq/INDEX.md`. This was the
  house's own dated-entries convention long before this book existed
  — kept as the archive tail; **new dated entries going forward should
  be short additions here, or better, folded directly into whichever
  chapter (usually `04-bugs` or `08-roadmap`) the outcome matters to**,
  per this migration's own condense-don't-just-relocate principle.
- `GLOSSARY-APPENDIX.md` — pointer; the core glossary lives at
  `01-orientation/GLOSSARY.md` and didn't need a fuller version in this
  pass.

## Dead references cleaned in this pass

`CALENDAR-LOG-ARCHIVE.md` (the old `INDEX.md`) already flagged ~15
`archive/<file>.md` pointers as dead (the `archive/` folder was
deleted 2026-08-29, references never cleaned up) and 3 docs explicitly
marked ARCHIVED/stale in-place (`archive/HAIKU_TASKS.md`,
`archive/DB_CONTEXT.md`, `archive/EVENTS_ROADMAP_NEXT_STEPS.md`) —
since all of those already lived inside the deleted `archive/` folder,
there was nothing left on disk to delete; the stale references
themselves are left as historical record inside the archive log
(consistent with its own append-only convention), not scrubbed, since
they're already clearly marked dead in their own text.
