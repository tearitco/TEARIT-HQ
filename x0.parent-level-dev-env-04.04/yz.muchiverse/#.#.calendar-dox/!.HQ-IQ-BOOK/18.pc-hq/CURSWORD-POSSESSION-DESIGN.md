# Cursword as possessor, shared inventory HUD, Place and Take

Written 2026-10-05 from an owner conversation. Design only: no code was
written for this. Every "exists" claim below was read in the code on that
date. Anything inferred is marked **(inferred)**.

## 1. What the owner wants

1. **Cursword is the player's cursor, on the desk and in pc-hq.** Selecting
   it gives arrow and camera control, exactly as on the desk today. The
   pc-hq-only levels (for example `test_walls`, desk 1) do not have it. They
   have only the xelector.
2. **Cursword can possess entities.** Moved over an entity, its menu should
   offer **Possess**. Possessed, it takes on that entity's vitals
   (hearts, hunger) and inventory. Unpossessed it is always full.
3. **The xelector is scaffolding** for the same behavior and the two should
   converge ("in the future xelector and cursword will be very similar").
4. **A shared inventory HUD** like Minecraft's, with hearts and hunger,
   above the taskbar on both screens at all times. Contents differ per
   entity. **Place** pops the top item of the active entity's inventory.
   **Take** moves a target into the active entity's inventory.

## 2. What already exists (read, not assumed)

| Piece | Where | State |
|---|---|---|
| Possession in pc-hq | `bv_menu_input.c` ~460-810; `xelector_01/state.txt` (`possessed_id`, `last_possessed_id`, `pre_possess_x/y/z`) | Real, hero-only in practice. Enter possesses the entity under the xelector if its `piece.pdl` has `possessable` (default 1). `9` releases, unless `de_possessible`. Reverse-jump to the last possessed. |
| Origin of that | mutaclysm `ops/choice.c` `possessed_id` | Hero-only toggle. Its comment: "items, monsters - never possessable today". |
| Possession-to-methods design | `@.apps/PORTABLE_ENTITY_ARCHITECTURE.md` section 4 | Designed, explicitly not built. Host sets `active_target_id`; `${piece_methods}` then shows that entity's METHOD rows. |
| Possession constraints | `@.apps/piececraft-xyz/civ-vs-piece.md` 6a/6b, `phase2-plan.md` | Designed: jump/mine/build apply only to the possessed entity. |
| Cursword on the desk | `khtpm_entity.c` (`g_is_cursword`, `g_cursword_armed`), `#.desktop/cursword_armed.txt` | Real. Click arms it; arrows move it on the grid; camera keys; Esc disarms. Display-wide `XGrabKeyboard` while armed. |
| Cursword menu | `pals/cursword/menu.chtpm` | Dir, Inventory, Chat, Bookmarks, Play, Act, Cli-io, Close, Cancel. **No Possess.** |
| Vitals | `hero_01/state.txt` `hp=20`; cursword `stats.pdl` `HP hp 38 / mhp 38` | Two formats. No hunger or hearts anywhere on desk entities. Hunger exists only in design docs and the avatar record. |
| Inventory | `<entity>/inventory/` dir (cursword has 3 items; 8 desk pals have the dir) | Real on the desk (drag and drop is a `mv`). The pc-hq hero has none. |

## 3. Gaps

- Possession is wired to the xelector and `hero_01` only. Nothing retargets
  menus, verbs or a HUD when something is possessed.
- Cursword is not present on pc-hq-only levels, so there is no cursor with
  desk behavior there.
- No uniform vitals. Nothing to draw hearts or hunger from.
- The desk has no concept of possession at all.
- Cursword's menu is static (generated from `meta.pdl`). A hover-dependent
  "Possess" row is a new kind of item.

## 4. Proposed shape

Follows [[desk-is-the-functional-parent]]: the desk (cursword) is the
reference; shared logic is text-included from `&.widgits/_shared-lib/`.

1. **Name the concept: possessor.** Cursword and the xelector are both
   possessors. `possessed_id` says what each currently drives. Empty means
   it drives itself.
2. **`khtpm_possess.c` (shared, `psx_*`)**, same style as
   `khtpm_page_rows.c` and `khtpm_move_range.c`: pure file I/O, no drawing.
   - `psx_effective(possessor)` returns the entity the player is acting as
     (the possessed one, else the possessor).
   - `psx_possess/release` set and clear `possessed_id` and record the
     return position (`pre_possess_*` already is that record).
   - `psx_can_possess(entity)` reads `piece.pdl` `possessable`.
3. **Vitals convention.** Hearts from `hp/mhp`, hunger from `hunger`,
   read from `state.txt` kv or `stats.pdl`, whichever the entity has.
   A possessor with no target reports full. Display only at first;
   decay and damage are a later, separate decision.
