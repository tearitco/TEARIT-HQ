# TPMOS Runtime Trace Diagram
**Date:** 2026-07-24
**Status:** AUTHORITATIVE REFERENCE for this session
**Purpose:** Document exact macro/micro behavior tree of the TPMOS render pipeline so agents understand what must work and what breaks it.

---

## 1. MACRO PIPELINE (x0.pet — Working Reference)

```
┌─────────────┐    ┌──────────────────┐    ┌─────────────────┐    ┌──────────────┐
│  Physical    │    │  keyboard_input   │    │  chtpm_parser   │    │   renderer   │
│  Keyboard    │───>│  (separate proc)  │───>│  (separate proc) │───>│ (separate    │
│              │    │                   │    │                  │    │  proc)       │
└─────────────┘    └──────────────────┘    └─────────────────┘    └──────────────┘
     /dev/tty          pieces/               pieces/               pieces/display/
     read()            keyboard/             display/              renderer_pulse.txt
                       history.txt           frame_changed.txt     stat() poll
                                             current_frame.txt     read current_frame.txt
                                             renderer_pulse.txt    stdout
```

**Three separate OS processes, connected only by files. Zero shared memory. Zero pipes.**

---

## 2. MICRO PIPELINE — Exact File Chain Per Keystroke

### Step 1: keyboard_input captures key
```
Source:  pieces/keyboard/plugins/keyboard_input.c (x0.pet)
         system/keyboard_input.c (0.ledger)

Input:   /dev/tty (x0.pet) or STDIN_FILENO (0.ledger)
Format:  read() → single byte → arrow detection → integer keycode

Write:   pieces/keyboard/history.txt
Format:  [YYYY-MM-DD HH:MM:SS] KEY_PRESSED: <code>\n
         (x0.pet has timestamp; 0.ledger omits timestamp)
Mode:    fopen("a") + fprintf + fclose  (per-keystroke open/write/close)
```

**x0.pet also writes to:**
- `pieces/keyboard/ledger.txt` (audit)
- `pieces/master_ledger/master_ledger.txt` (audit)

**0.ledger does NOT write to ledger files.**

### Step 2: chtpm_parser reads keyboard history
```
Source:  chtpm_parser.c main() while(1) loop

Mechanism:
  1. fopen("pieces/keyboard/history.txt", "r")
  2. fseek(history, last_history_position, SEEK_SET)  ← seek-based tracking
  3. while(fgets(line)) { strstr(line, "KEY_PRESSED: ") → process_key(atoi(...)) }
  4. last_history_position = ftell(history)
  5. fclose(history)

last_history_position: global long, starts at 0
File is append-only: writer always appends, reader only reads forward
CRLF debounce: skip \n after \r (Windows compat)
```

### Step 3: process_key() handles the key
```
Source:  chtpm_parser.c process_key(int key)

NAV MODE (active_index == -1):
  ARROW_UP / 'w' / 'W' / JOY_UP    → focus_index-- (cycle)
  ARROW_DOWN / 's' / 'S' / JOY_DOWN → focus_index++ (cycle)
  Digits 0-9                         → digit_accum / do_jump()
  Enter / JOY_BUTTON_0               → activate focused element
    - cli_io element → active_index = focus_index (enter text mode)
    - ACTIVATE button → open submenu
    - href button → switch layout
    - onClick button → send_command()
  'q' / 'Q' → exit(0)

ACTIVE MODE (active_index != -1):
  cli_io active:
    Enter → save_cli_io_gui_state() + el->input_buffer[0]='\0' + inject_raw_key(13)
    Backspace → delete last char
    Printable ASCII → append to input_buffer + save_cli_io_gui_state()
  INTERACT active:
    Forward key via inject_raw_key()
  ESC → deactivate (active_index = parent or -1)

AT THE END of process_key() — THE CRITICAL LINE:
  FILE *mf = fopen("pieces/display/frame_changed.txt", "a");
  fprintf(mf, "K\n");
  fclose(mf);
```

**This is the single unified render trigger. process_key() NEVER sets dirty=1 directly.**

### Step 4: Parser main loop detects marker growth
```
Source:  chtpm_parser.c main() while(1)

Each iteration:
  1. Check waitpid(current_module_pid) → if exited, dirty=1
  2. Read keyboard history (seek-based)
  3. stat(frame_changed.txt) → if size > last → dirty=1
  4. stat(state_changed.txt) → if size > last → reload vars + re-parse + dirty=1
  5. stat(layout_changed.txt) → if size > last → switch layout + dirty=1
  6. if (dirty || clear_nav_on_next) → compose_frame()
  7. usleep(16667)  ← ~60fps

CRITICAL RULE: compose_frame() ONLY fires when frame_changed.txt grows.
DO NOT add dirty=1 from keyboard directly. The marker IS the throttle.
```

