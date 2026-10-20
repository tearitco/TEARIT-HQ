# Palette tile-switch lag: measured cause, the fix, and how systems usually handle slow loads

Written 2026-10-20 (owner report: "when it switches tiles in palettes there is a slight lag, which could cause the user to re-enter or change"). Code: `&.widgits/_shared-lib/sprite_cache.c` (new, pure, text-included), `khtpm_draw_core.c` (`hq_sprite_ex`), `khtpm_core_render.c` (`redraw()` wrapper, one line in the event loop), op `&.widgits/palettes/ops/sprite_cache_op.c`. Bug bounty: `04-bugs/bug_bounty.md` (2026-10-20 section).

## 1. How systems usually handle "the UI is slow while content loads"

The owner's question first, because the answer shaped what we built. None of these is the fix on its own; they answer different questions.

| Technique | Answers | Used here? |
|---|---|---|
| **Make the work cheaper** (cache decoded results, pre-convert to a cheap format) | why is it slow | yes, the big win (section 4a) |
| **Do less work** (load only what is visible, cancel loads nobody will see) | why is it slow | already true for the visible part; stale loads cancel for free (4c) |
| **Progressive load** (draw placeholders, fill in over several frames, keep input alive) | the window freezes | yes (4b) |
| **Busy flag / ignore input until loaded** | the user acts twice | not needed once paints stay short; see section 6 |
| **Latest wins** (sequence number, drop results of superseded requests) | an old load finishing over a newer choice | automatic here: a tile that is no longer drawn is never asked for |
| **Queue and coalesce** | every action must land | no |
| **Optimistic update + ack** | perceived latency | no |
| **Marker-file signal** (house rule: size growth, never mtime) | a reader needs to know a writer finished | already how the manager, projector and renderer talk; not the problem here |
| **Visible pending state** (spinner, dimmed tile) | users re-click when nothing moves | the empty cell is the pending state for now |

The lesson: the order of fixes is cheaper work, then progressive, then input guards. A guard (ignore clicks until loaded) only hides a slow load; it does not shorten it.

## 2. The measured cause (nothing below is assumed)

A palette tile is a **text file**: `<dir>/sprite.csv`, a `# resolution=48` line then one `r,g,b,a` line per pixel (about 31 KB for a 48x48 tile, 7,070 files, about 250 MB of text under `&.widgits/palettes/sprites/`). `hq_sprite()` in `khtpm_draw_core.c` decoded one with `fgets` + `sscanf` per pixel, inside the paint, for every visible tile.

* The swatch-grid layout gives off-screen tiles `w = h = 0` (`khtpm_core_render.c`, the `row < 0 || row >= visible_rows` line), so `draw_elem` returns early for them: only about 12 rows are decoded, about 324 tiles on the cdda furniture page. The sprite-grid-row and sprite-flow layouts keep their size when parked at `y = -100000`, so they would decode hidden tiles too (not changed here, see section 7).
* Decode cost with a warm file cache (`XO/palette_lag/parse_cost.c`, replaying the decode loop): cdda 512 tiles 347 ms (0.68 ms per tile), df 256 tiles 223 ms, ohrrpgce 160 tiles 126 ms, emojis 30 tiles 40 ms (1.3 ms per tile, 64 px). A cold disk or a busy CPU is slower; this machine has had load and heavy background work.
* In the real window (`KH_REDRAW_TRACE=1`, cache off and no budget, which is the old behaviour): the first paint of the cdda page took **268-269 ms** (twice), one frozen stretch. With the cache warm the same paint took **63-64 ms** and spent 8-10 ms on tiles.

## 3. What it is NOT (also measured, so nobody re-chases it)

* **Not the blits.** 324 tile blits cost 5-15 ms per paint including the per-tile backdrop readback. Not the nav badges, not layout (a whole warm paint is about 70 ms).
* **Not the in-memory cache.** `g_hq_sprite_cache` (512 slots, LRU) already existed and works; it only helps after a tile has been decoded once in that process, and it dies with the window. A palette larger than 512 tiles (cdda has 1,556 sprites) evicts as it scrolls.
* **A second, separate lag source: the event loop waits `select(...150 ms)` before repainting**, even when a repaint is already owed (`hq_run_event_loop`, `tv = { 0, 150000 }` for every ordinary window). Every repaint request that is not a direct `redraw()` call waits up to 150 ms plus the 30 ms `house_wait_us` floor. Measured as the 0.23-0.28 s spacing between the repaints of a freshly opened palette window (strace: repeated `pselect6` calls blocking 0.150 s each). This is general input latency for every khtpm window and is **not changed here** except for the one case below (section 5, bug bounty item 2): a blanket "dirty means no wait" would make windows that re-dirty every tick (hover fills) repaint at the 33 Hz floor.

