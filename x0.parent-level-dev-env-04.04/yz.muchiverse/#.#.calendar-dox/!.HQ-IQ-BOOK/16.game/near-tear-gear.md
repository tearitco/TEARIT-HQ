# 🧰 near-tear-gear — rpg-edit opens every pets-like game

📅 2026-10-28 · written by Claude (Sonnet 5.5) for the owner · reading and measuring only, no game code changed for this document.

Legend: 🏃 RAN today · 📖 READ in a file · 🧠 MINE (my estimate or proposal) · ❓ not known yet · ✅ done · 🟡 partial · ⛔ not built · 🅰️ Road A · 🅱️ Road B


## 🎯 1. What you asked for

In rpg-edit: the **Book dropdown lists all the pets-like projects**, you can load one, its **pages** appear in the Page dropdown, and **the entire game loads in the editor — toolbar, sidebars and all**.

> 📌 **How close are we?** The feature itself is ⛔ **not started** (0 of 11 feature tasks on the near road). But **5 of 6 preconditions are ✅ and the last one is 🟡**, and they are the expensive part. The cheapest road to your picture is **11 tasks (5 S · 6 M · 0 L)**, no renderer changes, and its first sprint needs no live window at all.


## 🔬 2. What I measured today (11 games on the shared core)

| game | template lines | toolbar tabs | pages | event pages | template lines unlike cdda-hq | game-specific C lines | installed as a desk book |
|---|---|---|---|---|---|---|---|
| cdda-hq | 113 | 10 | 7 | 36 | 0 | 473 | ⛔ |
| civ-rpgce | 113 | 10 | 6 | 33 | 0 | 310 | ⛔ |
| df-hq | 113 | 10 | 6 | 36 | 0 | 469 | ⛔ |
| dsr-rpg | 113 | 10 | 6 | 33 | 0 | 240 | ⛔ |
| gta-hq | 113 | 10 | 6 | 33 | 0 | 557 | ⛔ |
| mcl-hq | 113 | 10 | 6 | 37 | 0 | 500 | ⛔ |
| rpg-dust | 132 | 13 | 7 | 14 | 49 | 1745 | ⛔ |
| rpg-edit-hq | 119 | 11 | 6 | 21 | 26 | 395 | ⛔ |
| rpg-pet | 113 | 10 | 3 | 37 | 4 | 223 | ⛔ |
| rpg-space | 140 | 14 | 7 | 15 | 57 | 1823 | ✅ |
| tcg-rpgce | 113 | 10 | 6 | 33 | 0 | 314 | ⛔ |
| **total** |  |  | 66 | 328 |  |  | 1 of 11 |

- 🏃 **7 of 11 window layouts are identical** once the game name is swapped (cdda-hq, civ-rpgce, df-hq, dsr-rpg, gta-hq, mcl-hq, tcg-rpgce). rpg-pet differs by 4 lines, rpg-edit-hq 26, rpg-dust 49, rpg-space 57. ➜ **tabs and sidebars are data, not code.**
- 🏃 Every game has the **same three bars**: a rail sidebar plus a left and a right overlay bar, and a bottom hotbar; the tabs differ only slightly (cdda has `in book page shop menu player time db load loadw`; rpg-space adds `pals air airw debug`).
- 🏃 **All edit verbs are in the shared core** (`place remove newroom newpal delpal save load goto control`), so every game already answers them. ➜ editing is not a per-game feature.
- 🏃 **Today's editor is just another game**: rpg-edit-hq has its own 6-room town. Its **Book dropdown holds 3 fixed rows** (blueprintz, + new pal, - remove last pal, - cancel) and its **Page dropdown lists its own 6 rooms**. 📖 No op in the house lists books (searched `openbook`, `book_list`, `list_books`: none).
- 🏃 **A game binary is hard-wired to its own name**: `PET_APP` is a compile-time define used **24 times on 23 lines** of the 1,695-line core, so one binary cannot be pointed at another game at runtime. ➜ this is what splits the two roads below.
- 🏃 `book_check` says `conformant=no` for **all 11**; the one failing item I could list (A2 `source=board`) is the same everywhere it was itemised (5 of 11).
- 📖 The desk has **8 books** (`rpg-space`, `s1`..`s7`), of which **1** is a game.


