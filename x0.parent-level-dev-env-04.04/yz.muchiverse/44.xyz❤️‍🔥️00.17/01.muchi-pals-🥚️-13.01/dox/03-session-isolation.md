# Session isolation for `button.sh run` — apply this while refactoring muchi-pals

Written for whichever agent is currently working on this project, by a
different agent working on `pal-chain`/`pal-forum` in a sibling
directory (`yz.muchiverse/2.muchi-verse/`) the same day. This is a
**real, live-caught, live-fixed** bug, not a theoretical concern — see
`yz.muchiverse/2.muchi-verse/!.xyzos-standards.txt` sec. 23 for the full
incident writeup (a user's own live typing got corrupted by an agent's
own leftover test process, more than once, before this fix landed).
Read that section for the complete story; this doc is the *applied*
version, written directly against muchi-pals' own current
`button.sh`.

## The bug, in one sentence

Every `button.sh run` invocation of this project currently writes its
entire ephemeral UI/input state — `pieces/keyboard/history.txt`,
`pieces/apps/player_app/history.txt`/`interact_relay.txt`/`state.txt`,
`pieces/system/pets_screen_state.txt`/`last_message.txt`/
`active_target.txt`/`quit_flag.txt`, `pieces/display/
pending_command.txt`/`muchi_screen_changed.txt`,
`projects/muchi-pals/manager/gui_state.txt` — to ONE fixed set of
paths under the project root. Two concurrent invocations (a real user
+ an agent testing, or two of either) read and write the SAME files.
Confirmed live, in the sibling `pal-forum` project: one session's own
old keystrokes replayed into another session's live typing, and one
session's own login state got silently cleared by a different
session's own navigation. **Do not "fix" this by killing whatever's
already running** — that was tried, and was directly rejected by the
user: "we dont want to kill processes just because they are running.
we only want to kill them if the user is done running them." The real
fix is per-session file isolation, described below.

## The fix, already built and live-verified in `pal-forum/button.sh`

Every `run` generates a session id and a private, throwaway directory
holding ONLY that invocation's own ephemeral state, with symlinks back
to the real project root for everything static/shared, and for the
real persistent game data. `PRISC_PROJECT_ROOT` points at that private
directory for every process the invocation launches. On exit (normal
quit, Ctrl+C, or any signal that lets the trap fire), the private
directory is deleted — nothing to ever accumulate or manually clean up.

**Live-verified** (not just designed): two genuinely concurrent
sessions were launched, typing DIFFERENT text into the same field
simultaneously — zero cross-talk, each session's own `gui_state.txt`
held exactly what that session typed. Quitting one session left the
other's process, directory, and typed state completely untouched, and
left the real shared persistent data intact.

## Applying this to muchi-pals' own `button.sh` `run` action specifically

Your own `run` action (`button.sh` lines 84–146) currently does:

```bash
run|r|start|chtpm|menu)
    cd "$SCRIPT_DIR"
    mkdir -p pieces/system pieces/display pieces/apps/player_app pieces/keyboard \
             projects/muchi-pals/manager
    : > pieces/apps/player_app/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/pending_command.txt
    : > pieces/display/muchi_screen_changed.txt
    : > pieces/system/pets_screen_state.txt
    : > pieces/system/last_message.txt
    : > pieces/system/active_target.txt
    : > projects/muchi-pals/manager/gui_state.txt
    : > pieces/system/quit_flag.txt
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=muchi-pals
active_target_id=main
EOSTATE
    export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
    export PRISC_PROJECT_ID="muchi-pals"
    "$(bin_path system/renderer)" &
    RENDERER_PID=$!
    "$(bin_path system/chtpm_parser_pal)" pieces/chtpm/layouts/main.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!
    trap 'kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; pkill -f "system/prisc\+x" 2>/dev/null' EXIT INT TERM
    "$(bin_path system/keyboard_input)"
    kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null
    pkill -f "system/prisc\+x" 2>/dev/null
    ;;
```

Every one of those `mkdir -p .../: >` lines is real, per-invocation
ephemeral UI state — every single one of them belongs inside the
session directory. Here is the concrete rewrite:

```bash
run|r|start|chtpm|menu)
    cd "$SCRIPT_DIR"
    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/muchi-pals/manager"
    # SHARED (symlinked, never copied) - static definitions, never change per session:
    ln -s "$SCRIPT_DIR/system" "$SESSION_DIR/system"
    ln -s "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
    ln -s "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
    ln -s "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
    ln -s "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
    ln -s "$SCRIPT_DIR/projects/muchi-pals/pieces" "$SESSION_DIR/projects/muchi-pals/pieces"
    # SHARED (symlinked) - the REAL persistent game data (tokens/pet
    # stats) every session must see and mutate together:
    ln -s "$SCRIPT_DIR/pieces/world_01" "$SESSION_DIR/pieces/world_01"

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/pending_command.txt
    : > pieces/display/muchi_screen_changed.txt
    : > pieces/system/pets_screen_state.txt
    : > pieces/system/last_message.txt
    : > pieces/system/active_target.txt
    : > projects/muchi-pals/manager/gui_state.txt
    : > pieces/system/quit_flag.txt
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=muchi-pals
active_target_id=main
EOSTATE

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="muchi-pals"

    "$(bin_path system/renderer)" &
    RENDERER_PID=$!
    "$(bin_path system/chtpm_parser_pal)" pieces/chtpm/layouts/main.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    # See "the pkill trap" section below before touching this.
    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            [ "$cwd" = "$SESSION_DIR" ] && kill -9 "$pid" 2>/dev/null
        done
    }
    trap 'kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

    "$(bin_path system/keyboard_input)"

    kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null
    kill_own_module
    ;;
```