## 4. What was built

### 4a. Decoded-sprite disk cache (`sprite_cache.c`)

The first decode writes `~/.cache/lyfedesk-sprites/<fnv1a64 of the directory path>.rgba`: a 24-byte header (`SPR1`, resolution, source csv size and mtime) plus the raw RGBA. Every later load reads that one block instead of parsing about 1,000 text lines.

* **Outside the repo** on purpose: the csv files are tracked in git, so sidecar files next to them would show up as thousands of untracked files. Override with `LYFEDESK_SPRITE_CACHE=<dir>`; `off` disables it.
* **Validity**: the header's csv size and mtime must equal the csv's current `stat`; the file must be exactly header + `res*res*4` bytes (a torn write is shorter, a foreign file longer). Anything else is ignored and re-decoded, then rewritten. This is source validation of a derived file, not change polling, so the house "marker not mtime" rule for pollers does not apply; the renderer already compared csv mtime for its in-memory cache.
* **Safe to delete at any time** (`sprite_cache_op purge`). Writes are tmp (`.tmp<pid>`) + `rename`, so two windows filling the same entry cannot corrupt it.
* **Any failure degrades to the old behaviour**: cannot create the dir, the path is a file, the dir is read-only, `HOME` unset, a short or garbage cache file: the tile is decoded from the csv exactly as before.
* Disk: 85 MB for all 7,070 sprites (4 KB blocks included).

### 4b. Progressive load (the per-frame budget)

`hq_sprite_ex(dir, defer_ok)`: palette grid tiles (`class swatch` / `pal-tile`) pass `defer_ok = 1`. When the current paint has already spent `LYFEDESK_SPRITE_BUDGET_MS` (default **60**, `0` = unlimited = old behaviour) on cold decodes and done at least one, further cold tiles return NULL: the cell is left empty, and `redraw()` (now a wrapper around `redraw_body()`) sets `g_frame_dirty` so another paint follows. The event loop skips its idle `select` wait for that one owed paint. Every paint loads at least one tile, so it always finishes. Cache hits and every other caller (taskbar, dock, skins) behave exactly as before.

With the disk cache warm, the budget almost never triggers (decoding 327 tiles from cache is 8-10 ms). It matters for the **first view of a page on a machine that has not seen it**, and when the CPU is busy.

### 4c. Stale loads cancel themselves

A tile that scrolled away or belongs to a palette that was switched away from is no longer drawn, so it is never asked for and its load never happens. No cancellation code is needed; the paint is the request.

### 4d. Tools

`ops/+x/sprite_cache_op.+x` (built by `ops/build_sprite_cache_op.sh`): `bench <state.tsv>` (cache hits / decodes / ms), `warm <root>` (fill the cache for a tree; run niced: `nice -n 15 ionice -c3`), `frames <state.tsv> [budget_ms]` (simulate paints under the budget), `stat <dir>`, `purge`. Same code the renderer runs. `KH_REDRAW_TRACE=1` now also prints a `[REDRAW-COST]` line per paint: total ms, tile decode ms and count, blit ms and count, whether tiles were deferred.

To pre-fill the cache once (about 5-11 s of CPU): `nice -n 15 ionice -c3 sprite_cache_op.+x warm "<house>/&.widgits/palettes/sprites"`. Done once on the owner's machine 2026-10-20 (7,070 files, 85 MB). After that the progressive path rarely runs.

## 5. Proof (2026-10-20, all fresh runs; scratch files in `XO/palette_lag/`)

