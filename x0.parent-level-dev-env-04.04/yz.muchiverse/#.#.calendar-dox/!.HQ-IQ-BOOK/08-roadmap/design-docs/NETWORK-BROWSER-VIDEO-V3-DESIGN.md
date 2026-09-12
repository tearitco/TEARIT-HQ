# NETWORK-BROWSER-VIDEO-V3 DESIGN

Status: ACTIVE — design locked, build happening. Live checklist / evidence
log: NETWORK-BROWSER-EDITING-AND-MEDIA-CHECKLIST.md (Task C V3 section).

## 1. Decision (user, 2026-09-11)

- **Where video shows:** in OUR x11-hq window — the browser's own renderer
  window, composited by the shared `khtpm_core_render.+x`. Not a sibling
  window, not reparented.
- **Scope:** ONE milestone. Local mp4 + http(s) mp4 + (non-DRM) YouTube, all
  at once. No phasing.
- **DRM reality:** Widevine-locked (netflix/spotify/… ) decrypts in
  yt-dlp's browser-session only; post-URL extraction the stream is
  DRM-armed and libav cannot decode vp09/encrypted. These pages report an
  explicit "DRM content cannot be decoded" message. Non-DRM YouTube is in scope.

## 2. Surface — the generic `<canvas>` channel (ZERO renderer changes)

The V3 surface is the shared renderer's existing generic canvas element.
No `.raw`-vs-`.png` guessing, no new element type, no new tag. We composite
RGBA into an in-window canvas.

### 2.1 Producer → renderer contract (already in the shared renderer)

`khtpm_draw_core.c kh_draw_canvas()` (line ~539):

- Reads dims from a sibling receipt `<base>.receipt.txt` keyed
  `frame_w=` / `frame_h=` (also accepts `overlay_w=/overlay_h=`), then
  `XPutImage`s the raw BGRA/RGBA bytes at native size, clipped to the box.
- Re-reads the sprite file every draw (hq_sprite mtime re-read ~119-200) —
  a producer can overwrite `<base>.raw` in place per-frame and the renderer
  always paints the newest pixels.
- Caches last-good-frame: a bad tick (mid-write read, w/h=0) is skipped,
  not flickered.
- Draw side needs nothing: `draw_elem()` already dispatches tag==`canvas`
  to `kh_draw_canvas` regardless of layout.

`g_has_canvas` (renderer line ~2283) reports "a canvas is on the current
page" and the event loop then ticks at **~30fps** (9845/9867) — the pacing
the live-feed needs. Set automatically just by having a `<canvas>` in the
projection. NOT pc-gated.

### 2.2 The layout facts that drove the design

Three canvas placement paths exist in `khtpm_core_render.c`:

1. **Full-region canvas** — `kh_layout_canvas_in_region()` (~3938, called
   from `layout_sidebar_panel` at 4341): a `<canvas>` as a DIRECT child of
   `<panel>` fills the entire panel box (pad 6, min 64x64). Proven live by
   pc-hq (Milestone A). It swallows the whole content region — the browser's
   toolbar / address bar / status rows and content scrolllist inside that
   same panel would get no geometry.
2. **Flat-list path** (~5707): `<canvas>` as a direct child of `<page>` in
   the non-sidebar/panel default layout — gets x/y/w/h sized from CSS, else
   receipt dims, else window-width x 360. Falls through to YOUR OWN scroll/
   chrome layout only in the no-sidebar default window shape — not the
   browser's.
3. **Inside a `<scrolllist>`** — `layout_scroll_region()` (~3630) lays out
   only `item/text/cli_io/text_area` + a `<row class="sprite-grid-row">`.
   It skips canvases completely: a `<canvas>` sibling to the content rows
   gets y=-100000 and never draws.

### 2.3 Design decision: canvas as a content ROW in the browser panel

Because the browser chrome (toolbar, address bar via pinned `cli_io
class="top"`, status) lives in the SAME panel as the content scrolllist
(see template `network-browser-hq.xhtpm`, lines 44-71), the full-region
canvas (2.1) would erase the browser. The canvas must co-exist with chrome
and content.

We therefore place the `<canvas>` as one **row inside the content
`<scrolllist>`**, sized from its receipt dims so the video (e.g. 640x360)
is a real, scrollable, auto-refreshing in-page surface. This needs a small,
generic extension to `layout_scroll_region()` + `scroll_row_span()`: treat a
`<canvas>` child like the existing sprite rows, with `span =
ceil(receipt_h / ROW_H)` rows of height. This mirrors exactly how canvas is
sized/floated on the flat-list path (~5707 receipt-dims reading), just moved
into the scroll container. Zero new global flags; complies with
CENTROID_GOLD_STD rule 7 (no `g_is_<browser>` branches, no per-app
shortcuts — the canvas is a first-class row for ANY window that wants one).

HONESTY NOTE (2026-09-11): the earlier "zero renderer changes" claim applied
only to the pc-hq-style full-panel canvas. For the BROWSER shape a tiny,
generic property change is required:
`layout_scroll_region`'s child loop + `scroll_row_span` must recognize
`<canvas>`. It is ~15 lines, purely additive, no new globals, no per-app
branching. Checklist row C-V3 marks it.

## 3. Decode — real-time libav, not pre-extract

V2 used a wraith subprocess that pre-extracted N frames to sprite.csv
(8fps, poster-accurate but choppy). V3 replaces the pump with a live
decoder op.