4. **Cursword in pc-hq levels.** Add it as an entity (like the xelector
   bottom-bar entry in `XELECTOR-ENTITY.md`), with desk-equivalent behavior:
   arm, arrows, camera keys, Esc. The xelector's role shrinks to what
   cursword does not cover, and the two merge later.
5. **"Possess" in the menu.** When the cursword's cell holds a possessable
   entity, the menu gets a Possess row. Needs either a generated menu that
   is rebuilt on open or a conditional row; the desk menu is generated
   today, so the second is new work. **(inferred)**
6. **Inventory HUD.** One data-driven khtpm panel above the taskbar on both
   screens, reading `psx_effective()`'s `inventory/` and vitals. The same
   panel everywhere; only the data changes.
7. **Place and Take** (rewrites roadmap R5/R6 in `MOVE-AND-TAKE.md`):
   - **Take**: `mv` target into the effective entity's `inventory/`, plus a
     ledger line. Same operation as the desk's drag and drop, so no new
     storage format.
   - **Place**: the Move range flow, but on confirm pop the top inventory
     item onto the chosen cell instead of moving the entity.
   - "Top item" means the selected slot. Hotbar cycling is open, see 5.

## 5. Owner decisions (2026-10-05) and what is still open

Decided:
1. **Vitals are swapped.** Possessing an entity swaps its hearts and hunger
   in; cursword's own stay as they were (always full when unpossessing).
2. **Desk shows only cursword's menu.** Possession does not swap in the
   entity's own menu. **Possess** is one row of cursword's menu.
3. **Keys, the same on the desk and in pc-hq:**
   - **Enter** with cursword over an entity: possess it automatically.
     (pc-hq already does this for the xelector: Enter possesses the entity
     under it. The desk would match it.)
   - **Shift**: move to the next entity in the list. The bottom taskbar's
     nav follows that choice, so Enter from there opens that entity's
     context menu.
   - **Backspace** (or similar): stop possessing and be the sword again.
     Checked: it clashes with nothing in cursword's armed handler, and in
     pc-hq Backspace only edits a typed cell ref, so it is free whenever no
     ref is being typed. pc-hq's current `9` release can stay as an alias.

Resolved by the owner (2026-10-05, later): **Enter is one progression, driven
by the existing `click_two_step` setting** (`#.desktop/hq_ui.pdl`, house-wide,
toggled in Settings; 1 = first click focuses, second activates; 0 = one click
activates; the bottom bar and menu rows already work this way).
- **Possess is the "focus" step; opening the entity's context menu is the
  "activate" step.** "Focused" is simply `possessed_id == selected entity`,
  so no extra state is needed.
- **2-step:** Enter once possesses; Enter again opens the entity's menu.
  **1-step:** one Enter possesses and opens the menu.
- **Shift+Left / Shift+Right** moves the sword to the previous / next entity
  in **bottom-bar order** (a modifier plus arrow, not a Shift tap); the bar's
  focus follows. (That answers "which list": the bar order. It also removes
  the earlier worry about Shift key repeat.)
- It works on the desk too. The sword drives everything while armed (it
  holds the keyboard), so it calls the same activation helper the bar uses.
- **Build:** one shared helper in `khtpm_possess.c`, `psx_activate(entity,
  two_step)`: not possessed -> possess (and, if 1-step, open the menu);
  already possessed -> open the menu. The step setting is read through one
  shared reader (three private copies exist today in `khtpm_entity.c`,
  `khtpm_strip_parser.c`, `khtpm_core_render.c`; new code should not add a
  fourth, and the three are not refactored here).
- **Depends on `BUG-CURSWORD-ARMED-MENU-KEYS.md`:** the second Enter opens a
  menu from the armed sword, so the armed-branch-before-popup bug must be
  fixed first, and opening an entity menu must hand the keyboard grab to that
  menu. How the desk launches an entity's menu from the sword is not yet
  traced.
- Open: should the Shift teleport save the sword's position (overwrite
  `desktop_pos.txt`)? Assumed no.

Decided later the same day:
4. **Hotbar, modeled on Minecraft.** A fixed row of slots over the active
   entity's `inventory/`, exactly one selected; Place uses the selected slot
   (an empty slot does nothing). Slot 1 is selected by default, which is
   "top item". Our items are whole entities, so no stacking at first (one
   entity per slot). Cycling keys are not chosen yet: previous/next slot
   keys, plus digits for a direct pick only where they do not clash with the
   typed cell-ref input of the Move range finder (letters and digits type a
   ref while it is open). The selected slot is stored per entity so each
   entity remembers its own.
