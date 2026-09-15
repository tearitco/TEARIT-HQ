# DSR (Desk Street Raider) — dev log

Renamed from "Wall Street Raider" to "Desk Street Raider" - a real desk-based
game, not just a name pun.

Desk: `dsr` (new, real — see `sessions/s1/desks/dsr.pdl`).
Roadmap entry: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` §6, §6b.

## The real, deliberate architecture: two independent front doors
1. **Toy** — a real X11-HQ window (nav-numbered `.chtpm` status-screen menu,
   same shape as the original game's own UI), opened via this house's real
   `toy.pdl` convention. Playable with NO desk open at all.
2. **Desk** — the `dsr` desk itself (2 castles/4 banks/8 stores), walkable/
   touchable like `civ-test`. Playable with NO toy window open at all.
3. A real setting on the toy controls whether launching it ALSO auto-opens
   its own desk (defaults on, can be set off).

Both read/write the same real ledger/state - neither is a special case of
the other. See roadmap §6b for the full reasoning (this is meant to be the
proof that the toy/HQ-app convention and the desk/entity convention are
genuinely interchangeable front ends onto one real game, a pattern every
later game with both a spatial AND a quick-check-in presence can copy).

## Status
- Real toy built and live: `&.hq-apps/dsr/` — `toy.pdl`, `button.sh`,
  `dsr.xhtpm` (sidebar+panel window, real chrome, real CSS sizing),
  `dsr.css`, a real compiled manager (`ops/dsr_manager.c`) reading an
  editable `state/dsr_state.pdl` and publishing `state/ui.txt`. Content
  matches the original wsr-pal's own field values/shapes and its real
  33-row action menu (piece.pdl order, minus the fold-trigger row).
- File/Game Options/Settings/Help live in a real `<tabbar>` above the
  sidebar+panel, all real `action="void"` stubs for now.
- **2026-09-14 rollback**: a follow-up pass tried moving Actions into
  a 3-column flex layout (Actions | Live Table | Watchlist) but mixed
  Actions into the SAME `<sidebar>` as the status readout instead of
  giving each its own real region - broke nav, rolled back to the
  known-good sidebar(status)+panel(Actions)+tabbar state per direct
  instruction ("you would be better off rolling back than this").
  Current live version = that rollback + tabbar only, verified via a
  real frame dump.
- **2026-09-14, toys-scan gap found and fixed** (generic, not
  DSR-specific): `&.hq-apps/` (home of db-hq-pal/chat-hai/network/dsr/
  etc - every real HQ app) was never a scanned root for the taskbar's
  Toys menu at all (only house_root, `@.apps/`, `&.widgits/` were) -
  db-hq-pal's own `toy.pdl` had been sitting unscanned the whole time
  too. Fixed in `khtpm_taskbar_manager.c`'s `livedesk_build_toys_menu()`
  - `&.hq-apps` is now a real, fourth scanned root.
- The core ledger-driven economy mechanic (§6) not started.

## Real system dependencies
- Ledger-driven multi-entity economy (this is its real proving ground)
- Menu/status-screen game UI (nav-driven `.chtpm`, no map view)

## Layout plan (documented BEFORE building, 2026-09-14)

Direct instruction: reorganize to match the real Wall Street Raider
8.12 "Trading Desk" screenshot's own grouped-box structure (Active
Entity Selected / Research Menus and Tools / Transactions / Other /
My Balance Sheet / Financial News Headlines / Commodity Prices /
Quick Search Functions - each its own bordered, titled box), not a
single flat status list + single action list.

**Do we have the real HTML-like primitives for this? Yes - checked
the code, not guessed.** `khtpm_core_render.c`'s generic flex engine
(`css_layout_pass`, real CSS `display:flex`/`flex-direction`/
`flex-wrap`/`flex-grow`, already proven live by both `canvas-craft.xhtpm`
and this project's own earlier 3-column attempt) already supports
everything needed:
- `display:flex` + `flex-wrap:wrap` on the `<page>` lets multiple
  fixed-width boxes wrap into new rows automatically - this alone
  reproduces the screenshot's own multi-row grid of boxes, no new
  "row" grouping tag needed.
- Each grouped box = its own `<panel class="...">` (the renderer
  already supports MANY `<panel>` tags as siblings under a flex page,
  confirmed live - `assign_nav_and_layout()`'s own `page_is_flex` loop
  lays out and content-fills every one independently).
- Each box's own title ("My Balance Sheet:", "Commodity Prices/
  Indexes/Indicators:") = a plain `<text class="block-title">` as the
  panel's first child - the same real "legend" role HTML's own
  `<fieldset>+<legend>` plays, already used this way in the current
  DSR window and in db-hq-pal.
- **No renderer/parser change needed for this layout.** If a genuine
  gap turns up while actually building it (something the screenshot
  needs that flex+panel+text truly can't express), that's the trigger
  to add a new generic primitive - not before, and only if it's a
  real, obviously-HTML-standard one (matches this house's own "zero
  new per-project C, generic tag vocabulary only" rule).

**Real, learned caution before attempting this again**: the 2026-09-14
rollback happened specifically because Actions and the status readout
were crammed into ONE `<sidebar>` instead of each having its own real
region. Next attempt: give EVERY grouped box (Active Entity, Research
Menus, Transactions, Other, Balance Sheet, News, Commodity Prices,
Quick Search, Actions) its own real `<panel>`, verify the flex-wrap
row-breaks visually via a real frame dump BEFORE declaring it done,
and don't reuse rollback's sidebar+single-panel shape as a shortcut.

## City formation - built, 2026-09-14

Direct instruction, resolving `DESK-CITY-FORMATION-DISPLAY.md`'s own
open questions for DSR specifically: 14 real buildings now live on the
`dsr` desk, one real pal (own atlas/sprite/glyph) per building, real
`DESK` rows in `dsr.pdl` - not a legend/overview, real interactive
entities at real positions.

- **2 castles** (🏰 `dsr_castle_a`/`dsr_castle_b`) = the 2 governments,
  one per side.
- **4 banks** (🏦 `dsr_bank_a1/a2`, `dsr_bank_b1/b2`) = 2 per side.
- **8 stores** (🏪 `dsr_store_a1..4`, `dsr_store_b1..4`) = 4 per side.

Layout (grid cell = 80px, matches house convention): each side/city is
a real "row, empty row, row" block - row 0 (gy=0) holds castle+2
banks+1 store (4 cols), row 1 (gy=1) left empty as a real street/
navigation gap, row 2 (gy=2) holds the remaining 3 stores. Left city
at gx 10-13 (x=800-1040), right city at gx 20-23 (x=1600-1840), gx
14-19 (x=1120-1520, ~5 empty columns) left as the real gap between the
two territories - "a space in between them showing that they clearly
own either side of the screen."

Verified live: all 14 real X11 windows confirmed via `xwininfo -root
-tree` at their exact expected coordinates, named `tile:<name>-<iid>:
<glyph>`; a direct frame dump of `dsr_castle_a`'s own window confirmed
the real 🏰 sprite renders correctly. (A `scrot` full-desktop capture
came back blank for this same region - a known, already-documented
house pitfall, `03-pitfalls/HOUSE_CODE_PITFALLS.md` #4, "external
screenshot capture is unreliable" - the per-window `dump_frame_png_op`
dumps are the real, trusted proof here, not the scrot.)

**Real, deliberate simplification**: every building is currently a
simple deskpal (Events(hq)/Dir/Close/Cancel stubs only, same shape as
`book-stack`) - no real government/bank/store MECHANIC exists yet
(rate-setting, loans, etc. per roadmap §6's own ledger-driven economy
plan). This pass is "buildings in place," not "buildings that do
anything," matching the same display-first sequencing the toy itself
already followed.

**Population buildings added, 2026-09-14** - direct instruction: "add
🏨 on each side in the missing spot (where the population bank
lives)." The bottom row (gy=3) only had 3 of its 4 column slots filled
(3 remaining stores after the top row took 1) - `dsr_population_a`
(gx=13,gy=3 / x=1040,y=240) and `dsr_population_b` (gx=23,gy=3 /
x=1840,y=240) fill that real 4th slot on each side, same simple-deskpal
shape as every other building. Verified live: both real windows
confirmed via `xwininfo` at the exact expected coordinates, a direct
frame dump of `dsr_population_a` confirmed the real 🏨 sprite renders
correctly. 16 real buildings total now (was 14).

## Toy -> desk auto-open - built, 2026-09-14

Direct instruction: "the dsr desk should be opening when we open dsr
x11-hq widgit." `button.sh` now calls the real `mr_transfer_desk` op
(same mechanism `door_civ`'s own touch-trigger uses) right after
launching the toy's own window+manager, switching the active desk to
`dsr` so its buildings are live and visible. Real, honest toggle:
`state/open_desk.state.txt` (`mode=on|off`, same shape
`khtpm_play_mode.state.txt` already uses) - missing file defaults to
on. Verified live: `session.pdl`'s own `active_desk` flipped to `dsr`
and all 14 buildings came up as real processes on a real
`button.sh` invocation.

## Grouped-box layout - built, 2026-09-15

Direct instruction: "can we go back to the paneling for the dsr toy...
lets get back on track" - resumed the "Layout plan" section above.
5 real, separate status boxes now (Active Entity Selected/Your
Wallet/My Balance Sheet/Financial News Headlines/World Status), each
its own `<panel class="dsr-box">` (one is a real `<sidebar>` instead -
see below), wrapped via `.dsr-grid { display:flex; flex-wrap:wrap }`
on the page - confirmed live via a real frame dump, no overlap, real
nav/chrome intact.

Two real bugs hit and fixed live, same pass:
- **Window went empty/tiny** - `layout_sidebar_panel()`'s own gate
  requires BOTH a real `<sidebar>` AND a real `<panel>` present; the
  first attempt made every box a `<panel>` and had none left tagged
  `<sidebar>` at all. Fixed: "Active Entity Selected" is a real
  `<sidebar>` tag (same `.dsr-box` class, visually identical) - matches
  canvas-craft's own proven shape (exactly one sidebar + N panel
  siblings, sidebar not optional).
- **Box content overflowed into the row below** - `css_layout_pass()`
  sizes each flex box from its own CSS width/height BEFORE that box's
  own children are laid out in a later pass; it does not auto-grow
  height from content. Without an explicit `.dsr-box` height, every
  box came out of the flex pass too short, so a box's 2nd/3rd line
  painted past its own box into the next row. Fixed with an explicit
  shared height (130px, sized for the tallest real box content, My
  Balance Sheet's 3 lines) - confirmed by reading the real frame-file
  x/y dump before guessing at a fix.

Two real, honest scope decisions this pass (not silently done):
- "Commodity Prices" (the WSR screenshot's own 7th box) has no real
  data source - `population`/`temperature` fill that slot under their
  own honest "World Status" title instead.
- "Research Menus and Tools"/"Transactions"/"Other"/"Quick Search
  Functions" (the screenshot's own action-CATEGORY boxes) not split
  out yet - Actions stays one flat `<panel class="dsr-actions">`
  scrolllist, its own full-width row below the status boxes. This is
  the resolution (for now) of the open question below.

## Ideas / synergies (running notes, not commitments)
-

## Open questions
- ~~Where does "Actions" sit relative to the grouped boxes above~~ -
  resolved 2026-09-15: its own dedicated full-width scrolling panel
  below the grid, not itself a wrap-grid box.
- Next real step, direct instruction ("now lets fill in the op buttons
  by category"): split `dsr_manager.c`'s own flat `ACTION_LABELS[]`
  into named categories (Research Menus and Tools/Transactions/Other/
  Quick Search Functions, matching the WSR screenshot's own remaining
  4 boxes) and give each its own real `<panel class="dsr-box">` in the
  wrap-grid, same shape as the status boxes above - real, cheap,
  data-only work per this project's own "Layout plan" note (no
  renderer change needed), not yet started.
- The `open_desk.state.txt` toggle has no real UI to flip it yet (no
  menu row calls it) - real file-based toggle exists, real switch
  doesn't yet.
