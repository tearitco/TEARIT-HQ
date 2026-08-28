# opencode catch-up — 2026-08-27

**Read this FIRST if you are opencode/ox-alpha resuming work on
`COMMON-EVENTS-MANAGER-HANDOFF.md`.** Your own last log entry in that
doc is `### 2026-08-26, ox-alpha — Task 3 DONE (Conditional Branch +
OP_BNE) ⛔ STOP` at **line 2323**, claiming Task 3 was done and ready
for review. A LOT has happened in that doc since your quota ran out
right after writing that entry — this doc tells you exactly what, and
exactly where to resume reading, so you don't redo work or trust a
status claim (including your own) that's since been superseded.

## 1. Your Task 3 claim was premature — a real bug, now fixed

Read `COMMON-EVENTS-MANAGER-HANDOFF.md` starting at **line 2404**
(`### 🚨 Sonnet found this via direct code review`) through **line
2617** (`### ✅ TASK 3 DONE (Sonnet, 2026-08-27)`). Short version: your
`compile_page()` two-pass rewrite was real, correct STRUCTURE, but
`skip_depth` was incremented at `if` and decremented at `end` without
ever distinguishing "inside the true branch" from "inside the false
branch" — the practical effect was BOTH branches' real commands got
silently dropped from the compiled `.pal`, confirmed live (hand-built
IR, real recompile, read the actual output file). This has since been
fixed (skip_depth logic corrected) and fully re-verified with fresh
evidence — 13/13 tests pass on the FIXED binary, both compile-time
structure and runtime branch-divergence confirmed. **Task 3 is
genuinely done now** — don't re-open or re-verify it, the doc has real,
fresh evidence at line 2568 onward.

## 2. Everything after Task 3 is ALSO already done

Skim (don't re-verify unless something looks wrong) these sections, in
doc order, so you know the real current state:
- **Task 4 (Common Events manager, Autorun/Parallel)** — done, plus a
  real design gap found and closed (a "Switch" field UI was missing —
  fixed) and a real bug found and fixed (trigger/switch updates were
  silently destroying each other in `condition.pdl` — fixed). See the
  KPI table around **line 458** for the one-line status of every task,
  and search for `### 2026-08-27, Haiku (Final Agent)` for the fix
  detail.
- **Task 5 (Visual Scripting tab stub)** — done, built directly by
  Sonnet (not delegated). Real Scripting/Scratch/Blueprints toolbar,
  correct highlight-box sizing, a real rendering bug found and fixed
  along the way (`evhq_draw_elem()` had no `w<=0||h<=0` guard).
- **Task 6 (Common Events real inline editor)** and **Task 7 (command
  rows nav-reachable)** — both done. Search `✅ Task 6 DONE` and `✅ Task
  7 — CORE DONE`.
- **A real PNG-dump bug + fix, and a new frame-history signal
  mechanism** for events-hq/db-hq (`events_hq_frame_history.txt`/
  `db_hq_frame_history.txt`, one line per redraw with real state
  fields) — read `_.0.aigent-testing-k9.txt`'s tail section if you need
  the detail; you don't need to re-fix anything here, just know these
  files exist now if you're building a new harness.

## 3. Real NEW documents written since your last session (read as needed)

- `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` — what's still missing
  before a real RPG-Maker-style game can be built (tiles/map-authoring,
  message/choice commands, a player/collision runtime loop). Two of its
  three gaps have since been resolved/corrected — see its own top
  "UPDATE" note.
- `CURSWORD-SOUL-VISION.md` — cursword's identity as the user's "SOUL"
  entity, a capability roadmap, and a scoped-but-unbuilt idea (Gemma
  selecting FSM/BT actions to drive the real UI).
- `MARKETABLE-FEATURES.md` — a real, cited catalog of every taskbar
  feature's actual working state (was needed after two marketing/
  owner-report videos missed several real features the first time).
- `HARNESS-AUTHORING-GUIDE.md` — **the one you most likely need if
  you're picking up new work** (see §4 below).

## 4. The real next task queued for you (or whoever picks this up)

Read `COMMON-EVENTS-MANAGER-HANDOFF.md`'s own **`## 🔧 NEW TASK — Loop
+ Wait commands`** section (search for that heading — it's near the
end of the doc, right before `## ➡️ Confirmed next-steps order`). Short
version: two real, working proof-of-concept PAL-authored test harnesses
were built and verified live this session
(`cursword/harnesses/pal/task5_view_tab_switch_demo.pal` and
`task3_switch_branch_verify.pal` — read them, they're commented line by
line and show the real syscalls/registers in use). The next real,
scoped task is adding a `SYS_SLEEP` syscall plus real `Loop`/`Wait`
event commands so more/harder harnesses (starting with
`common_events_manager_test_harness.sh`'s Autorun/Parallel test) can
also be ported to PAL. **Read `HARNESS-AUTHORING-GUIDE.md` §3 in full
before starting this** — it has the exact feasibility findings, syscall
numbers already in use (1-7), and both working example files.

## 5. Where to resume reading in the handoff doc itself

Skip from your own line 2323 straight to **line 2617**
(`### ✅ TASK 3 DONE`) and read forward from there to the end of the
file (currently ~2925 lines) — that's the real, current, unbroken
narrative of everything that happened after your last session. Do not
re-derive status from your own line-2323 entry; it's superseded.

## 6. Standing house convention reminders (unchanged, still apply)

- Trust but verify — this doc's own history (including your Task 3
  claim above) is the live proof of why: a status claim, even a
  detailed one, isn't the same as fresh, independently-run evidence.
- Disposable fixtures only — never edit `cursword`'s own real
  `event_pkg/pages/page_1/`, `greet_player`, or `shop_open` to prove a
  point. This session hit that mistake twice; don't make it a third
  time.
- Zero stray processes, confirmed via `pgrep`, before and after any
  live test.
