# CPU-safety and headless testing discipline

*Condensed from `!.HOUSE_STDS.md` §C, 2026-09-02.*

- **CPU safety is paramount** — the house has crashed from unthrottled
  idle-poll loops before. Every idle-sync PAL loop uses `sleep <ms>`
  between poll iterations; never remove that. Every automated test
  must be `timeout`-wrapped.
- **`timeout` does not reliably kill backgrounded children** of the
  script it wraps — verify no leftover process afterward
  (`ps aux | grep yourproject`) and clean up by PID directly if
  anything survived.
- **`pkill` may be sandboxed/blocked** in some environments (every
  invocation can return non-zero regardless of pattern). Use
  `ps aux | grep` + targeted `kill <pid>` instead of assuming `pkill`
  works.
- **Never touch the user's own live testing sessions** — a long-
  running process you didn't start is very likely the user's own open
  window.
- **Cross-session state contamination**: shared, non-session-scoped
  state files can get stomped by a stale concurrent session. Kill all
  stale sessions before every test.
- **Headless testing**: `NO_GL=1 bash button.sh run-app <focus_root>`
  (or a project's equivalent) gets a real running session with zero
  display dependency. Inject raw decimal keycodes directly into
  `interact_relay.txt` (one per line) to simulate exactly what the
  real engine's key-relay would write.
- **Verifying pixel output without a screen**: read the raw RGBA
  buffer directly in Python (`open(path,'rb').read()`, index by
  `(y*W+x)*4`), sample known coordinates or scan for known colors —
  faster and more reliable than reasoning about the math by hand.
