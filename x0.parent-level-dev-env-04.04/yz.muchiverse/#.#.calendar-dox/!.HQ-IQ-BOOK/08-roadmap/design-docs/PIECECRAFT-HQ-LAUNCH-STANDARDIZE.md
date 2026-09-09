# piececraft-hq: standardize the launch pipeline

**Status: DESIGN (2026-09-09).** Direct report: clicking "Piececraft-HQ"
from the taskbar is flaky (needs 2–3 clicks; sometimes the relay code
fires and nothing opens), and clicking it again while it's open
**refreshes the board window blank**. User wants the launch cleaned up
to match the other x11-hq windows.

---

## 0. What launches today (researched)

### Two menu entries, two dispatch paths, one 500-line script

| Entry | Path | Manager case |
|---|---|---|
| HQ menu `hq_menu_12` "piececraft-hq" | `livedesk:open-piececraft-hq` | `khtpm_taskbar_manager.c` 4502 |
| Toys menu "Piececraft-HQ" (auto-discovered from `@.apps/piececraft-hq/toy.pdl`) | `livedesk:open-toy:<house>/@.apps/piececraft-hq/button.sh` | `khtpm_taskbar_manager.c` 4334 |

Both end up running `@.apps/piececraft-hq/button.sh run`.

### `button.sh run` (≈515 lines) does, in order

1. maybe `bash scripts/build.sh` (compile).
2. **`RECENT_SESSION<5s` heuristic** — if a session dir was created in the
   last 5 s, skip the self-kill; otherwise `kill_own_board_widget` +
   `kill_own_clock_daemon` + `kill_own_hq_status_manager` +
   `kill_own_orchestrator` (a broad `pkill -f "$SCRIPT_DIR/system/orchestrator"`).
3. `mkdir` + **`cp -r` a whole session tree** (`system/ ops/ pal/
   pieces/chtpm/ projects/…/pieces data/`) into
   `pieces/sessions/<epoch>-<pid>/`.
4. seed ~15 files; maybe run `pc_generate_chunk`.
5. launch `$SCRIPT_DIR/system/orchestrator &` → `ORCH_PID`.
6. if `game_state=playing` & `DISPLAY`: launch the legacy
   `board-viewer/button.sh run-widget` **and**, after `sleep 1.5`, launch
   `khtpm_core_render "$HOUSE" pchq-board.xhtpm piececraft-hq` — **the
   actual board window** — inside a nested `( sleep 1.5; setsid … & ) &`.
7. launch `pc_hq_status_manager`.
8. `trap '… kill …; kill_own_*; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM`.
9. **foreground `./system/keyboard_input`** (blocks button.sh for the
   whole session; it reads `pieces/keyboard/history.txt`, a file relay —
   fine detached).

One launch = **8 processes**: `sh -c` wrapper → `button.sh` →
`orchestrator` → `board-viewer` widget → `pc_hq_status_manager` →
`prisc+x` VM → `khtpm_core_render` (board window) → `pchq_board_projector`.

### The board window itself is already standard-compliant

`khtpm_core_render <house> pchq-board.xhtpm piececraft-hq` +
`<module> pchq_board_projector.+x` — same shape as db-hq / stats-hq /
sql-hq. The projector calls `ledger_peers.+x widget`, finds the live
`board-viewer:piececraft-hq` session, and publishes `bv_session=` /
`canvas_raw=<bv_session>/pieces/display/rgb_frame_3d_overlay.raw`. The
`<canvas id="view">` blits that `.raw`. **The board window is a thin
viewer of a board-viewer session — it does not itself need the game
engine, only a live board-viewer session to point at.**

---

## 1. Root causes

### 1a. Blank-on-reclick (the headline bug)

2nd click, >5 s after the 1st → `button.sh run`'s
`kill_own_orchestrator` does `pkill -f "$SCRIPT_DIR/system/orchestrator"`
→ **kills instance #1's orchestrator**. Nothing kills instance #1's
`khtpm_core_render` board window or `pchq_board_projector` (neither is
matched by any `kill_own_*` pattern — the window is the *shared*
`*.livedesk-taskbar/ops/+x/khtpm_core_render.+x`). The window survives,
now data-starved. Instance #1's `keyboard_input` then exits → its
`trap … rm -rf "$SESSION_DIR"` deletes the session out from under the
still-mapped window → `canvas_raw` file gone → **blank canvas**.
Instance #2 opens a *second* board window 1.5 s later.

