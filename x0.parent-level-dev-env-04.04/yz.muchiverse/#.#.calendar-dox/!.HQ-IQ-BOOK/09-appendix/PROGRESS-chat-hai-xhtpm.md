# PROGRESS — chat-hai → static xhtpm + compiled projector

**Branch:** `chtpm-var-substitution`  **Date:** 2026-09-03  **Status:** done (parallel), verified headless

## What changed
The bash projector `ops/chat_hai_projector.sh` (not an allowed projector
kind) is replaced by **`ops/chat_hai_projector.c`** (`+x/chat_hai_projector.+x`,
`ops/build_chat_hai_projector.sh`). Same job: read `chat_hai_loop.sh`'s
live state and publish `state/ui.txt` (key=value) instead of
regenerating `chat-hai.chtpm` markup.

- **`chat-hai.xhtpm`** — static template. `class="chat-hai-pal database-window"`
  (neither token hits a `g_is_*` exact-strcmp trigger; `g_is_chat_hai`
  was already deleted from `khtpm_core_render.c` on 2026-09-01).
  Sessions sidebar (`<repeat>`), status line, Stop / Speed controls,
  transcript as one `<repeat count="${n_msgs}">` of `<text>` rows.
- **`button-pal.sh`** — parallel launcher. Old `chat-hai.chtpm` +
  `.bootstrap` + `button.sh` + `chat_hai_projector.sh` stay as rollback.

## Inputs the projector reads (under `<app>/`)
```
state/sessions/active.txt      live session name
state/sessions/<name>.ledger   "[ts] speaker: text | Trigger: ..."
state/paused.txt               "1" = stopped
state/typing.txt               persona currently generating
chat_hai_config.pdl            SECTION | sleep_between | <secs>
```
Output `state/ui.txt`: `sessions_count` / `session_N_label` / `session_N_name`,
`status`, `pause_label`, `speed_label`, `n_msgs` / `msg_N` (each row's
`|` mapped away — frame-dump separator).

## Verified headless
Launched via `button-pal.sh`; frame dump: Sessions sidebar (main /
philosophy), `[running]` status, Stop + Speed items (nav 4-5),
transcript msg0..N as `<text>` rows — no overlap, no `|` corruption,
class did not trip `g_is_*` (generic chrome present).

## Left / gaps
- Streaming/typing indicator is a static `${typing}` line, not live
  per-token.
- Not wired into a launcher/menu — parallel only. Retarget `button.sh`
  + delete the bash projector after sign-off.
- Multi-`<module>` (loop + projector both forked by the renderer) — if
  only one `<module>` launches on this build, start the loop from
  `button-pal.sh` instead (check `kh_launch_window_modules` call count).
