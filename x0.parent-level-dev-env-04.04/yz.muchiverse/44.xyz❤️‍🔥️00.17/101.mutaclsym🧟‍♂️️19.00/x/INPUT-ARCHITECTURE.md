# MUTACLYSM INPUT ARCHITECTURE - The Problem & The Fix

## THE BUG (root cause of "keyboard input doesn't work")

keyboard_input.c writes to TWO files:
  1. pieces/apps/player_app/history.txt  — bare decimal ("13\n")
  2. pieces/keyboard/history.txt         — prefixed ("KEY_PRESSED: 13\n")

BUT the PAL loop (main_module.pal) reads from a THIRD file:
  pieces/apps/player_app/interact_relay.txt

keyboard_input NEVER writes to interact_relay.txt!
Only agent injection and CHTPM's inject_raw_key() write there.

Result: terminal keyboard input reaches the CHTPM parser (via keyboard/history.txt)
but NEVER reaches the PAL interpreter (mua_menu_input).

## THE FIX

Change main_module.pal to read from player_app/history.txt instead of
interact_relay.txt. This matches what keyboard_input actually writes to.

Also: change agent injection in mua_menu_input.c to write to history.txt
instead of interact_relay.txt, so all input sources use the same file.

## THE WRAITH-ALPHA REFERENCE

wraith-alpha's architecture (the canonical form we're copying):

```
[any input source] → append_key() → player_app/history.txt (bare decimal)
                                    keyboard/history.txt (KEY_PRESSED prefix)
                                         |                    |
                                         v                    v
                                    prisc+x (PAL)      chtpm_parser_pal
                                    OP_READ_HISTORY     process_key()
```

Both keyboard_input AND x11_mirror call the SAME append_key() function
that writes to BOTH files. The OS window manager guarantees only the
focused window receives keystrokes — no file locking needed.

## THE ULTIMATE FORM (after this refactor)

```
TERMINAL INPUT          X11/GL INPUT           AGENT INJECTION
     |                      |                       |
     v                      v                       v
 keyboard_input        x11_mirror             echo "13" >> history.txt
 (raw termios)         (XLookupString)
     |                      |                       |
     +---append_key()-------+---append_key()--------+
              |                      |
              v                      v
     player_app/history.txt    keyboard/history.txt
     (bare decimal)            (KEY_PRESSED prefix)
              |                      |
              v                      v
         PAL LOOP              CHTPM PARSER
      (read_history)          (process_key)
              |                      |
              v                      v
      mua_menu_input          UI navigation
      (game logic)            (focus, buttons)
```

## TASKBAR CONCEPT

Show game state in a terminal (like livedesk taskbar but text-mode).
The taskbar terminal also accepts keyboard input — same append_key()
writes to history.txt, same consumption by PAL loop.

## IMPLEMENTATION STEPS

1. Fix main_module.pal: change interact_relay.txt → history.txt
2. Fix new_game_module.pal: same change
3. Fix mua_menu_input.c agent injection: write to history.txt
4. Test: terminal input → END_TURN works
5. Later: x11 input → same append_key() → same history.txt
6. Later: taskbar terminal → same append_key() → same history.txt
