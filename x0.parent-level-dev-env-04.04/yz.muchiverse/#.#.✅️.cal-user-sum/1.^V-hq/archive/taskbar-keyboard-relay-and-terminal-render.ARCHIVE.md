---
name: taskbar-keyboard-relay-and-terminal-render
description: Two related, not-yet-started taskbar tasks - fix X11 menu keyboard-input relay/reconsumption, and build a terminal-rendered taskbar controllable via the same key-history file
metadata:
  type: project
---

# Taskbar keyboard relay + terminal-rendered taskbar — NEXT STEPS

Recorded 2026-08-18, direct user instruction during a mutaclysm 3D-voxel debugging session (see
`101.mutaclsym🧟‍♂️️19.00/muta-terrain-fix.txt` for that unrelated, separate work) — flagged as "a
step after this" (the terrain/texture fix). **Updated 2026-08-18, same day**: real code was read
this pass (see below) — this is no longer a zero-investigation doc, but it is also NOT a "bug
found" doc. Confidence remains LOW for a specific fix, for real, documented reasons below.

## REAL, CONFIRMED FINDING (2026-08-18 update): `tp_taskbar.c` no longer exists by that name

The file this doc originally pointed at (`&.widgits/livedesk-taskbar/ops/tp_taskbar.c`) was not
found by that path/name this pass. The REAL, CURRENT taskbar implementation lives at
`*.monads/*.livedesk-taskbar/ops/` (note the `*.monads/` prefix, matching the same real directory-
rename convention found elsewhere in the house this session - e.g. book-stack's own real entity
dir) as a SPLIT-PROCESS pair:
- `khtpm_strip_parser.c` — the real X11 PARSER process (rendering + raw KeyPress handling).
- `khtpm_taskbar_manager.c` / `khtpm_taskbar_manager_main.c` — the real MANAGER process (resolves
  all actual key-code dispatch/menu-state logic).

`khtpm_strip_parser.c`'s own header comment (~line 789-822, read in full this pass) confirms it is
a REAL, intentional PORT of `tp_taskbar.c`'s own `poll_agent_relay()`/`agent_relay_dispatch()` -
the old file is not just renamed, it's genuinely been split/refactored (relay dispatch now SHARES
`dispatch_key_code()` with the real KeyPress handler, rather than duplicating it by hand the way
`tp_taskbar.c` deliberately did - a real, documented architectural improvement, not a regression).

## Task 1 — X11 taskbar menu keyboard input: CODE READ, NO STATIC BUG FOUND (real, honest result)

**Direct user description, verbatim intent**: "i believe toolbar x11 menus arent yet working
exactly as desired. that is taking keyboard input 2 file and reconsuming."

**What was actually read this pass** (not just referenced/assumed):
1. `khtpm_strip_parser.c`'s real `poll_agent_relay()` (~line 872-898) — reads
   `#.desktop/livedesk_agent_relay.txt`, one decimal code per line, with real cursor/truncation-
   resync discipline (never replays backlog on first call, resyncs without replay on truncation,
   correctly leaves a partial trailing line for the next poll). Structurally sound - no obvious bug.
2. `dispatch_key_code()` (~line 823-857) — routes Enter/Escape/Backspace/printable to the SAME
   manager-owned `send_code()` a real KeyPress uses, with a real, documented precedence fix (open
   header submenu must be checked before bottom-tab focus, else a stale bottom_doc->focus_index
   could wrongly activate - real bug, real fix, dated 2026-08-11, already landed).
