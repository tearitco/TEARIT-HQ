# Real youtube.com vs the network-browser's native player — the gap, and the roadmap

**Status: ROADMAP, 2026-09-14.** Written the day the network-browser proved
a native `<bar>`/video player end-to-end (play/pause toggle row + click-
to-seek progress bar under the video canvas, flowing through
`nb_video_play` → `surface.raw`/`surface.playhead.txt` →
`nb_video_cmd.sh`). That is the *native player* surface. This doc answers
the question raised in review: does that get us **real youtube.com** (login,
comments, the actual site)? — No, it does not; here is the exact distance
and the ordering of the work.

## The short answer

The native-player path we just built is **a browser media feature**, not a
youtube.com implementation. It plays/frames/seek a stream once you already
have a direct CDN URL. Real youtube.com is a **different surface**: it is a
server-rendered HTML page (plus a giant client-side JS bundle) that depends
on cookies/credentials, and whose comments are loaded out-of-band through
the InnerTube `next` API, rendered by that JS into the DOM.

You cannot "finish" the native player and call youtube.com done. The two
share one primitive (a canvas that paints frames) and almost nothing else.

## What real youtube.com needs (the gap)

| Need | Where in the browser | Status |
|------|--------------------|--------|
| View a resolved googlevideo stream | `nb_video_play` (native player) | **DONE** |
| Generic `<bar>` seek/play row under the canvas | `khtpm_core_render` `<bar>` branch + manager VIDEO branch | **DONE** |
| Render youtube.com's real HTML page (DOM) | network-browser khtpm DOM rendering | **NOT built** |
| Execute youtube.com's client-side JS bundle (loads comments, lazy sections, player config) | browser JS engine | **NOT built** |
| Apply youtube.com's CSS (layout, right-rail, dark theme) | browser CSS engine | partial (`.css` files, not full page CSS) |
| Session cookies + login (yt-visitor, SAPISID, etc.) | browser cookie store | **NOT built** |
| Feed InnerTube API requests from the browser (needs a valid `yt-visitor_data` + API key + signature) | browser network layer | partial (`yt_resolve` standalone, not in-page) |
| Render comments / chat (they are API-loaded + JS-injected, not in static HTML) | browser DOM/JS after an API call | **NOT built** |
| Site JS that fights headless/simple UAs, TLS fingerprinting | browser HTTP stack | gate exists for UA/referer only |

## What we did NOT claim

The native `<bar>` commit (`810c381f` + `6d3e9c1c`) is strictly the native
player surface. It does not open youtube.com as a page. If the live browser
is pointed at `https://www.youtube.com/` today, the manager will attempt a
video projection only when a `VIDEO` row/`video:` op is present; the *page*
itself (HTML/CSS/JS of the real site) is not rendered by the khtpm DOM
engine.

## Decision on the native-player path (pause)

Because real youtube.com is the goal and the native path is a *different*
surface, the native video `<bar>`/player work is **PAUSED** after the V4
commit. It stays wired (binaries in `ops/+x/`, `nb_video_cmd.sh` script,
`rc+1`/`rc+2` manager projection) so resuming is a config/build step, not a
re-write. No further native-player features will be added while the roadmap
below is empty.

## Roadmap (ordering, per review 2026-09-14)

1. **Test real youtube.com rendering** — point the live browser at the real
   site, record what the DOM/CSS/JS renderer actually produces vs the bare
   `<bar>`/canvas we have. Concrete, no code.
2. **Scope to comments-first** (alternative path, same docs): implement the
   empty-youtube-'s comment API contract end-to-end in the browser first —
   a real `yt:` URL opens, InnerTube `next` loads comments, manager projects
   them as DOM rows — before attempting full page render.
3. **Pause bar/player work** — the committed V4 stays, feature-frozen.
4. (done) **Explain the gap in docs** — this file.

## Cross-references
- `HTML-MEDIA-AND-SCRIPTING.md` (02-architecture) — same story for
  images/video/JS: one engine, media primitives, not a new DOM.
- `network-browser-hq.xhtpm` + `network_browser_manager.c` VIDEO branch —
  the native bar projection this doc pauses.
- `nb_video_play.c` / `nb_video_cmd.sh` / `yt_resolve.c` — the native
  player trio (frozen post-V4).
