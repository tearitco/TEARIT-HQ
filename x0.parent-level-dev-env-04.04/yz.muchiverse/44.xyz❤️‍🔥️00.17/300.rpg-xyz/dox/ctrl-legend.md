# Control Legend — mutaclsym key bindings

## Key Ownership Map

| Key | Owner | Action | Notes |
|-----|-------|--------|-------|
| `1` | choice.c | POV mode 1 (first person) | render_mode must be 1 (3D) |
| `2` | choice.c | POV mode 2 (third person) | render_mode must be 1 (3D) |
| `3` | choice.c | POV mode 3 (free roam) | render_mode must be 1 (3D) |
| `4` | choice.c | POV mode 4 (bird's eye) | render_mode must be 1 (3D) |
| `0` | choice.c | Toggle 2D/3D render mode | No-op if already in target mode |
| `9` | choice.c | Release possession / reverse-jump | context-dependent |
| `w` | camera_control.c | Pan forward / up | Modes 3/4 only |
| `a` | camera_control.c | Pan left | Modes 3/4 only |
| `s` | camera_control.c | Pan backward / down | Modes 3/4 only |
| `d` | camera_control.c | Pan right | Modes 3/4 only |
| `q` | camera_control.c | Yaw -10 (rotate left) | Modes 1/2/3 only |
| `e` | camera_control.c | Yaw +10 (rotate right) | Modes 1/2/3 only |
| `r` | camera_control.c | Pitch +10 (look up) | Modes 1/2/3, clamped to 89 |
| `t` | camera_control.c | Pitch -10 (look down) | Modes 1/2/3, clamped to -89 |
| `c` | camera_control.c | Camera Z level +1 | All modes |
| `v` | camera_control.c | Camera Z level -1 | All modes |
| `z` | move_player.c | Hero Z level -1 | 3D only (render_mode=1) |
| `x` | move_player.c | Hero Z level +1 | 3D only (render_mode=1) |
| arrows | move_player.c | Hero / cursor movement | Left-click moves hero, interact_mode moves cursor |
| `esc` | choice.c | Exit interact mode / close panel | Does not exit possession |
| `enter` | choice.c | Commit panel selection | Panels: craft/inventory/xlector_ctx |
| `Space` | choice.c | Open/close **xlector context menu** | **Only if interact_mode=1**; at xlector cell |
| `1`–`5` | choice.c | While xlector_ctx open: Event/Copy/Paste/Delete/Exit | Immediate fire; Event → desktop + event-editor request |
| `Ctrl+C` | keyboard_input.c | Quit game | Only way to quit |

## Camera Mode Reference

| Mode | Name | Pan (wasd) | Rotate (q/e/r/t) | Z (c/v) |
|------|------|-----------|-------------------|---------|
| 1 | First person | No | Yes | Yes |
| 2 | Third person | No | Yes | Yes |
| 3 | Free roam | Yes | Yes | Yes |
| 4 | Bird's eye | Yes | No | Yes |

## Dispatch Chain

```
key press
  → keyboard_input.c (terminal) or gl_mirror.c (GL window)
  → history.txt
  → prisc+x PAL loop reads history.txt
  → main_loop.pal:
      1. move_player x2    (arrows, x/z)
      2. choice x2         (0-4, 9, esc, enter, panels)
      3. camera_control x2 (q/w/a/s/d/e/r/t/c/v)
```

## Interact Mode

Entered via: panel commit (Enter on possessable piece), `9` reverse-jump.
Exited via: `esc`.
While active: arrows move xlector cursor, not hero.
Not a direct hotkey — entered through gameplay, not a dedicated key.
