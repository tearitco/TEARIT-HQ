# X11, build, and session pitfalls

*Hard-condensed from `!.HOUSE_STDS.md` §F (22 numbered, live-caught
bugs), 2026-09-02. Full narrative reports (F-18, F-19 with complete
diagnostic trails) are cut here — the standing rule each one produced
is kept.*

- **`'q'` must never quit anything** — standing house rule. Verify
  before assuming any binary honors it; `gl_mirror.c` once had a
  leftover exception that was fixed at the source.
- **Camera orientation bugs are not obvious from code review** — verify
  empirically (pixel sampling), every time.
- **A 3D view's default camera must match the 2D view's own start**
  (board center/selector default), not `(0,0)`.
- **Don't unconditionally pad rendered cells with literal spaces** —
  the font rasterizer already advances by real pixel width per glyph.
- **A writable state file missing from `button.sh`'s copy-in +
  `persist_session_state()` lists silently vanishes on session
  cleanup** — recurring bug class, always audit both lists against
  everything your ops actually write.
- **Never create symlinks** — they break on Windows checkouts/zip
  transfers; the whole house migrated off them 2026-08-20/21.
- **`pkill` may be sandboxed/blocked** — use `ps aux | grep` +
  `kill <pid>` instead of trusting `pkill`'s exit code.
- **A session's `.pal` script and its copied `ops/` binaries load once
  at startup** — a rebuild needs a session restart to take effect, it
  does not apply live.
- **`system() + &` does not create a new process group** — use
  `setsid` when spawning something that must be genuinely independent
  of the spawner's own signal/timeout handling.
- **Relative commands executed via `system()` are CWD-dependent, and a
  launched process's CWD is whatever it inherited — not house root.**
  Real incident: a taskbar restart command resolved to
  `$.crypts/$.crypts/button.sh` (nonexistent) because the process
  inherited the wrong CWD, failing completely silently. Fix: any
  process executing PDL/user-configured command strings via `system()`
  must anchor relative tokens to `house_root` (resolve the first token
  against it, or `chdir(house_root)` at start). A stale-path sweep must
  also check relative indirection targets, not just absolute paths.
