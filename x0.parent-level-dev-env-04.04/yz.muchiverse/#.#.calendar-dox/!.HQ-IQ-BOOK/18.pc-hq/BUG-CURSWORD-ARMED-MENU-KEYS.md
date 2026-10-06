# Bug: armed cursword's context menu never gets the arrow keys

Reported by the owner 2026-10-05. Cause found by reading the code the same
day (first diagnosis retracted, see Cause). **Fixed; the owner confirmed it
works live after a `button.sh reset` (2026-10-05).** The history-line and
non-armed checks below were not run by the agent.

## Symptom

With cursword in control mode (armed: arrow keys move it around the
desktop), open its context menu. The arrow keys keep moving cursword and
the menu's focus row does not change. It breaks the native feel of the menu.

## Cause (corrected 2026-10-05 - the first diagnosis was wrong)

**Retracted:** this doc first said the armed branch of the `KeyPress` chain
sits ahead of the popup branch. That is not what happens. A legacy in-process
popup (`popup_win`) is handled by its own `KeyPress` branch earlier in the loop
(~line 6016), before the armed branch is ever reached, so branch order is not
the problem. An attempted guard on that branch changed nothing and was reverted.

**Actual cause (read in `_.monads/_.livedesk-taskbar/ops/khtpm_entity.c`):**
cursword has a `menu.chtpm`, so `g_use_khtpm_menu` is set and
`open_context_menu()` returns `None` after `launch_khtpm_menu()` forks a
separate `khtpm_core_render` process. In that mode `popup_win` stays 0 and the
menu is its own window and process. Arming cursword takes a display-wide
`XGrabKeyboard` plus `XSetInputFocus` on its own window. Right-click does not
release it. So every arrow/Esc goes to the sword's window, the sword's armed
branch moves the sword, and the menu process receives no key at all.

## Fix applied (owner-verified live)

In the right-click branch, before the menu is launched: if cursword is armed
and the menu is the khtpm kind, run the same disarm sequence the Escape and
focus-lost paths use (release pointer grab if awaiting placement, release the
keyboard grab, clear armed, write `cursword_armed.txt`, history line
`CURSWORD_DISARMED_MENU_OPEN`, shape/redraw). The menu then opens exactly as it
does for an unarmed cursword. Once the menu process has exited, the sword
re-arms itself (keyboard grab and focus again, history line
`CURSWORD_REARMED_MENU_CLOSED`). Kept deliberately simple: no timers, no focus
check. Known catch: if a menu action opens another window (Chat, Inventory),
the sword re-arms and holds the keyboard over it until Esc. The click-to-place
pointer grab is not restored on re-arm (a stray click would place the sword).

A running cursword only picks the fix up after a relaunch (`$.crypts/button.sh reset`).

## Not covered

The legacy in-process popup path (`popup_win`, entities without a
`menu.chtpm`) was not affected and is unchanged.

## Why tests would miss it (inferred)

The menu's own arrow handling (`popup_focus_row` changes near line 5504)
sits in the relay/history path, not in this `KeyPress` chain. Driving the
menu through the per-pid relay file would likely pass while a real keyboard
fails. This matches the existing note
`relay-testing-may-mask-real-focus-bugs`. Verify with real key events.

## Verification when fixed

Real keyboard, not the relay, with a freshly launched cursword: arm it, right-click it
(history shows `CURSWORD_DISARMED_MENU_OPEN`), press Down and Up (the menu's
focus row moves, the sword does not), Esc (menu closes, history shows `CURSWORD_REARMED_MENU_CLOSED`), arrows
(sword moves), Esc (disarms). Also check `input_active` (a typed
Cli-io field), since Esc there should cancel the field and not the arm.