### 1b. "3 fast clicks did nothing"

All within 5 s → `RECENT_SESSION=1` each time → self-kills skipped → 3
concurrent `button.sh run`, each `cp -r`-ing a session tree, racing on
the shared board window + `config.txt`/`board.txt` copy-in/out + each
other's `rm -rf` EXIT traps → mutual destruction, nothing stays mapped.
(Confirmed from `#.desktop/strip_history.txt`: `4011 5014` ×3 — the
relay dispatched correctly all three times; the failure was 100 % in
`button.sh`.)

### 1c. Structural

- **No real single-instance guard.** `RECENT_SESSION<5s` is a
  double-click band-aid, not a lock. db-hq/stats-hq/sql-hq each
  `pgrep -f "khtpm_core_render\.\+x .*<their chtpm>"` → TERM→KILL any
  existing → relaunch. pc-hq has nothing.
- **`kill_own_orchestrator` on the `run` start path** is self-sabotage —
  it matches *any* orchestrator under `$SCRIPT_DIR`, including one a
  concurrent/previous launch still wants.
- **Session lifecycle is tied to one `button.sh` process.** The board
  window (which the taskbar user actually cares about) can outlive the
  `button.sh` that `rm -rf`s its session.
- **Two menu entries.** The toys-menu one is the leftover: `e5ade388`
  added the HQ-menu pin `hq_menu_12` as a workaround for the (now-fixed)
  toys-dropdown click bug, but never removed the `toy.pdl` entry.
- **The board window is registered nowhere** — a taskbar quit
  (`ktb_reap_launched` / `livedesk_proc_list.txt`) doesn't reap it.

---

## 2. Target — match the x11-hq standard

### 2a. One dispatch path

- **Keep** `livedesk:open-piececraft-hq` (HQ-menu pin `hq_menu_12`) — the
  house's standard "open an hq window" verb.
- **Delete** `@.apps/piececraft-hq/toy.pdl` → piececraft-hq stops
  appearing in the toys dropdown. (`@.apps/piececraft-xyz` keeps its own
  `toy.pdl`; only the `-hq` duplicate goes.)
- Manager case 4502 → `exec`/`ktb_system_recorded` the new
  `open_pchq_board.sh` instead of `button.sh run`.

### 2b. `open_pchq_board.sh <house_root>` — the standard launcher

New file `@.apps/piececraft-hq/open_pchq_board.sh`, modelled line-for-line
on `&.hq-apps/stats-hq/open_stats_hq.sh`:

```
resolve HOUSE_ROOT, PKG=@.apps/piececraft-hq, BIN=<shared khtpm_core_render>
ensure BIN + pchq_board_projector.+x built (build-on-demand, like stats-hq)

# 1. single-instance guard — raise, don't relaunch
board_pids() { pgrep -f "khtpm_core_render\.\+x .*pchq-board\.xhtpm" ; }
if board_pids alive:
    # raise the existing window instead of opening a second one
    write "RAISE" to <PKG>/state/raise.txt   (projector picks it up ->
        khtpm_core_render's generic raise; OR wmctrl -a "piececraft-hq board")
    exit 0

# 2. ensure a board-viewer session exists for the canvas to show
if ledger_peers has no "board-viewer:piececraft-hq":
    setsid nohup bash "$PKG/button.sh" engine >/dev/null 2>&1 &   # see 2c
    wait (poll ledger_peers, ~5s) for the session to register

# 3. launch the board window — standard shape
setsid nohup "$BIN" "$HOUSE_ROOT" "$PKG/pchq-board.xhtpm" piececraft-hq \
    >/tmp/pchq-board.log 2>&1 < /dev/null &
printf '%s %s 0 0 pchq-board\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt"
disown
```

### 2c. `button.sh engine` — new engine-only action

Everything `run` does through step 7 (session tree, orchestrator,
board-viewer widget, status manager) **minus**:
- the foreground `keyboard_input` (return after spawning),
- the destructive `rm -rf "$SESSION_DIR"` EXIT trap — instead write a
  `pieces/system/engine.pid` + `pieces/system/session_dir.txt` so
  `button.sh kill` / a taskbar quit can find and tear it down cleanly,
- the `kill_own_orchestrator` self-kill on the start path (keep it only
  in `kill`).
- Register `orchestrator`, `prisc+x`, the board-viewer widget and
  `pc_hq_status_manager` PIDs into `livedesk_proc_list.txt` (5-field
  `pid pgid 0 0 pchq-engine`) so the taskbar quit reaps the whole stack.

