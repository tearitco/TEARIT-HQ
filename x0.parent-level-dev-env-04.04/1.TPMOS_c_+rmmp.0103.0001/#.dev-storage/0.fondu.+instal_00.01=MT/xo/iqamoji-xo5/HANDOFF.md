# iqamoji-xo5 Handoff

This is the first scaffold pass for the `iqamoji-xo5` exo bot.

Current shape:
- copied from `xo/bot5`
- existing bot5 recorder/orchestrator/baseline kept intact
- new `ops/` directory introduced for focused methods
- future design is FSM + RL + curriculum + small chat node, not a large LLM

What the bot is meant to learn:
- traverse directories
- inspect files and folders safely
- record what it observed
- accept reward and punishment signals
- label sessions and goals
- progress through curricula
- eventually coordinate an FSM with RL memory and one small language node

Near-term implementation plan:

1. Keep `bot5` as the known-good baseline.
2. Use `ops/` for small executable methods.
3. Keep state on disk in `pieces/`.
4. Use `curriculum/` for staged learning tasks.
5. Use `memory_map.kvp` as a simple registry.
6. Add reward/punishment bookkeeping before any larger policy work.
7. Add directory traversal and observation ops before any chat expansion.

Reference style:
The IQABOD orchestrator pattern shows the direction for curriculum-aware control
flow, but `iqamoji-xo5` should stay simpler.

Important boundary:
Do not try to build the final bot in one pass. The first milestone is a small,
file-backed, testable exo bot with explicit ops and clear state files.

Suggested future files:
- `curriculum/`
- `pieces/rl/`
- `pieces/goals/`
- `pieces/observations/`
- `ops/README.md`

This note is the local handoff for future agents working on `iqamoji-xo5`.
