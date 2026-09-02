# pchq board window: history-injection parity checklist (2026-08-30)

Real, repeatable local test that isolates the render/engine pipeline from
X11 input delivery entirely - answers "does write-history-then-render-
from-frame actually work" without needing real hardware input or a
mapped window at all. Direct instruction: "why cant u inject keys into
history .txt and check frame history against rgb dump for parity and
make a local checklist?"

## What this does and does NOT prove

**Proves**: the real engine pipeline - `chtpm_parser_pal` (reads
`pieces/keyboard/history.txt`) -> internal nav/interact state machine ->
`chtpm_rgb_render` compositor (writes `pieces/display/rgb_frame.raw` +
`.receipt.txt`) - works end to end. This is the SAME pipeline
`run_pchq_board_mode()`'s KeyPress/ButtonPress handlers forward into via
`pchq_append_key()`/`pchq_write_click_kv()`.

**Does NOT prove**: that real X11/Mutter hardware input actually reaches
the khtpm window's event loop in the first place. That is a wholly
separate, upstream question (see the override_redirect fix, same-day
commit `402c812b`) - this checklist starts AFTER that step, by injecting
directly into the files the window's own handlers would have written to.

## Real steps (verified live, 2026-08-30)

1. Find the live board-viewer session dir for the host project:
   `&.widgits/board-viewer/pieces/sessions/<ts>-<pid>/`
   (or resolve it the real way via `ledger_peers.+x`, same as
   `pchq_find_board_session()` does).

2. Note `pieces/display/rgb_frame.receipt.txt`'s `checksum_fnv1a64`
   BEFORE.

3. Append directly to BOTH real relay files (matching
   `pchq_append_key()`'s own dual-write):
   ```
   echo "<code>" >> pieces/apps/player_app/history.txt
   echo "KEY_PRESSED: <code>" >> pieces/keyboard/history.txt
   ```
   Real codes: `13`=Enter/Interact-toggle, `1000..1003`=
   LEFT/RIGHT/UP/DOWN (`pchq_map_special_key()`'s own arrow mapping).

4. Poll `pieces/display/current_frame.txt`'s mtime (or the receipt's
   checksum) for up to ~15s - the real engine loop picks up a new key
   within ~1-2s in every live test run so far. NOT instant - a synchronous
   diff immediately after the `echo` will show no change yet; this is
   real polling latency, not a broken pipeline.

5. Dump `rgb_frame.raw` + its receipt's `frame_w`/`frame_h` to a PNG
   directly (no X11 window needed) via a plain RGBA-buffer-to-PNG
   encode, and read it to confirm the real visual delta.

## Real, live-verified result (this run)

- Injected `1002` (ARROW_UP). Checksum changed
  `1c39d7e4c6f5299f` -> `69c7e1fbbccb5754` within 2s.
- Status text changed `Selected (7,9)` -> `Selected (7,11)`,
  `cam 0,4` -> `cam 0,6` - real selector/camera movement, matching the
  injected key exactly.
- Confirms: the real engine, real compositor, and the real relay-file
  contract `run_pchq_board_mode()` writes into are all functioning
  correctly, independent of any X11/window-focus question.

## Standing checklist for future regressions on this window

- [ ] `pieces/keyboard/history.txt` and `pieces/apps/player_app/
      history.txt` both receive the dual-write on every real key.
- [ ] `pieces/apps/player_app/state.txt`'s `last_click_x`/`last_click_y`
      update on click (via `pchq_write_click_kv()`).
- [ ] `rgb_frame.raw`'s checksum changes within ~2s of a valid injected
      key/click.
- [ ] The real text status block (`current_frame.txt`) reflects the
      expected state change (selector coords, interact-mode glyph,
      camera yaw/pitch/pan).
- [ ] Only THEN - if all the above hold but real hardware input still
      doesn't work - is the bug upstream in X11/window-focus territory,
      not the render/relay pipeline. This checklist exists specifically
      to make that distinction fast to re-run.