### Step 5: compose_frame() writes the frame
```
Source:  chtpm_parser.c compose_frame()

Write destinations:
  1. last_rendered_frame[] (in-memory) — for mouse hit-testing
  2. pieces/display/current_frame.txt — the actual frame (write mode "w")
  3. pieces/display/renderer_pulse.txt — appends "P\n" (append mode "a")

compose_frame() does NOT write to frame_changed.txt.
frame_changed.txt is written by: process_key(), render_map(), clock_daemon.
compose_frame() is the CONSUMER of frame_changed.txt, not the producer.
```

### Step 6: renderer picks up the frame
```
Source:  pieces/display/renderer.c (x0.pet) / system/renderer.c (0.ledger)

x0.pet renderer (CORRECT):
  Polls ONLY: pieces/display/renderer_pulse.txt
  Detects: stat() size change
  Action:  read current_frame.txt → stdout + session_frame_history.txt
  Sleep:   usleep(16667) per iteration

0.ledger renderer (BROKEN — polls TWO files):
  Polls: frame_changed.txt AND renderer_pulse.txt
  Detects: stat() size change
  Action:  if frame_changed.txt changed → render IMMEDIATELY (no wait)
           else if renderer_pulse.txt changed → wait for file to stabilize → render
  Sleep:   usleep(16667) per iteration
```

---

## 3. THE CRITICAL CHAIN — Visual

```
keystroke
  │
  ▼
keyboard_input: append "KEY_PRESSED: 1002" to history.txt
  │  (fopen "a", fprintf, fclose — one atomic write per key)
  │
  ▼
parser main loop: fopen history.txt, fseek to last_position, fgets new lines
  │  strstr("KEY_PRESSED: ") → atoi → process_key(1002)
  │
  ▼
process_key(1002):
  │  active_index == -1 → nav mode → ARROW_UP → focus_index--
  │  THEN at end of function:
  │  fopen("frame_changed.txt", "a") → fprintf("K\n") → fclose
  │
  ▼
parser main loop (same iteration, after keyboard block):
  │  stat(frame_changed.txt) → size grew by 2 → dirty = 1
  │
  ▼
compose_frame():
  │  load_vars() → parse_chtm() → render elements → build frame string
  │  fopen("current_frame.txt", "w") → fprintf(frame) → fclose
  │  fopen("renderer_pulse.txt", "a") → fprintf("P\n") → fclose
  │
  ▼
renderer main loop (next iteration, ~16ms later):
  │  stat(renderer_pulse.txt) → size grew → render_display()
  │  fopen("current_frame.txt", "r") → fread → printf to stdout
  │
  ▼
Terminal displays updated frame
```

**Total latency: ~32ms (2 loop iterations × 16ms) from keystroke to screen update.**

---

## 4. FILE INVENTORY — Who Writes What

| File | Writer | Trigger | Reader |
|------|--------|---------|--------|
| `pieces/keyboard/history.txt` | keyboard_input | Every keypress | chtpm_parser |
| `pieces/display/frame_changed.txt` | process_key(), render_map(), clock_daemon | Every nav/render tick | chtpm_parser (stat check) |
| `pieces/display/current_frame.txt` | compose_frame() | When dirty=1 | renderer |
| `pieces/display/renderer_pulse.txt` | compose_frame() | When dirty=1 | renderer |
| `pieces/apps/player_app/state_changed.txt` | game_manager / ops | After state mutation | chtpm_parser |
| `pieces/apps/player_app/manager/gui_state.txt` | parser (save_cli_io_gui_state) + game_manager | Every keystroke / after submit | parser (sync_cli_input) |
| `pieces/display/layout_changed.txt` | External (href buttons) | Layout switch | chtpm_parser |
| `pieces/apps/player_app/history.txt` | inject_raw_key() | cli_io Enter submit | game_manager |
| `pieces/system/quit_flag.txt` | keyboard_input (on exit) | Ctrl+C / exit | renderer, orchestrator |

---

## 5. x0.pet vs 0.ledger — DIFFERENCES

### 5a. Orchestrator Architecture

| Aspect | x0.pet | 0.ledger |
|--------|--------|----------|
| Process model | pthreads + fork/exec per service | Sequential fork/exec |
| Services launched | keyboard, joystick, response_handler, parser, renderer, gl_renderer, clock_daemon (7) | renderer, parser, keyboard_input (3) |
| keyboard_input launched? | Yes (pthread) | Yes (fork) |
| game_manager launched? | N/A (no game_manager in x0.pet) | **NO** — compiled but never launched |
| Compilation | Pre-compiled binaries in +x/ | Compiles from source on every run |
| Shutdown trigger | keyboard_input exits → orchestrator joins thread → kills all | Ctrl+C → SIGTERM → SIGKILL |
| State reset | Clears proc_list.txt, writes state.txt defaults | Clears 8 state files, runs word_compose_frame |

