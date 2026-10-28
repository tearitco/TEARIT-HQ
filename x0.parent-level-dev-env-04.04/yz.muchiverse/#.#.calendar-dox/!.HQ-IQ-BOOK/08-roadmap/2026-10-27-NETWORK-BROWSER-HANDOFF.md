# NETWORK BROWSER - HANDOFF (2026-10-27)

Branch: `opencode` (HEAD `453790ddd`)  
Nothing pushed. All gates green on fresh build.

## What landed this session
**Commit `453790ddd`** - `fix: survive hostile page scripts instead of silently killing the worker`

### ops/nb_js_worker.c
- Replaced `signal(SIGALRM) + alarm() + _exit(128+sig)` with QuickJS runtime interrupt handler. Slow scripts now abort with `InternalError: interrupted` and are catchable by C, worker survives with DOM intact. 
- Measured: abort is **NOT catchable by page JS** (fixture with 3x `try{while(true){}}catch` ended at first abort with single "interrupted" line).
- `NB_CRASH_BT=1` (env-gated): fatal-signal backtrace written to worker log via `backtrace_symbols_fd` (no gdb on box). 
- `-rdynamic` added in build.sh so backtraces resolve function names.

### network_browser_manager.c  
- Reap unexpected worker death on every main loop tick, publish `script aborted - partial page`, log signal/exit, respawn on next request (no more zombies/silent dead pane).
- `NB_EVAL_BUDGET` from launcher now honored (default still 60).
- `worker_load()` failure no longer discarded - publishes partial page status.

## Verified evidence
- Loopy `while(true){}` → `ready`, worker alive, 1x `interrupted`
- CatchLoop fixture → `ready`, worker alive  
- `kill -SEGV` worker → `script aborted - partial page`, no zombie, `[worker] died unexpectedly (signal 11)`, eval recovers after respawn
- Gates: gBCR 100% (9/9), flex PASS, grid PASS, wasm PASS

## Cloudflare / glassdoor (measured honestly)
Orchestrate script **finishes in seconds** now, throws `TypeError: not a function` at `<script>:1:28370`. Missing browser API = **R4 fingerprint surface**, not budget issue. 4chan not challenged (200, 884 thread rows). To get cf_clearance need R4 API work.

## Current state
- Manager running (PID varies), worker alive
- Status on glassdoor: `ready`, title `Security | Glassdoor` (challenge page loads, scripts run, fail on missing APIs)
- Last commit on branch opencode: `453790ddd`

## Next up (user said "both" - R4 + @media)
**R4 fingerprint surface** is blocking Cloudflare. Need to:
1. Identify exact missing API at orchestrate failure points (errors show native call sites)
2. Add minimal shims for missing APIs (currentScript, URL, etc.) as needed
3. Do @media (G2.5) in parallel where possible

Relay files: `#.desktop/network_browser_request.txt`, `#.desktop/network_browser_status.state.txt`, `#.desktop/network_browser_page.state.txt`, `#.desktop/network_browser_console.txt`, `#.desktop/network_browser_worker.err.log`

Build: `bash &.hq-apps/network/build.sh` from house root or network dir.
Tests: `sh &.hq-apps/network/tests/nb_gbcr_test.sh && sh &.hq-apps/network/tests/nb_flex_test.sh && sh &.hq-apps/network/tests/nb_grid_test.sh && sh &.hq-apps/network/tests/nb_wasm_test.sh`
