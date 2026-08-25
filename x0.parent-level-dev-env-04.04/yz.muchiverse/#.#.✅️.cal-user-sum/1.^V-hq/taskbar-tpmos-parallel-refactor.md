---
name: taskbar-tpmos-parallel-refactor
description: Dedicated scoping doc - refactor the livedesk-taskbar to match TPMOS's real start-menu/orchestrator/dynamic-menu shape (header bar, linear list, "-----" separator, independently-dynamic bottom bar)
metadata:
  type: project
---

# Taskbar refactor — TPMOS parallel

## CURRENT STATUS (2026-08-18, updated as of the "cli" ASCII mirror build-out)

**Working, verified live:**
- HQ menu's "cli" row opens a real terminal running the taskbar's ASCII mirror
  (`khtpm_strip_render_ascii.+x` + `khtpm_strip_keyboard_ascii.+x`, launched via `open_cli.sh`).
- Strip (15 header cells) and HQ popup (when open) both render with the real `[cursor] N. [Label]`
  format, ported directly from `chtpm_parser.c`'s own `render_element()`.
- Bottom tab bar renders (read-only) when tabs are open.
- Frame is written to a real `.txt` file (`strip_ascii_current_frame.txt`) before being displayed,
  matching TPMOS's `current_frame.txt` convention — plus an auditable, timestamped
  `strip_ascii_frame_history.txt`.
- Relay-driven HQ open → digit-select → Enter-activate all confirmed working end-to-end (two real
  gaps found+fixed in `khtpm_strip_parser.c`'s `dispatch_key_code()` to make this possible — see
  below).
- No more `\r\n`/staircase rendering bug — render and keyboard-input are two separate binaries now,
  matching TPMOS's real `renderer.c`/`keyboard_input.c` split exactly (this was the actual root
  cause of "staggered" output, not a timing issue).
- Parser/manager poll cadence brought in line with TPMOS's real dual-rate standard (60Hz active /
  10Hz idle, from `!.TPMOS_ONBORD_BIBLE_10.md` §3) — was a flat, non-standard 300ms.

**Strip-cell `[>]` + arrow-key nav — DONE, verified live (2026-08-18, same pass as the report
above)**: direct user report was "i still dont see '>' or have index input jump arrow control like
chtpm parsers are used to having". Real finding: the GUI's own real arrow-key nav (`unified_step()`
in `khtpm_strip_parser.c`) is 100% local to that process's own `g_nav_focus` variable — it NEVER
reaches the manager at all (confirmed by direct read of the real `KeyPress` handler: `XK_Left`/
`XK_Right`/`XK_Up`/`XK_Down` call `unified_step()`/`lay_focus_delta()` directly, never
`dispatch_key_code()`, outside `cli_io` typing). But the MANAGER already had a fully correct,
already-written `KSC_FOCUS_LEFT`/`RIGHT` branch (`ktb_nav_focus_delta()`, updates the exact
`strip_focus_cell` field this ASCII mirror reads) — it just had no real caller outside `cli_io`.
Three-part fix, all in the same pass:
1. `khtpm_strip_parser.c`'s `dispatch_key_code()` now forwards `KSC_FOCUS_LEFT`/`RIGHT` via
   `send_code()` unconditionally, same safe pattern as the earlier `KSC_HQ_HEADER_BASE`/`KSC_ENTER`
   fixes — zero behavior change for real keyboard use (arrows there never hit this branch at all,
   confirmed above), just gives the manager's own already-correct logic a real caller.
2. `khtpm_strip_keyboard_ascii.c` now relays arrow keys (`ESC [ A/B/C/D`) as `KSC_FOCUS_LEFT`/
   `RIGHT` instead of discarding them — Left/Up both step -1, Right/Down both step +1, matching the
   real GUI's own real collapse (a 1D strip has no vertical axis).
3. `khtpm_strip_render_ascii.c`'s `compose_strip_line()` now reads `strip_focus_cell` from
   `strip_state.txt` and marks the matching cell `[>]` instead of always `[ ]`.
Verified live: relaying `1002` (right) twice moved `strip_focus_cell` from 0→2, and the renderer
correctly drew `[>] 3. [file]`. Added both new binaries to `build_khtpm_strip.sh` so future rebuilds
pick them up automatically.

**Still open**: bottom-tab-bar arrow/digit ACTIVATION (rendering already works — see the nav-claims
note above; this is specifically about driving `ktb_jump_nav()`'s bigger shared numbering system via
the relay, not yet attempted with the same debug-traced rigor the strip fix above got), and `cli_io`
typing support.

