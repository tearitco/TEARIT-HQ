# PROGRESS — network-browser V3 video (live YouTube in the canvas) + state of the JS engine + keyboard-focus bug

**Status:** V3 milestone DONE & committed on `opencode` (`80b93028`). Two
outstanding threads documented below (YouTube comments + browser-window
keyboard focus); neither blocks V3.
**Branch:** `opencode` (this agent's branch). NOTE: `main`/`claude` have
diverged with keyboard-focus work this agent's branch never merged.
**Docs:** `08-roadmap/design-docs/NETWORK-BROWSER-VIDEO-V3-DESIGN.md`
(probe matrix V3-A/V3-B marked ✅),
`08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md` (JS engine rungs),
`04-bugs/BUG-LOG.md` (focus bug re-occurrence history).

---

## 1. What landed today (V3 video) — DONE

A bare YouTube URL now plays live video inside the browser's canvas, wire
to window, with no per-app renderer hacks.

1. **Root cause fixed — blank canvas.** Ops-local `khtpm_draw_core.c` copy
   only matched `overlay_w=/overlay_h=` receipts; `nb_video_play` writes
   `frame_w=/frame_h=`. The canonical shared-lib copy already had the
   `frame_w/h` fallback, but the ops copy shadowed it at the quoted-include
   (`khtpm_core_render.c:2544`). Fixed the fallback AND deleted the stale
   ops copy so the shared lib is the one compiled.
2. **Missing video-URL classifier.** `do_fetch` only classified *images*;
   a bare YouTube/mp4 URL never yields a `<video>` tag in fetched HTML
   (YouTube is JS-driven), so the page parser could never emit the
   `VIDEO|` page-state row the V3 start path keys on. Added
   `url_is_video()` + `publish_direct_video()` in `do_fetch` (checked
   BEFORE curl so a multi-MB JS page is never downloaded), mirroring the
   image classifier's success shape.
3. **googlevideo 403s.** The default `android_vr` player client mints URLs
   the CDN rejects even for yt-dlp itself (repro'd pure-yt-dlp 403 across
   multiple videos/formats). Forced `player_client=android` in the op's
   `resolve_url()`; also sends a browser UA + youtube referer on
   `avformat_open_input` (Lavf's default UA is rejected).

**Verified live against the window 2026-09-12:**
- `file://…/video_test.mp4` (640x360 24fps, no audio) → plays in canvas,
  receipt `frame_w=640 frame_h=360`.
- `go:https://www.youtube.com/watch?v=jNQXAC9IVRw` → `VIDEO|` row → op
  decode → canvas blit → clean EOF (19s clip). User confirmed on-screen.
- `ls -l --time-style=…` churn + `dump_frame_png_op.+x` evidence in
  `/tmp/opencode/v3_canonical_blit.png`.

**Committed:** `80b93028` on `opencode`, scoped to build.sh,
network-browser-hq.xhtpm, network_browser_manager.c, ops/nb_video_play.c,
khtpm_core_render.c, V3-V3-design doc. (Also-missing: this same V3 code was
never on `main`/`claude`; the two branches share only the Initial commit.)

---

## 2. The JS engine exists — the browser DOES run JS

Earlier statement "the browser doesn't do JS" was WRONG as a general claim.
The house already has a resident Duktape engine:

- `ops/nb_js_worker.c` — resident worker, own heap, line-RPC over a
  socketpair; spawned lazily by the manager; stays alive across pages.
- Wired in: `run_page_scripts()` → `worker_load()` at
  `network_browser_manager.c:1589`; post-JS DOM returns as `RENDER` rows →
  `merge_render_rows()` (line 972/2550) overlays them onto
  `page.state.txt`.
- Rungs landed per `NB-JS-ENGINE-ROADMAP.md`: 1 (globals/URL polyfills), 2
  (real DOM tree), 6 partial (sessionStorage, cookies, `http://` fetch),
  7 slice (CSS cascade).
- Phase-1 steps 1-5 committed on branch `chtpm-js-rungs` (see
  `09-appendix/PROGRESS-nb-js-worker-phase1.md`).

**What the JS engine does NOT change:**

- **YouTube comments** won't render even with JS, because comments arrive
  over a **`youtubei/v1` JSON API call the page makes after initial
  scripts**. That async net round-trip is not part of the page pipeline
  the DOM serializer sees. This is a content-channel gap (a real, separate
  thing), not a "browse correctly" problem.
- **YouTube search** IS reachable in raw HTML — results are embedded as
  `ytInitialData` JSON in `youtube.com/results`. Cheap future probe
  (small JSON→VIDEO-row mapper). Comments are NOT the same story.

**Did browsing wrongly cause missing comments? NO.** Video pages are
browsed correctly today; comments are simply fetched by the page via an
API call after the initial render, a channel the engine doesn't drive yet.
Both entries are scoped as a future V4 idea (search = easy, comments =
harder, needs the youtubei API call or a small manager-side op).

---

## 3. Keyboard focus / esc / backspace in the browser window

User-reported: esc and backspace don't work in the browser window due to a
window-specific focus issue. **Who has the fix?**

- The fix(es) live on `main` (= `claude`), NOT on `opencode`:
  - `aca1a762` + `0a845704` — cli_io/text_area reparse no longer re-grabs
    keyboard needlessly (grab is Window-level, kept valid across reparse by
    saved key).
  - `c8ff5656` — docs: real hardware still fails after 3 fixes.
  - `bdb30794` — diag logging.
  - `1d066bc2` — history delete-on-backspace + clear-all.
- This agent's `opencode` branch has ZERO of these (0 vs 15 match count on
  the renderer for `kh_grab_keyboard_retry`/`kh_find_input_by_key`/
  `saved_input_key`).
- Even on `main`, `BUG-LOG.md` records: relay-driven tests pass, **real
  hardware still loses focus** — suspected `override_redirect` pc-hq
  focus bug (synthetic XTest input masks it). So "porting the branch" (or
  having them already) is necessary but may not be sufficient.

**Implication / recommendation:** the focus fix must be merged into
`opencode` from `main`/`claude` (careful, scoped cherry-pick) AND the real
hardware path needs the deeper `override_redirect` investigation. Neither
task was part of V3's scope; both are now written down here.

---

## 4. Next steps (candidate, in rough priority)

1. **Merge the keyboard-focus work** from `main`/`claude` onto `opencode`
   (cherry-pick the cli_io/text_area grab-survival commits; verify with a
   real typing pass, not just relay).
2. **V4 probe: YouTube search** via `ytInitialData` JSON in raw HTML
   (small mapper → `VIDEO|` rows → reuse V3 playing path). No new network.
3. **V4 probe: YouTube comments** via `youtubei/v1` — cheapest form is a
   small manager-side fetch op writing `COMMENT|` rows; the JS-engine's
   rung-6 fetch is the heavier alternative.
4. Close V3-C (pause/resume/stop/EOF audit) and V3-D (Widevine explicit
   "DRM cannot decode" message) probe rows if time permits.

---

## 5. Update 2026-09-14 — watch pages migrated to V4 (see the V4 progress doc)

The original project goal — "YouTube watch pages render as normal" — landed
as V4 on `opencode` and supersedes items 2/3 above. All watch-page rows
(title/channel/meta/description + clickable related grid) come from the
page's own embedded `ytInitialData`, no `youtubei/v1` API call. Details,
evidence, and the remaining work (YouTube search, comments, focus-branch
merge, real-hardware click test) are in
`09-appendix/PROGRESS-network-browser-2026-09-14-v4-youtube-watch.md`,
which is the running owner for the watch-page feature going forward.