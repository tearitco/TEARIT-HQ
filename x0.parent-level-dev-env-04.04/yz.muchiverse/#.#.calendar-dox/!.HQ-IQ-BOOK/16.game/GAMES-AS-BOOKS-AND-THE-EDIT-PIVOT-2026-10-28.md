# Games as BOOKS (BOOK:PAGE), the ship as an ENTITY, and the edit pivot (design, 2026-10-28)

Written after the owner said: **"all those pet evolution plans to hand to Grok sound cute, but if we don't have BOOK:PAGEs none of that matters, because the games are unshippable / unplayable and non compliant."** This is the pivot. Labels: 📖 **READ** (a file I read today), 📝 **PROPOSED**, ❓ **OPEN** (owner decides), ⚠️ **NOT VERIFIED**. The pet/food/terumon work is **backburned** (section 8), not dropped.

## 0. The owner's words, kept close (2026-10-28)
* rpg-edit and rpg-space / rpg-dust **still have no BOOK:PAGE**. They have "pages" for rooms, which is fine, but they are really **a book**: rpg-space / rpg-dust should be **openable and editable in rpg-edit**.
* The **ship rooms** (the ones you teleport to), the **planets**, and even **blank space** are all different **maps** (pages). Obviously the whole ship teleports into a planet map (because there is empty space).
* Therefore **the ship itself is not a map, it is an entity** (which the owner does not see on the bottom toolbar yet).
* **The plan:** all of these load through edit (the burgeoning game editor engine) and can be **exported as a game binary**. "This is what we should pivot to and start documenting."
* Then probably **"beam down"** on the helm: an entity beams down to the nearest planet by picking a position on the map.
* Flexible backlog, to be sussed out as we go: **stock trading** (anywhere, through the phone), **school** (go on time to get better grades), **quests** ("rpg maker version of EVE Online", ask Google / Grok), and the parities **Grok** will do: RPG Maker MV first, then Minecraft, then CDDA / GTA / DF.
* Why it matters (owner): some of this will solidify so that TEARIT is **a game / platform, not just an engine**, "because that's what people want and sells, and it helps development".

## 1. What exists today (📖 READ 2026-10-28)
1. **BOOK and PAGE are already a house concept.** `&.widgits/book-page/ops/book_page_op.c`: a **BOOK** is a livedesk session `<sessions_root>/s<N>/` with `session.pdl` (`STATE | name | <book>`, `STATE | active_desk | <page>`) and `desks/`; a **PAGE** is `desks/<page>.pdl`. The op creates, lists, checks and renames them (`book:page` is its address form, e.g. `pre-design:eden-test`), with a locked harness (`book_page.pal`). The pet-trainer header already lists "book:page" as a header item (`@.apps/pet-trainer/ROADMAP.md`).
2. **A pc-hq board is bound to a page, or owns its own.** `bv_render_2d.c` documents `pieces/display/open_book_page.txt`: `source=desk` = the page is a livedesk desk file (`pdl=`), `source=board` = **the board's own map, no desk file**. 📖 rpg-space's pc-hq package says `source=board`: **it is not a citizen of the book/page system**, which is the owner's observation exactly.
3. **rpg-space already has a book-shaped folder, just not registered as a book.** `rpg-space-pchq/pieces/system/maps/rpg-space/` holds `game.pdl` (`GAME | type | board-game`, `n_desks 7`, `desk_N_id/label`) and one folder per room (`bridge`, `quarters`, `hold`, `engine`, `region`, `area`, `space`), each with `map.txt`, `cells.txt/.rgba/.png`, `events.txt`, `extrusion.pdl`, `hotspots.txt`, `lights.pdl`, `bar.txt`. So **pages exist as maps**; what is missing is the **BOOK:PAGE identity, header, and editor binding**.
4. **rpg-edit-hq is not an editor of other games today.** 📖 It is a **petslike skin like the others** (own `catalog/rooms/party/verbs...`, own `rpg-edit-hq-pchq`), and its `maps/rpg-edit-hq/game.pdl` is labelled `RPG-Pet` with six desks (`start`, `bank`, `store`, `hotel`, ...): a game with its own maps, not a tool that opens a book. ⚠️ I read 16 lines of that file and the folder list, not its code.
5. **The ship is already a piece, but not in the entity bar.** 📖 `rpg-space-pchq/pieces/ship_01/model.txt` exists (a vehicle piece), `state/ship.txt` holds its position, and the skin treats the land as an entity too (`pieces/land_01`, owner 2026-10-24 "ship and land are entities"). In the live window's bottom bar (screenshot today) I saw `Solvent`, `Wren (skiff)`, `rock`, `ore`, `crystal`: **the ship itself is not listed**. Meanwhile the **ship rooms are maps** (desks), and the planet is painted under the bridge desk as a backdrop (`sk_build_ship`), so the model is **mixed**: hull = entity, interior = maps, planet = backdrop copy.
6. **The packaging design exists.** `GAMES-AS-PACKAGES-AND-STORE-DESIGN.md` (`game.pdl` parts and slots, packs, `pack_check`, Store, screening) and `VISION-CLARIFIED-2026-10-27.md` ("one space with several small games, to prove that one central editor works"). The pivot should **land on those**, not invent a second package format.

