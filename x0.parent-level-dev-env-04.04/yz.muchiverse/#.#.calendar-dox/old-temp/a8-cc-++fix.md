# Navigation Numbers Bug: Fixes + Architecture Warning for Future Agents

## Status: bug fixes applied and building clean in `tp_taskbar.c` (the LEGACY file — see
critical architecture warning at the bottom before touching this again).

## Problems fixed this session
1. Nav numbers auto-incremented every poll cycle (97→98→99...) — stale
   `KIND=btn` entries from dead taskbar PIDs were never pruned.
2. Numbers started too high (14+) instead of 1 — `sync_tab_claims()` counted
   `KIND=btn` rows when computing `max_nav` for tabs, which fed back into
   `sync_strip_claims()`'s own max_nav and compounded every restart.
3. Strip buttons and bottom-bar tabs were interleaved instead of ordered —
   fixed by making strip buttons always claim fixed nav `1..n_cells`, tabs
   always start at `n_cells+1` (currently 8).
4. `[ ]`/`[>]` selection brackets weren't shown on strip buttons at all —
   `format_strip_cell()` only drew them in armed mode; now always drawn
   when `nav > 0`.
5. Arrow-key nav (armed mode) never moved focus onto the strip buttons at
   all — only cycled bottom-bar tabs. Added `strip_focus_cell` cycling
   through buttons 1..n_cells before falling through to tabs, wrapping
   both directions.
6. **Two cursors bug**: right-clicking to arm nav showed `[>]` on BOTH
   strip button 1 AND bottom-bar tab (nav 8) simultaneously. Root cause:
   `draw_bar()`'s own focus-highlight logic only checked `nav_armed`, not
   whether focus had actually moved past the strip into tab range. Fixed
   by adding a `strip_focus_cell` parameter to `draw_bar()` and only
   showing a tab as focused when `nav_armed && strip_focus_cell < 0`.

## Debug infrastructure added (per direct instruction to stop guessing blind)
`tp_taskbar.c` now writes real, inspectable state to
`#.desktop/tp_taskbar_debug/`:
- `strip_frame.raw` + `strip_frame.receipt.txt` — an actual RGBA capture of
  the strip window (via `XGetImage`, the TRUE rendered pixels, not a
  reimplemented rasterizer) using the same raw+receipt contract
  `014.wsr-pal💸️📌️+2/ops/dump_rgb_png.c` already reads (frame_w/frame_h keys).
- `strip_frame_log.txt` — one block per real frame change, listing every
  cell's label/nav/focused state.
- `strip_frame_changed.txt` — append-only marker file. `draw_strip_if_marked()`
  only redraws + recaptures when this file's SIZE grows, mirroring
  `@.apps/piececraft-xyz/system/chtpm_parser_pal.c`'s own documented
  contract ("RENDER TRIGGER — MARKER-DRIVEN, SINGLE SOURCE OF TRUTH...
  compose_frame() ONLY fires when frame_changed.txt grows... the marker
  file IS the throttle").
- `key_history.txt` — every armed-mode keypress logged BEFORE it's acted
  on, mirroring `keyboard_input.c`'s `append_key()` ordering
  (`pieces/keyboard/history.txt`).

To inspect visually: read the receipt for `frame_w`/`frame_h`, then decode
`strip_frame.raw` (RGBA8888, row-major) into a PNG (e.g. with
`stb_image_write.h`, same header `dump_rgb_png.c` already vendors).

---

## ⚠️ CRITICAL ARCHITECTURE WARNING FOR FUTURE AGENTS

**`tp_taskbar.c` (where all of the above was fixed) is marked LEGACY.**

Per `&.widgits/tile-picker/ops/KHTPM-ARCH.txt`:
```
Taskbar — ONE logic set (done)
------------------------------
  khtpm_taskbar_core.c/h     SHARED: tabs, nav claims, shortcuts, theme,
                             digit jump, ACTIVATE/OPEN_CONTEXT, quit+save
  khtpm_taskbar_main.c       shared main
  khtpm_taskbar_plat_win.c   Win draw/events only
  khtpm_taskbar_plat_x11.c   X11 draw/events only
  livedesk-taskbar/ops/tp_taskbar.c     LEGACY — do not add design here
  livedesk-taskbar/ops/tp_taskbar_win.c LEGACY stub
```

The intended architecture (matching how the top strip and bottom bar
*should* work, and matching how the entity window already does via
`khtpm_core.c` + `khtpm_plat_x11.c` + `khtpm_main.c`) is:
- **One shared "core"** file holds all design logic — state structs, nav
  claim math, digit-jump/arm logic, focus cycling — platform-independent.
- **One shared "main"** wires core init to whichever platform backend.
- **A thin platform file** (`_plat_x11.c` / `_plat_win.c`) does ONLY
  drawing and raw event translation, delegating all real logic back to
  core. This is what lets the top bar and bottom bar (and Windows/Linux)
  run the exact same nav/focus/digit-jump behavior instead of two
  divergent hand-rolled copies.

**Real, present danger found while fixing this bug**: `khtpm_taskbar_core.c`
has **zero strip-button support** — no `StripCell`, no `sync_strip_claims`,
no HQ popup, nothing (confirmed via `grep -n strip khtpm_taskbar_core.c`,
zero hits). Yet `&.widgits/tile-picker/ops/build_khtpm.sh` builds
`khtpm_taskbar_main.c + khtpm_taskbar_core.c + khtpm_taskbar_plat_x11.c`
and **copies the result over
`&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x`** — the exact binary
this whole strip feature (and today's fixes) lives in. **Running
`build_khtpm.sh` right now would silently delete the entire top strip
(HQ/user/file/desks/player/db/plugins) from the running taskbar**, since
the khtpm-core build has no idea the strip exists.

### What a future agent should actually do
Do NOT keep patching `tp_taskbar.c`. Instead, port the strip feature
(StripCell/StripBtn structs, `sync_strip_claims`, `open_cell_popup`,
`draw_strip`, the `strip_focus_cell` arrow-nav cycling fixed today, and
the HQ/submenu popup machinery) into `khtpm_taskbar_core.c` +
`khtpm_taskbar_plat_x11.c`, following the exact same core/plat split the
entity window (`khtpm_core.c` + `khtpm_plat_x11.c`) already uses. Once
that's done, `tp_taskbar.c` can be retired the same way
`KHTPM-ARCH.txt` already plans to retire `tp_desktop_window.c` once
`khtpm_plat_x11.c` is complete for the entity side. Until that port
happens, **do not run `build_khtpm.sh`** — it will regress the live
taskbar by dropping the strip entirely.

This doc should be treated as the authoritative note on that gap until
the port happens.