- **Bare `XSetInputFocus` on a new override-redirect window is not
  reliable under Wayland/Mutter (Xwayland-rootless)** —
  `XGetInputFocus` can report success while key events never arrive.
  Fix: raise-then-focus-then-flush (`taskbar_soft_focus()`'s sequence)
  before assuming a popup can receive input; verify empirically with
  real/injected keys and a visible state change, never trust
  `XGetInputFocus` alone.
- **XTest injection requires GLOBAL X focus** and will silently steal
  it from a real concurrent human user. For agent-driven
  testing/control, prefer file-relay polling (bypasses X11 focus
  entirely) — reserve XTest injection for diagnosing genuine
  human-input focus bugs specifically.
- **A persistent top-level khtpm/-hq window must be a real WM-managed
  window with decorations hidden via `_MOTIF_WM_HINTS`, not
  `override_redirect`.** `override_redirect` windows are exempt from
  window-manager focus handling by X11 protocol definition — no
  client-side call fixes that for a long-lived window that must regain
  focus after the user's attention has been elsewhere. Short-lived
  popups/submenus correctly stay `override_redirect` — this rule is
  for persistent top-level windows only (db-hq/events-hq/chat-hai's
  merged window setup in `khtpm_core_render.c` is the reference
  implementation to copy).
- **Every navigable list in a khtpm/-hq window uses the same real
  digit/arrow/Enter nav — including submenus/pickers opened from it.**
  A sub-list gets its own separate focus counter (must not collide
  with the outer window's), but the same nav convention and badge
  rendering, never a bespoke "press 1/2/3 directly" scheme.
- **A path derived by climbing a fixed number of `..` from an
  ARGUMENT breaks the instant that thing can live in more than one
  place.** Climbing from your own script's fixed location
  (`dirname "$0"`) is safe; climbing from a caller-supplied variable
  location is not. When a fork/exec chain has no argv room left for
  context, use an exported environment variable — it survives `exec`
  even when argv can't grow.
- **Doc comments and cross-references go stale** — verify against real
  code/the doc's own latest dated entry before trusting a claim about
  current status, especially anything load-bearing.
- **2026-09-04 real incident confirming the rule two bullets up**:
  `#.desktop/livedesk_override_redirect.pdl` is a single, HOUSE-WIDE
  setting (`khtpm_core_render.c`'s `g_override_redirect`, read once at
  startup, no per-window-class override anywhere) — it does not
  distinguish "persistent top-level window" from "short-lived
  popup/dropdown." Flipping it to `override_redirect=false` house-wide
  (to test the exact fix this file's own rule above prescribes, for
  one specific window — pc-hq's board — that needed real keyboard
  focus) made the taskbar's OWN dropdowns/popups (the "toys" menu
  specifically) stop working entirely — "no windows are opening from
  tb now," confirmed by direct live user report, immediately after the
  flip; confirmed working again immediately after reverting. Root
  cause not fully diagnosed (most likely candidate: Mutter takes over
  click/focus/positioning for a WM-managed popup in ways an
  `override_redirect` popup never experiences, breaking the "click a
  dropdown row" interaction the toys launcher and similar menus depend
  on) — flagged here as a real, live-confirmed hazard rather than
  fully root-caused. **Standing rule going forward**: this setting
  must never be flipped house-wide again. If a specific persistent
  window (matching this file's own rule above) genuinely needs real
  WM-managed focus, it needs its OWN scoped flag — e.g. a per-window
  override read from the window's own `<window>` class or a dedicated
  argv/env signal at that one window's launch site — never the shared
  global. Before attempting this again: (1) add the scoped flag first,
  (2) test the ONE target window in isolation, (3) explicitly verify
  the taskbar's own dropdowns/popups still work before calling it
  fixed, not after.
- **2026-09-05 — this exact bug came back, in the taskbar itself this
  time, because the scoped fix above never actually got written.**
  Direct live report: "when i click tb, i expect arrows to control
  'nav', this broke again. remember it broke before? remember the
  fix? make sure it never happens again. is a bad bug." This is a
  REPEAT of a bug that was already found and fixed ONCE before, for an
  OLDER file (`eb74b733`, 2026-08-30, "Fix taskbar's own intermittent
  click failures: WM-managed, not override_redirect" — targeted
  `khtpm_strip_parser.c`, the taskbar's implementation at the time).
  That file was later fully replaced by `khtpm_core_render.c`'s own
  dock mode (this file), which never inherited the fix — it just read
  the shared `g_override_redirect` PDL like every other window, so the
  very next time that PDL got reverted to `true` for an UNRELATED
  reason (the toys-menu regression two bullets up), the taskbar's own
  arrow-key/click reliability silently broke again as a side effect,
  with nobody realizing the two bugs were connected until this report.
  **This is exactly the failure mode this file exists to prevent — a
  known, already-fixed bug returning because the fix lived in a file
  that got deleted/replaced, not in a place a future refactor would
  see.** Real fix this time (commit history around 2026-09-05,
  `khtpm_core_render.c` — search `dock_managed` in that file, comment
  begins "REAL FIX 2026-09-05... never remove this comment"): the two
  PERSISTENT dock windows (`win` when `window_is_dock()`, and
  `g_dock_peer_win`, the bottom "pals" row) are now unconditionally
  WM-managed (`override_redirect = False` + `render_managed_wm_hints`)
  — hardcoded, independent of the shared `g_override_redirect` PDL, so
  a future revert of that PDL for some other window class cannot ever
  silently undo this again. `g_dock_menu_win` (the toys dropdown, a
  genuine short-lived popup) deliberately keeps its own separate,
  already-hardcoded `override_redirect = True` — untouched, since that
  IS the correct fix for the toys-menu regression bullet above.
  **Known alternative approaches, for the record** (not chosen, but
  worth knowing if this resurfaces in a different shape):
  1. *Per-window opt-in via the `<window>` tag itself* (e.g.
     `managed="true"` read at parse time into the Elem tree) — more
     general than hardcoding `window_is_dock()`, but adds a new xhtpm
     attribute for something so far needed by exactly one window
     class; not worth the surface area yet.
  2. *A second, separate PDL key scoped to just the dock* (e.g.
     `dock_override_redirect=false` alongside the existing house-wide
     key) — keeps the "data file controls it" convention this house
     prefers over hardcoding, at the cost of one more setting a future
     session has to know to check. Rejected mainly because the
     hardcoded fix cannot regress via a stray PDL edit at all, which
     is the actual property being optimized for here.
  3. *Give every persistent top-level window (not just the dock) the
     same unconditional treatment*, removing `g_override_redirect`
     entirely — the most thorough fix, matching this file's own
     standing rule above ("a persistent top-level window must be
     WM-managed, not override_redirect") to the letter. Not attempted
     this pass because it re-opens the exact toys-menu regression risk
     without the isolated single-window testing that bullet's own
     "before attempting this again" steps call for — a real candidate
     for a FUTURE, deliberate, isolated pass, not a same-session
     follow-on to an urgent live bug report.
  **Standing rule, addition to the one above**: when a fix like this
  one lives in a specific window-creation code path, and that code
  path is ever rewritten/replaced/merged into another file, the fix
  must be explicitly re-verified against the new code, not assumed to
  have carried over - add a code comment at the fix site strong enough
  that a future refactor cannot miss it (see `dock_managed` in
  `khtpm_core_render.c`, marked "never remove this comment").

---

## 2026-09-03 — khtpm_core_render.c live-debugging session (flicker + palette perf)

*Keep this section until each item has a permanent guard in code and
has stopped recurring in review.*

- **A khtpm window keeps running the binary it was launched with.**
  Rebuilding `khtpm_core_render.+x` does NOT change an already-open
  window — it is a live process. This session lost ~an hour to
  "arrow nav is broken in db-hq-pal" / "the flicker fix didn't work"
  / "palettes still slow" that were all just a stale pre-rebuild
  renderer. After ANY renderer rebuild: kill every
  `khtpm_core_render.+x` (read pids from
  `#.desktop/livedesk_hq_windows_*.txt`, or
  `run_khtpm_strip.sh new` for the whole stack) and reopen the window
  before concluding anything about a code change. `time <dump>`
  showing `real` ≈ `user`+`sys` means you ARE on fresh code doing real
  work; `real` ≫ `user`+`sys` means X round-trip bound (see next).

- **`draw_core.c` `alloc_pixel()` MUST stay cached.** `cmap` never
  changes after startup, so every distinct colour needs one
  `XParseColor`+`XAllocColor` round-trip for the process lifetime.
  Uncached, a palette repaint (256+ tiles × several colours each) is
  ~1000+ synchronous round-trips ≈ 0.5 s wall per frame — "palettes
  are incredibly slow". Symptom signature: `time` shows large `real`,
  tiny `user`/`sys`. A 64-entry spec→pixel cache fixes it. Do not
  "simplify" it away.

- **`draw_core.c` `xft_color()` must NOT be cached** — its callers
  (`draw_elem`, and ~9 sites in `khtpm_core_render.c`) `XftColorFree()`
  the result. A shared/cached `XftColor` would be use-after-free /
  double-free. It is one round-trip (vs `alloc_pixel`'s two), so the
  payoff is small anyway. If you ever want it cached, you must remove
  every `XftColorFree` on its result first.

- **Any per-cell fill pattern in the tile hot path must use a
  FillTiled GC + a prebuilt Pixmap, not a per-pixel loop.** The first
  cut of the palette transparency checkerboard did ~49
  `alloc_pixel()`+`XFillRectangle` per tile × 256 tiles ≈ 13 k colour
  round-trips per frame. Build the pattern once as a small Pixmap,
  stamp it with one `XFillRectangle` per cell.

- **The palette grid must not be re-laid-out / re-serialised on every
  redraw.** `dbhq_redraw_content()`'s palette branch ran
  `dbhq_layout_pass()` + `dbhq_assign_nav_indices()` +
  `dbhq_write_palette_frame_file()` (serialise all 256 tiles to
  `#.desktop/palettes_frame.txt`) on EVERY call — focus move, unrelated
  tick, expose. Gate all three on a content signature (scroll / tile
  count / tab / tileset / category / window size); an unrelated redraw
  repaints straight from the existing frame file. First paint 198 ms →
  repaint ~22 ms. Do NOT also skip the repaint-from-file itself on a
  "nothing changed" guess — the armed-tile title feedback and the
  focus ring both need that repaint, and an over-aggressive early
  `return` silently swallows them.

- **A short cell (`e->h < 64`) is not automatically a taskbar-strip
  button.** `32104e91` gave every `h<64` sprite cell the strip layout
  (sprite capped 24 px, left-anchored past the nav badge, caption
  beside). Palette / swatch grid tiles are also `h<64` and must keep
  the centered full-cell sprite. `draw_elem` now gates the strip path
  on `!is_grid_tile` (class `pal-tile` / `swatch`). Any new short-cell
  sprite layout must make the same distinction by class, not by height
  alone.

- **`FocusIn`/`FocusOut` on a focused override-redirect window fire a
  `NotifyGrab`+`NotifyUngrab` pair every time ANY process grabs the
  pointer/keyboard anywhere on the desktop.** A handler that repaints
  (or does any real work) on raw focus events must first ignore
  `mode` `NotifyGrab`/`NotifyUngrab`/`NotifyWhileGrabbed` and `detail`
  `NotifyPointer`/`NotifyPointerRoot`/`NotifyInferior` — otherwise the
  window repaints continuously while the user just mouses around
  (this was the db-hq-pal "redraws with no change" flicker; full
  writeup in `09-appendix/forensic-report-flicker.md`).

- **`Expose` arrives one event per damaged rectangle**, and a fresh
  burst per restack over an override-redirect window. Drain the whole
  burst (`XCheckTypedWindowEvent(... Expose ...)`) before repainting
  once; never `redraw()` per `Expose` event.
