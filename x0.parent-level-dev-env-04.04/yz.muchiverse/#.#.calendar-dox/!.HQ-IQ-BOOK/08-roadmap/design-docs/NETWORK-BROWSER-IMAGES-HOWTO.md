# Task B: In-page images via existing sprite path — HOW-TO

**Goal**: on a JS page containing `<img>`, the image tile draws with real
pixels (not broken/blank). Phase-1 scope: fix the existing pipeline so
JS-page images flow through the same fetch→sprite→blit path as static
pages.

---

## 1. The existing image pipeline (static pages work today)

All file paths are under `44.xyz.01.00/&.hq-apps/network/` unless noted.

### Static extractor
`extract_and_publish()` (`network_browser_manager.c:429–683`)
linear-scans the fetched HTML and emits metadata rows into `page.state.txt`:

```
TITLE|page title
TEXT|visible text
LINK|href|label
MEDIA|I|image_url|alt          ← static <img> sources
MEDIA|V|poster|video_url       ← static <video> poster or src
```

### Sprite fetch (collect_page_media)
`collect_page_media()` (manager `:1075`) reads `page.state.txt`, finds
every `MEDIA|` row, and for each one:

1. Calls `fetch_to_sprite(fetch_url, dir)` — this is
   `ops/nb_media_to_sprite.+x`, an stb_image decoder (PNG/JPEG/GIF/BMP/
   TGA/PSD) that writes 64×64 `sprite.csv` into `dir`.
2. On success, rewrites the row in `page.state.txt` as:
   ```
   IMG|<sprite_dir_path>|<alt>
   ```
   where `sprite_dir_path` = `<house>/#.desktop/nb_sprites/m<N>`.
3. On failure (fetch/decode error), the row is **dropped entirely** —
   that's D5 gap #2 (no placeholder tile).

### Worker RENDER (nb_js_worker.c:409–441)
After `extract_and_publish`, the manager forks the JS worker
(`run_page_scripts`, manager `:1382`). The worker serialises the
post-JS DOM as RENDER rows:

```
TITLE|...
TEXT|...
LINK|href|label
IMG|src_url|alt              ← ← ← THE BUG: raw URL, not sprite dir
```

The worker emits `IMG` for every `<img>` in the live DOM. The value
is the raw `src` attribute, NOT a sprite dir.

### Merge step (merge_render_rows)
`merge_render_rows()` (manager `:965`) overlays the worker's RENDER
frame onto `page.state.txt`. It **replaces** all TITLE/TEXT/LINK/IMG
rows with the worker's version, but **leaves MEDIA rows untouched**.

After the merge on a JS page, `page.state.txt` contains:

| Source | Rows |
|--------|------|
| Worker | `IMG|<raw_src_url>\|<alt>` — broken (not a sprite dir) |
| Static | `MEDIA\|I\|url\|alt` — still present, not replaced |

### collect_page_media then runs
`do_fetch()` calls `run_page_scripts` (merge) THEN `collect_page_media`
(manager `:2238–2239`). collect_page_media finds the surviving
`MEDIA|I` rows and converts them to `IMG|<sprite_dir>|<alt>`.

**Result**: page.state.txt has TWO `IMG` rows per image:
- `IMG|<sprite_dir>|alt` (correct, from static MEDIA conversion)
- `IMG|<raw_url>|alt` (broken, from worker RENDER)

The projector renders both as separate tiles. The broken raw-URL
tile renders as a blank/missing sprite (the `sprite=` attr points at
the literal URL, not a valid filesystem path).

---

## 2. The specific gap

The worker's RENDER emits `IMG` (not `MEDIA`) for `<img>` elements.
`merge_render_rows` replaces static `IMG` rows with the worker's
broken ones. `collect_page_media` only processes `MEDIA` rows.
Result: JS-page images get a broken tile + (sometimes) a correct tile
from the static `MEDIA` rows surviving the merge.

---

## 3. Fix D4: route worker `<img>` through the sprite pipeline

Two changes required:

### 3a. nb_js_worker.c — emit MEDIA, not IMG, for `<img>`

In `dom_walk_render()` (ops/nb_js_worker.c, around line 431–440):

```c
// BEFORE (broken — raw URL as IMG):
snprintf(imgbuf, sizeof(imgbuf), "%s|%s", srcbuf, altbuf);
rw_row(b, "IMG", imgbuf);

// AFTER (correct — emit MEDIA|I so collect_page_media fetches it):
snprintf(imgbuf, sizeof(imgbuf), "I|%s|%s", srcbuf, altbuf);
rw_row(b, "MEDIA", imgbuf);
```

This makes the worker's `<img>` output the same format as the static
extractor (`MEDIA|I|url|alt`). `collect_page_media` then converts
every one to `IMG|<sprite_dir>|<alt>` uniformly.

### 3b. network_browser_manager.c — merge should also replace MEDIA rows

In `merge_render_rows()` (manager `:980–981`), add MEDIA to the
replacement list so worker MEDIA rows replace (not duplicate) the
static ones:

