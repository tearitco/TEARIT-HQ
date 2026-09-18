# Inventory drop + reusable window highlight — design (2026-09-18)

**Status:** DESIGN ONLY. Next sprint. Do not implement in this pass.
**Palette overlay:** user confirmed the 64px wireframe looks good.

## Three different “drag” stories (do not mix them)

| What | Mechanism | Xdnd? |
|---|---|---|
| **Palettes Place** (RMMV / emoji brush) | `tp_arm_placer_rmmv.+x`: full-screen click-capture strips, now **tic-tac-toe lines**. Click stamps a **tile** on the desk. | **No.** Mutter/XWayland never delivered real clicks to `XGrabPointer`; the overlay exists so a click lands on an X11 surface. |
| **Muchi-pals “Xdnd”** | `01.muchi-pals/system/xdnd_source.c` | **Not live.** Header: not compiled into egg; never worked E2E (WM reparent + gl_mirror CPU spin). Egg uses **coordinate + file handoff** on button release (`check_drop_on_release`). Kept as reference. |
| **khtpm windows as drop targets** | `khtpm_core_render.c` (~2026-08-24): `<window drop_action="...">` attaches `XdndAware`. First `text/uri-list` path → `$DROP_PATH`, run action. First consumer: **bookmarks** (drop a dir). Blocking event loop, no idle-poll spin. | **Yes, as TARGET only.** File Explorer and entity menus do **not** set `drop_action` today. |

Palette placing is **not** the muchi Xdnd module. Inventory-in should not reuse the full-screen yellow/grid overlay (that is “stamp on the empty desk”). Dropping **into a window** is a **target highlight** problem.

## Problem: drag INTO inventory

There is no desk-wide highlight grid for “this bag.” The Inventory **window** should show it is the drop target (border / fill / title cue) while a pal or file is over it, then `mv` the entity dir into `cursword/inventory/` (or `$DROP_PATH` into the current explorer dir).

Same cue should work for **any** HQ window that opts in — not Inventory-only.

## Proposed reuse (next sprint)

1. **Opt-in attribute** already exists: `drop_action=` on `<window>`. Add File Explorer:
   `drop_action="..."` → manager `cmd=DROP` + path, `mv`/`cp` into `current_dir`.
2. **Highlight while hovering** (new, generic):
   - On `XdndEnter` / `XdndPosition` with a valid uri-list: set a flag, **redraw a chrome/frame accent** (not a screen overlay, not a new `layout_*`).
   - On `XdndLeave` / drop / timeout: clear flag.
   - Reuse for bookmarks, Inventory, any later “drop onto this window” HQ.
3. **Source side (desk pal → window):**
   - Do **not** revive muchi `xdnd_source.c` first.
   - Prefer: khtpm entity window as Xdnd **source** (uri-list = pal dir) **or** the egg-style coordinate handoff if Xdnd source still dies under Mutter.
   - Palettes Place stays the overlay+click path. Different verb.
4. **Out of inventory:** Cut/Copy/Paste already `mv`/`cp` inside the explorer. “Drag out to desk” = Place armed + overlay **or** Xdnd source from the explorer. Separate burst after INTO works.
5. **No symlinks.** Real dirs.

## Out of scope this sprint

- Example pals copied into `inventory/` (empty dir is fine until drop works).
- Wiring File Explorer Place to `tp_arm_placer_rmmv`.
- New renderer layout for a drop grid inside the window.

## Check after implement

- Drop a folder onto **bookmarks** still works (regression).
- Hover Inventory window: visible highlight; leave: highlight gone.
- Drop: path appears under `cursword/inventory/` as a real directory.
- Palettes Place overlay still wireframe, not a yellow wash.
