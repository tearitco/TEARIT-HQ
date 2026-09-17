# 🤖 small-agent-guide.md — standing orientation for small/low-reasoning models (kilo-cli class)

🚨 You have a SMALL context window and a SMALL reasoning budget. Both
are finite and don't refill mid-task. This file is written to be read
ONCE, in full, at the start of any session — then you go find your
OWN task's specific files. Do not re-read this file mid-task; if you
need more, write a short note instead (see §5). This doc does not
teach you the whole house — see `!.HQ-IQ-COMPACT🧭v0.1.md` for that
(long, human/high-context-agent doc — don't read it unless told to).

This doc assumes your task is one of: **deskgames** (Civilization,
TSOTS, DSR, etc.), **pc-hq / piececraft-hq (mineclonia-style)**,
**LLMUD**, or **fixing an emergent bug**. If it's something else, this
doc still teaches you how to find your bearings fast.

---

## 1. House rules that will actually bite you (don't skip these)

Full source: `02-architecture/CENTROID_GOLD_STD.md`. Read that file's
numbered list only if you're about to touch a `.chtpm`/`.xhtpm`
window, a `*_manager.c`/`*_render.c` pair, or `khtpm_core_render.c`
itself — otherwise these 4 compressed rules are enough:

1. **Never add a new branch/mode inside `khtpm_core_render.c` or any
   other shared parser/renderer.** Business logic lives in your OWN
   separate manager process; the shared renderer only draws what a
   real `.chtpm`/`.xhtpm` + real CSS produced (GOLD_STD items 2, 7;
   also pitfall #11).
2. **A launcher's `button.sh` is not decoration — read it before
   assuming a `.c` file is the live implementation.** This house has
   real cases of a `.c` file that looks current but isn't what
   actually launches (see `piececraft-hq-launch-and-interact` memory;
   also pitfall #1, stale-binary).
3. **Marker/state files are append-only, one writer, size-based change
   detection — never `st_mtime`.** (GOLD_STD item 8; house rule
   `prefer-marker-files-not-mtime`.)
4. **A window's launcher must never block the window from mapping.**
   Do expensive setup after the window is on screen, not before
   (GOLD_STD item 10).
5. **Rebuild AND restart, every time, no exceptions.** A relaunched
   process can silently still be running the OLD binary — this has
   produced real "my fix didn't work" false alarms. `pkill -f <house
   pattern>` is unreliable against this house's emoji/star-globbed
   paths; use `button.sh reset`, not manual kills
   (`03-pitfalls/HOUSE_CODE_PITFALLS.md` #1 and #2).

---

## 2. Where things are

**Deskgames** — `08-roadmap/design-docs/TEST-GAMES-ROADMAP.md`. It's a
VISION doc (2026-09-14): a shared-systems table (§2) + one real
mechanism already live (desk/entity/event trigger layer — Civilization
proves it today) that every other game is meant to build on top of,
not reinvent. Read §2's table to see what system your specific game
task needs and its current status before writing anything.

**pc-hq / piececraft-hq (mineclonia)** — `44.xyz.01.00/@.apps/piececraft-hq/`.
Real convention inside it: **"file = dir, desk = map"** (see comments
in `ops/pchq_board_action.sh` and `ops/pchq_board_projector.c`, dated
2026-09-15) — a project directory is a "file", a desk within it is a
"map". Manager/engine code lives in `ops/` (`pc_world_manager.c`,
`pc_hq_status_manager.c`, `pc_trigger_watcher.c`, `pc_clock_daemon.c`,
etc.), state in `state/`, the ledger at `data/master_ledger.txt`.
Mineclonia's own real reference data (interact rules, core mechanics)
is at `44.xyz.01.00/#.ref/menu/event-guides/mineclonia/`
(`mcl_core.pdl`, `interact.pdl`) — read those before guessing at
mineclonia mechanics.

**Windows/Mac porting** — `00-compact/compact-win-mac.md`: real
current status (Windows partial, Mac ~zero native work), the
`win_posix_shim.h` / `*_win.c` pattern to copy, what's genuinely not
started. Read before touching any cross-platform code.

**LLMUD** — `08-roadmap/design-docs/LLMUD-INTEGRATION-DESIGN.md` did
not exist as of 2026-09-17. If it's still missing when you look, a
parallel design task is expected to create it soon — check for it
first, don't treat LLMUD as undocumented and start from nothing.

**Bug-fixing** — the real, current trackers (not stale docs):
- `04-bugs/BUG-LOG.md` — append-only dated entries, "Open" section at
  top. Don't rewrite old entries; add a dated 🔄 CORRECTION under one
  if it goes stale.
- `04-bugs/bug_bounty.md` — hard-to-pin/recurring bugs, tracked until
  closed, with real root-cause writeups (read a CLOSED entry for the
  house's standard of evidence before writing your own).
- `08-roadmap/OPEN-ITEMS.md` — short numbered list of real open work,
  each with a file pointer.
- `03-pitfalls/HOUSE_CODE_PITFALLS.md` — 23+ numbered, real,
  live-confirmed bugs and fixes. Skim the numbered headers before
  re-diagnosing anything that "looks broken" — several entries
  describe symptoms that look like a regression but aren't (stale
  binary, timing races after edit+reset, relay files keyed by path
  not PID, atomic-write no-file windows, silent X errors, etc.).

---

## 3. How to test things

- **Relay-file injection is the house-standard way to drive a window**
  — write a line into that window's per-PID relay/history file
  (`entity_menu_history/<pid>.txt`-style convention) instead of
  `xdotool`/screenshots (`HOUSE_CODE_PITFALLS.md` #10). Search
  `!.HQ-IQ-COMPACT🧭v0.1.md` for "relay" (§9, "Input & the relay") if
  you need the full mechanism — read only that section.
- **Pixel-level proof**: `44.xyz.01.00/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x`
  — use this when you need to prove a render actually changed, not
  just that a state file updated.
- **Relay tests can pass 3x and still miss a real hardware bug** —
  don't treat a relay-only pass as final proof for focus/grab issues
  (house rule: `relay-testing-may-mask-real-focus-bugs`).
- **Rebuild, then restart with `button.sh reset`, every single time**
  before testing — see §1 rule 5 above. Skipping this is the single
  most common false "still broken" report in this house.

---

## 4. Git

Branch `claude` only. Never touch `main`/`opencode`/`grok` directly.
Commit with a real, specific message (see `git log` in this repo for
tone — short imperative summary + a real "why", not "fix bug").

---

## 5. Writing your OWN compaction/handoff note

When your context is getting heavy mid-task — **don't restate this
guide.** Write a short, separate note that assumes the next reader
already has it. Real, existing house convention for where these go:

- `12.calendar/<YYYY-MM-DD>/` — dated working notes for that day's
  session (e.g. `12.calendar/2026-09-17/your-note.md` today).
- `13.agent-coms/<YYYY-MM-DD>/` — cross-agent handoffs, addressed to a
  specific agent by name if relevant (e.g. a file named `GROK.md` or
  `CURSWORD-DISAPPEARS-ON-CLICK-HANDOFF.md`).

Pick whichever matches what you're writing: a dated log of what you
did → `12.calendar`; a handoff for the NEXT agent to pick up your task
→ `13.agent-coms`. Keep it short: what you found, exact file paths,
what's left, where you got stuck. Don't repeat house rules or paths
already in this doc — just point back to the section number.
