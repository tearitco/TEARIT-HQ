# 2026-09-05

## Done today

- **Swatch picker made data-driven** (`&.widgits/taskbar-settings/`):
  new shared `swatches.pdl`, a generic `bg="${var}"` xhtpm attribute
  (`Elem.bg` → `apply_attr()` → `draw_elem()` override), and a
  `<repeat>` block replacing 12 hardcoded `<item class="sw-<name>">`
  tags + matching CSS. Added grey, brown, tan with zero C/CSS changes
  needed for the last one — proves the refactor did what it was for.
- **click_two_step live-reload fix**: the taskbar (long-running
  process) never re-read `hq_ui.pdl` after startup, so toggling the
  setting in Settings never reached it. Fixed via `hq_idle_tick()`.
- **Dock badge-chip gap, round 2**: a short dock-cell WITH a sprite
  (the taskbar's own avatar/username badge) fell through all three
  badge-chip branches in `draw_elem()` with no chip at all — a real
  gap in the "every badge gets a chip" fix from the day before.
  Restructured to a `chip_drawn` flag so the general chip is a true
  fallback, not sprite-gated.
- **Dock label contrast fix**: plain dock-cell label text
  (`"HQ"`/`"jb"`/entity names) was hardcoded `#cccccc`, unreadable
  against the current light/tan theme. Added a real luma check against
  `g_theme_bg` (the actual surface, unlike the earlier — wrong —
  contrast fix attempt for badges).
- **Taskbar duplicate-number bug**: the bottom "pals" row baked
  `"N. <entity>"` into the label text on top of the real, independently
  numbered nav badge. Fixed by publishing the plain entity name only.
- **Taskbar arrow-key nav regression, root-caused and fixed for real**:
  the dock strip's own keyboard/click reliability fix (`eb74b733`,
  2026-08-30) lived in a file that was later fully replaced
  (`khtpm_strip_parser.c` → `khtpm_core_render.c`'s dock mode) and
  never carried over — it just followed the shared, house-wide
  `g_override_redirect` PDL, which got reverted for an unrelated
  reason and silently broke the dock's real keyboard focus again. Now
  hardcoded unconditionally WM-managed for the two persistent dock
  windows, independent of that PDL. Full writeup + alternatives
  considered: `03-pitfalls/X11-AND-SESSION-PITFALLS.md`, 2026-09-05
  entry. Loud "never remove" comment at the fix site in
  `khtpm_core_render.c` (search `dock_managed`).
- Started `11.brainstorm/` and this `12.calendar/` chapter themselves.
  **Convention update, same day**: `11.brainstorm/` is now ALSO
  organized by dated (`YYYY-MM-DD/`) subdirectories, same as
  `12.calendar/` — the font-size doc below already lives under
  `11.brainstorm/2026-09-05/`, not loose in the chapter root.

## Not started / open

- **Font size + UI scale in Settings** — brainstormed, not scheduled.
  See `11.brainstorm/2026-09-05/FONT-SIZE-AND-UI-SCALE-BRAINSTORM.md`.
  Real finding from today: the existing `font_scale` PDL key and
  `scaled()` choke-point function are NOT wired together — `scaled()`
  is currently a plain identity function, and the `g_dbhq_font_scale`
  variable its own comment describes doesn't exist in the file at all.
- **`pdl-reader` + a shared File Explorer widget** — brainstormed, not
  scheduled. See `11.brainstorm/2026-09-05/PDL-READER-AND-FILE-EXPLORER-WIDGET.md`.
  A PDF-style document reader driven by a `.pdl` doc list, blocked on a
  not-yet-built, reusable File Explorer widget that should ALSO back
  every window's own Save As/Load flow house-wide (the taskbar's own
  `[ ]3.file:` cell currently has nowhere real to route to for this).
  Captured tpmos's own `agy-text-editor` file-browser UX live as a
  reference spec (search field + save-target field + suggestions +
  directory browse with human-readable sizes) — real, existing prior
  art at `&.widgits/file-menu/` and `102.agy-txt/manager/
  agy_browser_manager.c` worth reading before designing the khtpm-side
  version from scratch.
- **`toys` text-editor refactor** — brainstormed, not scheduled, same
  doc as above (§5). Real UX reference captured (tpmos's
  `agy-text-editor` loader → editor → FILE MENU → Save As flow);
  biggest open design question is whether khtpm's existing single-line
  `<cli_io>` element gets extended for multi-line text or a new
  element type is needed. Depends on the File Explorer widget above
  for its own Save As/Load, so building that first is the efficient
  order.
- **`102.agy-txt`'s legacy launcher bug — FIXED.** See
  `102.agy-txt/LEGACY-LAUNCHER-PATH-FIX-2026-09-05.md` for the checked
  list of every house-level project with its own `button.sh` (only
  `102.agy-txt` and `102.editor-📄️00.00` had this bug), the exact
  fix, and live verification of both. Original finding kept below for
  context.
  `sh button.sh r` fails with `./system/renderer: not found` /
  `./system/keyboard_input: not found`. The binaries are completely
  fine (confirmed via `file` + reading them directly); the bug is a
  real cwd/path mismatch in `button.sh` itself — it `cd`s into a
  per-session directory, then still invokes `./system/renderer` etc.
  as RELATIVE paths left over from before a real refactor removed the
  old symlink-into-session approach (see the doc above, §4, for the
  exact lines and the one-line-per-call fix). Not a "mark this binary
  legacy" situation — flagged explicitly in the doc why that framing
  doesn't fit this specific bug, though a genuine legacy-binary-marker
  house convention might still be worth its own separate brainstorm
  later.
- Taskbar instance-count guard / "x&lt;N&gt;" sanity indicator next to
  the PID (requested earlier this week, never started).
- The bigger, deliberate pass considered (but not attempted today) of
  removing `g_override_redirect` entirely for every persistent
  top-level window, matching the house's own standing WM-managed rule
  — flagged in today's pitfalls entry as a real future candidate, not
  a same-day follow-on to an urgent bug report.
