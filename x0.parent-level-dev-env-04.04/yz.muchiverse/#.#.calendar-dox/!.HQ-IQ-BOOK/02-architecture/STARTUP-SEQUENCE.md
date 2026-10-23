# Start-up sequence and the loading strip (2026-10-23)

Owner 2026-10-23: "i no longer see loading bar. i should see loading bar immediately before anything else happens or it will feel slow / broken. currently thats how it felt". Labels: **READ** = from files, **RAN** = measured, **PROPOSED** = not built.

## 1. The ways a desktop is started
| Path | What runs, in order (READ) |
|---|---|
| Desktop shortcut / app grid (`$.crypts/livedesk-launch.sh`) | boot-splash (NEW first step) -> icon refresh in the background -> `build_khtpm_strip.sh` (hash-gated, about 1 s when nothing changed) -> `button.sh run` |
| `button.sh run / start / restart` | boot-splash (NEW first step) -> icon refresh (background) -> `close_listed.sh` -> `crypt_autostart` (quit the old desktop, then the autostart.pdl LAUNCH rows: tool-bar = `run_khtpm_strip.sh boot`, world-manager, hotbar-desk, ui-sounds) |
| `button.sh reset` (also the taskbar restart) | boot-splash (NEW first step) -> kill everything, `sleep 1`, `save-user-data.sh` (a few seconds) -> `build_khtpm_strip.sh` -> `crypt_autostart` |
`run_khtpm_strip.sh boot` truncates `#.desktop/boot_timeline.txt` ("script boot started"); the menu manager and the bottom dock then append their own marks.

## 2. What was wrong (RAN / READ)
1. **The strip started late.** The launcher ran `livedesk-icon-refresh.sh` first: **1.92 s** measured (Pillow) before the strip. On `reset` the strip was started only by the build step, AFTER the kills, `sleep 1` and the data snapshot, so the old desktop vanished and nothing showed for seconds. `button.sh run` never started it at all (the `boot` path skips the build, which was the only other place that did).
2. **The strip closed too early.** By a 2026-10-06 decision it existed "for the COMPILE only": with a hash-gated no-op build (the normal case) it closed the moment the build finished, or after 4 s if no build ran. But the new desktop needs about **3 s** to draw its bottom bar and **about 12 s** to finish (boot_timeline of the 11:41 launch: bottom bar drawn at :08.99, `dock redraw` marks until :20), so there was a blank screen for most of the start-up. That is the "slow / broken" feeling.
3. No record of the press-to-strip gap existed: `boot_timeline.txt` is reset when the new boot begins, which also wiped the strip's own "closed" line.

## 3. What changed (BUILT)
- `$.crypts/boot-splash.sh` (new): the ONE way to start the strip. Singleton (pid file + comm check), returns in about 20 ms, strip window up about 65 ms after the call (RAN). Called FIRST by `livedesk-launch.sh` and by `button.sh run|r|start|restart|reset`. `--trace <text>` appends a timestamped line to `#.desktop/boot_trace.txt` (not reset by the boot; capped at about 300 lines; git-ignored). Trace marks: press seen, strip started / already running, icon refresh done, build step done, reset: closed + snapshot done, reset: build done.
- `livedesk-launch.sh`: strip first, icon refresh moved into the background.
- `livedesk_splash.c` (--boot, no real compile): follows the NEW boot's own timeline marks instead of closing: phase 0 "Starting" (creeps to 25 %), 1 "Starting desktop" (script boot started, 30 %), 2 "Loading menus" (manager first publish, 55 %), 3 "Loading desktop" (bottom bar drawn, 75 %, creeping to 96 %), then "Ready" and close once no mark has appeared for 1.5 s (at the latest 25 s after the bar was drawn). Only marks written at or after (strip start - 3 s) count, so the previous boot or the still-running old desktop cannot move it. Gives up after 30 s if no boot follows. The real-compile display and the BUILD FAILED banner are unchanged.

## 4. Evidence (RAN, scratch house with synthetic marks; the live desktop was not restarted)
Strip visible 37 ms after start; still up at 3.9 s with only 90-second-old marks present (they were ignored); stage captures show Starting -> Starting desktop -> Loading menus -> Loading desktop -> Loading desktop (settling); closed 1.69 s after the last mark with `splash closed ... (ready)`. Screenshot montage: scratchpad `strip_montage.png`.

## 5. Not verified / PROPOSED
- **A real start has not been run** (it would restart the owner's desktop). After one, read `#.desktop/boot_trace.txt` (press -> strip gap, kill/snapshot time) and `boot_timeline.txt`.
- "Ready" is defined as quiet for 1.5 s after the bottom bar; entity windows that appear later than that are not waited for. The 4 phases are time-creep between real marks, not a measured percentage.
- Where the remaining seconds go (about 3 s to the bottom bar, about 12 s to settle, the `reset` data snapshot) is **not reduced**, only made visible. Candidates to shorten: the data snapshot on reset, the icon refresh, entity spawn order.
- No automated test of the strip (it needs a display); hand-run with captures.
