# In-game layouts: rendering user-made menus and bars inside the game

Written 2026-10-06 from an owner conversation. **Plan only.** Part 1 and 2
describe what exists (read or verified live on 2026-10-05); everything from
part 3 on is design, marked **(proposed)** where it is not yet decided.

## 1. What the owner wants

1. **Users will make menus (and bars) in their own games.** Those should be drawn
   *inside the game*, in the pc-hq board window, not as separate floating windows.
   This is where in-game drawing starts: user layouts first, then "come back later and
   fix the old ones" (the entity context menus).
2. **Layouts are saved and called by name** from the pc-hq **Events** menu, and the
   same layouts work on the **livedesk** too.
3. **A layout works with pal** (data and actions from pal/ops/managers, the usual house
   shape) and is made in an **interactive x11-hq editor that an agent can use as well as
   a human** (see the layout studio, part 6).
4. In-game layouts must **not leave the pc-hq viewport**, and must **minimize with the
   board** (the floating pc-hq context menus today do neither).

## 2. What exists today (verified)

| Thing | State |
|---|---|
| Toolbar (`In:`, book, page, Menu, Player) and the entities bar | In the board window's own layout. One window, one process; minimize with it. Works well (owner, 2026-10-05). |
| Dropdowns (Desk, Menu) | `class="dropdown-child"` rows, laid out as overlays relative to an active trigger, painted last. Open only while their trigger is the active scope; stacked vertically. |
| **Canvas overlay strip** (built 2026-10-05) | `class="canvas-overlay-bottom"` on a `<row>` that follows the `<canvas>` in the view panel: `kh_layout_canvas_in_region()` in `khtpm_core_render.c` lays it out centred at the bottom of the canvas: `<text>` above, `<item>`s as slots, `<cli_io>` below. Painted over the canvas by tree order, nav-numbered, hidden/shown with `show="${var}"` (dropped at parse, re-added on the live reparse). The pc-hq hotbar is the first user. |
| Second vars file | `vars="state/ui.txt state/pchq/ui.txt"`: a window can load several feeds. The hotbar manager publishes `hb_`-prefixed keys so they cannot collide with the board's own. |
| Toggle ("minimize") | A cell in the entities bar runs `hotbar_toggle.sh`, which flips `state/pchq/visible.txt`; the manager republishes `hb_visible`. |
| Armed text field vs Interact | Fixed 2026-10-05: an armed `<cli_io>` beats the board's "100% game input" Interact forwarding. |
| Entity context menus in pc-hq | **Separate windows.** `ops/pc_entity_ctx.sh` launches the shared `khtpm_core_render` on a generated menu at a screen position; rows append `CTX_<VERB> x y z id` to the game inbox. Nothing ties the window to the board: it can leave the viewport and does not follow a minimize. |
| Layout studio | A **seed doc only** (`08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md`, proposed 2026-09-29, not built). |

## 3. Gaps found while building the pc-hq hotbar (2026-10-05)

- **No overlay chrome.** The strip has no title bar, no `_` minimize button, and cannot be
  slid sideways (the desk hotbar can: `vars-positioned`, vertical forced, sideways kept).
- **One footer only.** The layout engine finds the first `<footer>`; a second bar cannot
  be added as a footer. The canvas fills its whole panel, so any other child of the view
  panel was never positioned (it silently disappeared until the overlay class existed).
- **Only one placement exists** (bottom-centre). Menus need "at this cell / at the cursor",
  "top", "left", "right".
- **Overlays are hand-written into the board's own `.xhtpm`.** A user's menu has to be a
  separate file the board loads.

## 4. Design (proposed)

**Principle (house standard, `CENTROID_GOLD_STD.md`):** one parsed, laid-out tree is the
source of truth; a layout is a real `.chtpm` + CSS document rendered by the shared
renderer. Logic stays in ops, managers and pal. The studio *generates* layouts; it never
adds a second rendering path.

### 4a. A layout is a fragment, not a window

A layout lives in a folder (names provisional):

```
<scope>/layouts/<id>/
    layout.chtpm        the tree (rows, items, text, cli_io, repeat ...)
    layout.css          its look
    layout.pdl          anchor, size, chrome flags, which feeds it reads
    pal/ or ops/        optional: the manager/projector that publishes its feed (ui.txt)
```

`<scope>` is the game (`@.apps/<game>/layouts/`) for a game layout, or the house
(`#.desktop/layouts/`) for one shared by every game and the desk.

The host window (the pc-hq board, or any khtpm window) loads a fragment into its own tree
with a generic **`<overlay src="..."/>`** tag **(proposed; not built)**. The fragment is
parsed with the same pipeline and merged through the existing keyed reparse diff
(`khtpm_reparse_diff.c`), so a field keeps its state across reparses. It lives inside
the host, so it clips to the host and minimizes with it.

### 4b. Placement family

Generalize today's `canvas-overlay-bottom` into anchors declared by the layout:
`bottom-centre` (exists), `top-centre`, `left`, `right`, `at-cell x y z` (follow a board
cell), `at-point` (a menu opened at a click), `at-entity id`. Anchors are clamped to the
host's viewport by construction, which fixes "menus leave the pc-hq view".