**Open architecture question, raised by the user, worth flagging for later**: is
`livedesk_agent_relay.txt` "just a bridge" that should eventually be replaced by the same
`pieces/keyboard/history.txt` convention mutaclysm/TPMOS use? Real answer: not quite an equivalent
swap. Real X11 `KeyPress` events in `khtpm_strip_parser.c` call `dispatch_key_code()` DIRECTLY,
in-process (confirmed, `main()`'s own event loop) — there's no file-based relay in the real GUI input
path at all, unlike mutaclysm's `game_dispatch.c` (a genuine one-shot poller reading a shared history
file). `livedesk_agent_relay.txt` is an intentional, separate side-channel for external/agent input
only. Full TPMOS parity here would mean rewriting the real X11 `KeyPress` handler itself to write to
a shared file instead of dispatching in-process — a bigger, real structural change to the live GUI
input path, not a bridge retirement. Worth doing as its own deliberate phase once the current
incremental work (bottom-tab activation, `cli_io`) is further along, not folded into this pass.

**Full investigation written up**: `taskbar-history-txt-migration-investigation.md` (same `au11-hq/`
dir) — real precedent to reuse (`pieces/keyboard/history.txt`'s `KEY_PRESSED:`/`MOUSE_EVENT:`
format), the real complication mutaclysm doesn't have (three separate X11 windows, not one, each
with its own hit-testing), the local-state risk (`g_nav_focus`, `header_doc`/`bottom_doc`), a
recommended two-phase path (additive mirrored-write first, only then retire direct dispatch), and
three open questions for the user before Phase 1 starts.

Companion to `taskbar-keyboard-relay-and-terminal-render.md` (read that first — has the original
"input+render bundled into one process" architectural finding). This doc is the dedicated
scoping pass the user asked for, keyed specifically to matching TPMOS's real shape: direct quote,
2026-08-18 — "the parallel will be 1.tpmos demo+- dynamic menu system and the tpmos start menu and
infrastructure that already exists, even its orchestrator and what not. the closer we are the shape
of tpmos the better. it will just be a linear list like tpmos projects list (header bar and bottom
bar) they will be visually separated by a ----- line or something since bottom bar has its own
dynamism."

**Process note**: this research was originally handed to a background agent, which was still
mid-run when the session hit its token/usage limit and the agent was terminated before finishing or
writing anything to disk. Everything below was re-verified directly, by reading the real TPMOS
source myself, not recovered from the failed agent run.

TPMOS root: `1.TPMOS_c_+rmmp.0103.0001/` (sibling of `yz.muchiverse/` under
`x0.parent-level-dev-env-04.04/`). All paths below are relative to that root unless stated otherwise.

## 1. TPMOS's real start menu — `pieces/chtpm/layouts/os.chtpm`

This is the literal file, in full:

```
<panel>
    <text label="+===========================================================+" /><br/>
    <text label="|                                                           |" /><br/>
    <text label="|           C H T P M + O S   C L I   v2.0                 |" /><br/>
    <text label="|                                                           |" /><br/>
    <text label="|  CHTPM+OS Main Menu                                      |" /><br/>
    <text label="|                                                           |" /><br/>
    <text label="|  " /><button label="Project Loader" href="pieces/apps/playrm/layouts/loader.chtpm" /><text label="                             |" /><br/>
    <text label="|  " /><button label="App Store (Fondu)" href="pieces/chtpm/layouts/appstore.chtpm" /><text label="                          |" /><br/>
    <text label="|  " /><button label="Status" onClick="LAUNCH:STATUS" /><text label="  " /><button label="GL-OS" onClick="LAUNCH:GL-OS" /><text label="                                          |" /><br/>
    <text label="|  " /><button label="Process Monitor" href="pieces/chtpm/layouts/processes.chtpm" /><text label="                            |" /><br/>
    <text label="|  " /><menu label="Settings">
        <button label="Toggle Frame History (Current: ${display_frame_history})" onClick="LAUNCH:TOGGLE_HISTORY" />
    </menu><text label="                                         |" /><br/>
    <text label="|  " /><button label="Help" onClick="LAUNCH:HELP" /><text label="                                            |" /><br/>
    <text label="|                                                           |" /><br/>
    <text label="+===========================================================+" /><br/>
    <text label="|  Running Applications: None                               |" /><br/>
    <text label="|  System Time: ${clock_time} | Turn: ${clock_turn} | Key: ${last_key} |" /><br/>
    <text label="+===========================================================+" /><br/>
    <text label="|                                                           |" /><br/>
    <text label="|  $ " /><cli_io id="input_text" label="" /><text label="                                                 |" /><br/>
</panel>
```

