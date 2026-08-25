# view-vs-muta.md — board-viewer's 3D render pipeline vs. mutaclysm's real, working one

Written 2026-08-02, direct instruction: "compare more closely to mutaclysm to achieve parity... trust mutaclysm and ask if unsure before deviating." This document is that comparison, done properly (direct code reads, both sides), not assumed.

**Bug being chased**: pressing `1`-`4` (camera_mode) briefly shows a new 3D view, then it reverts to the old 2D/emoji content moments later — a flicker-back, not a stable switch.

---

## 1. WHAT I BUILT (WRONG) — direct rgb_frame.raw write

My first `bv_render_3d.c` wrote `pieces/display/rgb_frame.raw` **directly**, same file the shared `system/chtpm_rgb_render` daemon also writes. I *thought* I'd avoided the race by suppressing board-viewer's own `frame_changed.txt` bump while `render_mode==1` — but that only blocks ONE of the triggers `chtpm_rgb_render` watches.

**Confirmed via direct code read of `014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c`**: `process_key()` unconditionally regrows `pieces/display/frame_changed.txt` at the very end, for *every* key — including ones relayed through the real `onClick="INTERACT"` element board-viewer's nav mode uses (`chtpm_parser_pal.c:3555-3559`, the `"NAV MARKER: For ALL layouts..."` comment makes this explicit — it says "ALL layouts"). My own suppression only touched board-viewer's *own* op; it never had any control over the shared engine's own unconditional write. Worse, `compose_frame()` (the engine's own internal chrome-composition function, run whenever its `dirty` flag is set) *also* unconditionally regrows `renderer_pulse.txt` (`chtpm_parser_pal.c:3031-3033`) — a completely separate trigger I never touched at all.

So every single arrow/camera key sent while in 3D mode regrew at least one of the two triggers `chtpm_rgb_render` watches, causing it to re-render `current_frame.txt` (still describing the OLD 2D board) right back over my fresh 3D pixels within ~30ms (its own poll cadence). That's the flicker: my 3D frame briefly wins the write, then the engine's own routine key-bookkeeping wins it back moments later.

**The wrong assumption, stated plainly**: I assumed "stop board-viewer's own op from triggering a re-render" was sufficient to stop the shared engine from re-rendering. It wasn't — the shared engine re-renders on ITS OWN bookkeeping, unconditionally, regardless of anything board-viewer's own ops do or don't trigger.

---

## 2. WHAT MUTACLYSM ACTUALLY DOES — never touches rgb_frame.raw from the 3D pass at all

Confirmed via direct code read of `101.mutaclsym🧟‍♂️️+18.01/ops/compose_rgb_frame.c` and mutaclysm's own (forked) `system/chtpm_rgb_render.c`:

Mutaclysm's 3D pass does **not** race for `rgb_frame.raw`. It writes a completely separate file:
```c
// ops/compose_rgb_frame.c:1870-1871
snprintf(overlay_path, ..., "%s/pieces/display/rgb_frame_3d_overlay.raw", project_root);
snprintf(overlay_receipt_path, ..., "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", project_root);
```
A flat RGBA8888 dump, viewport-sized (not full-frame), atomic-written (`.tmp` + `rename()`), with `overlay_w`/`overlay_h` recorded in the sidecar receipt (no in-file header).

