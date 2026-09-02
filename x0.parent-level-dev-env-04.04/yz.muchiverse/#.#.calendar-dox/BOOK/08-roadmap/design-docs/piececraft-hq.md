# Piececraft-HQ progress report (2026-08-30)

Consolidated status of the khtpm board-window work this session. See
`PIECECRAFT-HQ-BOARD-KHTPM-CONVERSION-2026-08-30.md` and
`CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` for the
full design history this builds on.

## ✅ Done, real, live-verified

- **Real input fixed**: the khtpm board window was `override_redirect`,
  which Mutter never routes real keyboard/mouse to (synthetic XTest
  input worked, masking the bug). Converted to a normal WM-managed
  window (`_MOTIF_WM_HINTS` decorations=0), matching `x11_mirror.c`'s
  own real precedent. (`402c812b`)
- **Restyled** the window's chrome: real khtpm nav-button styling
  (orange focused-border, gray unfocused) instead of raw blitted ASCII,
  plus real window transparency via `set_window_opacity()`. (`562eb172`,
  `b40afcd5`)
- **History-injection test checklist** written and proven - lets any
  agent verify the render/relay pipeline directly via file injection,
  no GUI needed. (`9430abe8`)
- **Taskbar's own File/Desk menus** converted from C-hardcoded to real
  PDL-driven (closing a real, separately-documented debt item found
  along the way). Two real bugs found and fixed in the same pass: a
  desk-switch data-loss bug (empty-registry snapshot could wipe a real
  populated desk file) and a taskbar-restart race. (`89fb13c2`,
  `a2b7e8df`)
- **Real default level/map fixtures** created:
  `@.apps/piececraft-hq/defaults/default-legacy/` (verbatim copy of the
  current chunk/world storage) and `defaults/default-pdl/` (the real
  hybrid BOARD-manifest format proposed in the design doc). (`70541223`)
- **File/Desk/Close are now real toolbar buttons** in
  `board_viewer.chtpm`'s own layout - same nav-index/arrow-nav/Enter/
  focus-highlight system every other numbered row uses (Interact Mode
  included), not a hand-drawn khtpm-side badge. Wired to real actions:
  File cycles between the two level fixtures (copying their chunk/world
  files into the live read path), Desk reloads the active level's
  board manifest, Close writes a real flag file the khtpm window polls
  (since the legacy engine has no concept of a separate outer window
  to close itself). (`2ab686d6`, `53e60d67`)
- **Window close fully fixed**: was leaving a blank "ghost" window
  alive - added `XDestroyWindow()`+`XSync()` before `XCloseDisplay()`
  and real `WM_DELETE_WINDOW` `ClientMessage` handling. (`2ab686d6`)

## 🔧 A real bug found and fixed along the way (worth flagging on its own)

`bv_menu_input.c` has a pre-existing `if (key >= '1' && key <= '4')`
camera-mode-switch range check. File/Desk/Close's first pass used key
codes 2/3/4, which that range silently intercepted before they ever
reached the real handlers - meaning File and Desk had never actually
dispatched anything at all, they'd been silently changing camera mode
instead. Confirmed via direct instrumented testing (strace + inline
debug prints across several rebuild cycles), not guessed. Moved to
genuinely free codes 5/6/7, checked against the file's full real key
list. Live-verified end to end via direct binary invocation (not just
visual inspection) that all three now dispatch correctly.

## ⏳ Still outstanding (not started)

1. **Level/map picker UI** - File/Desk currently just *cycle* between
   the fixtures that exist (2 levels, 1 board each) rather than showing
   a real pick-list. Fine for proving the mechanism; a real list UI is
   real future work once there's more than one board to choose from.
2. **Header/toolbar redesign** (direct, repeated live instruction,
   not yet started):
   - Real header row order: `1.HQ`, `Desk`, `File`, `4.Menu`.
   - The current plain-text info dump (Focused on/Possessing/Display/
     Z-level/Camera status block) moves under `4.Menu`, not shown by
     default.
   - Interact Mode moves to the top of the 3D view area itself, not
     mixed into the toolbar row.
   - Emoji-based compact icons instead of full words, to leave room for
     a toolbar clock.
3. **Events system for boards** - confirmed new scope in the design
   doc, not started.
4. **Minecraft-tile palette integration** (`#.NNEST_ASSETS/
   mc_extracted_csvs_8x8`) - confirmed separate, not-yet-built work;
   boards render with emoji tiles as a placeholder for now.
