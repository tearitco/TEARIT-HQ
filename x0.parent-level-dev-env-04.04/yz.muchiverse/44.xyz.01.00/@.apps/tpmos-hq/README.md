# tpmos-hq

A toy (Toys menu: one entry per `@.apps/<dir>/toy.pdl`). Shows a TPMOS frame in an x11-hq window.

- `display.pdl` - backend (`x11-hq`), the TPMOS folder, the frame file, the marker file, the keys file, rows.
- `ops/tpmos_hq.c` - `status`, `key <code>`, and resident mode (follows the TPMOS frame marker, writes `state/ui.txt`).
- `tpmos-hq.xhtpm` / `.css` - the window.
- `sh button.sh run` starts the REAL TPMOS (its own `button.sh run`, under a pseudo-terminal via `script`) if it is not running, then opens the window; `start` / `stop` / `status` do the parts. TPMOS's `run_orchestrator.sh` begins with `pkill -9 -f` patterns that can match house processes, so `start` refuses (and names the pids) when one exists; override with `TPMOS_HQ_FORCE=1`. `stop` signals only processes whose working directory or executable is inside the TPMOS folder, never pkill.
- Keys: click `INT` (top left), then type; every raw key is appended to TPMOS's own `pieces/keyboard/history.txt` as `[ts] KEY_PRESSED: code`, which its key handler / orchestrator reads. Off by default.
- Resize: the window is user-resizable (the corner grip at the bottom right). Do not add `fixed-size` to the window class: it switches the grip off.
- Test against a scratch TPMOS: `TPMOS_HQ_DISPLAY=<copy of display.pdl> ops/+x/tpmos_hq.+x status`.
