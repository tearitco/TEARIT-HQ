# PROGRESS — network-browser V4: YouTube watch pages render "as normal"

**Status:** V4 milestone DONE & verified live on `opencode` (uncommitted
at write time; commit expected with this doc). Watch URLs
(`youtube.com/watch?v=…`) now render the page's real content in the
network browser: title, uploader + meta line, wrapped description, the
main video playing in the V3 canvas, and a clickable related-video grid —
all from the page's own embedded `ytInitialData` JSON, one HTTP fetch, no
`youtubei/v1` API, no per-app renderer hacks.
**Branch:** `opencode` only (same divergence caveat as V3 — `main`/`claude`
share only the initial commit).
**Docs:** `PROGRESS-network-browser-2026-09-12-v3-youtube.md` (video
playing path V3 lives on), `NETWORK-BROWSER-VIDEO-V3-DESIGN.md` (canvas
contract), `NETWORK-BROWSER-EDITING-AND-MEDIA-DESIGN.md` (MEDIA|/LINK| row
contract → tile pattern).

---

## 1. What landed 2026-09-14 — DONE, verified live

`go:https://www.youtube.com/watch?v=dQw4w9WgXcQ` produced this live
`#.desktop/network_browser_page.state.txt` (83 lines):

- `VIDEO|<poster-m0>|<watch url>|video` — main player row; spawns the V3
  op (`+x/nb_video_play.+x … tmp/nb_video0`) so the current video plays in
  the canvas while the page rows render above it.
- `TITLE|Rick Astley - Never Gonna Give You Up (Official Video) (4K Remaster)`
- `TEXT|Rick Astley · 4.54M subscribers · 1,815,148,235 views · Oct 24, 2009`
  (meta line, single row, ` · `-joined).
- 55 `TEXT|` rows: the attributed-description wrapped at 78 cols.
- 12 `IMG|` + `LINK|` pairs — the related grid from the page's
  `endScreenVideoRenderer` entries (thumb cache URL + full title).

Live `network-browser-hq_ui.txt` projection confirms the wiring end to end:
- `c_0_kind=video c_0_is_canvas=1 c_0_sprite=…/tmp/nb_video0/surface.raw`
  (surface.receipt.txt present → canvas blitting).