## ✅ 3. Preconditions (the gears already turning)

| id | what | state | evidence |
|---|---|---|---|
| P1 | One shared core under all 11 games, and every edit verb (place, remove, newroom, newpal, delpal, save, load, goto, control) is in it | ✅ done | RAN today: all 9 verb names found in petslike_core.c; the built rpg-space and rpg-dust binaries contain newroom / newpal / place |
| P2 | The window layout (toolbar tabs, rail, left/right bars, hotbar) is a data template, and 7 of 11 are identical once the game name is swapped | ✅ done | RAN today: diff of board/pchq-board.xhtpm vs cdda-hq = 0 lines for civ-rpgce, df-hq, dsr-rpg, gta-hq, mcl-hq, tcg-rpgce; 4 for rpg-pet; 26 / 49 / 57 for rpg-edit-hq / rpg-dust / rpg-space |
| P3 | Every game already has pages (rooms) in rooms.pdl: 66 pages over 11 games | ✅ done | RAN today: ROOM rows per game 7 6 6 6 6 6 7 6 3 7 6 |
| P4 | book_check can judge any game: all 11 report conformant=no, and the 5 games I itemised fail only the known 'source=board' item (A2) | ✅ done | RAN today on all 11 (conformant=no for each); the failing item was listed for 5 of 11 (cdda-hq, rpg-edit-hq, rpg-space, tcg-rpgce, rpg-pet) and is A2 in each; the other 6 were not itemised |
| P5 | Games are BOOK:PAGE capable: page file, game_page_export (with other characters as rows), page_follow, taskbar Synch for game boards | ✅ done | RAN this week in scratch houses: harnesses game_page_export 147/0, page_follow 44/0, pc_synch_games 22/0; commits 336385812, b79f820aa, 1b0086ff7. The live taskbar was not pressed |
| P6 | One game (rpg-space) is installed as a book on the owner's desk | 🟡 partial | READ today: desk sessions = rpg-space, s1..s7 (8 books); 1 of 11 games |


## 🛣️ 4. Two roads to your picture

