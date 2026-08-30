# How to drive piececraft-xyz locally (setup -> game -> board GL widget)

**2026-08-29.** Recipe for future agents to reproduce this session's own
live verification without re-deriving it from scratch. Real, tested
commands only - everything below was actually run.

## 0. The two gotchas that cost the most time here

1. **Never `pkill -f "piececraft-xyz"`** (or any substring of the project's
   own path) while your own shell command's cwd/args also contain that
   string - `pkill -f` matches your OWN invoking process too and kills
   itself mid-command (symptom: mystery `exit code 144` with zero output,
   even on trivial commands). Match a specific binary instead:
   `pkill -f "piececraft-xyz/system/orchestrator"`.
2. **Don't background with `cmd & disown`** inside one Bash call meant to
   run detached - it also produces the same `144` symptom here reliably.
   Use `setsid cmd > log 2>&1 < /dev/null &` (no `disown`) inside a single
   plain (non-`run_in_background`) tool call instead - this is the pattern
   that actually worked, verified live.

## 1. Build + launch the text UI

```bash
cd @.apps/piececraft-xyz
bash button.sh build          # confirm clean build first
rm -rf pieces/sessions        # fresh session
setsid bash button.sh run > /tmp/pc_run.log 2>&1 < /dev/null &
sleep 6
tail -30 /tmp/pc_run.log      # real ATLAS-EDITOR setup screen should render
ls -dt pieces/sessions/*/ | head -1   # note the session dir, needed below
```

## 2. Drive it via the REAL test-harness key-injection primitive

Don't fight for a real pty/terminal - civ-txt's own test harness already
solved this. Build the one real op you need:

```bash
gcc -O2 -o /tmp/tk_inject_key @.apps/civ-txt/test-harn-same/ops/tk_inject_key.c
```

`tk_inject_key <session_dir> <decimal_key_code>` appends one real
`KEY_PRESSED:` line to that session's `pieces/keyboard/history.txt`, in
the exact format `chtpm_parser_pal` requires. Nav items need **digit code,
then 13 (Enter)** - digit alone only moves the cursor, doesn't activate
(confirmed live, matches civ-txt's own documented behavior).

```bash
SESS="@.apps/piececraft-xyz/pieces/sessions/<your-session-id>"
/tmp/tk_inject_key "$SESS" 50; sleep 0.3   # '2' = Confirm & Start (Debug Flat World)
/tmp/tk_inject_key "$SESS" 13; sleep 1     # Enter
/tmp/tk_inject_key "$SESS" 51; sleep 0.3   # '3' = Enter Game
/tmp/tk_inject_key "$SESS" 13; sleep 2     # Enter
tail -25 /tmp/pc_run.log                   # should now show Tick/Hero HP/Pos/Chunk
```

ASCII digit codes if you need others: '1'=49 '2'=50 '3'=51 '4'=52 '5'=53
'6'=54 '7'=55 '8'=56 '9'=57 '0'=48, Enter=13.

## 3. Open the board widget (real 3D board, separate GL window)

From the in-game menu, `2` = "View Board (opens separate GL window)":

```bash
/tmp/tk_inject_key "$SESS" 50; sleep 0.3
/tmp/tk_inject_key "$SESS" 13; sleep 3
ps aux | grep -iE "board.viewer|gl_mirror"   # confirm the widget's real processes came up
```

This spawns `&.widgits/board-viewer/button.sh run-widget <piececraft-xyz path>`
as a real child, which forks its own session under
`&.widgits/board-viewer/pieces/sessions/<id>/`.

## 4. "Interact mode" - the real possess/unpossess toggle

Confirmed real, live, in `&.widgits/board-viewer/ops/bv_menu_input.c`:
**key `9`** (`key_possess`, line ~318) toggles the xelector cursor's
possession of the hero on/off. **The game starts with the hero ALREADY
possessed by default** (confirmed live:
`@.apps/piececraft-xyz/pieces/xelector_01/state.txt` shows
`possessed_id=hero_01` right after entering the game, before ever
pressing `9`). Pressing `9` again releases possession and does a real
"reverse jump" back to the xelector's pre-possess position.

Camera modes are the digits `1`-`4` (gated on `camera_mode`, same file) -
these are read from the board-viewer session's OWN `state.txt`
(`pieces/system/bv_state.txt` under the widget's session dir), NOT from
piececraft-xyz's own session - two separate keyboard-history streams.

**IMPORTANT, found live 2026-08-29:** widget/GL-mode profile
(`run-widget`) deliberately has **no `keyboard_input` process at all** -
`button.sh`'s own comment says "GL owns input via gl_mirror's own
interact_relay forwarding," meaning real keys are meant to go straight to
the GL window via real X11 focus, NOT through the
`pieces/keyboard/history.txt` relay `tk_inject_key` writes to. Injecting
into that file for a widget-mode session is a no-op (confirmed live: key 9
injected this way, `xelector_01/state.txt` did not change). The
`tk_inject_key` recipe above only works for the TEXT UI (non-widget,
`run`/`run-app` profiles) - a different, not-yet-found mechanism is needed
to drive the GL widget's own real keyboard input headlessly.

## 5. ROOT CAUSE FOUND (2026-08-29, direct user diagnosis): board-viewer was never converted to x11_mirror

The board-viewer widget's `gl_mirror` process starts and stays alive
(confirmed via `ps aux`), but its window never showed up in `xwininfo
-root -tree` from this agent's side, even though the user could see and
drive it live on their own real screen. Direct user diagnosis, confirmed
by checking the binaries on disk: **`&.widgits/board-viewer/system/`
only ships the legacy `gl_mirror` binary - no `x11_mirror` at all**
(`ls system/ | grep mirror` -> `gl_mirror`, `gl_mirror.exe`, nothing
else). This is the OLD direct-GLX display shim `legacy-shared-fix.md`
already documents as being phased out house-wide in favor of the new,
plain-Xlib `x11_mirror.c` (proven on mutaclysm, already the default for
piececraft-xyz's own TEXT UI - confirmed live this same session, its
main session ran a real `x11_mirror.+x` process) - `gl_mirror`'s window
apparently isn't a normal top-level X11 window the way `x11_mirror`'s
is, which is exactly why standard X11 tooling (`xwininfo`, this agent's
own `dump_frame_png_op.+x`) can't see or reach it, even though it's
real and live on a real display.

**Real, additional gap found**: `legacy-shared-fix.md`'s own tracked
scope is the 16 top-level legacy-GL *projects* (mutaclysm, civ-txt,
piececraft-xyz, etc.) - `&.widgits/board-viewer` is a shared *widget*
used BY several of those projects, not one of the 16, so it fell
outside that sweep entirely and was never queued for conversion. Since
piececraft-xyz's own real 3D board view depends entirely on
board-viewer's widget mode, this needs its own real conversion pass
(gl_mirror.c -> x11_mirror.c, same pattern already proven on
mutaclysm/piececraft's own main session) before this agent (or any
X11-tooling-based remote verification) can reach it. Not done yet -
real, scoped follow-up work, not attempted in this session.
