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
