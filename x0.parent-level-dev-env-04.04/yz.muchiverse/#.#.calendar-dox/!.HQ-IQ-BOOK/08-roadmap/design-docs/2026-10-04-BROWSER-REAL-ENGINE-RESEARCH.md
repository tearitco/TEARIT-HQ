# NETWORK-BROWSER REAL-ENGINE RESEARCH — 2026-10-04

**For:** Claude (next machine) to act on. Read `CENTROID_GOLD_STD.md` first,
then `NETWORK-BROWSER-FRONTEND-PLAN.md`, then this doc's decision section.

## Why this doc exists

Direct live report 2026-10-04: YouTube/Google-class sites render as raw
script text, "no videos", address-bar fetch of modern SPAs stalls at
`Status: loading`. The browser is not a browser — it is a manager that
fetches HTML and **text-strips** it
(`network_browser_manager.c` → `nb_dom.c` tolerant parser → rows). QuickJS
exists as a worker but its value is gated on a real layout/DOM stack that
only partially shipped (rungs 1–5 scattered, §8 known gaps: no text-flow,
`getBoundingClientRect` always 0, `@media` never fires). See
`NB-JS-ENGINE-ROADMAP.md` and `NETWORK-BROWSER-FRONTEND-PLAN.md §1`.

User's ask, verbatim: "get real chrome like browser capabilities, instead
of w/e nightmare is going on right now."

## What was decided before, and why it no longer matches the ask

`NB-JS-ENGINE-ROADMAP.md §8` (2026-09-03) DECIDED path A: hand-built,
no engine embed, no CEF, no WebKitGTK. Reasoning: khtpm must own every
pixel; CEF rejected as "all of chromium"; WebKitGTK XEmbed owns its
rect. Shelved PoC in `&.hq-apps/network/_attic-gtk-embed/`.

That decision was right for "our own chrome window drawing text rows".
It is **wrong** for the stated new requirement "real chrome like browser
capabilities" — pixel-faithful, real CSS, real JS, real video on real
sites. No amount of rung plumbing closes that gap; it is an engine.

## Options, as of 2026-10-04 (Debian 12 box)

1. **CEF via offscreen render (OSR) → blit.** Full Chromium inside our
   khtpm chrome: real CSS/JS/video, our pixels.
   - Cost: ~200 MB of CEF shared libs, vendored or packaged; OSR requires
     handling OnPaint buffers ourselves. This was the rejected-but-now-
     relevant option. **Most aligned with the ask.**
2. **CEF as a plain child window (no OSR).** Engine draws its own rect
   inside our window. Loses khtpm overlay on the content pane but keeps
   our chrome/strip. Much simpler; some HUD-over-page dreams die.
3. **WebKitGTK (`nb_webkit_view` PoC).** Already on the box (86 MB .so),
   fastest to revive, but GTK-flavored real-world compat is worse than
   Chromium for Google's properties (YouTube logged-in flows are fine,
   some sites break). Parked PoC exists; `XEmbed` reparenting was the
   buggy part — a separate window or GtkSocket path was the answer.
4. **Servo.** Rust, OSR-capable, interesting, smaller ecosystem, riskier
   packaging. Not on this box.
5. **Keep hand-rolled `nb_dom`+QuickJS+text rows.** Zero. Equivalent of
   "not a browser". Only worth continuing for interstitials/links-view.

## House constraints that survive any choice

- **No per-app C in `khtpm_core_render.c`** (house law, §8 of the
  standards skill). Whatever engine we pick talks to khtpm through the
  **manager process + `.chtpm`/`ui.txt` projection / state files**, never
  new tags in the shared renderer.
- The manager already owns fetch/console/tabs/bookmarks/history via plain
  files — keep that contract; swap only what fills the *content* rect.
- Trackpad/mouse events on the page area need a real X child window
  (engine-owned rect) or an event-forwarder shim for OSR. A plain
  separate child X window (option 2/3 style) avoids the forwarder.
- Relay file driving (`entity_menu_history/<pid>.txt`) stays the house
  test mechanism for the chrome; engine content is verified via the
  engine's own DOM/screenshot, not the relay.

## Recommendation

**Spike option 3 first (WebKitGTK as a child X window, reusing the
parked `_attic-gtk-embed` code), then jump to option 1 (CEF OSR) if
YouTube/Google parity is the bar.** 3 is one redirection of existing
parked code and gets us real rendering this week; 1 is the true
"chrome-like" target and should be the follow-through if the PoC
behaves. Option 2 is the pragmatic middle if 3 fights and 1 is too big
to land in one pass.

Do NOT sink more time into making the hand-rolled path look like a
browser. Keep it as the "reader mode / link extraction" fallback.

## Concrete next steps (for Claude)

1. Read `_attic-gtk-embed/README.md` + `nb_webkit_view.c`. Fix or bypass
   the XEmbed reparenting (separate top-level or GtkSocket). Prove
   example.com renders real layout inside the khtpm frame.
2. Register the engine window in the window list / livedesk proc list
   the same way `button.sh` does for the renderer.
3. If Google/YouTube behaves, declare win. Otherwise write the same
   behind-the-manager harness for CEF OSR (cefpython or the C API) and
   blit `OnPaint` → X11 image.
4. Update `NB-JS-ENGINE-ROADMAP.md §8` with a dated reversal note — do
   not silently overwrite the old decision.
5. Videos: with a real engine, `nb_video_play`'s canvas path becomes
   redundant for normal pages; keep it for the hand-rolled fallback.

## Loose ends surfaced during today's testing (log, don't fix now)

- YouTube home/watch fetch leaves manager at `Status: loading` — the
  module-graph JS extraction never settles. Moot once an engine lands.
- `/tmp/opencode` is root-owned; agent temp files land in `/tmp`.
- `_.hq-apps/network/tmp/nb_video0/video.state` says `stopped` after a
  remote http `go:`; local file/`file:///` verified working after the
  seekable fix (`d1bc745ce`).
