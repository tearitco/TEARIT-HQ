# 09 — Appendix

- `HANDOFF-2026-09-02.md` — **read this FIRST if resuming cold after
  2026-09-02.** New working directory, what shipped today (path
  migration both phases, the full book rebuild, a friend's
  network-browser branch review, 3 new architecture/security docs),
  and an in-progress, not-yet-verified login-menu label fix with exact
  next steps to finish it.
- `CROSS-REFERENCE-INDEX.md` — slim "topic X → chapter/file Y" map
  (includes 2026-09-08 tilesets/events/clone pointers).
- `S1_HOUSE_PATH_MIGRATION.md` — the 2026-09-01 house-path migration
  record (both phases done). Moved verbatim.
- `CALENDAR-LOG-ARCHIVE.md` — the historical daily log/todo/progress
  archive, moved (near-)verbatim from `1.^V-hq/INDEX.md`. This was the
  house's own dated-entries convention long before this book existed
  — kept as the archive tail; **new dated entries going forward should
  be short additions here, or better, folded directly into whichever
  chapter (usually `04-bugs` or `08-roadmap`) the outcome matters to**,
  per this migration's own condense-don't-just-relocate principle.
- `PC-HQ-BOOK-PAGE-SYNCH.md` — Player > Synch row (pc-hq tab 5 and
  taskbar cell 9), why livedesk BOOK:PAGE and pc-hq Book/Page are
  different files, and the open copy questions. The durable front
  door is now `../18.pc-hq/00-INDEX.md`.
- `WINDOWS-TASKBAR-PORT.md` — **the khtpm taskbar on Windows.** Read this
  before changing the taskbar on Linux, or after a Linux change, to get
  Windows back up to speed. Covers the header-funnel port (why
  `khtpm_core_render.c` compiles unmodified and must stay that way), the
  `build_khtpm_strip_win.ps1` / `run_khtpm_strip_win.ps1` twins, the
  two load-bearing gcc flags, why WMI `Win32_Process.Create` is the only
  bulletproof Windows `setsid`, the shim's real limits, and why the
  bottom bar looks empty (it is a data fact: `n_tabs=1`). **§8 is the
  one to read before touching navigation**: it documents the
  `g_package_dir` dirname bug (backslash paths left it pointing at a
  *file*, killing the bottom-bar peer window and forcing a second
  renderer process with its own stuck-at-1 `g_focus_nav`) and the
  `WH_KEYBOARD_LL` nav-key hook plus the `XGetInputFocus()` fix, without
  which the grab cancels itself on the first tick.
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