### 4c. Generic chrome (reuse, no per-layout code)

Window classes that already exist on x11-hq windows, applied to an overlay: title bar,
`_` minimize (hides it and leaves a cell in the host's bottom bar, the way the hotbar
toggle cell does today), optional `slide-x` / `slide-y` (drag along one axis only, the
`vars-positioned` behaviour), `no-close`, `no-fullscreen` (added 2026-10-05).

### 4d. Data and actions (pal compatible)

- **Data in:** the layout's feed is a vars file (`state/<id>/ui.txt`) written by a manager
  or a pal projector, loaded as an extra vars file (done for the hotbar). `<repeat>`
  expands rows from it (done).
- **Actions out:** an item's `action=` runs an op or appends a line to the game inbox
  (`CTX_<VERB> x y z id`, already used by the pc-hq menus). Pal drives the loop, ops do the
  work, as in the rest of the house.
- **Focus:** an armed `<cli_io>` has priority over game input; a failed keyboard grab
  retries each tick (both done 2026-10-05). A permanent field coexists with the other
  grab holders; the modal popup lock is only for popups.

### 4e. Saved layouts, called by name

- Layouts are registered as **event commands** in the existing zero-recompile registry
  (`#.ref/menu/event_commands.registry.pdl`): `layout.show <id>`, `layout.hide <id>`,
  `layout.toggle <id>`. So a layout can be called from the pc-hq **Events** menu, from any
  event page, from a key, or from another layout.
- A small **layouts menu** lists what is saved in the current game and the house.
- **Livedesk:** the desk has no game canvas, so the same fragment renders as an HQ window
  (nav number, minimize into the bottom bar) or as a **rail** in the dock stack
  (`CURSWORD-POSSESSION-DESIGN.md` 5g). One layout, two hosts.

### 4f. First users

1. **Hotbar** (done as the first overlay; add chrome + slide).
2. **User-made game menus**: the real target, since users will author them.
3. **pc-hq entity context menus**: migrate last. The verb menus (Inspect, Copy, Delete) are
   easy as an `at-point` overlay driven by `pick.txt`; the desk entities' own `menu.chtpm`
   (Cli-io, Dir, Chat ...) is harder and can stay a window until the layout system is solid.

## 5. Phases (proposed)

| # | What | Done when |
|---|---|---|
| 0 | Canvas overlay strip, second vars file, toggle cell, armed-field fix | **done 2026-10-05** |
| 1 | Overlay chrome: `_` minimize into the bottom bar, slide along one axis; apply to the pc-hq hotbar | pc-hq hotbar minimizes and slides like the desk one |
| 2 | Anchor family + viewport clamp as classes | a test overlay at each anchor never leaves the board |
| 3 | `<overlay src>` fragments + `layout.pdl`; hotbar becomes a fragment | the hotbar is loaded from `layouts/hotbar/`, not hand-written into the board |
| 4 | Event commands `layout.show/hide/toggle`; layouts menu; saved per game and house | a layout opens from the pc-hq Events menu |
| 5 | Studio ops (part 6) | an agent builds and saves a layout from the command line |
| 6 | Studio window | a human edits the same layout with live preview |
| 7 | Livedesk host (HQ window / dock-stack rail) | the same layout runs on the desk |
| 8 | Migrate pc-hq entity context menus | old floating menus retired |

## 6. The layout studio (builds on the seed doc)

The seed (`08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md`) already fixes the rules:
output is a real `.chtpm` + CSS; a layout can read an existing event page or take an event
drag-dropped into it; it lives as a sub-entity in the proposed ☁️ entity. This plan adds:

- **Ops first, window second (house rule: every op is independently testable).** One CLI,
  `layout_op`, does everything: `new | add <kind> | set <id> <attr> <value> | move | remove |
  bind <id> <feed-key> | on <id> <event> | preview | save | load | list`. It edits the
  layout files and nothing else.
- **The x11-hq editor is a thin window over `layout_op`**: a live preview of the layout
  plus a `<cli_io>` that runs the same commands. A human can also click and drag; an agent
  uses the commands or the per-window relay (`entity_menu_history/<pid>.txt`) and reads the
  result from the files, with no pixel work.
- **Preview is the real renderer** on the real feed, so what you see is what runs.
- **Saved layouts are plain files**, so they diff, merge, ship in a game and survive the
  kind of working-tree loss described in `03-pitfalls/INCIDENT-2026-10-05-WORKING-TREE-WIPE.md`.

## 7. Open questions

- `<overlay src>` merge semantics: one global id space or per-fragment prefixes (collisions
  between a game's and the house's layouts)?
- Per game or per house when both define `hotbar`: which wins?
- Nav numbering for overlay items: today they continue the host's sequence (the pc-hq hotbar
  cells are 26-34). A per-overlay base like the desk's `nav-after-top` could restart them.
- Does `at-cell` follow the camera (needs the board's projection)?
- Where does the ☁️ entity come first: the seed doc recommends building it minimally first.

## 8. Related

`CURSWORD-POSSESSION-DESIGN.md` (hotbar, dock stack, digit echo), `PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md`
in `@.apps/piececraft-hq/` (why menus are windows today), `08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md`,
`02-architecture/CENTROID_GOLD_STD.md`.
