# Network browser: video in the canvas (V2, wraith player) — HOW-TO

Task C of `NETWORK-BROWSER-EDITING-AND-MEDIA-CHECKLIST.md`. Goal: a local
`<video>` plays INSIDE the browser canvas (not just poster + external
`ffplay`). V2 = wrap the wraith-alpha player as a resident house op; V1
(ffplay action on the tile) stays as the real-playback fallback.

Status: DOC WRITTEN, build not started. Follow this doc top to bottom;
evidence goes in the checklist's per-task log.

## 1. The one mechanism that makes it work (why this is easy)

`hq_sprite(dir)` in `khtpm_draw_core.c` (~145-187) is the renderer's sprite
loader, used by every `<item sprite=...>` tile. It keeps an LRU cache keyed
on `<dir>/sprite.csv`, but on a cache hit it compares `st_mtime` of the file
to the cached value and **reloads + redraws whenever mtime changes** (line 160:

    if (mt != g_hq_sprite_cache[i].mtime) { free; memset; break; }

That alone is the animation channel: whoever rewrites `m0/sprite.csv` gets a
new picture on the existing tile, on the next render pass, with zero changes
to the renderer. The project already writes stills this way every navigation
(`nb_media_to_sprite` → `#.desktop/nb_sprites/m<N>/sprite.csv`).

So V2 needs THREE parts, none of which touch khtpm_core_render.c:
a player that produces fresh `current_frame.png` at 8fps, an 8fps pump that
converts that frame into `sprite.csv`, and the manager wiring that starts/
stops the pair when a `<video>` is on the page.

## 2. Player: the wraith-alpha contract (researched, 2026-09-11)

Source of truth:
`…/x0.moke-pet-project-04.04/x0.5-liz.fiter4-mew-00.03/projects/wraith-alpha/wraith-projects/chtmgl-video-isolate/`
(`ops/src/wraith_video_player.c`, self-contained, no libs beyond libc).

Contract — `wraith_video_player <video_path|--stop|--pause|--resume> [root]`:

- On play it pre-extracts frames once: `ffmpeg -i <video> -vf fps=8,scale=320:240
  <root>/session/video_frames/frame_%04d.png` (blocking, first run).
- Then forks a child loop: each tick `cp frame_%04d → session/current_frame.png`,
  writes `session/video.playback` (`state=playing|stopped|paused`,
  `frame_index=`, `frame_total=`), then `usleep(1000000/8)`.
- Control in: `session/video.control` ("play"/"stop"); PID in
  `session/video.pid`. Stale-frame guard: `video.source.txt` stamps
  `<path>|<mtime>|<size>`, clears `video_frames/` when source changes.
- `--stop` writes control + SIGTERM/SIGKILLs the child (reads `video.pid`).

It holds the whole session under `<root>/session/`. We'll gift root =
`&.hq-apps/network/tmp/nb_video0` (created per video, cleaned with the
session).

House build decision: vendor the file as `ops/nb_video_player.c` (its own
copy, same code) and build with the existing `build.sh` into
`ops/+x/nb_video_player.+x` — same pattern nb_js_eval/nb_media_to_sprite use.
Rationale over pointing build.sh at the wraith path: the wraith path lives in
a DIFFERENT tree (`x0.moke-pet-project-04.04`), the remove is tripwire-free,
and the file is tiny/stable (351 lines). Note the vendor copy in the file
header comment so forwards-diffs stay honest.

## 3. Pump: `ops/nb_video_pump.c` (new, resident)

The manager's main loop is `usleep(300000)` = ~3.3fps (main() ~3119) — too
slow to carry 8fps. So a tiny NEW op does the frame→sprite hop:

Usage: `nb_video_pump <session_root> <sprite_dir> <fps>`

Loop (until player exits / control is "stop" and frame_index stopped moving):
1. Read `<session_root>/session/video.playback`.
2. If `state` != playing → sleep 125ms, continue (still alive: poster stays).
3. `current_frame.png` exists and its mtime > last converted → convert it.
4. Convert = run the SAME conversion `nb_media_to_sprite` uses (stb_image
   decode + write 64x64 `# resolution=64` + rows) — simplest honest reuse:
   call `nb_media_to_sprite.+x <current_frame.png> <sprite_dir>` via
   `system()`, exactly like the manager's `fetch_to_sprite` does today.
   (≈8 small subprocesses/sec, fine for real p.o.c. page; if it's ever a
   perf complaint, inline stb later — proactively DON'T, keep 1 copy.)
5. Termination: `video.pid` process gone AND `state=stopped with
   frame_index==frame_total` → write one last frame, exit 0.
   Also exit when `video.control` reads "stop".

Keep it dumb and loop-fast (usleep ~100ms worst case); never exit early just
because playback is momentarily "stopped" (a real player can pause).

## 4. Manager wiring (network_browser_manager.c)

Fail-closed: video is OPT-IN per page. If the page has no `VIDEO|` row in
page.state, we do nothing (V1 ffplay action stays). When a VIDEO row exists
(post `collect_page_media`, which already:
`MEDIA|V|<src>|<poster>` → `fetch_to_sprite` → `VIDEO|<sprite_dir>|<url>|video`):

New state (module-scope):
```
static char g_video_sess[PATH_BUF];   /* root for player+playback, ""=off */
static int  g_video_pump_pid = -1;
static int  g_video_player_pid = -1;  /* from session/video.pid */
```

New internal helpers:
- `video_stop_all()` — write `video.control=stop`; kill + waitpid pump pid
  (and the wrapped player child via its own video.pid); rm -r session root.
- `video_start_if_page_has_video()` — called at end of `do_fetch`
  (after `collect_page_media`, ~2264). Reads page.state for a `VIDEO|` row:
  sprite_dir = field2 (`…/nb_sprites/m<N>`), video URL = field3? wait — the
  emitting line is `VIDEO|%s|%s|video` = `VIDEO|<rel>|<url>|video`, so
  url = field 3 after splitting field 2; verify guard: only start if the URL
  is a local file (`file://`/absolute path) OR `http(s)` — for the p.o.c.,
  allow file://; remote mp4 stays V1-only (no range/streaming here).
  Then: stop previous, mkdir session root, spawn
  `nb_video_player <url> <root>` (fork+execlp, stdout/err → /dev/null, don't
  waitchild — it self-daemons), sleep ~150ms, spawn
  `nb_video_pump <root> <sprite_dir> 8`.
- `video_stop_all()` also called from main loop teardown (before
  `worker_quit()`) AND from `do_fetch` for a page that has no VIDEO row
  (page change → kill old video, only when g_video_sess[0]).

Controls (optional for C1, cheap): reuse request lines — handle
`video:pause`, `video:resume`, `video:stop` in `handle_request` by writing
`video.control` (player + pump both honor it). C1 doesn't need UI buttons;
the status line can show `frame_index` via the existing status/label path.

Edge cases to state in code comments:
- `fetch_to_sprite` already made m<N>/sprite.csv = POSTER before we start —
  pump keeps overwriting it per frame; between pump death and page change,
  the last frame stays visible (good, no blank tile).
- ffmpeg pre-extract is blocking for the whole clip (74 frames @8fps ~ 1-2s
  for a 10s clip) — spawn player via fork WITHOUT waitpid so do_fetch isn't
  blocked; pump just idles until frames exist.
- Player writes `session/video.pid` only AFTER its own fork (race-safe to
  poll for file existence before killing on stop).

## 5. Fixture + probes

- `tests/fixtures/video_test.mp4` — build with ffmpeg so the C1 evidence is
  visually undeniable (a running counter): e.g.
  `ffmpeg -y -f lavfi -i testsrc2=duration=10:size=640:360:rate=24
  -vf "drawtext=text='%{n}':fontcolor=white:fontsize=80:x=240:y=120" -pix_fmt
  yuv420p tests/fixtures/video_test.mp4`
  (fallback if drawtext font missing: use `testsrc2` alone — it has a moving
  element and timestamp; or `color=red` + fade, but counter is clearest).
- `tests/fixtures/video_test.html`:
  `<html><body><h1>Video test</h1><video src="video_test.mp4"></video></body></html>`

Probes:
- C1 (must-pass): navigate `go:file://…/tests/fixtures/video_test.html` →
  a) page.state has `VIDEO|…#.desktop/nb_sprites/m0|file://…mp4|video`;
  b) `…/tmp/nb_video0/session/video.playback` shows `frame_index` climbing
  to `frame_total` (sample twice >300ms apart);
  c) `…/nb_sprites/m0/sprite.csv` mtime advances across the same window;
  d) a screenshot (`xwd`/the house presentation recorder) shows the counter
  mid-clip, not the poster. Bowl 8fps ceiling: ±1s, PNG-copy model.
- C2 (control, cheap): `echo 'video:pause' > …/network_browser_request.txt`
  → frame_index/current_frame.png freeze; `video:resume` resumes; `video:stop`
  kills session dir + tile freezes on last frame. State visible in
  `video.playback` + sprite.csv mtime.

Verify with fresh build + live window evidence per AGENTS.md (state files +
screenshot, never compile-only). Then update the checklist + PRESENTATION-
VIDEO-PIPELINE note, commit scoped on branch `opencode`.

## 6. Out of scope (stated here so nobody re-litigates)

- V3 rawvideo pipe playback, audio-in-browser, remote mp4 streaming,
  playback in page tiles (JS `<video>` element), seek bar, volume.
- Wraith's own GL paths / `OJBECT source_ref` — this renderer has no
  source_ref; sprite.csv mtime reload IS the mechanism.
- Interlacing the pump into the main loop (300ms is 3.3fps < 8fps).