**The compositing happens INSIDE mutaclysm's own fork of `chtpm_rgb_render.c`**, not in mutaclysm's own ops at all. `ops/compose_frame.c` (the plain 2D/ASCII composer, which produces the text `current_frame.txt`/`view.txt` mutaclysm's engine copy reads) emits a single sentinel byte — `0x01` (SOH), **not** a printable string — as its own line, immediately before the map's own viewport rows, but *only* when `render_mode==1`:
```c
// ops/compose_frame.c:1088
if (render_mode == 1) { fputc(0x01, out); fputc('\n', out); }
```
Its own header comment (`compose_frame.c:573-580`) states the reasoning directly: *"chtpm mode's own GL rendering (shared-ops/chtpm_rgb_render.c) font-rasterizes THIS file's own text output verbatim — it has zero game-state awareness by design... this file emits a sentinel marker line... to let it composite mutaclsym's own real 3D view into the right screen rectangle without hardcoding pixel offsets."* A real ASCII terminal doesn't visibly render a bare control byte, so this is invisible/harmless there — the same underlying text stream serves both renderers.

Mutaclysm's own forked `chtpm_rgb_render.c` watches for that `0x01` byte as it walks `current_frame.txt` line-by-line (`chtpm_rgb_render.c:705-713`), and on finding it, calls `blit_overlay(fb, row*GLYPH_H)` — reads `rgb_frame_3d_overlay.raw`/`.receipt.txt` fresh off disk right there, blits it 1:1 (no scaling) into the shared framebuffer at the row position the marker happened to occupy in the text stream, then skips ahead `(ov_h+GLYPH_H-1)/GLYPH_H` further source lines so the plain-text viewport rows underneath (still present in the file, just now geometrically "covered") don't also get font-rasterized over the same rectangle.

**Critically**: this does NOT eliminate the unconditional `frame_changed.txt`/`renderer_pulse.txt` regrowth — those still fire on every key exactly as before. It makes the race **harmless instead of eliminating it**: since `compose_rgb_frame.c` never writes `rgb_frame.raw` itself, there is no write-write race on the shared output at all. `chtpm_rgb_render` remains the SOLE writer of `rgb_frame.raw`, full stop — and every single time it redraws (for *any* reason, triggered by *anything*), it re-reads whichever overlay file is currently newest on disk as part of that same redraw. Staleness becomes structurally impossible: there is nothing external for the engine's redraw to race against, because the 3D content isn't a competing writer, it's an input the sole writer reads fresh every time.

---

## 3. THE FIX — port mutaclysm's exact mechanism, not a workaround

Direct instruction: *"use the parser within mutaclysm, copy it if u need."* Applied as:

1. **`system/chtpm_rgb_render` is now copied from mutaclysm** (`101.mutaclsym🧟‍♂️️+18.01/system/chtpm_rgb_render`), not the generic `014.wsr-pal` copy — the generic copy has NO `MAP3D_MARKER`/`blit_overlay()` support at all (confirmed: neither function name appears in it). Verified the two files are otherwise near-identical in size/function inventory (877-898 lines, same function set) before swapping, per the standing instruction to check for regressions before reusing a fork wholesale — the only real difference is this overlay-compositing addition; `load_glyphs`/`write_receipt`/plain-ASCII rendering are unaffected.
2. **`bv_compose_frame.c`** now emits the same `0x01`+`\n` sentinel line, in the same position (immediately before the viewport rows), only while `render_mode==1` — followed by exactly enough blank filler lines to match the overlay's own pixel height in text-line units (`ceil(overlay_h_px / GLYPH_H)`), so the engine's own row-skip lands exactly where the real 2D viewport rows would otherwise have shown through.
3. **`bv_render_3d.c`** no longer writes `pieces/display/rgb_frame.raw`/`rgb_frame.receipt.txt`/`rgb_frame_changed.txt` at all — it writes `pieces/display/rgb_frame_3d_overlay.raw` + `rgb_frame_3d_overlay.receipt.txt` instead, matching mutaclysm's exact field names/format (`overlay_w=`/`overlay_h=` in the receipt).
4. The earlier `if (!render_mode) ping_chtpm_render_marker(...)` suppression hack in `bv_compose_frame.c` is now **removed** — it was solving the wrong problem (trying to stop the shared engine's own redraw) and is unnecessary once board-viewer stops competing for `rgb_frame.raw` in the first place. The engine can redraw as often as it wants now; it'll simply always pick up whatever overlay is freshest.

---

## 4. OPEN ITEM, NOT YET RESOLVED

Mutaclysm's own overlay dimensions are computed from its own `VIEWPORT_W`/`VIEWPORT_H`/`TILE_PX` constants (its 3D raymarch resolution). Board-viewer's own overlay size in this fix is a fresh choice (not a mutaclysm value carried over) — chosen to keep the marker/skip-line arithmetic self-consistent within board-viewer's own file, not because it matches any specific mutaclysm number. If the resulting 3D viewport looks too small/cramped once tested live, that's a tunable size, not a parity requirement — flag it and it can be resized freely without touching the marker mechanism itself.
