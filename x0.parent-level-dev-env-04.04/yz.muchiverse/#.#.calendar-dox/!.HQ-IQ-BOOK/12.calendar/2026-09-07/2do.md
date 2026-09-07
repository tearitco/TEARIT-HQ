# 2026-09-07

## Done

- **pc-hq board: "clicking File traps you in Interact Mode" — FIXED.**
  User report: "arrow/nav focus keeps jumping back to `[]3.file`."
  Diagnosis: not a nav bug — `pchq_board_action.sh`'s `file`/`desk`
  verbs auto-engaged Interact Mode (`interact_on || append_key 13`) and
  never released it, so `g_interact_relay_on` stayed armed and the
  renderer forwarded every key (arrows included) to the game instead of
  moving local nav — highlight froze on the File item.
  Fix: `file`/`desk` now engage only if it was off and restore it
  afterward (live-re-checked toggle, never double-toggles). Full
  writeup: `09-appendix/pc-hq-bugs.md` Bug 5. Verified with a
  fake-engine harness (both the not-engaged and already-engaged cases).

## Housekeeping: disposable-file policy (founder ask)

"rgba / ascii frames / log files should be trimmed/deleted, never over
250 KB."

- **`44.xyz.01.00/#.desktop/tidy-runtime.sh`** (new) — deletes
  framebuffers (`rgb_frame*.raw`, `*_3d_overlay.raw`, `canvas.raw`,
  `*.rgba32`) when no session engine is running, empties
  `#.desktop/ascii_frames/`, and tails `*.log` / `*frame_history.txt` /
  `gl_cli_out.txt` to their last 250 KB. Skips anything git-tracked.
  `-n` for a dry run.
- **`rezip-house.sh`** (repo root, new) — timestamped `.7z` that now
  excludes `.git`, `*.raw`, `*.rgba32`, `ascii_frames`, `*.log`,
  `*frame_history.txt`, `gl_cli_out.txt`, `node_modules`; deletes the
  prior archive.
- Ran the cleanup once: tree 617 MB → 580 MB (~37 MB of dead
  framebuffers + oversized frame-history mirrors). Touched **zero**
  tracked files (all deletions were gitignored runtime state).
- New `.7z`: **70 MiB** (was 79 MiB). The rest of the size is nested
  component `.7z` archives + `.exe`/`.dll` twins + `duktape.c` +
  ledgers + `.mp3` — real committed content, not touched.
- Added `.gitignore` rule for `x0.parent-level-dev-env-04.04_*.7z`.
- Still git-tracked and oversized (left alone, need a decision):
  `Mar$.$treetRace…/gl_cli_out.txt` (8.9 MB), the 9
  `blockchain.txt.pre-harness-run-*` July backups (~10 MB), a handful
  of tracked `rgb_frame_3d_overlay.raw`.

## CPU-throttling check (founder reported lag)

- **Not thermal** (40 / 53 °C), **not the house.** Load avg ~14 on 8
  cores = contention. Top consumers: **Chrome ~225% (22 procs)**,
  Firefox ~40%, Claude Code ~15%, opencode + its 400%+ `git` snapshot
  bursts on the 561 MB tree. **The whole house = ~6%.**
- The "7 idle khtpm procs" first flagged as strays are **not strays** —
  they're the livedesk pals (asa/ava/book-stack/cursword/m1_ninjadragon/
  m8_redhorned/self) + one placed tile, one renderer each, **0.7% CPU
  total, parked/idle.** No duplicate or orphaned house processes.
  Nothing to kill or prevent there.
- **Real cruft found + cleaned:** `#.desktop/entity_menu_frame_<pid>.txt`
  (760) + `entity_menu_history/<pid>.txt` (807) — per-PID context-menu
  scratch + input-relay files that never self-clean. Reaped the
  1560 whose PID is dead (kept 4 live). `#.desktop/` 19 MB → 6 MB.
- `tidy-runtime.sh` extended with that dead-PID reap so it self-
  maintains going forward.

## Open / next

- Everything still open in `2026-09-05/` and `2026-09-06/` 2do.
- Back to gameplay work.
