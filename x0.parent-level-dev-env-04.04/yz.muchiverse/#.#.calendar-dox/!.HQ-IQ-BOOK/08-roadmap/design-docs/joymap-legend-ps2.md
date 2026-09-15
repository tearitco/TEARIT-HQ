# Joystick button legend (PS2-style USB gamepad)

**2026-09-15.** Real, live-captured button/axis mapping for the actual
hardware plugged into this house (`lsusb`/`/proc/bus/input/devices`:
"USB Gamepad", `/dev/input/js0`, 6 axes / 12 buttons per
`JSIOCGAXES`/`JSIOCGBUTTONS`). Every row below was confirmed by the
user physically pressing the named button while a raw
`struct js_event` logger (reading `/dev/input/js0` directly, the same
API `khtpm_joystick_daemon.c` uses) printed the real button index —
not assumed from a PS2 diagram.

## Buttons

| Physical label | `struct js_event` button number |
|---|---|
| 1 | 0 |
| 2 | 1 |
| 3 | 2 |
| 4 | 3 |
| L1 | 4 |
| R1 | 5 |
| L2 | 6 |
| R2 | 7 |
| Select | 8 |
| Start | 9 |
| L3 (left stick click) | 10 |
| R3 (right stick click) | 11 |

All 12 buttons this pad reports are now named. Note the face buttons
are printed as plain digits "1"-"4" on this specific unit, not
PlayStation glyphs (△/○/×/□) - a real hardware detail, not a
naming choice made here.

## Axes (directional input)

Only axis 0 and axis 1 fired during live testing (moving the D-pad):
- **Axis 0**: negative = LEFT, positive = RIGHT
- **Axis 1**: negative = UP, positive = DOWN
- Both report full ±32767/0 swing with no intermediate values - this
  pad's D-pad behaves as a digital 3-state axis, not a true analog
  stick.

**Open, not yet tested**: axes 2-5 (this pad reports 6 total via
`JSIOCGAXES`) were never exercised during capture - likely a right
analog stick's X/Y plus 1-2 unused/reserved axes, but this is a real
guess, not confirmed. L3/R3 existing as separate digital buttons
(10/11) suggests there IS at least one real clickable analog stick
physically, whose movement axes haven't been identified yet. Test
these before assuming any mapping for them.

## What's wired today (v1, arrows only)

`khtpm_joystick_daemon.c` currently only acts on axis 0/1 (see
`JOYSTICK-INPUT-HOUSE-WIDE-DESIGN.md`) - none of the 12 buttons above
are wired to any action yet. This table is the reference for doing
that next: a natural v2 default (not yet built) would be:
- Button 0 ("1") -> Enter/confirm (13) - matches the real precedent
  found in the sibling TPMOS prototype (`JOY_BUTTON_0` -> Enter there,
  same button-0-is-confirm convention, though that codebase's face
  buttons aren't labeled "1"-"4").
  Direct confirmation: the button that would need to be treated as
  primary Enter/confirm is the one labeled "1" here.
- Select/Start (8/9) and L1/R1/L2/R2 (4-7) - not yet assigned, real
  open question for the .pdl remap default table.
