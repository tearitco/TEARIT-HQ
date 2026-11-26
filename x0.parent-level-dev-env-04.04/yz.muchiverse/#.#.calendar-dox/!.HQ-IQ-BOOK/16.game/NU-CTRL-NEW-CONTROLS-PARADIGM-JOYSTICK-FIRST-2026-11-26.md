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
- **CORRECTION (same day, owner asked "we already have working joystick in some parts of the house, find that code first"):** an earlier version of this page said no window reads a joystick. WRONG: my search was too narrow. The house has `_.monads/_.livedesk-taskbar/ops/khtpm_joystick_daemon.c` (built by `build_khtpm_strip.sh` to `+x/khtpm_joystick_daemon.+x`, written 2026-09-15 live against THIS controller: USB Gamepad, 6 axes / 12 buttons, D-pad on axis 0 and 1). It reads `/dev/input/js0` (raw kernel API) and appends `KEY_PRESSED: 200..203` (up, down, left, right) to the relay file `#.desktop/entity_menu_history/<pid>.txt` of the window named in `#.desktop/joystick_focused_pid.txt`, which every khtpm window writes while it holds X focus. v1 is ARROWS ONLY. Buttons and a `joystick_map.pdl` remap (`BUTTON | n | key code`) are designed in `08-roadmap/design-docs/JOYSTICK-INPUT-HOUSE-WIDE-DESIGN.md` and not built. **Nothing launches the daemon** (design open item 2: where it starts); it was not running on 2026-11-26, which is why the owner saw no response in any mode. The old terminal-era twin is `1.TPMOS_c_+rmmp.0103.0001/pieces/joystick/plugins/joystick_input.c`.
- Plan (not built): do NOT write a new op. Launch the existing daemon through the house funnel (registered, exits with its owner), then build the buttons half the design already specifies: `joystick_map.pdl` rows read by the daemon, first a probe that prints which button number is which so the owner presses each one. Note the code families: the window relay uses 200-203 for arrows, the rmmc game relay `interact_relay.txt` uses 1000-1003; check which one a focused In-mode window turns the daemon codes into before mapping buttons.
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

## Console style inside In mode (owner 2026-11-26, late; decision D143, DESIGN only)

Owner: use Metal Gear Solid / GTA / Minecraft style controls; the same map should later serve GTA or MGSV style play, or Doom / shooter mechanics; In mode plays like a console, so the hotbar may be disregarded unless L/R cycle it.

**What is built and seen on the real pad (2026-11-26):** the D-pad walks, a held direction repeats (`REPEAT | 120 | 250` in `#.desktop/joystick_map.pdl`), button 0 jumps, button 1 mines, button 2 places, button 3 crafts (the game input file got the codes and the ledger shows three jumps and a DAMAGE row; place and craft were refused because an axe was held). Guard: harness case `joy_map`.

**The shape (D143):** the pad sends INTENTS, not key codes. A style profile (one file per style: `minecraft`, `mgs`, `gta`, `doom`) maps pad buttons to intent names; each book's `keybinds.pdl` already maps intent to key code; one op generates the daemon's `joystick_map.pdl` from the two. The daemon is not changed. Why: the key code then lives in ONE place, a new game or style is data, and the intent names are the verbs a harness or a model calls.

**A first layout to react to (a proposal, not built, and the physical numbers are NOT known):**

- D-pad or left stick: move. Right stick: look (the pad has 6 axes; axes 2 to 5 are ignored today and which one is the right stick is unproven).
- Bottom face button: jump. Left face button: primary (mine / attack / use). Right face button: secondary (place / aim). Top face button: craft / inventory.
- L1 and R1: cycle the held item backward and forward. This replaces the hotbar digits in In mode, so digits stay nav numbers. The hotbar stays on screen as the readout of what L1 / R1 selected.
- L2 or R2: aim / sprint (shooter and MGS style, needs game-side stances).
- Stick click: crouch (MGS style). Start: menu. Select: map or release control.

**What it needs, in order:** (1) a `cycle` verb as an OP (D92), next and previous over `hotbar.pdl`, writing the same PREFERENCE `play/pick.txt` that `rmmc_pick_op` writes; (2) per-button repeat in the map (hold to keep mining); (3) the intent generator op and its harness case; (4) the physical numbers of L1, R1, the sticks (buttons 4, 5 and 7 were seen being pressed, not yet identified); (5) animations and stances for crouch and aim are GAME-side states, not pad work, and are a separate design.

**Open:** how Start enters In mode (the key sequence the editor accepts is not found yet; a relayed `1` + Enter did nothing, a real click works); whether L1 / R1 cycling should wrap; whether the hotbar is hidden in In mode or only dimmed.

**Animations (owner 2026-11-26: Assassin's Creed, take-downs, weapon animations, climbing: "std animations since they will reappear in lots of games"):** the intents of D143 trigger the same ledger verbs and therefore the same clips. The standard clip list, what exists today and what is not a simple clip (two-actor take-downs, no IK) are in `BOX-LIMB-RIG-FOR-WALKING-CHARACTERS-2026-11-25.md`, last section. Intent -> ledger verb -> pose -> clip is one chain, so a pad, a harness and a model all start the same animation.
