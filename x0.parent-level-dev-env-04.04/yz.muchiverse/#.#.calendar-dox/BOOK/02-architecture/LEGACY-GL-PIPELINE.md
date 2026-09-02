# The legacy chtpm_parser_pal → GL rendering pipeline, end to end

*Condensed from `!.HOUSE_STDS.md` §B, 2026-09-02. Applies to
chtpm_parser_pal-family projects still on the ASCII→GL mirror path
(Stage 1/2 in `CENTROID_GOLD_STD.md`'s history) — required reading
before touching any GL-related work in one of them.*

```
your compose op (writes text)
  → pieces/apps/player_app/view.txt        (your project's raw content)
  → pieces/display/current_frame.txt       (chtpm_parser_pal's chrome-wrapped composition, substituting ${game_map} etc.)
      [trigger: pieces/display/frame_changed.txt   — grown by process_key() on EVERY key, unconditionally]
      [trigger: pieces/display/renderer_pulse.txt  — grown by compose_frame(), also unconditional]
  → system/renderer (ASCII terminal path)  — polls the trigger files, prints current_frame.txt, logs frame_history.txt
  → system/chtpm_rgb_render (GL path, persistent daemon) — polls BOTH triggers, font-rasterizes current_frame.txt into:
        pieces/display/rgb_frame.raw (raw RGBA32) + rgb_frame.receipt.txt (frame_w/frame_h/checksum)
      [trigger: pieces/display/rgb_frame_changed.txt]
  → system/gl_mirror (real GLUT window) — polls rgb_frame_changed.txt, uploads rgb_frame.raw as a GL texture, blits one quad
      [receipt: pieces/display/gl_display.receipt.txt — cross-checks source vs. what actually got uploaded]
```

Key facts that cause real bugs if missed:

- `chtpm_rgb_render` is genuinely project-agnostic — zero game-state
  awareness, it rasterizes whatever text is in `current_frame.txt` and
  re-renders on either trigger growing, unconditionally, regardless of
  what your own ops do. You cannot suppress this from your side.
- If your project ALSO writes `rgb_frame.raw` directly (a custom 3D
  renderer), you get a real write-write race against this daemon —
  write a separate overlay file instead of writing that path directly.
- The **receipt + checksum pattern** (`frame_w`/`frame_h`/
  `checksum_fnv1a64`, plus `gl_display.receipt.txt` on the GL-upload
  side) is load-bearing — `gl_mirror.c` reads its own window/texture
  dimensions from the receipt dynamically, since two different
  renderers can write different-sized frames to the same path. Any new
  renderer should follow this pattern rather than hardcoding
  dimensions downstream.
- **Headless GL verification**: `ops/dump_rgb_png.c` converts
  `rgb_frame.raw` into a viewable PNG (an agent can't look at a live
  GLUT window). Direct pixel-buffer sampling in Python
  (`open(path,'rb').read()`, index by `(y*W+x)*4`) is the faster
  alternative actually used in practice.
