# 12.calendar — durable learnings from archived dated dirs

Condensed from `2026-09-05/` through `2026-09-15/`, all moved to
`#.Zarchive-2-trash/12.calendar/` — this file is the "still worth
finding later" extract, not a day-by-day diary. Routine status chatter
(what got committed, CPU-throttle checks, git housekeeping already
resolved) is left out; see `03-pitfalls/`, `04-bugs/BUG-LOG.md`,
`08-roadmap/design-docs/` and the biz-book chapters below for where
the durable artifacts of this work actually live now.

## The trigger layer — CLOSED, end to end (2026-09-13/14)

The real missing piece this week: nothing in-game ever fired a Common
Event automatically — the only path was a human pressing the events-hq
editor's own "▶ Play" button. Built and proven live (commits `ac10a579`,
`3e621a4d`, `15075cbe`, `7f64428d`): a real `pc_trigger_watcher.c`
daemon watches the existing multi-writer `master_ledger.txt`
(discovered already real and already shared — no new ledger needed),
and fires the matching Common Event when a player walks onto a
`trigger=player-touch` tile. Verified live: walking onto
`cdda_sample`'s `x=6,y=5` tile produced a real `SHOW_TEXT_FILE` command
with zero manual steps. Full design doc:
`08-roadmap/design-docs/EVENT-TRIGGER-LAYER-PLAN.md`.

Real, still-open gap left behind (small, contained): **no per-entity
trigger scoping** — only one `player-touch` Common Event exists
house-wide, so a second one would currently fire every matching-trigger
event, not just the right one. Fix this before scheduling a second
trigger-driven Common Event.

This unblocked `PLAY-MODE-ENTITY-HARNESS-DESIGN.md` (see
`11.brainstorm/LEARNINGS.md`), which was not started as of 2026-09-15
and is still open per `08-roadmap/OPEN-ITEMS.md`.

Also root-caused along the way and worth remembering: a test harness
that looked broken for one reason was actually five independent drift
bugs stacked together (dead relay target, a whole feature — the
Scratch `view_mode` — that no longer exists but the harness still
tested for, house-wide shared nav numbering not reset per window, a
wrong hardcoded PNG-dump path, and a stale-built VM binary). Lesson:
when a harness "just doesn't work," don't assume one root cause —
verify each assertion against real, current code, not what a comment
or an older harness assumed.

## Sept 6–7: architecture/process hygiene, still-standing rules

- **Multi-agent git**: the `claude`/`opencode`/`grok` branch split is
  save-lanes, not competition — confirmed both agents' work reliably
  ends up combined via `git push origin <branch>:main` fast-forwards.
  Full protocol: `01-orientation/GIT-WORKFLOW-FOR-BEGINNERS.md`,
  `01-orientation/BRANCH-STRATEGY.md`.
- **Cross-platform discipline held up under audit**: a real day's work
  (deleting a bespoke scroll-translate branch, routing through the
  shared `css_layout_pass`/`generic_sbar_register` instead) was
  confirmed net cross-platform-positive — no new Xlib calls, no
  `#ifdef`s needed. The actual unlock item is still open:
  `khtpm_core_render.c` has zero `_WIN32` guards (`CROSS-PLATFORM-
  PENDING-2026-08-29.md` item #1) even though the taskbar *strip* got
  a full Win32 GDI twin. `kh_plat.h` (shared-lib platform shim) shipped
  as the real proof-of-pattern, one manager migrated onto it.
- **Disposable-file / runtime hygiene**: `tidy-runtime.sh` (framebuffers,
  ascii frames, oversized logs) and dead-PID reaping for
  `#.desktop/entity_menu_*` scratch files are real, standing
  maintenance scripts — run them, don't assume disk bloat is a house
  bug.
- **`X.quit` no longer logs the whole session out** — was calling
  `kill(getppid())` to "clean up," which under `setsid`/`nohup`
  launch is `systemd --user`, not the taskbar's own parent. Never
  SIGTERM your own launcher's parent to tidy up; give session-ending
  actions their own named verb. (`03-pitfalls/HOUSE_CODE_PITFALLS.md`
  #15.)

## Sept 15: IPC protocol drift, and the monolith-vs-microservices question

Real bug (`KTB_STRIP_N_CELLS` hardcoded one behind the real cell
count) reopened the standing "is the renderer a monolith?" question.
Conclusion, still valid: the *process* split (one binary per
window/entity, separate taskbar-manager binary) is real and healthy;
`khtpm_core_render.c` being one large file is a deliberate trade-off
(house rule against dynamic linking/plugins), not an accident — its
growing size is a real, accepted cost, not a bug to "fix" by breaking
it up. The actual weak point this bug exposed is that cross-process
IPC is bare integer code-ranges each side hardcodes independently
(`KSC_TAB_BASE`, `KSC_SET_FOCUS_BASE`, etc.) — the fix is a
self-describing contract (derive counts from live data instead of a
duplicated constant), not a transport change. Don't reach for
pipes/shared memory here — the flat-text relay convention is a real,
deliberate, greppable-by-hand feature of this house, not a limitation.

## Sept 11 handout — tool-assignment reasoning (durable; % status is not)

The Sept 11 handout's own code-state percentages (events/db wiring
~30–40%, drag/drop ~0%) are now stale — the trigger layer closed
2026-09-14 — but its **tool-assignment reasoning**, which the handout
itself flagged as durable, is still a fair rule of thumb:
- **Kilo** (tiny context, hard-stops on overflow): narrow, single-file,
  clear-finish-line tasks only.
- **Browser-prompting sessions**: research-shaped questions (compare
  options, read external docs) — weak fit for anything needing this
  house's own file-relay conventions.
- **By hand, not delegated**: anything needing real judgment about
  what's actually "done" vs. worth flagging — audits are the classic
  case an agent will rubber-stamp wrong while trying to look
  productive.
- The cheapest real path to a multiplayer game loop identified there
  (IRC-chat-hq/chain-hq's already-structured per-message state as a
  Common Event trigger source — "a chat message matching pattern X in
  room Y fires Common Event Z") is still real and still not started;
  worth re-reading if multiplayer work is ever picked up.

## cursword disappearing on click — RESOLVED 2026-09-15

Root-caused and fixed the same session (ARGB Visual/Colormap mismatch
in a shared draw helper, silent `exit(1)` via Xlib's default error
handler). Full writeup, including the durable `strace`-based
investigation technique: `13.agent-coms/LEARNINGS.md`; permanent
pitfall entry: `03-pitfalls/HOUSE_CODE_PITFALLS.md` #23.
