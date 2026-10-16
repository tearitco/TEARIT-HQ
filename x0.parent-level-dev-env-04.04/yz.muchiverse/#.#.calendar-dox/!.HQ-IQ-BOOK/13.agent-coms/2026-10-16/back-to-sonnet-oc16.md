# Back to Sonnet — 2026-10-16

From grok. The DF board is open and playable. This is the CPU check the owner asked for, so the idle tax can be cut before the games come back next week. Mechanics to build are in `16.game/GAME-MECHANICS-SPRINT.md`. This note is only the cost of the board that is already running.

## What was measured

2026-10-16, DF board up about 15–21 minutes. `ps` lifetime `%CPU` is the average since start, so it overstates a quiet window. A 3 second read of `/proc/<pid>/stat` (utime+stime, 100 jiffies/s) is the live rate.

| pid | what | lifetime %CPU | live, 3 s | RSS | open fds |
| --- | --- | --- | --- | --- | --- |
| 592947 | board window, `khtpm_core_render` on `df-hq/board/pchq-board.xhtpm` | 5.0 | 7 ticks, about 2% of one core | 22 MB | 5 |
| 592958 | `pchq_board_projector` | 1.9 | about 1% | 2 MB | 4 |
| 539345 | `df_hq.+x` | 1.6 | about 1% | 21 MB | 5 |
| 768196 | `bv_render_3d.+x --daemon` | 1.4 | about 0.3% | 104 MB | 10 |
| 539517 | board-viewer `prisc+x` | 2.0 | about 1.7% | 2 MB | |
| 539483 | df-hq-pchq `prisc+x` | 1.2 | about 1% | 2 MB | |

The family together is under 10% of one core while idle. Nothing is pegged. Firefox and gnome-shell were above the board on the same `ps`. This is not a runaway and not a Chrome-style hog.

`canvas_raw` during the check was `.../board-viewer/pieces/sessions/1792151376-539475/pieces/display/rgb_frame_3d_overlay.raw`, 1,427,600 bytes, mtime unchanged across 2 seconds. The picture was not being rewritten. The cost below is the idle poll, not a frame storm.

File-descriptor counts are 4–10. No descriptor leak in this snapshot. RSS was not watched long enough to call a heap leak; the 104 MB on `bv_render_3d` is the resident raymarch set, and it did not move in the 3 second window.

`strace` could not attach (`ptrace_scope`). The numbers are from `/proc` and from reading the loops.

## Is it unsafe, or just the cost?

Just the cost, and the sleeps are formed. `house_wait.h` floors every wait at 30 ms and the wait sits at the bottom of the loop, not in an `else`. No zero-sleep and no bare busy `while` in the four loops below.

The waste is work that still runs on every wake after the wait.

### Board window — the one worth cleaning

`khtpm_core_render.c` main loop, around the canvas block and the `house_wait_us(HOUSE_WAIT_FLOOR_US)` after it.

- `select` timeout is 16.667 ms while a canvas is up and the window is not minimized. Then every pass also waits 30 ms. Idle pace is about 20 wakes a second, not 60.
- The 16 ms `select` is the old "don't miss a click" timeout. The floor was added because an Expose makes `select` return at once and the core pegs. Both are real. Together they mean `hq_idle_tick()` (theme stat, history poll, dock poll, sidebar restore check) runs even when the frame file did not change.
- A full layout+draw is supposed to happen only when `canvas_raw` size or mtime moves, plus a safety redraw every 5 s. The comment on that safety redraw says a board layout was measured around 67 ms, and at once a second that was about 7% of a core. It is 5 s now. That matches a lifetime average near 5% that falls to about 2% once the window is quiet.
- Cleanup: if `canvas_raw` is unchanged, skip `hq_idle_tick`'s file work, not the wait. The 5 s safety redraw can stay a paint of the pixmap already in hand, not `assign_nav_and_layout`, unless a var file actually changed. Do not remove the 30 ms floor. Do not put the wait back in an `else`.

### Projector — rebuilds a string it rarely writes

`pchq_board_projector.c` `for (;;)` ends in `usleep(300000)`. The wait is fine. Each pass still builds the whole `ui` buffer, including `emit_entities` and `append_entity_sprites`, then writes `state/ui.txt` only when the string differs. That is the ~1%. Stat the inputs (or keep the last mtimes) and skip the build when nothing moved.

### df_hq daemon — 100 ms sleep, full pet reload

`df_hq.c` `daemon_main`: `usleep(100000)` at the top of every pass. Well formed. `load_data` is already gated to `reload_ms` (default 2 s) because reparsing the catalog every 100 ms was the old cost. `pet_load()` is still called every pass and re-reads every pet file plus the trainer, then `calc_view()`. Gate it the same way: mtime or a dirty flag, not a blind read. The INT edge and click polls are already 400 ms and 150 ms.

### 3D daemon — idle wait is fine, the poll is not

`bv_render_3d.c` `--daemon`: `house_wait_us(30000)` at the bottom of every pass, and a slow frame waits for its own duration before the next. That part is right, and it is why this process was near 0% while the raw file sat still.

Every pass still `fopen`s `pchq_board_view.txt` and, when a click camera is primed, `pchq_canvas_click.txt`. Stat first. Open only when the size or mtime moved. The 104 MB RSS is the cost of keeping the raymarch up, not a leak observed here. Quitting the daemon on the existing idle timeout is already implemented (`BV_IDLE_EXIT_TICKS`, about 270 s). It had not tripped because the process was only a few minutes old in the sample (elapsed 4 min on the first `ps`).

### The two prisc processes

About 1–2% each, lifetime. Not read this pass. If the board is still warm after the four items above, the next look is the `prisc+x` tick in the board-viewer session and the df-hq-pchq session. Do not start there.

## What not to do

- Do not drop the 30 ms floor to "make it 60 Hz."
- Do not kill `bv_render_3d` to save the 104 MB while a 3D board is the thing on screen. Idle-exit already covers a board that went away.
- Do not treat Firefox's 6% as this bug.
- The map and INT came back after the board was opened with `PCHQ_PKG`. A bare `khtpm_core_render` on the template is what blanked the canvas earlier. The projector now also resolves `@.apps/<game>/<host-id>` next to the board folder when `PCHQ_PKG` is unset (`ee59c1a00`). Leave that.

## Done on grok, not for this cleanup

Pushed through `ee59c1a00` (bright-skin labels, verse text, board rows, the `PCHQ_PKG` fallback). Local after that, not pushed at the time of this note: `6a8762cfd` and `90a407fad`, the sprint mechanics page. Push those if they are still ahead.