```c
// BEFORE:
if (strncmp(row, "TITLE|", 6) == 0 || strncmp(row, "TEXT|", 5) == 0 ||
    strncmp(row, "LINK|", 5) == 0 || strncmp(row, "IMG|", 4) == 0)
    continue;

// AFTER:
if (strncmp(row, "TITLE|", 6) == 0 || strncmp(row, "TEXT|", 5) == 0 ||
    strncmp(row, "LINK|", 5) == 0 || strncmp(row, "IMG|", 4) == 0 ||
    strncmp(row, "MEDIA|", 6) == 0)
    continue;
```

Without this, the worker's MEDIA rows sit alongside the static MEDIA
rows and produce duplicate tiles.

---

## 4. Fix D5: placeholder tile on fetch failure

Currently (`collect_page_media:1117`):
```c
if (!fetch_to_sprite(fetch_url, dir)) continue;  // row dropped
```

On decode failure the row is silently dropped. Fix:

1. Create a default placeholder sprite at startup:
   `…/&.hq-apps/network/ops/placeholder_sprite.csv` — a small
   (e.g., grey with "?" character) sprite.csv, committed.
   OR generate it at runtime from a 1-byte PNG via stb_image.
2. On fetch failure, emit a fallback row using the placeholder dir:
   ```c
   if (!fetch_to_sprite(fetch_url, dir)) {
       snprintf(rel, sizeof(rel),
           "%s/&.hq-apps/network/ops/placeholder_sprite",
           g_house);
       fprintf(wf, "IMG|%s|%s\n", rel, alt);
       media_i++;
       continue;
   }
   ```

Phase-1 scope: grey 16×16 tile with the alt text shown. The sprite
engine (`hq_blit_sprite`) renders the tile; the projector's `<item
label=>` shows the alt text below it.

---

## 5. Build and verify

### Build (fresh make, no cached objects)
```bash
# Renderer (shared):
cd "…/*.monads/*.livedesk-taskbar/ops"
./build_core_render.sh          # → +x/khtpm_core_render.+x

# Browser manager + worker:
cd "…/&.hq-apps/network"
./build.sh                      # → +x/network_browser_manager.+x
                                #   + ops/+x/nb_js_worker.+x
                                #   + ops/+x/nb_media_to_sprite.+x
```

### Relaunch
```bash
HOUSE_ROOT="<…>/44.xyz.01.00"
# Kill old browser; clean stale module_parent.pid
kill $(pgrep -f "network_browser_manager")
rm -f "$HOUSE_ROOT/&.hq-apps/network/module_parent.pid"
# Launch fresh
cd "…/&.hq-apps/network"
./button.sh "$HOUSE_ROOT"
```

### Probe B1: static page image tiles
Navigate to a static page with `<img>` tags. In the address bar:
```
go:<url_with_images>
```
Expected: image tiles appear in the sprite grid with real pixel content,
each labelled with alt text. No broken tiles.

### Probe B2: JS page image tiles
Navigate to a JS-rendered page that injects `<img>` via JavaScript.
(or eval `document.body.innerHTML='<img src="https://httpbin.org/image/png" alt="test">'` via
`eval:...`). Expected: the injected image tile shows a real sprite
sprite tile, not a blank rectangle.

### Probe B3: broken image placeholder
Navigate to a page with an `<img src="https://invalid/broken.png">`.
Expected: a grey placeholder tile with alt text, no blank gap.

---

## 6. Relevant code anchors (post-2cbf0019 merge)

| File | Line(s) | What |
|------|---------|------|
| `network_browser_manager.c` | 429–683 | `extract_and_publish` — static row emitter |
| `network_browser_manager.c` | 965–992 | `merge_render_rows` — worker overlay |
| `network_browser_manager.c` | 1019 | `media_skip_url` — skip pixel/analytics/data URIs |
| `network_browser_manager.c` | 1075–1131 | `collect_page_media` — MEDIA→sprite→IMG conversion |
| `network_browser_manager.c` | 2238–2239 | `do_fetch` ordering: run_page_scripts THEN collect_page_media |
| `ops/nb_js_worker.c` | 409–441 | `dom_walk_render` — worker's IMG emission |
| `ops/nb_media_to_sprite.c` | (binary) | stb_image decode → 64×64 sprite.csv |
| `&.widgits/_shared-lib/khtpm_draw_core.c` | ~145/~219/~533 | `hq_sprite` / `hq_blit_sprite` / `kh_draw_canvas` |

---

## 7. Notes for handoff

- The projector (`write_chtpm_projection`, manager `~2490–2660`)
  consumes `IMG|<sprite_dir>|<alt>` rows and emits
  `<item sprite="<dir>" label="<alt>"/>`. After D4, only sprite-dir
  paths appear in IMG rows (no raw URLs).
- `nb_media_to_sprite` writes `sprite.csv` (a specific text format for
  ≤64px indexed-color sprites), NOT a raw PNG. The sprite engine reads
  this format.
- Phase-2 (D6, optional later): a full-size canvas view
  (`<canvas id="media-view">`) using `kh_draw_canvas` (stb_image → raw
  RGBA framebuffer). Separate from this task.