### 5b. keyboard_input

| Aspect | x0.pet | 0.ledger |
|--------|--------|----------|
| Input source | `/dev/tty` (direct terminal) | `STDIN_FILENO` (fd 0, inherited) |
| Write format | `[timestamp] KEY_PRESSED: %d\n` | `KEY_PRESSED: %d\n` (no timestamp) |
| Audit writes | history.txt + ledger.txt + master_ledger.txt | history.txt only |
| Raw mode | `enableRawMode()` via termios on /dev/tty | `enableRawMode()` via termios on STDIN_FILENO |
| Quit flag | Writes `1` to quit_flag.txt on exit | Writes `1` to quit_flag.txt on exit |

### 5c. Renderer

| Aspect | x0.pet | 0.ledger |
|--------|--------|----------|
| Polls | `renderer_pulse.txt` ONLY | `frame_changed.txt` AND `renderer_pulse.txt` |
| Render trigger | Size change on renderer_pulse.txt | Size change on either file |
| Race condition | None — renderer_pulse.txt is written AFTER compose_frame() | YES — frame_changed.txt is written BEFORE compose_frame() |
| Stabilization loop | None (not needed) | 20 × 2ms wait when renderer_pulse.txt changes |
| Session history | Writes to `session_frame_history.txt` | Writes to `frame_history.txt` |
| Clear on start | Overwrites `session_frame_history.txt` | Overwrites `frame_history.txt` |

### 5d. Parser

| Aspect | x0.pet | 0.ledger |
|--------|--------|----------|
| Source file | `pieces/chtpm/plugins/chtpm_parser.c` | `system/chtpm_parser.c` |
| Differences | Reference | DEBUG printf removed + [Map Loading...] guard |
| process_key() | Identical | Identical |
| compose_frame() | Identical | Identical |
| sync_cli_input() | Identical | Identical |

### 5e. game_manager

| Aspect | x0.pet | 0.ledger |
|--------|--------|----------|
| Exists? | No (not part of x0.pet architecture) | Yes (custom for this project) |
| Launched by orchestrator? | N/A | **NO** |
| Clears gui_state.txt? | N/A | Yes (clear_input_text()) |
| Uses fork/exec? | N/A | Yes (run_op()) |

---

## 6. KNOWN BUGS in 0.ledger

### BUG 1: Renderer polls frame_changed.txt (RACE CONDITION)
**Impact:** Renderer can render BEFORE compose_frame() writes the new current_frame.txt
**Evidence:** User sees stale frame, then correct frame on next iteration
**Fix:** Match x0.pet — renderer should ONLY poll renderer_pulse.txt
**File:** system/renderer.c lines 92-94

### BUG 2: game_manager not launched by orchestrator
**Impact:** Words submitted via cli_io have no backend processing
**Evidence:** Recent Words shows old data from previous sessions
**Fix:** Add game_manager to orchestrator's launch sequence
**File:** system/orchestrator.c line 116 (add launch after keyboard_input)

### BUG 3: keyboard_input reads STDIN_FILENO not /dev/tty
**Impact:** May not capture keys if stdin is not a terminal
**Evidence:** User reports arrow keys don't work in live session
**Fix:** Open /dev/tty directly like x0.pet does
**File:** system/keyboard_input.c lines 61-66

### BUG 4: keyboard_input has usleep(10000) in main loop (x0.pet has none)
**Impact:** 10ms delay per keypress, unnecessary
**Fix:** Remove usleep from main loop (x0.pet has no sleep in keyboard capture loop)
**Note:** Our keyboard_input.c does NOT have this sleep (verified), but x0.pet's does.

---

## 7. DEBUGGING CHECKLIST

When keys don't process live:

1. **Is keyboard_input running?** `ps aux | grep keyboard_input`
2. **Is keyboard_input writing to history.txt?** `tail -f pieces/keyboard/history.txt` while pressing keys
3. **Is parser reading history.txt?** Check parser PID is alive: `ps aux | grep chtpm_parser`
4. **Is process_key() writing to frame_changed.txt?** `ls -la pieces/display/frame_changed.txt` — size should grow on each keypress
5. **Is compose_frame() running?** Check `pieces/display/renderer_pulse.txt` — 'P' lines should grow
6. **Is renderer picking up changes?** `ls -la pieces/display/renderer_pulse.txt` — check if renderer is alive
7. **Is the frame being written?** `cat pieces/display/current_frame.txt` — should show latest frame

**Quick smoke test (headless):**
```sh
echo "KEY_PRESSED: 1002" >> pieces/keyboard/history.txt
sleep 1
cat pieces/display/current_frame.txt
# Frame should show focus moved up
```

---

*"The marker is the clock. The file is the state. If it's not in a file, it's a lie."*
