# 2026-09-05

## Done today

- **text-edit-hq and csv-hq — BUILT, verified, registered as real
  toys.** `@.apps/text-edit-hq/` (plain-text editor: New/Open/Save/
  Save As) and `@.apps/csv-hq/` (small CSV/spreadsheet editor:
  New/Open/Save + SUM/AVG/MIN/MAX/COUNT over a named column, cells
  addressed by typed ref like "B3"). Both managers delegated whole to
  a fresh Haiku agent each (parallel), both compiled clean first try
  against a complete spec. Verified directly by driving each
  manager's action/UI files (not X11 click simulation, which is
  unreliable in this sandbox per earlier notes) - SAVE/SAVEAS/NEW/
  SETCELL/FUNC and the shared File Explorer pick-poll path all produce
  exactly the expected published state and on-disk file changes. Real
  v1 scope, documented in each app's own xhtpm header: Save As has no
  File-Explorer SAVE-mode (typed path only, File Explorer is LOAD-only
  today); csv-hq caps at 26 cols (A-Z)/500 rows, plain-comma CSV (no
  quoted-comma support). Checked the external CSV ops tree pointer
  first (see prior todo below) - reused its 2D-grid + function-bank
  shape as built-in functions rather than porting its code directly.
  Also fixed a real regression from earlier today: `PDL_OPENFILE`'s
  own File Explorer launch call still passed `house_root` as argv[1]
  after button.sh was rewritten to the toy.pdl `run`-only convention,
  so clicking "file" in pdl-read silently launched nothing (live
  report: "opened a mess within same window, didn't see any files") -
  fixed the call site, confirmed live that File Explorer now opens as
  its own separate real X11 window as originally designed.
- **pdl-read — BUILT, live-verified, registered as a real toy.**
  `@.apps/pdl-read/` — a real paginated document reader, demonstrated
  reading the house book's own docs (`docs.pdl` lists 9 real book
  pages). Manager delegated to a fresh agent against a complete spec;
  `GOTOPAGE:<n>` added directly afterward for a real PDF-style page-
  jump sidebar (per direct live feedback, along with moving the page
  indicator above the content and Prev/Next below it - the first
  layout was crowded into a side panel). Registered via a real
  `toy.pdl` (title/launch keys) - the "toys" dock cell already scans
  for these live, no taskbar rebuild needed. New house-wide capability
  that came out of building this: a generic `content="${var}"`
  attribute for `<text_area>`, since its `label=` turned out NOT to be
  newline-safe through the frame round trip (only `text_area_buffer`
  has that escaping) - found live when a multi-line label corrupted
  the frame file past that element. Also re-learned a real gotcha
  already hit once today: `g_current_page` defaults to `"main"`, not a
  template's own first `<page>` - naming a first page anything else
  silently produces a 0×0 window.

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
- **`102.agy-txt`'s legacy launcher bug — FIXED.** Root cause: `button.sh`
  `cd`s into a per-session directory, then still invoked
  `./system/renderer` etc. as RELATIVE paths left over from before a
  refactor removed the old symlink-into-session approach. Checked every
  house-level project with its own `button.sh` — only `102.agy-txt` and
  `102.editor-📄️00.00` had this bug; both fixed and live-verified. Not
  a "mark this binary legacy" situation (the binaries were always
  fine) — full list and reasoning: `102.agy-txt/
  LEGACY-LAUNCHER-PATH-FIX-2026-09-05.md`.
- **`cli_io` real cursor + new `<text_area>` element — IMPLEMENTED.**
  Both built and live-verified same day as planned (see
  `08-roadmap/design-docs/CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-DESIGN.md`,
  now marked implemented). `cli_io` gained real left/right/home/end/
  insert/delete-at-cursor editing (was append/backspace-at-the-end
  only). `<text_area>` is a real, working multi-line element: actual
  `\n` preserved, Enter inserts a newline instead of submitting,
  logical-line Home/End/Up/Down, its own save file and frame
  round-trip field. Found and fixed a real related bug while verifying
  visually (via `dump_frame_png_op` invoked directly by window ID, so
  an armed field's true state could be checked without disarming it
  first): the cursor bar was gated on nav-focus equality instead of
  the same real armed-state check the `^` badge already used
  correctly — a tabbed-past-but-not-armed field could show a
  misleading cursor. Fixed before commit.