`button.sh run` = `button.sh engine` **+** foreground `keyboard_input`
**+** the persist-and-clean EXIT trap — i.e. the terminal/dev experience,
unchanged for someone running it in a real terminal. The taskbar never
calls `run` again.

### 2d. `button.sh` no longer launches the board window

Delete the inline `khtpm_core_render … pchq-board.xhtpm …` block (≈ lines
415-422) and the auto board-viewer-widget launch's coupling to it — the
board window is `open_pchq_board.sh`'s job now, single-instanced. `run`
(terminal) can still open one by calling `open_pchq_board.sh` at the end
if `$DISPLAY` is set.

### 2e. Teardown

- The board window PID is in `livedesk_proc_list.txt` → a taskbar quit
  (`ktb_reap_launched`) already reaps it (PROC-LIFECYCLE work).
- `button.sh engine`'s stack PIDs likewise → reaped on taskbar quit.
- `button.sh kill` stays the manual "kill everything for this project"
  and now also removes `engine.pid`/`session_dir.txt` and the ledger
  rows.

---

## 3. Migration steps (each its own commit + verification)

1. **`open_pchq_board.sh`** (new) with the guard + board-window launch +
   ledger record. Point manager case 4502 at it. **Do NOT yet touch
   `button.sh`** — `open_pchq_board.sh` step 2 calls `button.sh run`
   detached for now (still works, just no longer foreground-blocking the
   taskbar). Delete `toy.pdl`.
   *Verify*: click piececraft-hq → board window opens once; click again
   → same window raised, **no blank, no 2nd window**; `ps` shows one
   `khtpm_core_render …pchq-board`; a taskbar quit leaves zero pc-hq
   procs.
2. **`button.sh engine`** action + `engine.pid`/`session_dir.txt`; drop
   `kill_own_orchestrator` from the `run`/`engine` start path; register
   stack PIDs in the ledger. `open_pchq_board.sh` step 2 → `button.sh
   engine`.
   *Verify*: engine starts once, survives the launching shell exiting;
   `button.sh kill` tears it down; taskbar quit reaps the stack.
3. **`button.sh run`** = `engine` + foreground `keyboard_input` +
   persist/clean trap; remove the inline board-window launch (2d).
   *Verify*: `bash button.sh run` in a real terminal still gives the
   full interactive session; Ctrl-C cleans up; the board window (if
   opened) is left to `open_pchq_board.sh`'s lifecycle, not `rm -rf`'d.
4. Drop the `RECENT_SESSION<5s` heuristic (superseded by the guard).
5. Docs: OPERATIONAL-LANDMINES / OPEN-ITEMS; note the board window is now
   a standard registered x11-hq window.

---

## 4. Tests / KPIs

- **Reclick**: board window open → dispatch `livedesk:open-piececraft-hq`
  again → exactly one `khtpm_core_render …pchq-board` process, window
  raised, canvas still live (not blank). *(the headline bug)*
- **Rapid triple-click**: 3 dispatches within 2 s → one window, one
  engine session, no orphan `orchestrator`/`prisc+x`/session dirs.
- **Cold open**: no engine running → one click → engine session +
  board window come up; `canvas_raw` non-empty within ~5 s.
- **Taskbar quit** (`ktb_reap_launched`): `ps aux | grep -E
  'piececraft|pchq|orchestrator.*piececraft|prisc\+x.*piececraft'` (this
  house) → **zero**, no manual step.
- **Terminal `button.sh run`**: unchanged interactive experience;
  Ctrl-C leaves no orphans and no stray session dirs.
- `git grep -l 'livedesk:open-toy.*piececraft-hq'` and the toys menu →
  no `Piececraft-HQ` row (only `Piececraft` = xyz).
- One launcher script, ≤120 lines, in the stats-hq shape.

## 5. Related
- `&.hq-apps/stats-hq/open_stats_hq.sh`, `&.hq-apps/db-hq-pal/button.sh`
  (the standard shape).
- `PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md` / `livedesk_proc_list.txt`
  (teardown registration).
- `PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md` (the board window's Interact
  behaviour — unaffected by this).
- `khtpm_taskbar_manager.c` 4334 (toys dispatch), 4502
  (`livedesk:open-piececraft-hq`).
- `@.apps/piececraft-hq/button.sh`, `ops/pchq_board_projector.c`,
  `pchq-board.xhtpm`, `toy.pdl`.