| Check | Result |
|---|---|
| cache vs csv decode, **every real sprite** (`verify_all`) | 7,070 / 7,070 byte-identical, all served from the cache |
| unit test of the real `sprite_cache.c` (`test_sprite_cache`) | 20 PASS, 0 failures: first load decodes, second hits, pixels identical, changed mtime re-decoded, changed size with same mtime re-decoded, truncated / trailing-garbage / random cache ignored, missing csv NULL, `resolution=0` NULL, short body NULL and no cache written, budget first-load-allowed / deferral flagged / new frame resets |
| negative controls | the flipped-byte comparison fails as it should; mutants (mtime check removed, size check removed, trailing-byte check removed, deferral flag removed) each fail exactly the matching check. One mutant survived and is equivalent (see "limits") |
| failure paths | cache dir uncreatable, path is a file, read-only dir, `HOME` unset: all load correctly by decode, no crash |
| four writers racing on 113 cold entries | all 113 identical afterwards, no `.tmp` files left |
| bench, cache hit vs csv decode | cdda 512 tiles 347 ms -> 14.5 ms, df 223 -> 4.7, ohrrpgce 126 -> 3.1, emojis 40 -> 0.7 (first view with the cache write is 10-35 % slower than a bare decode) |
| frames simulation, cdda 512 tiles | 14 paints at most 31.5 ms each under a 30 ms budget (old: one 347 ms frame); cache warm: one 17.8 ms frame; budget 0.001 ms: still finishes, one tile per frame (256 of 256) |
| **real window**, same page, cdda furniture, about 324 visible tiles, longest single paint | old behaviour **268 / 269 ms**; new code cold cache, budget 60 **124 ms** in 5 paints (budget 30: about 92 ms in 10 paints; budget 120: about 190 ms in 4); warm cache **63-64 ms**; real defaults after warming the real cache **71.5 ms** |
| **screenshots** of the window (`dump_frame_png_op`) | old, cold progressive, warm, default: all **byte-identical** (same md5) |
| before the event-loop change | cold fill took 11 paints over 2.7 s because each slice waited out the 150 ms select (negative control for that change); after it, 10 paints over 1.2 s at budget 30 |
| build gate | `^.grave/quests/Q003-build-gate-include-list/verify.sh core` PASS (`sprite_cache.c` added to `CR_SRCS`, otherwise editing it would leave a stale `khtpm_core_render.+x`) |

## 6. Re-clicking and the input question

The owner's worry was that a laggy switch makes the user click again. With paints bounded (about 64 ms warm, about 125 ms on a first view) and the window repainting between slices, a second click is handled normally and simply wins: the next paint draws whatever tiles are current. **Not tested**: a rapid double click on two palette tabs against a live window, and whether `set-cdda-mod` / `arm-cdda` actions (`palettes_menu.sh`, a shell script per click) double-apply when two arrive close together. If that proves real, the usual answers in section 1 apply: latest-wins on the manager side, with the marker file as the acknowledgement.

## 7. Limits and what is not done

* **Not changed**: the 150 ms idle wait for ordinary repaints (section 3). It is the next real lag target; it needs a CPU-safe design (a marker-driven wake, not "dirty means no wait").
* **Not changed**: sprite-grid-row and sprite-flow layouts decode parked (hidden) tiles. The palettes use the swatch grid, which does not; other windows that use those layouts may.
* **Not changed**: `projector` + `<repeat>` expansion cost of a 512-row palette (reparse, var table). Not measured separately: the whole warm paint is about 70 ms.
* **Cache key** is a 64-bit hash of the directory path; the header's size and mtime make a wrong hit very unlikely, but a collision is not impossible in principle.
* **Same size and same mtime but different pixels** (an edit that preserves both) would be served stale. Not seen; `purge` clears it.
* **Equivalent mutant**: removing the "at least one load per frame" clause in `spc_may_load()` changes nothing, because the spent time is 0 at the start of every frame; the guarantee comes from that ordering, which the tiny-budget simulation exercises.
* **Not tested**: the taskbar/dock (non-deferring callers) against the new `spc_load`, beyond the 7,070 palette sprites and code review; a palette of more than 512 tiles scrolling live; a second window of the same palette open at the same time; behaviour under heavy CPU load (the deferral is built for it but the live runs were on a quiet machine, load average about 1.4).
* Windows already open keep the old renderer until relaunched.

## 8. Rules this adds

1. A paint must not do unbounded work. Anything that decodes, scans or fetches per element goes through a budget or a cache, never straight into `draw_elem`.
2. Cache a derived file **outside** a tracked tree, validate it against its source, and treat every defect in it as "not there".
3. When a number is surprising, instrument the repaint (`KH_REDRAW_TRACE=1` now prints per-paint cost) before choosing a fix: the first design here (progressive alone) made the cold view slower until the event-loop wait was measured.