## 2. The model (📝 PROPOSED; one thin layer over what exists, no conversion of the maps)
* **Book = a game** (id `rpg-space`, `rpg-dust`, `rpg-edit`...). **Page = a map** inside it, addressed **`<book>:<page>`**: `rpg-space:quarters`, `rpg-space:region`, `rpg-space:space`, `rpg-dust:region`. A page has a **kind**: `interior` (belongs to an entity, reached by a door/teleport: the ship rooms), `surface` (a planet page), `space` (blank space).
* **Entity = a placed thing with identity**, a position on a page, and optionally **`interior=<page,page,...>`**. The **ship is an entity** (`kind=ship`, `hull=pieces/ship_01`, `interior=bridge,quarters,hold,engine`) placed on a surface or space page. Its rooms are pages **it owns**; the door from a surface page into the ship is a teleport to `rpg-space:bridge`. "The whole ship teleports into a planet map" = **the ship entity's `at=` changes page**, nothing is copied under the bridge. (The painted-backdrop copy then becomes a *view* of the page the ship stands on, not a second source of truth: ❓ see 7.3.)
* **Rows, not code** (house rule): a `book.pdl` per game:
```
BOOK   | rpg-space | RPG Space | maps=pieces/system/maps/rpg-space
PAGE   | bridge   | Bridge   | kind=interior | owner=ship_1
PAGE   | region   | Region   | kind=surface
PAGE   | space    | Space    | kind=space
ENTITY | ship_1   | Ship     | kind=ship | hull=pieces/ship_01 | interior=bridge,quarters,hold,engine | at=space:5,5
```
  It is **generated from / checked against** the existing `game.pdl` and the map folders, so nothing is hand-duplicated. `tree_lint` (BUILT, 53/0) already validates `NODE` rows with `parent=` / `needs=`: an entity's interior pages and a page's owner are exactly such edges, so the same linter can be reused.
* **The header shows BOOK:PAGE from data** (`rpg-space:quarters`) in every game that has a `book.pdl`: no per-game renderer code (the khtpm house rule). 📖 The 3D debug overlay already prints `rpg-space:quarters`, so the engine knows the pair; the header is the missing display.
* **rpg-edit opens a book**, not "is a game": list the book's pages, show a page, then edit (cells, events, hotspots, entities) through ops that write the map files with a ledger row. **Export** = package the book + the pinned engine binaries + a launcher (design 16.game games-as-packages), and prove it by running the same playthrough gate on the exported copy.

## 3. Why "unshippable" today, in checkable terms (📝 the compliance gate)
A game is **BOOK:PAGE compliant** when `book_check <game>` passes: (a) it has a `book.pdl`; (b) every `PAGE` has a map folder with the required files; (c) every `ENTITY` `at=` names an existing page and every `interior=` page exists and names that entity as owner; (d) page ids are unique and valid names (the `book_page_op` rule: letters, digits, `-`, `_`, 1..48); (e) no map folder exists that is not a page; (f) the header shows `book:page` (a state-file check, not a screenshot). Until that passes for rpg-space, rpg-dust and rpg-edit, nothing built on top is shippable.

