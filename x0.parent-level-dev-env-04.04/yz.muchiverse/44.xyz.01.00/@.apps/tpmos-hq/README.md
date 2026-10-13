# tpmos-hq

A toy (Toys menu: one entry per `@.apps/<dir>/toy.pdl`). Shows a TPMOS frame in an x11-hq window.

- `display.pdl` - backend (`x11-hq`), the TPMOS folder, the frame file, the marker file, the keys file, rows.
- `ops/tpmos_hq.c` - `status`, `key <code>`, and resident mode (follows the TPMOS frame marker, writes `state/ui.txt`).
- `tpmos-hq.xhtpm` / `.css` - the window.
- It does not start TPMOS. TPMOS's `run_orchestrator.sh` begins with `pkill -9 -f` patterns that can match house processes.
- Test against a scratch TPMOS: `TPMOS_HQ_DISPLAY=<copy of display.pdl> ops/+x/tpmos_hq.+x status`.
