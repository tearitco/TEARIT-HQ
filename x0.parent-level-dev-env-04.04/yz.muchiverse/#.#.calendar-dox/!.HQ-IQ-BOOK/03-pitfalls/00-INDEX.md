# 03 — Pitfalls

- `HOUSE_CODE_PITFALLS.md` — real, live-confirmed problems + fixes,
  general-purpose. Moved verbatim (`git mv`) from
  `44.xyz.01.00/HOUSE_CODE_PITFALLS.md`. **Append new entries here.**
- `OPERATIONAL-LANDMINES.md` — habits/judgment landmines (build-script
  overwrites, per-mode file broadcast, xdotool dangers, verification
  discipline). Condensed from `SKILLS.md` §3-4.
- `X11-AND-SESSION-PITFALLS.md` — X11 focus, build, and session
  pitfalls, hard-condensed from `!.HOUSE_STDS.md` §F's 22-item list
  (full narrative diagnostic reports cut, standing rules kept).
- `CLI_IO-AND-RENDER-PIPELINE-PITFALLS-2026-09-11.md` — one dedicated
  incident dossier (direct instruction, not the usual "append to
  HOUSE_CODE_PITFALLS.md" shape) covering a full day's real bugs
  around cli_io/reparse: the destroy-and-rebuild reparse root cause
  (now fixed - see `khtpm_reparse_diff.c`), the `content=`/`label=`
  double-echo trap, a real `parse_element()` NULL-deref crash, missing
  select-all, and two testing-methodology pitfalls (relay tests
  masking real-hardware bugs; dual-driving one window from the agent
  and a human at once). Read this before touching cli_io/reparse code
  again.

This is the live pitfalls tracker. When you hit a new landmine, add it
to `HOUSE_CODE_PITFALLS.md` in the same shape as its existing entries
(Symptom / Real cause / Real fix) rather than starting a new file.

## CPU safety

This house runs on a weak CPU. The recurring drain is **leaked engine
stacks**: closing a board / piececraft window with `[X]` does not always
tear down its `button.sh → orchestrator → chtpm_parser_pal → prisc+x`
session, and an abandoned board-viewer diamond loop keeps ticking ~60×/s
forever — ~10 % of a core, per stack, indefinitely. Two of them survived
23 h once.

- **`proc-mon`** — taskbar HQ menu → **`mon`**. Lists every board/engine/hq
  process split into **GOOD** (owned) vs **BAD** (stray/leaked), with a
  one-click **KILL ALL BAD**. Standalone too:
  `sh 44.xyz.01.00/&.hq-apps/proc-mon/mon_scan.sh list` / `… kill-all`.
  Full writeup incl. the 2026-09-10 incident and how GOOD/BAD is
  defined:
  [`&.hq-apps/proc-mon/README.md`](../../../44.xyz.01.00/&.hq-apps/proc-mon/README.md).
- `OPERATIONAL-LANDMINES.md` #9 — kill child processes, not just the
  window process.
- `HOUSE_CODE_PITFALLS.md` #17 — the grey-frame flash (a *different*
  weak-box symptom: a producer/consumer file-handoff gap).
- `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/kill_hq_windows.sh` —
  the ledger-pgid emergency reaper behind the `!kill hq` row; blind
  (kills by registry, shows nothing). `proc-mon` is the observable,
  engine-aware companion.