5. **Hearts and hunger are display-only now, but build the hooks.** Entities
   will soon get real hunger and health. So the vitals reader in
   `khtpm_possess.c` should be the single place that knows where each value
   lives and carry comments marking the future write side: a `vitals_set`
   hook (damage, eating, decay tick) that does nothing yet, a
   `ledger` line type reserved for vital changes, and a note that cursword's
   "always full" is a rule in that one reader, not a stored value. Comments
   should say what each hook is for and point back to this doc.
6. **Take removes the entity from the screen and puts it in the taker's
   inventory.** It still exists, as an item in `inventory/`; it is not
   deleted. On the desk that is the same `mv` drag and drop already does.
   Not yet checked: that moving the directory is enough to make the desktop
   window disappear (the entity's own process may need to be told to close)
   and what happens to its DESK row in a pc-hq page file (the row must stop
   drawing the entity, and Place must be able to put it back with its old
   state). Both need a look before Take is built.

Still open: the Shift+arrow teleport position-saving question above.

## 5a. Reminder: revisit later (owner, 2026-10-05)

- **"An armed cursword seems like a bug, but I'm not sure."** Deferred on
  purpose, to be raised again. What is known (see
  `BUG-CURSWORD-ARMED-MENU-KEYS.md`): arming is a click toggle that takes a
  display-wide keyboard grab, by design (2026-08-30 "stingy focus" request),
  and that grab is what breaks the menu keys. Whether the armed state itself
  (not just the menu clash) is wrong is the owner's open question; do not
  assume either way.

## 5b. Status: inventory data layer built (2026-10-05)

Built and tested on a throwaway house (not wired to any menu, key or HUD yet):
- `&.widgits/_shared-lib/khtpm_inventory.c` (`inv_*`): list (alphabetical =
  slot order), selected slot (`<entity>/inventory_slot.txt`), Take, Place,
  stop-a-running-desk-entity (pid checked against /proc, so a stale pid file
  cannot signal an unrelated process), ledger line, and `inv_project` - the
  plain-text feed the visual hotbar renders.
- `khtpm_page_rows.c`: new `pgr_remove_row` / `pgr_append_row`.
- `&.widgits/entity-cli/ops/inventory_op.c`: one CLI both environments call
  (`list | slot | project | take | place`).
- Take keeps the entity's page row inside the item (`taken_row.txt`), so Place
  restores its glyph; Place picks a fresh page index.
- Tested: take, list, slot step, projection, place by slot and by name, name
  collision (`ember_2`), refusing self/parent/missing targets, empty inventory,
  no page file, and an unrelated live pid left untouched.
- Not done: wiring (menu rows, Enter/Shift keys, pc-hq pieces rows), the HUD
  panel, hearts/hunger. The desk `--spawn` path (launching a placed entity) is
  written but was not exercised live. Non-directory files in an inventory
  folder are ignored by the hotbar (it lists entities only).

## 5c. The hotbar must follow the bottom taskbar as it grows (owner, 2026-10-05)

Read in `khtpm_core_render.c`: the desk's bottom dock is built from rows of
`DOCK_BAR_H` (36 px, UI-scaled by `font_scale`). The `-` / `+` buttons send
`PAGEROW:-1` / `PAGEROW:+1`, which change `g_dock_visible_rows` between 1 and
the number of packed rows. So the dock's height changes at runtime, and also
whenever the UI scale changes. **Nothing publishes that height today** (no
geometry file), so a hotbar placed at a fixed y would overlap the dock or float
above it as soon as a row is added.

Decision: do not hardcode a y. Two options considered:
1. **Dock publishes its geometry; the hotbar anchors to it (chosen).** One small
   projection, e.g. `#.desktop/dock_geom.txt` (`x`, `y`, `w`, `h`, `bar_h`,
   `rows`, `visible_rows`), rewritten with tmp+rename at relayout and signalled
   with an append-only marker (house rule: marker file, not mtime). The hotbar
   sits at `dock_y - hotbar_h` and re-anchors on change. Any consumer can use
   the same feed (desk hotbar now; others later).
2. Make the hotbar a row of the dock's own layout (like the cli_io rows added
   above/below the bar). It would move for free, but welds the hotbar to the
   dock window and does not help pc-hq, whose board window has its own bottom
   bar.

pc-hq draws inside its own window, so there the hotbar anchors to that window's
own bottom bar; it needs the same feed shape from the board viewer. Not yet
checked: how the dock window itself is positioned (anchored to the screen
bottom and grown upward is assumed, not verified).

## 5d. Hotbar gets a nav number; digit echo on both bars and pc-hq (owner, 2026-10-05)

Three pieces, to be done together because the hotbar needs all of them:
1. **Dock geometry feed** (5c).
2. **The hotbar is a numbered nav item.** Nav numbers are unified across every
   khtpm window (entities first, then HQ windows; see `khtpm_strip_bottom.xhtpm`),
   so the hotbar takes a number like any other cell, and a new window must not
   renumber what already exists. Typing its digits then Enter focuses it; its
   slots are chosen with the slot keys.