|  | 🅰️ Road A — launch-through | 🅱️ Road B — one host window |
|---|---|---|
| What you see | Pick a game in the editor's Book dropdown → **that game's own window opens** with its own tabs, sidebars, hotbar, pages; edit mode on. | Pick a game → **the same editor window turns into that game** (its tabs and bars swap in place). |
| Why it is cheap / dear | Every game already is a complete window; the editor only needs the **list**, the **hand-over** and edit mode. | Needs the core to stop being hard-wired (`PET_APP` ×24), per-game rule code (240–1,823 lines each) forwarded, ui.txt mirrored, template swapped live. |
| Tasks 🧠 | 11 tasks: 5 S · 6 M · 0 L | 5 tasks: 0 S · 1 M · 4 L (plus Road A's list part A1–A3) |
| Touches guarded files? | No (no renderer / khtpm_core_render change). | Probably (live template swap) — would need your OK first. |
| Biggest unknown ❓ | S1: is a second window launch safe on this machine (the 15:21 freeze). | S3 + S4: the ui.txt gate and live re-pointing are unproven. |
| Does 'entire game incl. tb + sidebars' hold? | ✅ by construction — it **is** the game's window. | 🟡 only if every template/css/toolbar/side_style swap works. |
| My recommendation 🧠 | **Start here.** Sprint 1 needs no live window. | Keep for later, only if one single window matters to you. |

> 💡 🧠 Road A's list work (A1–A3) is **also the first half of Road B**, so choosing A first loses nothing.


## 🗺️ 5. The tasks (data — same rows are in near-tear-gear.json)

| id | road | size | task | needs | proof (must be observable) | lane 🧠 | risk |
|---|---|---|---|---|---|---|---|
| A1 | 🅰️ | S | book_list op: scan @.apps for games (toy.pdl + *-pchq + rooms.pdl), write BOOK rows: id, app, pchq, pages, conformant, on_desk | — | harness: scratch houses with 0, 1, 11 games; live repo gives exactly 11 rows; 6+ mutations fail it | claude | low |
| A2 | 🅰️ | S | page_list for a book: pages from the game's rooms.pdl, cross-checked with its maps/<g>/game.pdl desk ids (book_check B2 logic) | A1 | harness: 11 live games give 66 pages total; a game whose rooms and desks disagree is reported | claude | low |
| A3 | 🅰️ | M | Core publishes the lists to ui.txt (n_book, book_N_label/act, target page list) and gets verbs openbook <id> / openpage <book> <room> that only record state/edit_target.txt | A1, A2 | all 11 games rebuild; per-game verb-script diff gate shows no behaviour change when the toolbar data does not ask for the list; new cases for the lists | claude | medium: shared core touched, 11 binaries |
| A4 | 🅰️ | S | Editor template: Book dropdown repeats the books, Page dropdown shows the chosen book's pages (rpg-edit-hq's own xhtpm and toolbar.pdl only) | A3 | ui.txt says n_book=11; picture of the open Book dropdown with 11 rows (needs a live window) | claude | medium: live window |
| A5 | 🅰️ | M | openbook <id>: start the target game's own daemon and board window (its tabs and sidebars come with it), mark it edit mode, hand over from the editor window | A3, S1 | relay-driven: pick cdda-hq, its window appears with its 10 tabs and 3 bars; state file shows edit_target; no stray processes after close; footprint line idle + active | claude | high: process launch, the 15:21 freeze |
| A6 | 🅰️ | S | openpage <book> <room>: goto in the target via its existing goto verb | A5 | state file room changes in the target; picture of that room | claude | low |
| A7 | 🅰️ | M | Edit affordances in the loaded game: confirm place / remove / newroom / newpal work from the target's own window; add an edit-mode strip only if they do not | A5, S2 | place a block from the window; ledger row + state file; remove it again; bytes restored | claude | medium: unknown scope |
| A8 | 🅰️ | M | Install the other games as desk books, one at a time (additive; backup before, restore rule after), so the desk Book dropdown and Synch see them | A1 | sessions tree: files added only, 0 existing changed, sha256 list kept under the home folder (not /tmp); desk dropdown picture | claude+owner | medium: owner data |
| A9 | 🅰️ | M | Round trip: open a book, save unchanged, bytes identical (S4 of the pivot); then one real edit with a ledger row | A7 | sha256 before == after for the unchanged save; one edit shows in the page file and the ledger | claude | medium |
| A10 | 🅰️ | M | Picture pack: for each of the 11 games, Book dropdown, Page dropdown, loaded game with tabs and bars (the house's evidence rule) | A4, A5, A6 | 11 x 3 pictures with receipts, taken one process at a time, niced, machine cool | claude+owner | medium: machine load |
| A11 | 🅰️ | S | Recipe + README + harness rows: how a new game appears in the list; update GAMES-AS-BOOKS doc and the roadmap | A1 | doc exists, a fresh reader can add a 12th game from it (test: a scratch game appears in book_list) | any | low |
| B1 | 🅱️ | L | Make PET_APP a runtime value (24 uses in 23 lines of the core) so one binary can serve any game folder | A3 | all 11 games rebuild with identical behaviour (diff gate); editor binary answers for another folder | claude | high: shared core everywhere |
| B2 | 🅱️ | L | Per-game rule code (240 to 1,823 lines each) cannot live in one binary: forward verbs to the target's own binary | B1 | a verb typed in the editor changes the target's state files exactly as the target's own window would | claude | high |
| B3 | 🅱️ | L | Mirror the target's ui.txt into the editor's, and satisfy the daemon's 'window alive' gate | B2, S3 | editor window shows the target's live variables | claude | high: the gate was only read in a harness comment |
| B4 | 🅱️ | M | Swap the editor's template, css, toolbar.pdl and side_style.pdl to the target's at runtime (live reparse) | B3, S4 | picture: the editor window shows cdda's tabs and bars, then rpg-space's different 14 tabs | claude | high: renderer family is guarded |
| B5 | 🅱️ | L | Headless daemon mode (no window needed) so a target can run without its own window | B3 | daemon ticks and publishes with no window process; idle cost measured | claude | high |

Sizes 🧠: **S** = about one focused session including harness and mutation tests (my estimate) · **M** = two to three sessions · **L** = several sessions or needs a design first. They are my estimates, not measurements. 'lane' is who I think should take it; **any** = a bounded, checkable task another agent can do on its own branch with an explicit pathspec.


## ❓ 6. Spikes — answer these before the tasks that need them

| id | question | blocks | why unknown |
|---|---|---|---|
| S1 | Is opening a second game window while another is open safe on this machine (CPU, heat, freeze)? Do it once, niced, watching temperature, only when the machine is cool and idle. | A5 | the 15:21 freeze happened ~30 s after a board launch; cause unproven |
| S2 | Does a game's own window already expose enough editing (place from the hotbar, new room, new pal), or does edit mode need its own strip? | A7 | verbs exist in all 11 binaries; whether the window reaches them was not tested for 10 of 11 |
| S3 | What exactly gates ui.txt publishing (a process whose command line holds <game>.xhtpm was read in a harness comment, not in the code path)? | B3 | matters only for Road B |
| S4 | Can one window be re-pointed to another game by rewriting its template and letting the live reparse pick it up (with the other game's variables)? | B4 | matters only for Road B |
| S5 | What does closing the editor do to the target's daemon and engine (stray processes)? Memory: button.sh leaks engine stacks | A5 | a footprint finding would be ours, not the browser's |


## 🏁 7. Sprint cut 🧠

| sprint | theme | items | exit criterion (observable) |
|---|---|---|---|
| Sprint 1 | list | A1, A2, A3, A11 | ui.txt of the editor lists 11 books and any book's pages; all 11 games rebuild with no behaviour change; harnesses green; NO live launch needed |
| Sprint 2 | open | S1, A4, A5, A6 | relay-driven: two different games opened from the dropdown with their own tabs and bars, pictures + state files + footprint line; one process at a time |
| Sprint 3 | edit + install | S2, A7, A8, A9 | one real edit round-trips with a ledger row; one more game installed as a desk book with the restore rule honoured |
| Sprint 4 | prove all | A10 | 11 x 3 pictures with receipts; the feature is declared done only now |

- 🧊 **House rules that apply to every sprint:** a fresh build + fresh run + a state file or picture before anything is called done; negative-test every check; `nice -n 15`, one heavy process at a time, a footprint line (idle + active, browser open) in every commit; live-desk changes backed up under the home folder and restored by hash; commit only your own paths.
- 🚫 **No live board launch until the machine is cool and idle** (the 2026-10-28 15:21 freeze is unexplained).


## 🤔 8. Decisions only you can make

| id | question | note |
|---|---|---|
| D1 | Road A first (the editor launches the target's own window with its tabs and bars) and keep Road B for later? | A recommended; B only if you need one single window to turn into any game |
| D2 | Does the Book dropdown list all 11 games, or only conformant ones? | All 11 fail book_check A2 today, so conformance cannot be the filter yet; recommend list all, show a conformant flag |
| D3 | After you pick a game: does the editor window stay as a small launcher bar, or does the target's window replace it? | decides A5 hand-over |
| D4 | Install the other 10 games as desk books too (A8), one at a time? | touches your desk data; additive with backup and restore rule |
| D5 | Is rpg-edit-hq's own 6-room town also a book in the list ('rpg-edit'), or only the editor shell? | it is a pets-like game today |


## 📊 9. Scoreboard

| measure | value |
|---|---|
| Preconditions done | 5 of 6 (P6 partial) |
| Road A feature tasks done | 0 of 11 |
| Road B feature tasks done | 0 of 5 |
| Games in the Book list today | 0 of 11 (no op builds the list) |
| Games on the desk as books | 1 of 11 |
| Window layouts that are data-identical | 7 of 11 |
| Spikes answered | 0 of 5 |


## ⚠️ 10. Limits of this document

- Sizes, lanes and the road comparison are my judgement (🧠), not measured.
- I did not launch any game window for this; nothing about Road A's launch step is tested (that is spike S1).
- The 'identical templates' result compares file text after swapping the game name; it does not prove two windows look the same on screen.
- `book_check` items were itemised for 5 of 11 games, not all.
- 'Event pages' counts folders under `events/`, not their quality.


## 📦 11. The same plan as data

`near-tear-gear.json` (same folder) holds games, preconditions, tasks, spikes, decisions and sprints, so a roadmap or sprint tool can read it directly. The block below is the same JSON.

```json
{
 "generated": "2026-10-28",
 "title": "near-tear-gear: rpg-edit opens every pets-like game",
 "glossary": {
  "S": "about one focused session including harness and mutation tests (my estimate)",
  "M": "two to three sessions",
  "L": "several sessions or needs a design first"
 },
 "games": [
  {
   "id": "cdda-hq",
   "template_lines": 113,
   "tabs": 10,
   "pages": 7,
   "event_pages": 36,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 473,
   "on_desk": false
  },
  {
   "id": "civ-rpgce",
   "template_lines": 113,
   "tabs": 10,
   "pages": 6,
   "event_pages": 33,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 310,
   "on_desk": false
  },
  {
   "id": "df-hq",
   "template_lines": 113,
   "tabs": 10,
   "pages": 6,
   "event_pages": 36,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 469,
   "on_desk": false
  },
  {
   "id": "dsr-rpg",
   "template_lines": 113,
   "tabs": 10,
   "pages": 6,
   "event_pages": 33,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 240,
   "on_desk": false
  },
  {
   "id": "gta-hq",
   "template_lines": 113,
   "tabs": 10,
   "pages": 6,
   "event_pages": 33,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 557,
   "on_desk": false
  },
  {
   "id": "mcl-hq",
   "template_lines": 113,
   "tabs": 10,
   "pages": 6,
   "event_pages": 37,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 500,
   "on_desk": false
  },
  {
   "id": "rpg-dust",
   "template_lines": 132,
   "tabs": 13,
   "pages": 7,
   "event_pages": 14,
   "template_diff_vs_cdda": 49,
   "game_c_lines": 1745,
   "on_desk": false
  },
  {
   "id": "rpg-edit-hq",
   "template_lines": 119,
   "tabs": 11,
   "pages": 6,
   "event_pages": 21,
   "template_diff_vs_cdda": 26,
   "game_c_lines": 395,
   "on_desk": false
  },
  {
   "id": "rpg-pet",
   "template_lines": 113,
   "tabs": 10,
   "pages": 3,
   "event_pages": 37,
   "template_diff_vs_cdda": 4,
   "game_c_lines": 223,
   "on_desk": false
  },
  {
   "id": "rpg-space",
   "template_lines": 140,
   "tabs": 14,
   "pages": 7,
   "event_pages": 15,
   "template_diff_vs_cdda": 57,
   "game_c_lines": 1823,
   "on_desk": true
  },
  {
   "id": "tcg-rpgce",
   "template_lines": 113,
   "tabs": 10,
   "pages": 6,
   "event_pages": 33,
   "template_diff_vs_cdda": 0,
   "game_c_lines": 314,
   "on_desk": false
  }
 ],
 "preconditions": [
  {
   "id": "P1",
   "title": "One shared core under all 11 games, and every edit verb (place, remove, newroom, newpal, delpal, save, load, goto, control) is in it",
   "state": "done",
   "evidence": "RAN today: all 9 verb names found in petslike_core.c; the built rpg-space and rpg-dust binaries contain newroom / newpal / place"
  },
  {
   "id": "P2",
   "title": "The window layout (toolbar tabs, rail, left/right bars, hotbar) is a data template, and 7 of 11 are identical once the game name is swapped",
   "state": "done",
   "evidence": "RAN today: diff of board/pchq-board.xhtpm vs cdda-hq = 0 lines for civ-rpgce, df-hq, dsr-rpg, gta-hq, mcl-hq, tcg-rpgce; 4 for rpg-pet; 26 / 49 / 57 for rpg-edit-hq / rpg-dust / rpg-space"
  },
  {
   "id": "P3",
   "title": "Every game already has pages (rooms) in rooms.pdl: 66 pages over 11 games",
   "state": "done",
   "evidence": "RAN today: ROOM rows per game 7 6 6 6 6 6 7 6 3 7 6"
  },
  {
   "id": "P4",
   "title": "book_check can judge any game: all 11 report conformant=no, and the 5 games I itemised fail only the known 'source=board' item (A2)",
   "state": "done",
   "evidence": "RAN today on all 11 (conformant=no for each); the failing item was listed for 5 of 11 (cdda-hq, rpg-edit-hq, rpg-space, tcg-rpgce, rpg-pet) and is A2 in each; the other 6 were not itemised"
  },
  {
   "id": "P5",
   "title": "Games are BOOK:PAGE capable: page file, game_page_export (with other characters as rows), page_follow, taskbar Synch for game boards",
   "state": "done",
   "evidence": "RAN this week in scratch houses: harnesses game_page_export 147/0, page_follow 44/0, pc_synch_games 22/0; commits 336385812, b79f820aa, 1b0086ff7. The live taskbar was not pressed"
  },
  {
   "id": "P6",
   "title": "One game (rpg-space) is installed as a book on the owner's desk",
   "state": "partial",
   "evidence": "READ today: desk sessions = rpg-space, s1..s7 (8 books); 1 of 11 games"
  }
 ],
 "tasks": [
  {
   "id": "A1",
   "title": "book_list op: scan @.apps for games (toy.pdl + *-pchq + rooms.pdl), write BOOK rows: id, app, pchq, pages, conformant, on_desk",
   "road": "A",
   "size": "S",
   "depends": [],
   "proof": "harness: scratch houses with 0, 1, 11 games; live repo gives exactly 11 rows; 6+ mutations fail it",
   "lane": "claude",
   "risk": "low",
   "touches": "new op in _shared-lib/ops + pal harness",
   "status": "todo"
  },
  {
   "id": "A2",
   "title": "page_list for a book: pages from the game's rooms.pdl, cross-checked with its maps/<g>/game.pdl desk ids (book_check B2 logic)",
   "road": "A",
   "size": "S",
   "depends": [
    "A1"
   ],
   "proof": "harness: 11 live games give 66 pages total; a game whose rooms and desks disagree is reported",
   "lane": "claude",
   "risk": "low",
   "touches": "same op, second verb",
   "status": "todo"
  },
  {
   "id": "A3",
   "title": "Core publishes the lists to ui.txt (n_book, book_N_label/act, target page list) and gets verbs openbook <id> / openpage <book> <room> that only record state/edit_target.txt",
   "road": "A",
   "size": "M",
   "depends": [
    "A1",
    "A2"
   ],
   "proof": "all 11 games rebuild; per-game verb-script diff gate shows no behaviour change when the toolbar data does not ask for the list; new cases for the lists",
   "lane": "claude",
   "risk": "medium: shared core touched, 11 binaries",
   "touches": "petslike_core.c, petslike_toolbar.c",
   "status": "todo"
  },
  {
   "id": "A4",
   "title": "Editor template: Book dropdown repeats the books, Page dropdown shows the chosen book's pages (rpg-edit-hq's own xhtpm and toolbar.pdl only)",
   "road": "A",
   "size": "S",
   "depends": [
    "A3"
   ],
   "proof": "ui.txt says n_book=11; picture of the open Book dropdown with 11 rows (needs a live window)",
   "lane": "claude",
   "risk": "medium: live window",
   "touches": "rpg-edit-hq/board/pchq-board.xhtpm, toolbar.pdl",
   "status": "todo"
  },
  {
   "id": "A5",
   "title": "openbook <id>: start the target game's own daemon and board window (its tabs and sidebars come with it), mark it edit mode, hand over from the editor window",
   "road": "A",
   "size": "M",
   "depends": [
    "A3",
    "S1"
   ],
   "proof": "relay-driven: pick cdda-hq, its window appears with its 10 tabs and 3 bars; state file shows edit_target; no stray processes after close; footprint line idle + active",
   "lane": "claude",
   "risk": "high: process launch, the 15:21 freeze",
   "touches": "new op + launcher glue",
   "status": "todo"
  },
  {
   "id": "A6",
   "title": "openpage <book> <room>: goto in the target via its existing goto verb",
   "road": "A",
   "size": "S",
   "depends": [
    "A5"
   ],
   "proof": "state file room changes in the target; picture of that room",
   "lane": "claude",
   "risk": "low",
   "touches": "verb glue",
   "status": "todo"
  },
  {
   "id": "A7",
   "title": "Edit affordances in the loaded game: confirm place / remove / newroom / newpal work from the target's own window; add an edit-mode strip only if they do not",
   "road": "A",
   "size": "M",
   "depends": [
    "A5",
    "S2"
   ],
   "proof": "place a block from the window; ledger row + state file; remove it again; bytes restored",
   "lane": "claude",
   "risk": "medium: unknown scope",
   "touches": "target templates or a shared strip",
   "status": "todo"
  },
  {
   "id": "A8",
   "title": "Install the other games as desk books, one at a time (additive; backup before, restore rule after), so the desk Book dropdown and Synch see them",
   "road": "A",
   "size": "M",
   "depends": [
    "A1"
   ],
   "proof": "sessions tree: files added only, 0 existing changed, sha256 list kept under the home folder (not /tmp); desk dropdown picture",
   "lane": "claude+owner",
   "risk": "medium: owner data",
   "touches": "xyzfs/users/<id>/home/livedesk/sessions",
   "status": "todo"
  },
  {
   "id": "A9",
   "title": "Round trip: open a book, save unchanged, bytes identical (S4 of the pivot); then one real edit with a ledger row",
   "road": "A",
   "size": "M",
   "depends": [
    "A7"
   ],
   "proof": "sha256 before == after for the unchanged save; one edit shows in the page file and the ledger",
   "lane": "claude",
   "risk": "medium",
   "touches": "save/load ops",
   "status": "todo"
  },
  {
   "id": "A10",
   "title": "Picture pack: for each of the 11 games, Book dropdown, Page dropdown, loaded game with tabs and bars (the house's evidence rule)",
   "road": "A",
   "size": "M",
   "depends": [
    "A4",
    "A5",
    "A6"
   ],
   "proof": "11 x 3 pictures with receipts, taken one process at a time, niced, machine cool",
   "lane": "claude+owner",
   "risk": "medium: machine load",
   "touches": "scratch only plus live windows",
   "status": "todo"
  },
  {
   "id": "A11",
   "title": "Recipe + README + harness rows: how a new game appears in the list; update GAMES-AS-BOOKS doc and the roadmap",
   "road": "A",
   "size": "S",
   "depends": [
    "A1"
   ],
   "proof": "doc exists, a fresh reader can add a 12th game from it (test: a scratch game appears in book_list)",
   "lane": "any",
   "risk": "low",
   "touches": "docs",
   "status": "todo"
  },
  {
   "id": "B1",
   "title": "Make PET_APP a runtime value (24 uses in 23 lines of the core) so one binary can serve any game folder",
   "road": "B",
   "size": "L",
   "depends": [
    "A3"
   ],
   "proof": "all 11 games rebuild with identical behaviour (diff gate); editor binary answers for another folder",
   "lane": "claude",
   "risk": "high: shared core everywhere",
   "touches": "petslike_core.c",
   "status": "todo"
  },
  {
   "id": "B2",
   "title": "Per-game rule code (240 to 1,823 lines each) cannot live in one binary: forward verbs to the target's own binary",
   "road": "B",
   "size": "L",
   "depends": [
    "B1"
   ],
   "proof": "a verb typed in the editor changes the target's state files exactly as the target's own window would",
   "lane": "claude",
   "risk": "high",
   "touches": "dispatch op",
   "status": "todo"
  },
  {
   "id": "B3",
   "title": "Mirror the target's ui.txt into the editor's, and satisfy the daemon's 'window alive' gate",
   "road": "B",
   "size": "L",
   "depends": [
    "B2",
    "S3"
   ],
   "proof": "editor window shows the target's live variables",
   "lane": "claude",
   "risk": "high: the gate was only read in a harness comment",
   "touches": "daemon + ui relay",
   "status": "todo"
  },
  {
   "id": "B4",
   "title": "Swap the editor's template, css, toolbar.pdl and side_style.pdl to the target's at runtime (live reparse)",
   "road": "B",
   "size": "M",
   "depends": [
    "B3",
    "S4"
   ],
   "proof": "picture: the editor window shows cdda's tabs and bars, then rpg-space's different 14 tabs",
   "lane": "claude",
   "risk": "high: renderer family is guarded",
   "touches": "khtpm window files",
   "status": "todo"
  },
  {
   "id": "B5",
   "title": "Headless daemon mode (no window needed) so a target can run without its own window",
   "road": "B",
   "size": "L",
   "depends": [
    "B3"
   ],
   "proof": "daemon ticks and publishes with no window process; idle cost measured",
   "lane": "claude",
   "risk": "high",
   "touches": "core",
   "status": "todo"
  }
 ],
 "spikes": [
  {
   "id": "S1",
   "question": "Is opening a second game window while another is open safe on this machine (CPU, heat, freeze)? Do it once, niced, watching temperature, only when the machine is cool and idle.",
   "blocks": "A5",
   "why": "the 15:21 freeze happened ~30 s after a board launch; cause unproven"
  },
  {
   "id": "S2",
   "question": "Does a game's own window already expose enough editing (place from the hotbar, new room, new pal), or does edit mode need its own strip?",
   "blocks": "A7",
   "why": "verbs exist in all 11 binaries; whether the window reaches them was not tested for 10 of 11"
  },
  {
   "id": "S3",
   "question": "What exactly gates ui.txt publishing (a process whose command line holds <game>.xhtpm was read in a harness comment, not in the code path)?",
   "blocks": "B3",
   "why": "matters only for Road B"
  },
  {
   "id": "S4",
   "question": "Can one window be re-pointed to another game by rewriting its template and letting the live reparse pick it up (with the other game's variables)?",
   "blocks": "B4",
   "why": "matters only for Road B"
  },
  {
   "id": "S5",
   "question": "What does closing the editor do to the target's daemon and engine (stray processes)? Memory: button.sh leaks engine stacks",
   "blocks": "A5",
   "why": "a footprint finding would be ours, not the browser's"
  }
 ],
 "decisions": [
  {
   "id": "D1",
   "question": "Road A first (the editor launches the target's own window with its tabs and bars) and keep Road B for later?",
   "note": "A recommended; B only if you need one single window to turn into any game"
  },
  {
   "id": "D2",
   "question": "Does the Book dropdown list all 11 games, or only conformant ones?",
   "note": "All 11 fail book_check A2 today, so conformance cannot be the filter yet; recommend list all, show a conformant flag"
  },
  {
   "id": "D3",
   "question": "After you pick a game: does the editor window stay as a small launcher bar, or does the target's window replace it?",
   "note": "decides A5 hand-over"
  },
  {
   "id": "D4",
   "question": "Install the other 10 games as desk books too (A8), one at a time?",
   "note": "touches your desk data; additive with backup and restore rule"
  },
  {
   "id": "D5",
   "question": "Is rpg-edit-hq's own 6-room town also a book in the list ('rpg-edit'), or only the editor shell?",
   "note": "it is a pets-like game today"
  }
 ],
 "sprints": [
  {
   "name": "Sprint 1",
   "theme": "list",
   "items": [
    "A1",
    "A2",
    "A3",
    "A11"
   ],
   "exit": "ui.txt of the editor lists 11 books and any book's pages; all 11 games rebuild with no behaviour change; harnesses green; NO live launch needed"
  },
  {
   "name": "Sprint 2",
   "theme": "open",
   "items": [
    "S1",
    "A4",
    "A5",
    "A6"
   ],
   "exit": "relay-driven: two different games opened from the dropdown with their own tabs and bars, pictures + state files + footprint line; one process at a time"
  },
  {
   "name": "Sprint 3",
   "theme": "edit + install",
   "items": [
    "S2",
    "A7",
    "A8",
    "A9"
   ],
   "exit": "one real edit round-trips with a ledger row; one more game installed as a desk book with the restore rule honoured"
  },
  {
   "name": "Sprint 4",
   "theme": "prove all",
   "items": [
    "A10"
   ],
   "exit": "11 x 3 pictures with receipts; the feature is declared done only now"
  }
 ],
 "scoreboard": {
  "preconditions_done": 5,
  "preconditions_total": 6,
  "road_a_tasks": 11,
  "road_a_sizes": {
   "S": 5,
   "M": 6,
   "L": 0
  },
  "road_a_done": 0,
  "road_b_tasks": 5,
  "road_b_sizes": {
   "S": 0,
   "M": 1,
   "L": 4
  },
  "road_b_done": 0
 }
}
```