This is EXACTLY the shape the user described, real and already-shipping:
- **Header bar**: the ASCII box top (title + "CHTPM+OS Main Menu" caption), static text.
- **Linear list**: static buttons (Project Loader, App Store, Status/GL-OS, Process Monitor,
  Settings submenu, Help) — one row each, no scrolling here (TPMOS's menu is short enough it
  doesn't need to scroll; see loader.chtpm below for the scrolling case).
- **Separator**: a literal `+===...=+` row, not a special element type — just another `<text>` line
  with a full-width `=` string. The user's "-----" idea is the same technique, different glyph.
- **Bottom bar with its own dynamism**: `${clock_time}`, `${clock_turn}`, `${last_key}` — template
  vars substituted at render time from state files, NOT hardcoded. "Its own dynamism" (user's phrase)
  is real and literal: these three vars are fed by a **separate, independent process**
  (`clock_daemon`, see §3) that ticks on its own cadence, not tied to the menu's own render pass at
  all — the menu just reads whatever the daemon most recently wrote, same as any other chtpm var
  substitution.

## 2. The scrolling "projects list" — `pieces/apps/playrm/layouts/loader.chtpm`

```
<panel>
    <module>pieces/apps/playrm/loader/plugins/+x/loader_module.+x</module>

    <text label="╔══════════════ P I E C E M A R K   L O A D E R ════════════╗" /><br/>
    <text label="║ LAYOUT ID: playrm/loader.chtpm                            ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  Select a project to begin:                               ║" /><br/>
    <text label="║                                                           ║" /><br/>
    ${project_list}
    <text label="║                                                           ║" /><br/>
    <text label="╠═══════════════════════════════════════════════════════════╣" /><br/>
    <text label="║  " /><button label="Back to Main Menu" href="pieces/chtpm/layouts/os.chtpm" /><text label="                                  ║" /><br/>
    <text label="╚═══════════════════════════════════════════════════════════╝" /><br/>
</panel>
```

`${project_list}` is a whole-block template substitution, not a single scalar — it's replaced with N
generated `<button>` rows, one per real project. The generator is `project_loader.c`
(`pieces/apps/playrm/ops/src/project_loader.c`, 187 lines): scans the real `projects/` directory,
builds a temporary PDL menu block from what it finds, one button per project directory — this is the
literal "linear list like TPMOS's projects list" the user means. It's dynamically generated from disk
state every time the layout loads, not a hand-authored static list like `os.chtpm`'s.

Same shape as `os.chtpm`: header box, dynamic linear list, a `╠═══╣` separator line, then a static
footer row (Back button) below it — the separator here divides the *scrollable/dynamic* list region
from a *static* footer, not from another dynamic region, but it's the same visual technique the user
is asking for.

## 3. The orchestrator — `pieces/chtpm/plugins/orchestrator.c`

This is the real process-lifecycle owner for the entire TPMOS session — 423 lines, and this is the
single most load-bearing precedent for the taskbar refactor. Its real `main()` (line 366) does,
in order:
1. Resets shared state files to clean defaults (`pieces/apps/player_app/manager/state.txt`, clears
   `pieces/os/proc_list.txt`, `pieces/display/layout_changed.txt`).
2. Registers itself (`log_pid`) so it can be found/killed cleanly later.
3. Spawns SEVEN independent threads, each of which `fork()`/`exec()`s (or `_spawnl`s on Windows) one
   real, separate child process and registers its PID:
   - `keyboard_thread_func` — raw input capture (this project's own equivalent of
     `system/keyboard_input`).
   - `joystick_thread_func` — joystick input capture, same shape.
   - `response_thread_func` — (not read in depth this pass; handles some response/relay channel).
   - `chtpm_thread_func` (line ~360) — launches `chtpm_parser` itself, the actual state-machine/
     dispatch engine, as ITS OWN separate process too, not run inline in the orchestrator.
   - `render_thread_func` (line 320) — launches `pieces/display/plugins/+x/renderer.+x` (the
     TERMINAL ASCII renderer — TPMOS's real equivalent of mutaclysm's `system/renderer.c`), then
     itself polls `renderer_pulse.txt`/`current_frame.txt` and prints "--- FRAME UPDATE ---" markers
     when `is_history_on()`.
   - `gl_render_thread_func` (line 342) — launches `gl_renderer.+x` (the GUI window renderer),
     `quiet=true`.
   - `clock_daemon_thread_func` (line 343) — launches `clock_daemon.+x` as a fully independent
     process. **This is the real mechanism behind "bottom bar has its own dynamism"**: the clock/turn
     display isn't refreshed by the menu's own render loop, it's a standalone daemon ticking on its
     own schedule, writing state the menu layout picks up via `${clock_time}`/`${clock_turn}`
     whenever chtpm_parser next composes a frame.
4. Waits on the keyboard thread (POSIX) or sleeps (Windows) until shutdown, then calls
   `handle_sigint()` to cleanly kill every tracked child process.

**Direct, confirmed parallel to mutaclysm's `button.sh`**: `button.sh`'s `run` verb already does a
bash-script version of exactly this — spawn `GL_PID` (mirror window), `RGB_PID` (rgb compositor),
run `keyboard_input` in foreground, trap-kill everything on exit. The one piece it's missing (see
`101.mutaclsym🧟‍♂️️19.00/PITFALLS.txt`) is the terminal-renderer thread — and this orchestrator proves
that omission really is a gap, not a design choice: TPMOS's own real orchestrator ALWAYS launches
`renderer` (terminal) alongside `gl_renderer` (GUI) as siblings, every single session, no
either/or branching. That's strong, direct evidence for the mutaclysm fix, and the reference shape
the taskbar refactor should also converge on: one coordinator, N independent sibling processes, all
driven off shared state files.

## 4. `+-demo` — TPMOS's real dynamic menu system (confirmed, via a completed research pass this session)

Location: `projects/+-demo/` (directory name is literally `+-demo`, not `demo+-` — the user
transposed it verbally, corrected mid-session).

This is a genuine, engine-backed demo of TWO distinct interaction patterns living in the same tree,
directly relevant to how taskbar submenus/right-click menus should be structured:

1. **Foldable directory tree** (`dir1`, `subdir4`, `dir2`, no `onClick`): children nest directly
   inside the parent `<button>`; fold state persists per-node in `manager/gui_state.txt` as flat
   `fold_<id>=open|folded` lines. The fold glyph (`[-]`/`[+]`) is authored INTO the label text
   itself, not computed at render time — e.g. `label="[dir1/][-] Dir1"`.
2. **Activation submenu** (`menu_a`, `nested_menu`, `onClick="ACTIVATE"`): children use
   `onClick="BACK"` (a literal `[Back]` button) and `onClick="KEY:1"`/`"KEY:2"`/`"KEY:3"` — the
   dispatch-string convention already documented in memory ("command must be a dispatch string + a
   matching handler, not raw shell"). Nesting is arbitrary depth (`nested_menu` lives inside
   `menu_a`'s own activated scope, with its own separate `BACK`).

The real engine behind this (`pieces/chtpm/plugins/chtpm_parser.c`, `chtpm_player.c`):
- `KEY:n` gets forwarded verbatim to the current module via `send_to_module("KEY:%d", key)`
  (`chtpm_player.c:295,465`).
- `BACK` (`chtpm_parser.c:1782`) resolves at RUNTIME by walking `elements[p].parent_index` upward
  until it finds the nearest ancestor whose `onClick == "ACTIVATE"` — it does NOT hardcode a fixed
  target. This is what makes arbitrary-depth nesting work with zero per-button wiring.
- Visibility (`chtpm_parser.c:~1892-1896`): an `ACTIVATE` node's children only render when that node
  is the currently active element — walked the same ancestor-chain way. Folding and activation are
  two SEPARATE concealment mechanisms coexisting in one tree, not variants of the same thing.
- `chtpm_parser.c:2318-2329, 2643, 2823-2865`: render-time re-walks the same ACTIVATE-ancestor chain,
  resetting a `scoped_counter` to 0 on entering a freshly-active node — this is what numbers the
  `KEY:1`/`KEY:2`/`KEY:3` children sequentially per-scope.
- `chtpm_parser.c:962-966`: this exact ACTIVATE/BACK/KEY:n shape is ALSO machine-generated elsewhere
  in the engine (auto-emitted `<button onClick="KEY:%d">` per discovered project method) — `+-demo`
  is a minimal hand-authored illustration of a pattern the engine already generates dynamically from
  real introspected data, not a one-off demo-only convention.

## 5. Proposed mapping — taskbar piece ↔ real TPMOS piece

| Taskbar piece today | TPMOS real equivalent | Gap |
|---|---|---|
| `khtpm_strip_parser.c` (does X11 capture + rendering + dispatch trigger, all one process) | `orchestrator.c` spawning `keyboard_thread_func` + `chtpm_thread_func` (parser/dispatch) + `render_thread_func`/`gl_render_thread_func` as SEPARATE sibling processes | Taskbar has no process split at all — one monolith stands in for what TPMOS treats as 3+ independent processes |
| `khtpm_taskbar_manager.c`'s `dispatch_code()` | `chtpm_parser.c`'s own state machine (ACTIVATE/BACK/KEY:n dispatch, focus/active_index) | Conceptually close already — this is the one piece that's roughly the right shape, just embedded in the wrong process |
| No terminal output at all | `render_thread_func` → `renderer.+x` (terminal ASCII, ALWAYS launched alongside the GUI renderer) | Missing entirely — no ASCII taskbar rendering exists in any form today |
| Taskbar tabs/cells (static, header-bar-like) | `os.chtpm`'s static button list | Reasonably close conceptually |
| Whatever refreshes independently in the taskbar today (if anything) | `clock_daemon_thread_func` — a fully separate process ticking on its own cadence | Needs confirming whether the taskbar has ANY equivalent independent-refresh mechanism today, or whether "its own dynamism" is aspirational, not yet built |
| `#.desktop/livedesk_agent_relay.txt` | `pieces/keyboard/history.txt` (mutaclysm/piececraft-xyz's real shared input relay) | Closest existing parallel, but only consumed by the one bundled taskbar process — nothing else can plug into it today the way `renderer.+x` and `gl_renderer.+x` both independently plug into mutaclysm's `current_frame.txt` |
| Right-click / cell-14 submenus (per memory: "cell 14's PDL rows are dead/unused; menu is C-hardcoded") | `+-demo`'s ACTIVATE/BACK/KEY:n pattern, resolved via nearest-ancestor walk | Taskbar's own menu dispatch is hardcoded in C rather than PDL-driven/ancestor-resolved — bringing it to the `+-demo` shape would also retire the "PDL rows are dead" problem already in memory |

## FIRST REAL IMPLEMENTATION PASS (2026-08-18) — MVP: strip + HQ menu, fully interactive

Direct user decision on scope: strip (top header cells) + HQ popup menu, driven by real keyboard
input from the start, not read-only. New files:
- `khtpm_strip_renderer_ascii.c` → `+x/khtpm_strip_renderer_ascii.+x` — the actual terminal
  renderer/input binary.
- `open_cli.sh` — launcher wrapper (see its own real bug/fix below).
- HQ menu gained a "cli" row (`livedesk_taskbar.pdl`, `hq_menu_7_*`) that launches it.

**Real gaps found and fixed in `khtpm_strip_parser.c` (the relay consumer), both confirmed live,
neither guessed:**
1. `dispatch_key_code()`'s own header comment already documented the relay as ENTER/ESCAPE/
   BACKSPACE/printable-only — no code path existed to OPEN a header cell's HQ popup
   (`KSC_HQ_HEADER_BASE+n`) via the relay at all. Fixed by forwarding that code range straight to
   `send_code()`, mirroring exactly what `dispatch_onclick()`'s own ACTIVATE branch already does for
   a real click (no local-doc dependency there either).
2. Deeper, only found via a debug trace (not guessed): even after (1), Enter did nothing. Root
   cause: `dispatch_key_code()`'s ENTER branch resolves entirely through THIS PROCESS's own local
   `header_doc`/`bottom_doc` nav state (`active_index`/`focus_index`) — state a real X11 click
   updates locally via `lay_activate()`, but a relay-injected HQ-open never touches. `header_doc
   ->focus_index` defaults to 0 (the HQ cell button itself) and reads as "navigable" regardless, so
   Enter kept re-clicking the HQ header cell instead of activating the digit-selected row — `hq_open`
   never closed, no command ever ran, no matter what digit was sent first. Fixed with a narrow guard
   (`st->hq_open && header_doc->active_index == -1` — a state combination a real click can never
   produce) that forwards a bare `KSC_ENTER` directly, letting the MANAGER resolve it against its
   own real `hq_focus` instead of the parser's stale local copy. Zero behavior change for real
   keyboard/mouse use (the guard is unreachable there).

**Real gap found in the launch mechanism itself (`gnome-terminal -- <path>`):** `gnome-terminal --
<path>` calls `execve()` on `<path>` DIRECTLY — no shell, no glob expansion — so this project's own
real, literal directory names containing `*` (`*.monads/*.livedesk-taskbar`) were passed through
unexpanded and failed with "Failed to execve: No such file or directory". A SECOND, separate
discovery while chasing this: the manager process's actual `cwd` is `ops/` itself, not `house_root` —
so even a correctly-shell-wrapped glob would have resolved from the wrong directory. Fixed with
`open_cli.sh`, a tiny wrapper (same real shape as `run_khtpm_strip.sh`) that resolves its own
absolute path via `$0` before exec'ing `gnome-terminal` — zero reliance on cwd or glob expansion at
the exec site. The PDL row itself just calls `sh open_cli.sh` (no path prefix needed, since cwd is
already `ops/`).

**Real rendering-format correction (direct user catch, 2026-08-18)**: the first pass rendered HQ rows
as `"  > 1. label"` — invented, not real. Read `chtpm_parser.c`'s actual `render_element()`
(~line 2376-2432) and ported the REAL format exactly: cursor is always a bracketed 3-char prefix
(`"[^]"` active/typing, `"[>]"` focused, `"[ ]"` neither), and the label itself is ALSO bracketed —
full shape `"[cursor] N. [Label]"`, e.g. `[ ] 1. [example1]` / `[>] 2. [example2]`. Fixed via a
shared `print_nav_row()` helper so strip cells and HQ items can't drift apart from each other again.

**Real "staircase" bug + architecture correction (direct user catch, 2026-08-18)** — the deepest
finding of the whole pass. First two attempted fixes (redraw double-fire, poll cadence) did NOT
resolve the actual complaint; the user then supplied a real terminal screenshot showing every
successive line MORE indented than the last, a classic staircase. Root cause, once seen: the
combined `khtpm_strip_renderer_ascii.c` (retired this pass) did BOTH raw-termios keyboard input AND
stdout rendering in ONE process. `enable_raw_mode()` there disabled `OPOST` (copied verbatim from
`keyboard_input.c`'s own real flags) — and termios raw mode is a property of the TTY ITSELF, not the
calling process, so once `OPOST` was cleared, `\n` stopped auto-translating to `\r\n` for EVERY
writer to that terminal, including this same process's own frame output — each bare `\n` moved down
a line without returning to column 0. The user's own question found the real root cause directly:
"it should display from a .txt frame file like tpmos. didn't u see that in tpmos render frame
pipeline?" — confirmed by re-reading `pieces/display/renderer.c` precisely: TPMOS NEVER combines
these two concerns. `renderer.c` never touches termios at all (plain cooked-mode `printf`, reads
`current_frame.txt` from disk); `keyboard_input.c` never prints. Fixed by actually splitting into
two binaries, matching TPMOS exactly rather than approximating it a second time:
- `khtpm_strip_render_ascii.c` — no termios, composes the frame, writes it to a real
  `#.desktop/strip_ascii_current_frame.txt` file (direct user request: "display from a .txt frame
  file like tpmos"), prints it, appends history. Emits explicit `\r\n` throughout as additional
  insurance (cheap, since this process shares the same tty as the keyboard binary below).
- `khtpm_strip_keyboard_ascii.c` — raw termios only, reads keys, writes relay codes. Never prints a
  single byte.
- `open_cli.sh` launches both against one terminal — renderer backgrounded, keyboard input
  foreground — same real shape mutaclysm's own `button.sh` already uses (`./system/renderer &` then
  foreground `./system/keyboard_input`), not a new pattern invented for this project.
Verified live: `cat -A` on both stdout and the new frame file show correct `^M$` (`\r\n`) line
endings throughout; a full relay-driven open→select→activate sequence correctly popped a real
terminal with both processes attached to the same real tty.

**Real audit-trail gap (direct user catch, 2026-08-18)**: "is it writing to frame-history so u can
audit it, like tpmos does?" — it was not. TPMOS's own `pieces/display/renderer.c` appends every
frame it draws, timestamped, to an audit file (confirmed via direct read, ~line 108:
`session_frame_history.txt`); mutaclysm's own `system/renderer.c` already adapted this house-wide
(`frame_history.txt`, single file, simpler than TPMOS's own 3-file ledger). Fixed by adding the same
pattern here — `#.desktop/strip_ascii_frame_history.txt`, opened fresh and appended every frame via a
small `out()` dual-write helper (stdout + history file), matching mutaclysm's per-frame-open shape
(never held open across the process lifetime, so the file is always complete even if killed
mid-session).

**Bottom tab bar, second increment (2026-08-18, same pass)**: `print_tabs()` renders open desks/
sessions (`TAB | pid | nav | entity | path` rows from `strip_state.txt`), same `[cursor] N. [Label]`
shape, focus compared against `tab_focus_idx`. RENDER ONLY, deliberately — tab *activation* goes
through `ktb_jump_nav()`'s shared nav-claims numbering (`khtpm_taskbar_manager.c` ~line 416-487),
which unifies header-cell clicks, tab activation, AND remote entities' own context-menu rows (a
totally different package's `interact_relay.txt`) into ONE global digit-navigable number space read
from `#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt`. This is a meaningfully bigger system
than HQ's self-contained `hq_focus`/`hq_menu[]`, and `dispatch_key_code()`'s local-doc-dependent
Enter handling (the same real gap already found+fixed once for HQ this pass) has NOT yet been
verified safe for this path via the same debug-trace method. Wiring real keyboard activation for
tabs is a named follow-up, not a blind extension of the HQ fix.

**Real "staggered output" fix (direct user report, 2026-08-18)**: read TPMOS's own real
`pieces/display/renderer.c` main() precisely (not guessed) and matched it exactly, two real bugs:
1. TPMOS draws once, THEN seeds its pulse cursor to whatever size the marker file IS at that
   moment — there's no separate "first sight, always redraw" branch in its poll loop, because
   seeding happens strictly after the first draw. This file's own `should_redraw()` had exactly
   that extra branch, which fired a second, redundant draw immediately after `main()`'s own
   separate initial `draw_frame()` call — a real, confirmed double-draw at startup (this is what
   "staggered" was describing). Fixed by moving cursor-seeding to a `seed_pulse_cursor()` call
   placed AFTER the initial draw in `main()`, and removing the redundant branch from
   `should_redraw()` entirely — verified live, exactly one draw at startup now.
2. Poll rate was `100000us` (10fps, self-invented, "close enough" to `x11_mirror.c`'s own cadence)
   — TPMOS's real renderer.c polls at `16667us` (~60Hz), matching its own render loop exactly.
   Matched exactly, not approximated.
3. Also ported TPMOS's own session-history-clear convention (`main()`, ~line 133-142): the frame
   history file is now truncated at startup with a timestamped `=== NEW SESSION ===` marker, so
   re-launching doesn't pile old sessions' frames onto the same ever-growing file.

**Still MVP-scoped, explicitly not yet done**: tab activation (see above), cli-io typing, arrow-key
nav (the relay still has no arrow support — digits are the only way to move focus, matching the
pre-existing, unmodified real convention), deeper HQ submenus (desks/pals/settings detail screens —
these likely already work mechanically since they reuse the exact same `ktb_hq_open()`/
`ktb_hq_digit()`/`ktb_hq_activate()` functions with a different `which`, but not yet live-verified),
and local strip-cell focus tracking (strip cells currently always render `[ ]`, never `[>]`, since
this process doesn't yet track that state independently).

## Shared convention: "pop open as a new terminal tab, only if not already in one"

Direct user instruction, 2026-08-18: "i think it should pop open as a new tab in terminal, only if
not launched from terminal. this is the same behavior we want for tb refactor."

**Why this came up**: the taskbar's own real launch command for toy projects
(`khtpm_taskbar_manager.c`, `"livedesk:open-toy:"` handler, ~line 2792) is
`setsid nohup sh -c 'sh "<button.sh>" run' >/dev/null 2>&1 &` — fully detached, no controlling
terminal, stdout/stderr binned. That means the mutaclysm terminal-renderer fix (§0 above / this
project's own `PITFALLS.txt`) only ever shows anything if the user manually opens a terminal and
runs `button.sh run` themselves — launching from the taskbar gives no tty at all for
`keyboard_input`/`renderer` to attach to.

**Fix landed in mutaclysm's `button.sh`** (2026-08-18, real, live-verified detection logic — not
just written and assumed correct): at the top of the `run` verb,
```bash
if [ ! -t 0 ] || [ ! -t 1 ]; then
    if command -v gnome-terminal >/dev/null 2>&1; then
        gnome-terminal --tab -- bash -c "\"$SCRIPT_DIR/button.sh\" run; exec bash"
        exit 0
    fi
fi
```
`[ ! -t 0 ] || [ ! -t 1 ]` is standard POSIX tty-detection (true when stdin/stdout aren't a real
terminal — exactly the taskbar's `setsid`-detached case, confirmed via a live `setsid`-wrapped
simulation this session). When a user runs `button.sh run` by hand inside an already-open terminal,
both are real ttys, the whole block is skipped, and it runs in place with zero extra tabs. When
launched from the taskbar (or anywhere else with no controlling terminal), it re-execs itself inside
a fresh `gnome-terminal --tab`, giving `keyboard_input`/`renderer` a real tty to work with.
`gnome-terminal` is the real, confirmed-installed terminal on this machine (`x-terminal-emulator`'s
own alternative already points at it) — falls through to the old headless behavior if it's ever
missing, rather than failing the launch.

**Apply the same convention to the taskbar refactor itself**: whatever process ends up owning the
taskbar's own terminal-ASCII rendering (§5's mapping table, the not-yet-built piece) should use this
exact same tty-detection gate before popping a terminal tab — same rationale, same shape, no need to
reinvent it per-project. Worth factoring this tty-detection + re-exec block into a small SHARED
helper (`_shared-lib/` is the natural home, matching `x11_mirror.+x`/`chtpm_rgb_render.c`'s own
already-shared precedent) once it's proven out in mutaclysm, rather than copy-pasting it into every
project's own `button.sh` by hand.

## OPEN QUESTIONS (need your answers before this can be scoped into real work)

1. **Scope**: should the taskbar refactor literally REUSE TPMOS's real binaries/files (e.g. actually
   run a `renderer.+x` against a taskbar-owned `current_frame.txt`), or just MIMIC the pattern with
   taskbar-specific code (own terminal renderer, own orchestrator-equivalent, same shape but not
   shared files/binaries)? This changes the effort enormously — literal reuse is far less new code
   but couples the taskbar to the chtpm/PAL-VM stack directly, which it currently is NOT part of at
   all (it's raw Xlib).
   
   
   if u could reuse code that would be great but obviously it needs 2 stay xlib for desktop rendering so w/e... 
   
   ----------------------------------------------------------
2. **Separator glyph**: `-----` (as you wrote it) vs. TPMOS's own `+===...=+` / `╠═══╣` style — do
   you want the exact TPMOS box-drawing look, or just "the same idea, taskbar's own visual language"?
   
   
   yes same idea w/e 
   ------------------------------------------------------------------------------------------
3. **The "own dynamism" bottom bar** — what should live there for the taskbar specifically? TPMOS's
   is clock/turn/last-key. Does the taskbar have an obvious equivalent (active window count? system
   clock? something from `strip_var_tabs.txt`/`strip_state.txt`), or is this still undefined?
   
   
   i dunno we will just try different stuff . 
   
4. **Does the taskbar need a scrolling dynamic list at all** (like `loader.chtpm`'s `${project_list}`,
   generated fresh from a directory scan each load), or is a short static list (like `os.chtpm`'s)
   closer to what you actually want for the taskbar's main body?
   
   i think the bottom bar is scrolling, right? adds w/e entities (in the future we will add running programs)
i would say this is kind of inspired by what wraith-alpha (tpmos program and first succesful foray into tpmos gl compat...ref it)
5. **Priority vs. the mutaclysm one-line fix**: since TPMOS's own orchestrator proves "terminal +
   GUI, always both" is the real standard, do you want the mutaclysm `system/renderer` fix done
   first (small, already scoped, gives a live reference) before starting to scope the taskbar
   refactor for real, or should these proceed independently?
   
   sure if u could do it quickly. 
6. **Right-click/submenu dispatch**: given `+-demo`'s ACTIVATE/BACK/KEY:n pattern directly addresses
   the existing memory note about taskbar cell-14's dead PDL rows and hardcoded C menu — do you want
   that specific piece (submenu dispatch becoming PDL-driven, ancestor-resolved BACK) folded into
   THIS refactor's scope, or handled as its own separate, later task?
   
   
   <i mean we should probably allow for it with codeshape for sure, otherwise its ur call> 
   
   