## 4. Build order, each step with a test that can fail (📝)
1. **S0, inventory (read-only, cheap, next):** the real desk-file format of a livedesk page versus a `maps/<game>/<room>` folder; how `open_book_page.txt source=board` is consumed; how rpg-edit-hq loads maps; what the bottom bar reads to list entities. Output: a short table, no code.
2. **S1, `book.pdl` + `book_check` op + harness** for rpg-space (then dust, then edit): fixtures with deliberate faults (missing map, duplicate page, entity at a missing page, interior owned by nobody, orphan map folder); mutations as always; **the check counts must rise when checks are added** (lesson of 2026-10-28).
3. **S2, the header** shows `book:page` from `book.pdl` in rpg-space, rpg-dust, rpg-edit (data row + the generic renderer path); proof: a state-file check plus a window picture.
4. **S3, the ship becomes an entity row** and appears in the bottom bar; ship rooms become `owner=ship_1` pages; the planet backdrop copy is derived from the page the ship stands on. Proof: the existing gates (planet map 101/0, fridge 29/0, helm) still pass.
5. **S4, rpg-edit opens a book:** read-only page list and picture first; then edits with a ledger; round-trip test (open, save unchanged, bytes identical).
6. **S5, export:** a book becomes a package that runs from a scratch house; gate = the same playthrough on the export.
7. **S6, beam down:** a helm verb that moves a crew entity from the ship interior to a chosen cell on the surface page under the ship, one ledger row, refused when there is no surface page. (Owner: "probably".)
Each slice ends with the owner's rule: a before/after picture and a click path they can see.

## 5. Backlog from the owner's note (flexible, not scheduled)
* **School:** go on time, better grades: 📖 the pet-trainer curriculum / `entity_grade` / skill-book machinery exists; needs a clock-driven "go to school" event page and a place (a page).
* **Stock trading through the phone:** 📖 the WSR economy engine ran in scratch (2026-10-10 exploration) and every pet has a phone on its own phone server; the order-book app on the phone is the missing piece.
* **Quests, "RPG Maker version of EVE Online":** ❓ ask Google / Grok for the quest-system parity list; keep as data rows (`QUEST` pages), nothing hard-coded.
* **Grok's parities:** RPG Maker MV first (the event/page/switch/variable gaps in the companion brief, Block B), then Minecraft (voxel feel, Block D play-test), then CDDA / GTA / DF.

## 6. What this changes about earlier plans
* The **terumon / research / visuals** designs stay valid; their forms and models become **entities and pages in a book** (a form's model lives in the book's assets). Grok's handoff should say **"build inside a book"** so nothing they make is orphaned.
* The **fridge** is a page object in `rpg-space:quarters`; its rows (`foods.pdl`) become a book asset. No rework needed.
* The **"central editor with several small games"** (VISION section 6) is this pivot's end state: one rpg-edit, many books.

## 7. Open decisions for the owner (❓)
1. **Which "book" format:** the livedesk session shape (`s<N>/session.pdl` + `desks/*.pdl`) or the pc-hq `maps/<game>/<room>` folder + a thin `book.pdl` manifest (my recommendation: the manifest, no conversion)?
2. **Do interior pages belong to the ship entity** (so a second ship brings its own rooms) or to the book (my recommendation: the entity owns them)?
3. **The planet backdrop under the bridge:** keep it as a derived view (my recommendation) or drop it and show the planet only through the window/helm view?
4. **Is rpg-edit one app that opens any book** (my recommendation, per the vision) or a mode inside each game?
5. **What does "exported as a game binary" mean to you:** a package folder with a launcher, or one compiled executable with the data embedded?
6. **Beam down:** pick the cell on the map (2D/3D pick) or "nearest surface cell with a free standing spot"?

## 8. Backburn decision (my call, 2026-10-28)
Safe to backburn pet evolution and food. The fridge slice is **committed and self-contained** (`27a37faab`; gates 58/0 and 29/0; nothing half-done); the terumon and research work is **documents plus two finished tools** (`views_to_voxel`, `tree_lint`), no code in flight. I do **not** hand it to a subagent: nothing is mid-air to hand over, a cold agent would re-derive all of it, and the pivot needs the same context I have. Parked, in order, to resume after S2: the camera-starts-in-the-floor bug (owner's report, cause unconfirmed), the greenhouse, `form_op`, `WHY` rows.

## 9. Not verified
No code was written for this pivot today. I did not read the livedesk desk-file format, rpg-edit-hq's code, the engine's entity-bar source, or how the khtpm header is generated; the `source=board` finding is from one file's two lines and the header doc comment. Whether Dust is also `source=board` was not checked. The claim that the 3D debug overlay prints `book:page` is from a window picture, not from the source.
