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

## Ideas / synergies (running notes, not commitments)
-

## Open questions
- Where does "Actions" sit relative to the grouped boxes above - is it
  itself one more box in the wrap-grid, or does it stay a dedicated
  scrolling region below/beside the grid? Not yet decided.