## The one real gotcha: `bin_path` returns an ABSOLUTE path

Your own `bin_path()` (line 43-45) is `echo "$SCRIPT_DIR/$1$EXE"` -
always the REAL project root's own binary, regardless of `cwd`. That's
fine and doesn't need to change (the binaries themselves are static,
shared, read-only - no reason to symlink or copy them individually).
What matters is that **the process's own working directory** is
`$SESSION_DIR` when it runs (so its own relative file I/O, and the
`pal/main_loop_chtpm.pal` argv `prisc+x` gets launched with, resolve
inside the session) - that's why the rewrite above does `cd
"$SESSION_DIR"` right after creating it and before launching anything,
even though the binary paths themselves stay absolute via `bin_path`.

## The pkill trap gotcha (READ THIS - it's the part most likely to be
copied wrong)

`system/prisc+x` (the persistent module `chtpm_parser_pal` spawns) is
always launched with a relative argv (`pal/main_loop_chtpm.pal`), and
that argv TEXT is identical across every session - a plain `pkill -f
"system/prisc\+x"` (your own existing line) cannot tell sessions apart
by argv alone. If you keep the bare `pkill -f "system/prisc\+x"` after
adding session directories, it will kill every OTHER concurrent
session's own module the instant any ONE session exits - silently
reintroducing the exact bug this whole doc exists to fix, just moved
from "typing corruption" to "your game randomly freezes when someone
else quits theirs." Match on each candidate's own real `cwd`
(`readlink -f /proc/$pid/cwd`) against `$SESSION_DIR` instead - cwd is
genuinely unique per session, argv text is not. The `kill_own_module`
function in the rewrite above does this; copy that shape exactly, not
the original bare `pkill`.

## What about `run-classic` (the non-chtpm mode)?

Same treatment applies if you want it session-isolated too (it's a
much simpler ephemeral-state list - just `pieces/system/quit_flag.txt`,
`pieces/apps/player_app/history.txt`, and `pieces/system/session.pid`).
Lower priority than `run` - `run-classic` is explicitly documented as
"not the primary way to play anymore." Apply the same pattern to it
whenever convenient, not urgently.

## Addendum, found AND REVERTED applying this to muchi-pals: do NOT
background `keyboard_input` to make SIGINT reach the trap faster - it
breaks real terminal input

An earlier pass in this same session found that `kill -INT` sent
directly to the script's own bash pid (bypassing the terminal, e.g.
from a supervisor or a direct-pid test) did not run the `trap` while
bash was blocked inside a plain foreground `"$(bin_path system/
keyboard_input)"` call - confirmed via `ps`, every process and the
session directory were still alive afterward. The fix tried first -
backgrounding `keyboard_input` too and `wait`ing on it explicitly, so
the interruptible `wait` builtin lets bash run pending traps
immediately - DID close that gap, confirmed via the same direct-pid
test... but broke something more important: **real keyboard input**.
User-reported, live: arrow keys (and eventually all input) started
showing as literal escape-sequence garbage instead of being decoded.
Root cause, confirmed by direct comparison against `pal-forum`'s own
real, working `button.sh` (`./system/keyboard_input`, plain foreground,
no `&`) - a BACKGROUNDED process reading the controlling terminal loses
proper foreground process-group status under job control, which is
exactly what `system/keyboard_input.c`'s own raw-termios reading needs
to work correctly. Reverted back to a plain foreground call.

**The real, corrected understanding**: the direct-pid-SIGINT gap this
was chasing is not actually a practical bug in normal interactive use.
A genuine terminal Ctrl+C sends SIGINT to the whole foreground *process
group* at once, hitting `keyboard_input` directly too - and
`keyboard_input.c` already has its own SIGINT handler
(`handle_signal()`) that disables raw mode, writes the quit flag, and
exits, letting the script's blocked foreground command return
NATURALLY - the exact same clean path pressing 'q' already uses
(verified working via direct fifo-based testing, zero lingering
processes, zero leftover session directory). The gap only matters for
`kill -INT <bash-pid>` specifically, bypassing the terminal entirely -
confirmed to be pal-forum's own current, already-accepted shape too
(identical foreground `./system/keyboard_input` call, identical trap),
not a muchi-pals-specific regression. **Do not "fix" this again by
backgrounding keyboard_input** - real terminal input correctness is
worth far more than closing an edge case that doesn't occur in normal
play, and every project in this family currently accepts the same
tradeoff.

## Verifying it actually works before considering this done

Don't trust the design - launch two sessions concurrently (two
terminals, or two backgrounded `./button.sh run` invocations if
testing headlessly via key injection into each session's own
`pieces/keyboard/history.txt`), type something different into each,
and confirm neither's own `gui_state.txt`/`pets_screen_state.txt`/etc.
ever shows the other's content. Then quit one and confirm the other's
own process and directory are completely unaffected, and that
`pieces/world_01/` (the real shared game data) still has whatever
either session wrote to it.