3. **Digit echo: typed nav digits shown next to the `^` focus mark, accumulating
   in the same spot, on the top bar, the bottom bar, and pc-hq windows.**

**Confirmed by frame dump, 2026-10-05** (digit `1` typed through
`#.desktop/strip_history.txt`; `strip_state.txt` showed `digit_buf=1`,
`nav_armed=1`; PNGs of the top bar `0xc00003` and bottom bar `0xc00006`, both
2096x45): **neither bar echoes the typed digits.** The nav box at the far left
of each bar shows only a lone `.` (the `^` focus mark when the bar has real
focus). The cursor `[>]` moves live as digits arrive, so the accumulator works;
only the display is missing. The echo code that does exist (`khtpm_strip_parser.c`
~2012-2025, `^[<digits>]` / `^[NAV]`) is on the HQ-menu box path, not these two
bars.
- **One shared buffer.** `digit_buf` lives in the manager's state, so the top
  bar, the bottom bar and any third surface can all show the same text at the
  same time, in the same spot (the `.` / `^` box), accumulating. No second
  buffer is needed.
- **Numbering is unified across both bars:** top bar cells are 1-16 (the last
  being the clock), bottom bar continues at 17 (cursword) through 22
  (door_civ). A new cell, such as a minimized hotbar, takes the next number
  after the existing entities and must not renumber any of them.
- pc-hq windows (the board window) were not open during the dump, so that
  surface is not yet checked; it needs the same echo, reading the same buffer.

Open: one shared echo formatter (text-include) used by the strip parser and
`khtpm_core_render`, so the three surfaces cannot drift apart.

## 5e. The hotbar is a real window with chrome, minimizable into the bottom bar (owner, 2026-10-05)

The hotbar gets the standard window chrome and can be minimized into the bottom
taskbar. So it should be built **as an ordinary HQ window** (a `khtpm_core_render`
window drawing the `inv_project` feed), not a custom overlay: HQ windows already
register in the nav-tab registry, take a nav number, and minimize to a bottom-bar
cell. That gives the hotbar its nav number and its minimize behavior from the
existing mechanism, with no new code for either.

Consequences:
- While visible it docks just above the bottom bar and follows the dock's growth
  (5c). While minimized it takes no space above the bar; its geometry feed
  reports that, and its bottom-bar cell carries its nav number.
- Open: is the visible hotbar locked to the dock edge, or draggable like other
  HQ windows? Assumed locked while docked.

## 5f. Owner answers and the layout direction (2026-10-05)

- The hotbar looks like the taskbar but **has window chrome**. It is **locked
  to the dock** (not draggable for now). The dock is **pinned to the screen
  bottom and grows upward**, so the hotbar's anchor is the dock's top edge
  (the earlier "assumed, not verified" is now the owner's statement).
- The nav box shows the focus mark and the accumulating index (`.`/`^` plus the
  typed digits). The owner wants this driven by **layout going forward**, and
  accepts tweaking the renderer/parser to get there.

Where that stands in the code (read, 2026-10-05): the dock is hand-packed in C,
not laid out by the generic flex/panel engine, and the dock migration
(`08-roadmap/design-docs/DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md`) has phase 1 done
and phases 2 (replace the `+`/`-` row pager with the generic scrollbar) and 3
(delete the dead constants) not started. That doc also says the focus box and
the peer-window split are separate, already-hardened mechanisms to leave alone
for now.

Approach that stays compatible with that migration:
1. **Variable first, drawing second.** One shared formatter produces the nav
   echo string (focus mark + `[digits]`) from `digit_buf` and real-focus. Today
   the C focus box draws it; once the nav box is a layout element, the same
   string is just the value bound to it (e.g. `${nav_echo}`), with no logic
   moved twice. The same string feeds the top bar, the bottom bar and pc-hq.
2. **The hotbar is layout from day one** (a chtpm over the `inv_project` feed),
   so none of it becomes new hardcoded dock C.
3. **Publish dock geometry from the laid-out result**, not from
   `DOCK_BAR_H * rows` arithmetic, so the feed stays correct when the migration
   replaces the pager.
Check when building: the focus box is a fixed width; confirm with a frame dump
that `^[123]`-style text fits before relying on it.

## 6. Order of work

1. This doc (done). 2. `khtpm_possess.c` plus unit harness, no UI.
3. Cursword entity in pc-hq levels. 4. Possess menu row, desk and pc-hq.
5. Vitals reader and HUD. 6. Take. 7. Place.

Related: `MOVE-AND-TAKE.md`, `XELECTOR-ENTITY.md`,
`BUG-CURSWORD-ARMED-MENU-KEYS.md` (a bug in the control mode this design
builds on; fix it first).
