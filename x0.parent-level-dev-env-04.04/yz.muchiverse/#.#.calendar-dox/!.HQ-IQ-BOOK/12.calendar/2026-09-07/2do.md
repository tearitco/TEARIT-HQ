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

## Open / next

- Everything still open in `2026-09-05/` and `2026-09-06/` 2do.
- Back to gameplay work.
