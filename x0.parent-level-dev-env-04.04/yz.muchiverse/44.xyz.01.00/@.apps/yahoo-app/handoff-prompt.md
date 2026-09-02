# HANDOFF — yahoo-app GL mirror blank

## Problem

`sh button.sh r` launches successfully. Terminal shows ASCII frames (bank screen, broker_select screen, piece_methods render correctly). The separate GL window (`gl_mirror`) opens but stays **blank/black**.

## Root cause (high confidence)

The GL mirror (`gl_mirror`) does NOT display the terminal ASCII frames. It only displays `pieces/display/rgb_frame.raw` — a raw RGBA32 texture buffer written by `chtpm_rgb_render` (which runs `compose_rgb_frame.+x` internally).

Currently:
- `yahoo_compose_frame.+x` writes ASCII text to `pieces/apps/player_app/view.txt`
- `chtpm_parser_pal` reads `view.txt`, substitutes `${game_map}`, renders terminal frame → `current_frame.txt`
- Terminal `renderer` reads `current_frame.txt` and prints ASCII to stdout ✓
- `chtpm_rgb_render` also watches `frame_changed.txt`, but it runs `compose_rgb_frame` internally — which **we have not written** for yahoo-app

**Missing piece:** `ops/compose_rgb_frame.c` (or `ops/yahoo_compose_rgb_frame.c`) — this is the op that converts the ASCII `current_frame.txt` into RGBA32 pixels in `rgb_frame.raw`. Without it, `gl_mirror` has nothing to display.

## What works

- App compiles clean
- `button.sh run` starts without crashing
- Terminal shows real content (balance, watchlist, broker list)
- `piece_methods` render correctly in terminal
- `frame_changed.txt` marker fires (terminal updates on keypress)

## What is broken

- GL mirror window stays black/blank
- No `rgb_frame.raw` is being generated (or it's empty/stale)

## Fix needed

1. **Write `ops/yahoo_compose_rgb_frame.c`** — modeled on `@.apps/my-chara-txt/ops/mychara_compose_rgb_frame.c` or `101.mutaclsym`'s equivalent. This op must:
   - Read `pieces/display/current_frame.txt` (ASCII frame)
   - Render each character as a textured glyph quad into an RGBA32 buffer
   - Write buffer to `pieces/display/rgb_frame.raw`
   - Write receipt to `pieces/display/rgb_frame.receipt.txt` (width, height, checksum)
   - Use glyph data from `pieces/registry/fonts/ascii/` (already present from my-chara-txt copy)

2. **Register it in `default_op.txt`** — add `ops/+x/yahoo_compose_rgb_frame.+x`

3. **Add to `scripts/build.sh`** — compile the new op

4. **Verify `chtpm_rgb_render` is finding and running it** — `chtpm_rgb_render.c` looks for `compose_rgb_frame.+x` in the ops directory. The naming must match what it expects, OR we need to confirm `chtpm_rgb_render`'s expected binary name.

## Files to check for reference

- `@.apps/my-chara-txt/ops/mychara_compose_rgb_frame.c` (if it exists)
- `101.mutaclsym*/ops/compose_rgb_frame.c`
- `@.apps/my-chara-txt/system/chtpm_rgb_render.c` — to see what binary name it execs
- `&.widgits/file-menu/ops/` — file-menu widget may have its own rgb composer

## Quick verification after fix

```bash
cd @.apps/yahoo-app
bash button.sh compile
ls -la pieces/display/rgb_frame.raw   # should exist and grow after keypresses
ls -la pieces/display/rgb_frame.receipt.txt  # should show frame_w/frame_h/checksum
```

The GL window should then show the same content as the terminal frame (mirrored as texture).
