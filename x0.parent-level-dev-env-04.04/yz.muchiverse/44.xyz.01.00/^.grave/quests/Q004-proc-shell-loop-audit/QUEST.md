# Q004 — replace per-pid shell loops over /proc

| field | value |
|---|---|
| status | open |
| tier | student or worker |
| size | S–M |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | none |

## Mission (one sentence)

Replace the shell loops that walk `/proc` forking tools per process with a single `pgrep`/`ps`, keeping each script's behavior identical.

## Why it matters

On this weak-CPU machine one such scan cost 1.9 s per call (349 processes); `run_khtpm_strip.sh` did it 3+ times per start
(fixed 2026-10-06, commit `7c7401090`, see `04-bugs/TASKBAR-STARTUP-LATENCY-RESEARCH-2026-10-06.md`). The same shape remains elsewhere.

## Read first

1. `04-bugs/TASKBAR-STARTUP-LATENCY-RESEARCH-2026-10-06.md` (the measurement and the fix pattern)
2. `_.monads/_.livedesk-taskbar/ops/run_khtpm_strip.sh` `strip_parser_pids()` (the finished example: one `pgrep -f` with the same predicate)

## Do

Audit these, found by `grep -rIl 'for p in /proc/\[0-9\]\*' --include=*.sh` at posting time, and fix each only if the loop forks per pid:
`$.crypts/button.sh`, `&.widgits/bookmarks/bm_menu.sh`, `&.widgits/entity-cli/ops/move_entity_on_desk.sh`,
`&.widgits/palettes/palettes_menu.sh`, `0.user-pal.../test-harn-same/scenarios/demo_session_character_window.sh`.
Keep the exact match each loop used. For each, time the old and new function on the same machine state.

## Acceptance

- [ ] For every changed function: old vs new timing and the identical list of matched pids (paste both).
- [ ] No behavior change in the script's own purpose (state what you ran to check).
- [ ] Do not restart the owner's desktop to test; test the function in isolation.

## Rules

Everything in `^.grave/README.md`. `$.crypts/button.sh` is the house's start/quit/reset script: change only the pid-finding helper, nothing else.

## Log

## Result