3. `khtpm_taskbar_manager_main.c`'s real `dispatch_code()` (~line 349-450+) — the actual menu/hq
   state machine (cli-io modal > hq popup > bottom-bar digit/focus/tab, in that precedence order,
   matching the parser's own header comment claim). Read a representative ~100 lines covering
   cli-io typing, right-click nav-arm, HQ-quit, and HQ-popup ESC/ENTER/FOCUS/digit/header-click
   dispatch. Found real, already-fixed historical bugs documented inline (e.g. 2026-08-12: clicking
   a different header cell while another's submenu was open used to get silently swallowed - fixed).
   Did NOT find an obvious, currently-live bug in what was read.

**Honest conclusion**: this is dense, actively-maintained, well-documented stateful dispatch code
with a real history of live-tested bug fixes already applied. Reading it cold did not surface an
obvious new defect. This CLASS of bug (a specific key sequence producing wrong behavior in a large
state machine with several interacting modes - cliio_active, hq_open, hq_quit_requested, nav-armed)
is genuinely much more findable via a live, reproducible test than via more code reading - matches
this same file's own repeatedly-referenced lesson (SCOPE ADDENDUM 2026-08-11's own cautionary tale
about building a wrong theory from one ambiguous live result instead of getting a clean repro
first).

**Still required before a fix can be attempted**: a precise, concrete repro from the user - which
specific menu/submenu, which specific key or sequence, observed behavior vs. expected. Nothing in
this doc should be read as "the bug is probably in X" - no specific location is implicated by what
was read. `TASKBAR-MENU-ARCHITECTURE.md` (same `au11-hq/` dir) was NOT re-read this pass either -
do that next, it may already have directly relevant context per `INDEX.md`'s own routing note.

## Task 2 — terminal-rendered taskbar, controllable via the same key-history file

**Direct user intent**: build a terminal (ASCII/text) rendered version of the taskbar, which can
ALSO be controlled/driven using the same key-history text file mechanism the GUI version uses (i.e.
one relay file, two possible renderers/frontends — GUI window and terminal — both consuming the
same real input stream).

**Not yet designed or started.** Real, relevant precedent to check before designing this fresh:
- This project's own chtpm/PAL-VM family (mutaclysm, piececraft-xyz, etc.) already has EXACTLY this
  shape natively — `system/renderer.c` (real terminal text output) and `system/gl_mirror.c`/
  `x11_mirror.c` (real GL/X11 window output) both read the SAME `current_frame.txt`/state files,
  driven by the SAME `pieces/keyboard/history.txt` relay convention. If the taskbar's own
  architecture can be reconciled with (or partially reuse) this existing dual-renderer pattern
  rather than building a parallel, bespoke terminal-render pipeline from scratch, that's likely far
  less real, new work — investigate this angle FIRST before designing something new.
- The real, current parser (`khtpm_strip_parser.c`, see the 2026-08-18 update above — supersedes
  the old `tp_taskbar.c` reference) is raw Xlib, not chtpm/PAL-VM based — a terminal renderer would
  need its own real text-composition logic (drawing menu state/cells/focus as ASCII), most likely a
  NEW function parallel to its existing real X11 drawing code, reading the SAME underlying taskbar
  state (`strip_var_tabs.txt`/`strip_state.txt` per `HARNECIENT-H-AI-RELAY.md`'s own reference to
  these real files, NOT yet re-confirmed this pass) rather than duplicating state.
- Real, existing relay convention to reuse, not reinvent: `#.desktop/livedesk_agent_relay.txt`
  (bare decimal ASCII per line, already used for taskbar digit-nav) is the natural single input
  stream both a terminal AND the existing GUI renderer could consume — confirm this is really the
  same file the user means by "same key history text file" before assuming, since this house has
  multiple distinct relay files with different formats/purposes (see the testing doc's own
  explicit warning: `pieces/keyboard/history.txt`'s `KEY_PRESSED: N` format is NOT the same
  contract as `pieces/apps/player_app/history.txt`'s bare-decimal format, "don't conflate the two").

## See also: `taskbar-tpmos-parallel-refactor.md` (dedicated doc, 2026-08-18)

Deep-dive scoping doc, same `au11-hq/` dir — real TPMOS start-menu (`os.chtpm`), scrolling projects
list (`loader.chtpm`), orchestrator (`orchestrator.c`, the process-lifecycle precedent), and the
`+-demo` project (real ACTIVATE/BACK/KEY:n dispatch engine), mapped directly onto what a taskbar
refactor would need. Has a proposed piece-by-piece mapping table and a list of open questions for
the user. Read that doc before starting any taskbar refactor work — this section below is the
original architectural-gap finding that doc builds on.

## REAL ARCHITECTURAL FINDING (2026-08-18, direct user correction) — the taskbar was built BACKWARDS from the house standard

Direct user statement, verbatim: "i want toolbar to work just like the og mutaclysm project works.
kbd input can go into either terminal or mirror window... we never had it for toolbar yet (tho we
wanted it), we developed it backwards from normal tpmos/chtpm standards. since u didn't understand
that pls communicate it in our documentation in case we need 2 hand off."

**The real, proven house standard** (confirmed live in `101.mutaclsym🧟‍♂️️19.00` this same session,
see that project's own `PITFALLS.txt`): ONE shared state file
(`pieces/display/current_frame.txt`) + ONE shared input relay (`pieces/keyboard/history.txt`,
`KEY_PRESSED: N` lines), with renderer/input front-ends that are separate, swappable PROCESSES
reading/writing those same two files independently of each other:
- `system/keyboard_input` — raw termios, terminal-side INPUT capture only. Writes
  `pieces/keyboard/history.txt`. No rendering.
- `system/renderer` — terminal-side OUTPUT only. Polls a pulse-marker file, prints
  `current_frame.txt` to stdout. No input handling.
- `system/gl_mirror.c` / the shared `x11_mirror.+x` — GUI window OUTPUT (and, for x11_mirror, also
  captures X11 KeyPress events and writes them into the SAME `pieces/keyboard/history.txt`).
All of these can run SIMULTANEOUSLY, independently, against the same two files — that's what makes
"input into either terminal or mirror window, output shown in either/both" possible at all: nothing
about the state/relay format is tied to a specific renderer.

**What the taskbar actually has instead**: `khtpm_strip_parser.c` is ONE process that does BOTH
raw X11 KeyPress capture AND all GUI rendering internally, dispatching resolved key codes directly
into `khtpm_taskbar_manager.c`'s in-process state machine (`dispatch_code()`) — there is no
independent, swappable renderer, no shared `current_frame.txt`-equivalent state file, and no
terminal-output equivalent to `system/renderer.c` at all. `#.desktop/livedesk_agent_relay.txt` (the
taskbar's own relay file, see Task 1/2 above) is closer in spirit to `pieces/keyboard/history.txt`
but is only consumed by this same single bundled process — nothing else could plug into it the way
`system/renderer.c` plugs into `current_frame.txt` today.

**Why this matters for a fix/handoff**: this is NOT a bug to patch — it's a real architectural gap.
Bringing the taskbar in line with the standard would mean splitting `khtpm_strip_parser.c` into a
real input-capture-only piece (writing to a shared relay file) and a real render-only piece (reading
taskbar state and drawing — one GUI version via Xlib, one terminal/ASCII version, parallel to
`system/renderer.c`), with `khtpm_taskbar_manager.c`'s `dispatch_code()` state machine staying as
the shared dispatch logic both call into (much like `sync_camera_to_bv_state()` and
`camera_control.c`/`move_player.c` already stay separate from rendering in mutaclysm). This is a
real refactor, not a bugfix — scope it as its own task, not folded into Task 1's dispatch-bug
investigation above.

**Also confirmed the same pass, mutaclysm's OWN half of "just like OG mutaclysm" is currently
broken too** (not a taskbar issue, but the same "standard" the user is comparing against): in
`101.mutaclsym🧟‍♂️️19.00`, `system/renderer.c` (the terminal ASCII half) exists and is fully intact,
but `button.sh`'s `run` verb never actually launches it — only `system/keyboard_input` runs in the
foreground terminal (capture-only, no display). See that project's own `PITFALLS.txt` for the full
writeup and a proposed one-line fix. Worth fixing that FIRST, since it's the smaller, already-scoped
fix and gives a live reference to point the taskbar refactor at.

## Priority / sequencing

Direct user framing: these are "next steps AFTER" the mutaclysm terrain/texture fix
(`muta-terrain-fix.txt`) — not blocking it, not currently being worked. Pick up after that fix
lands, or whenever explicitly reprioritized.