- `c_1` title, `c_2..c_57` text (meta + desc).
- `c_58..c_69` `kind=img is_media=1` with `sprite=…/nb_sprites/m1..m12` and
  `action='…/nb_write_go.sh' 'go' '<watch-url>'` — the related tiles are
  clickable; **zero residual `kind=link` rows** (IMG-absorbs-LINK took the
  12 LINK rows and turned them into tile actions, mirroring the chhtml
  writer's img+link run pairing).

### How it works (code map, all inside `network_browser_manager.c`)

- Mini JSON walker added above `do_fetch`: `js_ws`/`js_str_end`/
  `js_value_end`/`js_obj_val` (key len = `ke-p-2`, quotes counted)/
  `js_arr_val` (suffix components: `js_obj_val(key)` THEN `js_arr_val(idx)`/
  `js_path_val`/`js_get_str` (unicode-escapes, nbsp→space, bullet→'-').
- `youtube_watch_page(url)` = `strstr(url,"youtube.com/watch")`.
- `yt_vid_from_url()` — `v=` param extractor.
- `ingest_youtube_watch(html, url, out)` — the mapper. Title via
  `twoColumnWatchNextResults…videoPrimaryInfoRenderer.title.runs[0].text`,
  views/date/meta, channel via `videoSecondaryInfoRenderer.owner
  .videoOwnerRenderer.title.runs[0].text`, description via
  `attributedDescription.content`, related via
  `playerOverlays…watchNextEndScreenRenderer.results[i]
  .endScreenVideoRenderer.{videoId,title.simpleText,thumbnail.thumbnails[0].url}`.
- `do_fetch`: URL-classifier fast path is now
  `url_is_video(url) && !youtube_watch_page(url)` so watch URLs fall to the
  real HTML fetch; a second such block right after the curl starts the V3
  player; after curl the watch branch runs HTML read →
  `ingest_youtube_watch` → commit → `collect_page_media(html,url)` (turns
  MEDIA|V and MEDIA|I rows into VIDEO|/IMG| sprite rows, fetching
  poster/thumbs with the generic UA) → `video_start_if_page_has_video()`
  → `goto do_fetch_publish_done` (shared history/tab/projection tail). If
  the watch mapper fails (odd page shape) it unlinks the temp and falls
  through to the generic 4chan/DOM parse.
- `write_ui_projection()` content loop now reads page.state into a
  malloc'd row buffer (`NB_UI_ROWS_MAX 128`, PATH_BUF+512 each) and, when
  an `IMG` row is immediately tailed by a `LINK|` row, emits
  `c_N_action=go:` on the img and consumes the LINK (skips it) — the
  dual-row related-tile contract made clickable in the live UI writer the
  same way `write_chtpm_projection()` already did.

### Design decisions (why this shape)

- **One fetch, no live API.** `secondaryResults` (the real sidebar) is NOT
  in the initial HTML — it's produced by a `youtubei/v1/next` round-trip;
  the embedded continuation tokens in the initial HTML return only
  comments. What IS in the initial HTML: the full primary/secondary info +
  12 `endScreenVideoRenderer` related entries. So the grid = those 12, no
  extra network. (Re-validated identical on a second real page,
  `jNQXAC9IVRw`.)
- **Shared row contract, no renderer branch.** Main video = `MEDIA|V`
  (survives into VIDEO| → V3 canvas; chhtml rollback keeps ffplay action).
  Related = `IMG|`+`LINK|` pairs — both writers render an IMG tailed by a
  LINK as a clickable tile. Rows cap `MAX_MEDIA 24` = main poster + 12
  related + slack.
- **Thumbs need no special UA.** Generic UA fetch of
  `https://i.ytimg.com/vi/<id>/hqdefault.jpg` works (21 KB JPEG cached via
  the existing sprite flow).
- **C gotcha recorded:** macro-style `P"[0]…"` string folding doesn't
  compile — path templates must be built with snprintf (caught in the
  standalone ytest prototype before touching the manager).

### Verified on which pages

- `dQw4w9WgXcQ` (Rick Astley) — full live pass above.
- `jNQXAC9IVRw` (Me at the zoo) — JSON-path map validated in the
  standalone prototype; same paths resolve.

---

## 2. What's left to do (candidate, rough priority)

1. **Real-hardware click test of the related grid.** Relay/probe shows
   `c_N_action` go: correctly baked on each tile; the before-next-steps
   gate is clicking a related thumb with the real mouse on the live window
   and confirming (a) the new watch page renders its own metadata,
   (b) the V3 op restarts for the new video. (This is the standing focus
   caveat from V3 — see below.)
2. **Merge the keyboard-focus work** from `main`/`claude` onto `opencode`
   (`aca1a762` + `0a845704` cli_io/text_area grab-survival, `1d066bc2`
   history delete-on-backspace). Not needed for the tile clicks (they are
   `go:` writes), but needed before trusting full keyboard browsing.
   Also still required: the deeper `override_redirect` focus investigation
   — relay-driven tests pass while real hardware can still lose focus.
3. **Robustness pass on the mapper.** Handle: watch URLs with extra params
   (`&t=`, `&list=`, `&start_radio=`), `youtu.be/<id>` redirects (the
   classifier's `youtu.be/` path is currently bare-video only — a short
   link should resolve to a watch page render, not ffplay), shorts pages,
   and the `consent` redirect. Verify a member-only / age-restricted /
   live-stream page degrades to the generic parse instead of a blank page.
   Consider re-trying the embedded continuation token for the REAL
   sidebar (`compactVideoRenderer`) as a stretch — the 12-endScreen grid is
   the MVP, the sidebar is the polish.
4. **YouTube search** — `ytInitialData` is also embedded in
   `youtube.com/results`; a small mapper → `VIDEO|`/`LINK|` rows reuses
   everything here (V3 doc item 2).
5. **YouTube comments** — the `youtubei/v1/next` round-trip (V3 doc item 3)
   is still the only source; lowest-risk is a manager-side op writing
   `COMMENT|` rows. Unchanged scope.
6. **Meta-line polish** — currently `channel · subs · views · date` joined
   verbatim from segment strings; normalize view/subscriber counts and drop
   stray whitespace where the API omits segments. (Cosmetic, low value.)
7. Consider whether `description` should carry long lines wrap at a wider
   width, and whether `TITLE|` needs the side-label "Top comment" style
   header — parked until comments land.

**Reminder for any agent who picks this up next:** V3 wrote the ground
rules — verify against the live window (evidence file + window dump), not
just state; kill manager + renderer by exact PID and clean
`module_parent.pid`/`network_browser_manager.lock` before relaunch;
rebuild via `&.hq-apps/network/build.sh` → `+x/network_browser_manager.+x`;
commit scoped on `opencode` only.