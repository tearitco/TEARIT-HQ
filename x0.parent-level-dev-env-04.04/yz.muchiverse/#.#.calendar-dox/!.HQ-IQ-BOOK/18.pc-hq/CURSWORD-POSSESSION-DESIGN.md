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

Still open:
- **What is "the list" Shift walks?** The entities on cursword's cell, the
  entities in range, or every entity on the page (the order of the bottom
  bar)? I would use the bottom-bar order, since that is where the nav
  already lives.
- **Enter does two jobs:** possess (sword over an entity) and open a menu
  (focus on the taskbar). I read it as: the focus decides, and Shift moves
  the focus to the taskbar entry. Confirm.
- **Is Shift a tap or Shift+arrow?** A tap is simplest, but a held Shift
  also generates its own key event, so key repeat would cycle fast. I would
  trigger on press only, no repeat. Confirm.

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

Still open from before: the three items under "Still open" above (which
list Shift walks, how Enter chooses between possess and open-menu, and Shift
as press-only).

## 6. Order of work

1. This doc (done). 2. `khtpm_possess.c` plus unit harness, no UI.
3. Cursword entity in pc-hq levels. 4. Possess menu row, desk and pc-hq.
5. Vitals reader and HUD. 6. Take. 7. Place.

Related: `MOVE-AND-TAKE.md`, `XELECTOR-ENTITY.md`,
`BUG-CURSWORD-ARMED-MENU-KEYS.md` (a bug in the control mode this design
builds on; fix it first).
