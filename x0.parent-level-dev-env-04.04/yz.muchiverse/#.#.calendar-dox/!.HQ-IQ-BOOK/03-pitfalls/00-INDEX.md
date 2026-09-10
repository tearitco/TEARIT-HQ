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

- **`mon-hq`** — taskbar HQ menu → **`mon`**. Lists every board/engine/hq
  process split into **GOOD** (owned) vs **BAD** (stray/leaked), with a
  one-click **KILL ALL BAD**. Standalone too:
  `sh 44.xyz.01.00/&.hq-apps/mon-hq/mon_scan.sh list` / `… kill-all`.
  Full writeup incl. the 2026-09-10 incident and how GOOD/BAD is
  defined:
  [`&.hq-apps/mon-hq/README.md`](../../../44.xyz.01.00/&.hq-apps/mon-hq/README.md).
- `OPERATIONAL-LANDMINES.md` #9 — kill child processes, not just the
  window process.
- `HOUSE_CODE_PITFALLS.md` #17 — the grey-frame flash (a *different*
  weak-box symptom: a producer/consumer file-handoff gap).
- `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/kill_hq_windows.sh` —
  the ledger-pgid emergency reaper behind the `!kill hq` row; blind
  (kills by registry, shows nothing). `mon-hq` is the observable,
  engine-aware companion.
