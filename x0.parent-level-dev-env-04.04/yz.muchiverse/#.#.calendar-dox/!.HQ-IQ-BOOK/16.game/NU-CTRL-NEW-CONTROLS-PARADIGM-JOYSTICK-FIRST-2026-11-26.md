# nu-ctrl: a new controls paradigm, joystick first (owner 2026-11-26) - DESIGN, nothing built yet

Why this page exists: the keyboard is about to be needed for typing and relay (chat, names, commands) and every game hotkey we add will collide with it. The owner's direction: leave the keyboard and the digits empty for text and nav, and design game controls around a joystick, then see which ideas cross back to the keyboard.

## What the owner decided (his words in short)
- **Digits are NAV numbers, always, also in In mode.** Putting camera views on 1-4 and the 2D/3D toggle on 0 was a short-term solution that lacked foresight; the camera moves off the digits.
- **In In mode the external frame's nav numbers are not usable**, so they should read `#` (an honest "inactive") instead of a number that does nothing. The hotbar slots then take their own numbers, 1 and up.
- **Camera:** `c` cycles the camera view for now. Alt is to be CHECKED (does the key reach the renderer at all?) before it is used.
- **No new hotkeys without checking them against the engine's key table**, and prefer buttons that have no keyboard meaning.
- Decide the new paradigm by plugging in the joystick and asking which concepts cross over. A joystick has no digits, so shoulder buttons (L1/L2/R1/R2) fit "previous / next" better than numbers (the owner wrote "l12/r12 ... making things like `.` make sense").

## What is true today (checked 2026-11-26)
- The engine reads its camera keys from each book's `keybinds.pdl` rows `KEY | pov_mode_1 .. pov_mode_6` (and `_alt` rows): moving the camera off the digits is DATA per book, no engine code. rmmc already uses `pov_mode_1_alt = p`, `pov_mode_2_alt = o`. Other books (teario-play, rpg-edit) still use the digits.
- rmmc binds the hotbar to the digits (`GAMEKEY slot1..9 = 49..57`, `page = 48` in `rmmc-pchq/pieces/system/keybinds.pdl`): these rows are what has to go, with Help (`help_` rows in `@.apps/rmmc/harness.pdl`), the tutorial text and harness case `rmmc_help` updated together.
- Neither the renderer nor the engine handles Alt (no reference anywhere). Modifier keys are different from letters: the desktop (GNOME) grabs Alt for window drag and menus, so a bare Alt press may never arrive. Test before relying on it.
- Typed digits in In mode go to the game; the frame's badge numbers (`[ ]23`) stay drawn but cannot be used. The badge drawing is the SHARED renderer: a change there follows the khtpm house standards (generic, no per-project C, idempotent layout).

## The joystick (probed 2026-11-26)
- Device: "USB Gamepad" (usb id 0810), `/dev/input/js0` and an event node under `/dev/input/by-id/`; the owner's user can read it (ACL), no input-group change needed. It reports INIT events for its buttons and axes.
- **No window reads it.** The renderer (khtpm), the 3D engine (bv_render_3d) and the editor have no joystick code, in In mode or in Off mode. The only matches in the repo are old terminal-era copies of `keyboard_input.c` in session folders. So "some windows have joy and some do not" is not the situation: none do.
- Plan (not built): one small C op, `joy_relay_op`, reads `js0` (the kernel joystick API: 8-byte events, button/axis, init flag), maps each button/axis through a DATA file (`joy.pdl`: `BUTTON | n | relay code or verb`, `AXIS | n | direction | threshold`), and appends the result to the window's relay file like a person pressing a key (`KEY_PRESSED: <code>` in `#.desktop/entity_menu_history/<pid>.txt`, the house relay, never xdotool). Registered as a house process (proc_reg), exits when its owner exits, idle cost near zero (blocking read). Step 1 is a PROBE verb that prints which button number each physical button is, so the owner presses each one and we write the map from his hands, not from a guess.
- Suggested first map (to test, not decided): stick or D-pad = arrows (walk), one face button = mine, another = place, shoulders = previous / next hotbar slot, another shoulder pair = camera cycle, Start = Help, Select = In on/off.

## Open decisions
1. Alt: does it reach the renderer? (test: a harness-free probe that logs the keysym.) Fallback `c`.
2. `#` for inactive frame numbers while In is ON: shared renderer change, its own task and commit.
3. Hotbar numbering: slots 1.. while In is ON (owner), what the numbers do when In is OFF.
4. Which camera keys for rmmc now: `p` first, `o` third, `c` cycle (top-down and 2D inside the cycle).
5. The mapping file and whether it is per book (like keybinds.pdl) or per device.

## Pitfalls for the next agent
- Never assume a key is free: read the engine's table (`bv_menu_input.c`, the `key_pov_*` reads) and the book's keybinds first. I put the hotbar on the digits and worked around the camera collision instead of asking (2026-11-26).
- A relay test proves the verb, not the physical key path: test the joystick path with the real device, with the owner pressing buttons.
- Do not leave the joystick op running unregistered, and do not let it read while another window is typing (the cursword and cli_io fields grab keys).
