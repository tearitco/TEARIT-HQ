# Per-book db (2026-10-28): the db is not house-wide

Owner, 2026-10-28: the `db` tab is the db; **each game / book has its own db; it was never meant to be house-wide.** This is the first fix. Labels: RAN = I ran it today, READ = read in a file, MINE = proposal.

## 1. What was true before (RAN / READ)
- The `db ▾` tab of every game window opens **db-hq** (and sql-hq): one RPG-Maker-shaped, 15-tab database in the single folder `&.widgits/db-hq/data/` (14 files, 128K, 775 actors). **No game had a db of its own.** Six games read the same party rows (`party_row` = `DsrRpgParty` etc.).
- The game core (`petslike_core.c load_party`) read that shared folder; only the environment variable `PET_DB` could point it elsewhere.

## 2. What changed (step 1, built and tested)
- **Resolution order in the game core:** `PET_DB` (kept) > the game's own `<app>/db/` > the shared `&.widgits/db-hq/data` (fallback only for a game with no db yet).
- **New verbs:** `db` says which db the game uses; `db seed` copies the shared db into `<app>/db/` **once** and never overwrites.
- **All 11 petslike games were rebuilt and seeded** (each `<app>/db/` = 14 files, byte-identical to the shared source, verified by sha256). Behaviour is unchanged until a game's own db is edited, because the copy equals the source.
- **Evidence:** harness `book_db.pal` **14/0** (no own db, seed, second seed refuses and keeps a marker, own db wins, `PET_DB` still wins, removing the own db falls back); 5 mutations each fail it; the earlier core harnesses still pass (fridge 29, eat_page 17, page_follow 44); the core adds no new compiler warning (409 before, 408 after, same set).

## 3. What STILL reads the shared db (not done; these are the next steps)
1. **The db window itself (db-hq-pal).** I did not find where it resolves its data folder, and its launcher only receives the house path. It must be given the loaded book's `<app>/db` (a launch argument or a state file) before the `db ▾` tab edits a book's own rows. *Spike first: where does the 15-tab projector read from?*
2. **The window templates and the tab action:** every game's `db-hq` menu row launches `db-hq-pal/button.sh <house>` with no book; it should pass the book's db.
3. **Other readers of the shared path:** `_.monads/_.livedesk-taskbar/ops/actors_hq_manager.c` and `terms_hq_manager.c`, `&.widgits/entity-cli/ops/entity_cli_commit.sh` and `act_row.sh`, `@.apps/pet-trainer/ops/pet_event.sh`, `@.apps/rpg-pvp-hq/ops/rpg_pvp_hq.c`, the harness `playthrough_op.c`, the command registry, and a harness event page (`two-facts`). Each needs the same rule or a conscious "stays house-wide".
4. **Games outside the 11** (rpg-pvp-hq, game-dev-hq, bet-rpg ...) are not covered.

## 4. Open design questions (owner)
- **Copy or overrides?** Today a book's db is a full copy (simple, local edits). The alternative is a thin layer of overrides on top of the shared rows (smaller, shared fixes flow in, but every reader must merge). I chose the copy because it is the smallest thing that makes the db the book's own.
- **Where does a *desk* book keep its db?** Today the db lives with the game package (`<app>/db`). A desk book under `sessions/<book>/` has no db folder; if the editor opens a desk book for a game, it should use that game's `<app>/db`. Should a pure desk book (no game) get `sessions/<book>/db/`?
- **Seed policy for new games:** `db seed` is manual; should creating a game or a book seed it automatically?

## 5. Limits
Only the game core (party loading) uses the per-book db so far; no window was launched, so there is no picture of a game running on its own db; the db window, the taskbar managers and the entity tools are untouched.