- **File Explorer widget — BUILT, live-verified.** First real app off
  today's `text_area`/`cli_io` work:
  `&.widgits/file-explorer/` (manager, static `.xhtpm`+CSS, `button.sh`).
  Directory scanner manager delegated to a fresh agent against a
  complete, self-contained protocol spec (no house context needed);
  argv contract adjusted afterward to match `launch_module()`'s real
  convention. New `FE_ENTRY:<n>`/`FE_SAVEAS`/`FE_CANCEL` dispatch verbs
  in `khtpm_core_render.c`, same in-process convention `PICK:<n>`
  already uses. Two real bugs found and fixed while live-verifying: a
  `vars=` path that doubled this widget's own directory (silently
  expanded every `${var}` to nothing), and `<scrolllist>` needing a
  real `<sidebar>`+`<panel>` pair to be laid out at all (a bare
  top-level one just sits at 0×0 forever). Verified end to end via
  real X11 clicks: navigate into a real directory, pick a real file,
  manager writes the correct absolute path and exits on its own. `pdl-
  reader` and the `toys` editor's own Save/Load still depend on this
  but aren't built yet - see the brainstorm doc's own updated status.
  A v2 idea came up live while testing: use the widget's right-hand
  panel (currently just a hint + Cancel) for a file preview or a
  visual directory-tree view - real, not implemented this pass.

## Not started / open

- **Font size + UI scale in Settings** — brainstormed, not scheduled.
  See `11.brainstorm/2026-09-05/FONT-SIZE-AND-UI-SCALE-BRAINSTORM.md`.
  Real finding from today: the existing `font_scale` PDL key and
  `scaled()` choke-point function are NOT wired together — `scaled()`
  is currently a plain identity function, and the `g_dbhq_font_scale`
  variable its own comment describes doesn't exist in the file at all.
- **`toys` text-editor refactor** — brainstormed, not scheduled, same
  doc as above (§5). Real UX reference captured (tpmos's
  `agy-text-editor` loader → editor → FILE MENU → Save As flow). Its
  own Save/Load wiring now has the File Explorer widget to use, once
  the editor itself gets built.
- **New toy: CSV/spreadsheet editor** — brainstormed, not scheduled,
  direct request ("while we do the text editor, we also want to make
  a spreadsheet/csv editor with some functions"). Build alongside the
  `toys` text editor above (same File Explorer Save/Load wiring should
  apply). Direct pointer to reuse: an existing external CSV app/ops
  tree already has "some existing conventions/ops that maybe helpful"
  — no nav yet, but other useful pieces — at
  `/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/📲️📵️.GTFO.flxbx.📦️arcbit♏️/0.arc.bit.📲️📵️(✳️📳️)]a2/🧿️✳️.HALO:A*STD.MODS📍️/♍️]csv.irgo]🇬🇮️📠️]a0/`
  — check it before starting from scratch.
- **New desktop entity type: `FLEXFOLDER`/`FLEXBOX`** — brainstormed,
  not scheduled, direct request. A real, droppable-into desktop
  container entity (📁 2D "FLEXFOLDER", 🗄️ 3D "FLEXBOX" in 3D mode) that
  other entities/windows/dirs can be dropped into, itself openable as
  a 2D view, a 3D view, or a file-explorer-style listing later. Noted
  here for now - no design doc yet; needs its own real brainstorm
  before this graduates past a one-line idea (real questions: how does
  "drop an entity into it" actually work given the house's existing
  desktop-entity/tile conventions, does it reuse the File Explorer
  widget built today for its own "open as file explorer" mode, is
  FLEXBOX a real 3D mode or FLEXFOLDER's own icon just changing while
  2D/3D camera mode toggles house-wide).
  Depends on the File Explorer widget above for its own Save As/Load.
- Taskbar instance-count guard / "x&lt;N&gt;" sanity indicator next to
  the PID (requested earlier this week, never started).
- The bigger, deliberate pass considered (but not attempted today) of
  removing `g_override_redirect` entirely for every persistent
  top-level window, matching the house's own standing WM-managed rule
  — flagged in today's pitfalls entry as a real future candidate, not
  a same-day follow-on to an urgent bug report.
