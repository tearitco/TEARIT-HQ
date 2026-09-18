# Inventory drop + reusable window highlight — design (2026-09-18)

**Status:** SPEC + slice 1 in progress (window highlight + desktop pal
→ Inventory `mv`). Preview-while-drag and pc-hq↔desk are later slices.
**Palette overlay:** user confirmed the 64px wireframe looks good.

## Have we done pc-hq → desk?

**No.** Palettes Place stamps **tiles** onto the desk. Piececraft
`pchq_place_cell.sh` writes a **glyph into a board cell**, not a
livedesk pal window. Dragging a pal from a pc-hq window onto the
desktop (or the reverse) is **not built**. That is slice 3.

## Preview while dragging

Not required now. Useful later for inventory↔inventory and pc-hq↔desk
so you see the sprite follow the pointer. Slice 4. Slice 1 is
**window highlight only** (no ghost sprite).

## Slices (do one at a time)

| # | What | Status |
|---|---|---|
| 1 | Highlight drop-target HQ window (caller color). Desktop pal drag-release `mv`s into Inventory/`current_dir`. Xdnd `drop_action` still works for dir drops (bookmarks-style). | **mv works (m8, ninja). Highlight often invisible. List does not live-refresh.** |
| 2 | Drag **out** / Place from explorer onto desk (tic-tac-toe overlay) or onto **another Inventory** (same highlight + dotted slot as manual drag). | **not coded** — Place still only writes `fe_place_armed.txt` |
| 3 | pc-hq window ↔ desk (same highlight + dest registry). | later |
| 4 | Optional drag preview (ghost). | later |
| 5 | Inventory ↔ inventory (same as 1 once two explorers have dest dirs). | later |
| 6 | **Cli-io `mv <entity-nav-#> <window-nav-#>`** — agent or human types a move instead of dragging. CTXMENU row. Same `mv` as drag. | **docs only** — `UNFACTOR-PAL-X.md` |

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

## Live bugs after slice 1 (user 2026-09-18 — no code until OK)

### A. Inventory window does not live-update after a drop

**Seen:** ninja dragon `mv`d into `cursword/inventory/`; explorer still empty until close/reopen.

**Why:** the pal process `rename()`s the pal dir. File Explorer’s manager only calls `list_directory()` + `write_ui_file()` on **its own** actions (BACK, ENTRY, VIEWMODE, CTX_*). Nobody pokes `file_explorer_action.txt`. The renderer only re-reads `file_explorer_ui.txt` vars; if that file is stale, `n_entries` stays 0.

**Proposed fix (small, house-shaped):** in `file_explorer_manager.c` poll loop, `stat(current_dir)` each tick; if `st_mtime`/`st_nlink` changed vs last list, relist and rewrite ui.txt. That also picks up a terminal `mv`. Optional extra: pal drop writes `cmd=REFRESH` to the explorer’s action file (needs `pkg=` on the zone file). Prefer the **stat poll** so any external change works.

**Not proposed:** restarting the explorer, or rebuilding the xhtpm.

### B. Place from explorer context menu does not show the tic-tac-toe grid

**Seen:** Cut/Copy/Paste/Delete/Place menu works; Place does not arm the palette overlay.

**Why:** `CTX_PLACE` only writes `fe_place_armed.txt` + clipboard `mode=place`. It never execs `tp_arm_placer_rmmv.+x`. That binary is the **only** live wireframe overlay (palette RMMV click-capture). Palettes Place ≠ explorer Place.

**Proposed fix (one spawn, then a pal-specific click handler):**

1. On PLACE, `setsid tp_arm_placer_rmmv.+x` the same way `palettes_menu.sh arm_rmmv` does (hole around the explorer rect if we have it).
2. That op currently ends by calling `tp_place_desktop_rmmv.+x` (stamp a **tile**). For a pal dir we must **not** do that. Add a mode or a sibling: if `fe_place_armed.txt` has `path=`, on click write `desktop_pos.txt` (grid snap) and `exec khtpm_core_render.+x <pal_dir>` (or `mv` out of inventory onto `pals/` + spawn). Esc cancels, clears armed file.

**Do not** reuse the overlay to stamp RMMV tiles when the payload is a pal.

### C. Highlight bar often not seen (still)

Drop **does** fire (agent xdotool: m8 → inventory; user: ninja). PNG during drag did not show `[ drop: name ]`. Likely: FE idle redraw lags the hover file, and/or pals live at **y≈1440** while explorer is at **y=90** on a 2496×1664 screen so a short drag never crosses the window. Raise-while-drag is in the new binary; confirm pal process start time ≥ that binary.

**Proposed:** after hover pid matches, FE `hq_request_redraw()` is already there — also bump a `file_explorer_ui.txt` dummy `hover_name=` so vars reparse forces a paint. Secondary: don’t treat as a blocker if A+B land first.

---

**Check with user before any of A/B/C.** Suggested order: **A** (stat poll), then **B** (spawn overlay + pal place, not tile stamp), then C if still needed.

## Out of scope this sprint

- Example pals copied into `inventory/` (empty dir is fine until drop works).
- Wiring File Explorer Place to `tp_arm_placer_rmmv`.
- New renderer layout for a drop grid inside the window.

## Check after implement

- Drop a folder onto **bookmarks** still works (regression).
- Hover Inventory window: visible highlight; leave: highlight gone.
- Drop: path appears under `cursword/inventory/` as a real directory.
- Palettes Place overlay still wireframe, not a yellow wash.
