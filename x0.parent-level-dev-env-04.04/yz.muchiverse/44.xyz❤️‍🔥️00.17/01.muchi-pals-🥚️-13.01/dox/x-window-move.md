# egg_window: X11 Window Movement & Test Injection

## How the pet window moves itself

When a pet is opened from the muchi-pals menu, `egg_window <pet_id>` is
launched. It creates a small (80x80px) borderless GL window shaped to the
pet's sprite silhouette (X11 Shape Extension + GLX, override_redirect).
The window moves around the desktop autonomously on an invisible 80px grid.

The movement is a two-process pipeline:

```
tick_pets.c (game logic, the "brain")
    |
    | writes grid_x, grid_y, z, tick_seq, facing, hunger, etc.
    v
pieces/world_01/map_lobby/<pet_id>/state.txt
    |
    | polled every ~300ms by egg_window.c (the "renderer")
    v
egg_window.c converts to pixels and calls:
    XMoveWindow(dpy, win, grid_x * 80, grid_y * 80 - z * 20)
```

`tick_pets.c` decides WHERE the pet goes. `egg_window.c` just reads that
position and moves the window there. egg_window never decides pet behavior
— it's a dumb renderer + input relay (per egg-pals.txt direction).

## Position conversion formula

```
pixel_x = grid_x * GRID_CELL_PX          (GRID_CELL_PX = 80)
pixel_y = grid_y * GRID_CELL_PX - z * Z_PIXEL_OFFSET  (Z_PIXEL_OFFSET = 20)
```

- `grid_x`, `grid_y`: absolute grid cell coords (owned by tick_pets.c)
- `z`: altitude (negative = burrowing, positive = flying)
- Screen bounds are clamped by egg_window using DisplayWidth/DisplayHeight

## All XMoveWindow call sites (POSIX/X11 build)

| # | Trigger | What happens | Writes state.txt? |
|---|---------|-------------|-------------------|
| 1 | Mouse drag (MotionNotify) | Window follows cursor in free-pixel mode | No |
| 2 | Mouse drop (ButtonRelease) | Snaps to nearest grid cell | Yes (grid_x, grid_y) |
| 3 | state.txt poll (tick_seq changed) | Reads new position from state.txt, moves | No (reads only) |
| 4 | drag_drop_test.pdl poll (every 1s) | Moves to exact pixel coords from config | No |

Self-tick (egg_window shells out to `ops/+x/tick_pets.+x <pet_id>`) does
NOT directly move the window. It causes tick_pets.c to write a new state.txt,
which egg_window picks up on the next poll (call site 3).

## Test injection: drag_drop_test.pdl

Both egg_window AND gl_mirror already poll a config file for position
overrides. The file does NOT need to exist at startup — both readers
handle missing files gracefully.

### File location

```
<project_root>/drag_drop_test.pdl
```

For egg_window, `<project_root>` is the muchi-pals directory.
For gl_mirror, `<project_root>` is the mutaclsym directory.

### Format (pipe-delimited)

```
SECTION      | KEY                | VALUE
----------------------------------------
WINDOW       | gl_mirror_x        | 100
WINDOW       | gl_mirror_y        | 100
WINDOW       | egg_window_x       | 500
WINDOW       | egg_window_y       | 200
```

### How it works

- egg_window.c: `read_test_config(pet_id, &x, &y)` — looks for
  `egg_window_x` and `egg_window_y` in the WINDOW section. If found,
  calls `XMoveWindow(dpy, win, x, y)` and updates internal grid_x/grid_y.
  Polls every 1 second. Disabled while dragging (`!dragging` gate).

- gl_mirror.c: `read_test_config(&x, &y)` — looks for `gl_mirror_x` and
  `gl_mirror_y`. If found, calls `glutPositionWindow(x, y)`.
  Polls every 1 second in the timer callback.

### Limitations

- Position is one-shot per poll cycle — if tick_pets.c writes a new
  state.txt between polls, the window will jump back to the state.txt
  position on the next tick_seq change (call site 3 overrides call site 4)
- Does NOT write back to state.txt (pixel coords are external override)
- egg_window position injection is disabled during drag

### To make position stick across ticks

Either:
1. Keep drag_drop_test.pdl values persistent (egg_window re-reads every 1s,
   so it keeps re-applying the override), OR
2. Write to state.txt with a tick_seq value HIGHER than tick_pets.c's
   current value, so egg_window treats it as new data

## Testing drag-and-drop

### What can be done with file injection alone

Position both windows so they overlap or are adjacent:

```bash
# Position gl_mirror at (100, 100)
cat > /home/no/Desktop/.../44.xyz.../101.mutaclsym.../drag_drop_test.pdl << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
WINDOW       | gl_mirror_x        | 100
WINDOW       | gl_mirror_y        | 100
EOF

# Position egg_window at (500, 100) — to the right of gl_mirror
cat > /home/no/Desktop/.../44.xyz.../01.muchi-pals.../drag_drop_test.pdl << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
WINDOW       | egg_window_x       | 500
WINDOW       | egg_window_y       | 100
EOF

# Wait for both to poll (1 second)
sleep 2
```

### What CANNOT be done with file injection alone

The actual drag gesture (ButtonPress → MotionNotify → ButtonRelease) is
a mouse event that egg_window captures from X11. Writing coordinates to a
file moves the windows but does NOT simulate the mouse events that trigger
the Xdnd protocol. For that you need either:

1. **xdotool** (external tool, not currently installed):
   ```bash
   xdotool mousemove 540 140   # move to egg_window center
   xdotool mousedown 1          # start drag
   xdotool mousemove 140 140    # move to gl_mirror center
   xdotool mouseup 1            # drop
   ```

2. **Custom C program** using XSendEvent to simulate ButtonPress/
   MotionNotify/ButtonRelease — no external dependencies needed.

3. **Manual testing** — physically drag the pet window onto gl_mirror.

### What to look for (success indicators)

1. Green border appears on gl_mirror when dragging over it
   (g_drag_over = 1 in display())
2. "gl_mirror: XdndDrop received" on stderr
3. "gl_mirror: got pet_id='...' via XdndSelection" on stderr
4. "gl_mirror: importing pet '...'" on stderr
5. Pet window closes automatically (close request file mechanism)
6. Pet appears in mutaclsym's world

## See also

- `drag-drop-bugs.txt` — all 6 bugs found and fixed in the Xdnd pipeline
- `drag-drop-how2.md` — original test guide (some info outdated after fixes)
- `0.a-z-pets-plan/a-z-pets-plan.md` — full pet ecosystem roadmap
