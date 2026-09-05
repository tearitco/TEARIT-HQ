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

## Not started / open

- **Font size + UI scale in Settings** — brainstormed, not scheduled.
  See `11.brainstorm/FONT-SIZE-AND-UI-SCALE-BRAINSTORM.md`. Real
  finding from today: the existing `font_scale` PDL key and `scaled()`
  choke-point function are NOT wired together — `scaled()` is
  currently a plain identity function, and the `g_dbhq_font_scale`
  variable its own comment describes doesn't exist in the file at all.
- Taskbar instance-count guard / "x&lt;N&gt;" sanity indicator next to
  the PID (requested earlier this week, never started).
- The bigger, deliberate pass considered (but not attempted today) of
  removing `g_override_redirect` entirely for every persistent
  top-level window, matching the house's own standing WM-managed rule
  — flagged in today's pitfalls entry as a real future candidate, not
  a same-day follow-on to an urgent bug report.