New op: `&.hq-apps/network/ops/nb_video_play.c` (~one C file, house style).

```
URL (file:// | http(s):// | ytid)
   │
   ├─ if ytid   → fork/exec ~/.local/bin/yt-dlp --get-url
   │             (resolve to direct mp4/stream once, cache in memory)
   │
   ▼
avformat_open_input → find_stream(AVMEDIA_TYPE_VIDEO)
→ av_read_frame loop
→ sws_scale to fixed canvas size (default 640x360, receipt frame_w/frame_h)
→ write nb_video0/surface.raw + surface.receipt.txt (atomic rename)
→ audio: swr_convert → ALSA snd_pcm_writei (clock throttles frame pacing)
→ on AVERROR_EOF / video:pause / video:stop → video.state=stopped
```

Key properties:

- **Real-time full-fps.** Frame pacing is driven by the ALSA audio clock:
  `snd_pcm_writei()` blocks for audio backlog (~40ms/frame for 25fps) and
  the video frame is written right before its matching audio chunk. Drift:
  `av_frame` pts vs `snd_pcm_delay` — drop late frames, duplicate none.
- **ALSA, not SDL2.** SDL2 dev is ABSENT on this box; libav *and* swresample
  dev AND ALSA dev are present (FFmpeg 58.76.100). Link line:
  `-lavformat -lavcodec -lavutil -lswscale -lasound`. Default ALSA sink is
  the house a2dp bluetooth (`bluez_sink.40_C1_F6_2B_F6_89.a2dp_sink`) —
  audio is audible through the headset like the rest of the house.
- **No sound? No problem.** If audio stream absent / sink write fails,
  fall back to pts-clock pacing (NTP/hires µs between frames), same 30fps
  canvas, no crash.
- **3096→16384 Kb/s range pulls:** http(s) input is
  `avformat_open_input` directly (libav does range requests itself; no
  pre-download). yt-dlp's returned URL feeds the same pipeline.
- **Stop/EOF cleanliness:** op owns `nb_video0/video.pid` + control file
  contract (see §6) so the manager reaps it like V2's player.

## 4. Manager wiring (`network_browser_manager.c`)

- `video_start` is extended to pick the V3 path when `nb_video_play` exists:
  resolve URL (local path | http(s) | youtube) then exec the same
  `video_start`-style fork (helpers ~1163-1275, `video_start_if_page_has_video`
  ~1244/2388). The V2 pump path (sprite.csv 8fps) stays untouched as the
  offline fallback.
- `write_ui_projection` PAGE content: when the page is playing/ready,
  emit the canvas row into the content repeat:
  ```
  <canvas id="vc" width="640" height="360" sprite="${c.sprite}"
          show="${c.is_canvas}"/>
  ```
  alongside the existing per-kind `<item>` candidates (template lines
  75-79). The manager keeps emitting video:ready → content row for
  navigation consistency; playing → canvas row shows live feed (30fps tick
  active via g_has_canvas).
- `video:pause / resume / stop` and natural-EOF map straight onto the op's
  control file (same contract as V2). `video_reap` (waitpid WNOHANG) unchanged.

## 5. Probe matrix (run AGAINST THE LIVE WINDOW, evidence-logged)

| Probe | Input | Expect | Evidence |
|---|---|---|---|
| V3-A | `file://` → `video_test.mp4` (8fps source, now decoded full-fps) | surface.raw mtime changes ~30fps; receipt frame_w/h right; audio on headset | `ls -l --time-style=full-iso nb_video0/surface.raw` twice 200ms apart; screenshot `dump_frame_png_op.+x` |
| V3-B | YouTube URL (non-DRM sample, e.g. Big Buck Bunny) | yt-dlp resolves → stream plays in canvas | resolve log line; screenshot; audio |
| V3-C | pause / resume / stop / EOF | state transitions clean; no zombie | control-file trace; `video.state` file |
| V3-D | Widevine URL | explicit "DRM cannot decode" message | console + status row |

## 6. Files / contract summary

- Canvas contract (unchanged): `nb_video0/surface.raw` +
  `nb_video0/surface.receipt.txt` (`frame_w=`/`frame_h=`) — matched by
  `kh_draw_canvas` receipt reading.
- Op: `ops/nb_video_play.c` (new, live decoder).
- Control: `nb_video0/video.control` (`pause`/`resume`/`stop`),
  `nb_video0/video.state`, `nb_video0/video.pid`. Manager helpers reused.
- Renderer (small, generic): `layout_scroll_region()` + `scroll_row_span()`
  canvas-row support in `khtpm_core_render.c`.
- Template: canvas candidate line in the content `<repeat>` of
  `network-browser-hq.xhtpm`; emission in `write_ui_projection`.
- Branding: browser window is the shared renderer, manager is a `<module>`
  child; live projection `write_ui_projection()` → `network-browser-hq_ui.txt`.

## 7. Deploy / hygiene reminders (house book)

- User syncs + deploys to live NNEST-12.00; I never push/merge.
- Commit scoped to changed paths on branch `opencode`, never `git add -A`.
- Kill the EXACT manager/renderer PIDs (never `pkill -f
  network_browser_manager`, it takes the whole desktop).
- Relaunch: `rm -f module_parent.pid`, then `button.sh`.
- Screenshot: `dump_frame_png_op.+x` (OPERATIONAL-LANDMINES #5) — only
  reliable method.