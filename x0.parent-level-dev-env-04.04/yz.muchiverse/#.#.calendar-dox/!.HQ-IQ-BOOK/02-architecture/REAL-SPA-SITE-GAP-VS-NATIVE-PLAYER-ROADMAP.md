# Real SPA/media sites (youtube.com, discord, twitch, tiktok, reddit.new) vs the network-browser's native player — the gap, and the roadmap

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
| Render youtube.com's real HTML page (DOM) | network-browser khtpm DOM rendering | **jar-attach BUILT** (receipt sid=abc123 via NB_COOKIES_FILE); **login SAPISID NOT built** — see row 34 for unified store status |
| Execute youtube.com's client-side JS bundle (loads comments, lazy sections, player config) | browser JS engine | **NOT built** |
| Page-originated fetch/XHR (innerTube `next` comments, websocket chat, GraphQL) with session cookies attached — the network the page's own JS speaks, not a manager-side one-shot curl | browser network/XHR layer | **BUILT** — hermetic receipt green: worker_page_test (page-originated InnerTube-`next` XHR + NB_COOKIES_FILE session jar; WPT-EXIT=0, next200 + sid=abc123, file:// fixture, zero network) |
| Apply youtube.com's CSS (layout, right-rail, dark theme) | browser CSS engine | partial (`.css` files, not full page CSS) |
| Session cookies + login (yt-visitor, SAPISID, etc.) | browser cookie store | **PARTIAL** — unified jar built (wire `Set-Cookie` ingress + `Cookie` egress through `NB_COOKIES_FILE`; `document.cookie` and `nb_fetch_sync` share one store; chromium-parity proven by hermetic loopback receipt `worker_login_test` / `wlt`); real Google SAPISID handshake + `LOGIN`-grade credential injection **NOT built** (no per-site hardcode; page JS must do its own `SAPISIDHASH` signing via the one authoritative jar) |
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

## Roadmap (ordering, per review 2026-09-14, corrected direction)

The goal is a **real working browser** — the four walls above, built as
one generic engine (same one-engine rule: no per-site branches). Comments
are a *byproduct*: the day a real page's own JS can issue an InnerTube
`next` call *through the browser's* network wallaine, comments render as
real DOM rows. There is no "comments shortcut" step — a manager-projecting
raw JSON rows instead of the page rendering them is fake, and is NOT on
this roadmap.

1. **Test real youtube.com rendering** — point the live browser at the real
   site, record what the DOM/CSS/JS renderer actually produces vs the bare
   `<bar>`/canvas we have. Concrete, no code.
2. **Build the network/session wall first** — the one wall the user named
   (2026-09-14): a real page-originated XHR/fetch surface + a session
   (cookies/localStorage) store. This is the *bottom* of the stack, the
   one wall that must exist before any other wall is observable dead.
3. **Pause bar/player work** — the committed V4 stays, feature-frozen.
4. (done) **Explain the gap in docs** — this file.

## Cross-references
- `HTML-MEDIA-AND-SCRIPTING.md` (02-architecture) — same story for
  images/video/JS: one engine, media primitives, not a new DOM.
- `network-browser-hq.xhtpm` + `network_browser_manager.c` VIDEO branch —
  the native bar projection this doc pauses.
- `nb_video_play.c` / `nb_video_cmd.sh` / `yt_resolve.c` — the native
  player trio (frozen post-V4).

## Why this is the whole class, not youtube-only (2026-09-14 addendum)

The gap is NOT specific to youtube. Every target site above shares the
same four walls against the current khtpm-based browser:

1. **No `<DOM>` renderer.** khtpm renders one blessed element vocabulary
   (elem/bar/canvas/sprite...). Real sites ship an arbitrarily-nested
   HTML DOM (divs of divs, semantics, ARIA) that cannot be expressed as
   khtpm elems without a translator LAYER — which is itself a second
   renderer we refuse ("one engine").
2. **No `<CSS>` engine.** khtpm has a tiny fixed stylesheet vocabulary
   (fg/bg, score glyphs, fixed width/height). Sites depend on full
   layout (flex/grid/overflow/position/z-index), cascade/origin,
   media queries, @font-face — none of which exists as a rule model.
3. **No `<script>` engine.** Real pages are functional through their JS
   (DOM mutation, InnerTube/GraphQL/Pusher websockets). We have a
   pure-C one-shot subprocess model (yt_resolve, nb_video_cmd) — one
   command at a time, no persistent callable page JS, no event
   listeners on real DOM elements.
4. **No `<XHR>`/fetch + no session.** The page's own JS cannot issue
   HTTP (no fetch/XHR surface), and there is no cookie/store to hang a
   login/session on (no `yt-visitor_data`, no SAPISID Authorization, no
   localStorage). Comments on real sites are NOT in the DOM JSON — the
   page JS must call an InnerTube `next`-style API with the visitor +
   signed-cookie headers BEFORE any comment row exists. That call is
   precisely the `<XHR>+session` wall, not a DOM-layout problem.

All four walls are the SAME for youtube.com, discord.com, twitch.tv,
tiktok.com, reddit.com/new. There is no youtube-specific shortcut.

### What this means for the roadmap

- The historical "open site.com and be done" promise is out of reach
  until `<DOM>` + `<CSS>` + `<script>` exist AS GENERIC renderer
  surfaces (same one-engine rule: no per-site branches).
- The native player/bar (this doc's original trigger, committed V4,
  HEAD c9b0b7ff + 6d3e9c1c) is a MEDIA extension — it plays a resolved
  mp4/webm onto a canvas + poke toggles. It is **parked**: not a step
  toward any real site above, and kept only as a proven media-primitive
  to reuse the day `<DOM>/<CSS>/<script>` land.
- The correct next milestone = **the network/session wall itself** (the
  one the user named 2026-09-14): a real page-originated XHR/fetch

## Wall-2/3 specification (write-through, per roadmap 2026-09-14, appended 2026-09-16)

This is the working Specification for the two cells the roadmap table marks **NOT built** — the bottom-of-the-stack rungs that make comments real:

### Requirements
- [ ] A page-originated XHR/fetch surface — the page's own JS can issue an InnerTube `next` call *through the browser's* network layer.
- [ ] A session store (cookies/localStorage) — NB_COOKIES_FILE jar + cookie-save_file write shape; hermetic tmpdir, file:// fixtures, zero network.
- [ ] Session attach: the page-originated call carries the session cookie (4-tab + expires+secure shape) scoped to the page host, NOT the fixture host — cookie-scope separation survives page-originated dispatch.

### Technical / design
- Same generic engine (one-engine rule — NO per-site branch, NO new g_is_<project> global): the driver expands a template into page.js so the PAGE issues the XHR; no manager-side one-shot curl.
- Hermetic: NB_COOKIES_FILE jar only in tmpdir; file:// fixture; pid-pipe LOAD/RENDER/STATUS protocol (worker_page_test driver shape proven at worker_fetch_test/worker_cookie_test/worker_page_test); exit-code assert `WPT-EXIT=0`.

### KPIs
- Hermetic page driver run proves page-originated `next` XHR with session attached: RENDER = `invoke/yt_next` HTTP-200 fixture + session cookie `sid=` — through the same generic engine, byte-exact sha-receipted before commit.
- Rebuild + rerun the whole test wall (nbjs wdt wft wet wck wcn wst wps wcs wcl wpt) — green, hermetic.

**Recipe for the day-rung:** Comments = the day a real page's own JS issues InnerTube `next` through this XHR wall with the session attached; a manager projecting raw JSON rows is fake and NOT on this roadmap.
