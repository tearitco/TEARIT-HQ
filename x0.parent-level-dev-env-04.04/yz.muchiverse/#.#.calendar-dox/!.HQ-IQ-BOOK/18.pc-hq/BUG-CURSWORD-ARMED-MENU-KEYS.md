# Bug: armed cursword's context menu never gets the arrow keys

Reported by the owner 2026-10-05. Cause found by reading the code the same
day. **Not fixed, not reproduced live.** Only the code path was traced.

## Symptom

With cursword in control mode (armed: arrow keys move it around the
desktop), open its context menu. The arrow keys keep moving cursword and
the menu's focus row does not change. It breaks the native feel of the menu.

## Cause (read in `_.monads/_.livedesk-taskbar/ops/khtpm_entity.c`)

The `KeyPress` handler is an if/else chain, and the armed branch comes
first:

```
} else if (xev.type == KeyPress) {
    if (g_is_cursword && g_cursword_armed) {   // ~6563
        ... Esc  -> disarm
        ... Left/Right/Up/Down -> move cursword one grid cell (XMoveWindow)
        ... camera keys
    } else if (popup_win || user_popup_win || input_popup_win
               || text_popup_win || input_active) {   // ~6664
        ... menu key handling
    }
```

Right-click (`ButtonPress` button 3, ~6464) opens the menu in the same
process as `popup_win`, and does not disarm cursword or release the grab.
So while armed:

- Up/Down/Left/Right are consumed by the move branch and never reach the
  menu branch, so the focus row (`popup_focus_row`) never changes.
- Esc takes the disarm branch, so it disarms cursword instead of closing
  the menu.
- Armed mode also holds a display-wide `XGrabKeyboard`, so no other window
  can take the keys either. The grab is intentional: "stingy" focus was
  an owner request on 2026-08-30.

## Why tests would miss it (inferred)

The menu's own arrow handling (`popup_focus_row` changes near line 5504)
sits in the relay/history path, not in this `KeyPress` chain. Driving the
menu through the per-pid relay file would likely pass while a real keyboard
fails. This matches the existing note
`relay-testing-may-mask-real-focus-bugs`. Verify with real key events.

## Proposed fix (small)

Give an open menu priority over the armed move branch. Either test the
popup first, or guard the armed branch:

```
if (g_is_cursword && g_cursword_armed
    && !(popup_win || user_popup_win || input_popup_win || text_popup_win || input_active)) {
```

Effects: with a menu open, arrows and Esc go to the menu; closing the menu
returns arrow control to cursword; Esc with no menu still disarms. The
keyboard grab stays, since this window still receives every key.

Also decide: should opening the menu keep cursword armed (assumed yes, so
the sword resumes moving once the menu closes)?

## Verification when fixed

Real keyboard, not the relay: arm cursword, right-click it, press Down and
Up (focus row moves, sword does not), Esc (menu closes, sword stays armed),
arrows (sword moves), Esc (disarms). Also check `input_active` (a typed
Cli-io field), since Esc there should cancel the field and not the arm.
