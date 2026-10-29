# Pieces, identity and the Master Pal List (design, 2026-10-28)

Status: **DESIGN. Nothing in this document is built.** Written by Claude (Sonnet 5.5) from the owner's words and a scan of the 11 petslike games. Labels: **SAID** = the owner's words, **READ** = read in a file, **RAN** = measured by a command today, **MINE** = my proposal (the owner decides), **OPEN** = a question. Companion: `CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md` (price, fees, bonds, contracts), `PER-BOOK-DB-2026-10-28.md` (the per-book db layer), `@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md` (wallets, NFTs, exchange), `GAMES-AS-PACKAGES-AND-STORE-DESIGN.md` (store, escrow, courts).

## 0. What the owner said (SAID, 2026-10-28)
- "Some of the games may have the same pets, but they should either have **unique ids** if they are different, or be **legitimately shared** if they are the same piece in a different game, since **pieces are rare and have value**."
- "**Minting is an op / event**, because it will be reused a lot, by devs, who can **buy / copy / fork** depending on the chain in exchange" (the payment goes to the blockchain; see the chain document).
- "Technically **the boards aren't 'home' for entities, since they are all pals, they are in the USER'S MASTER PAL LIST**, and it's like **Bill's PC / the Pokedex**."
- "Remember **other agents and players want to reuse this** from menus and other ways." "We want **uniformity and order**." "Things are scattered. We are in dev, but it's not intentional."

## 1. What is true today (RAN, 2026-10-28)
| Fact | Evidence |
|---|---|
| Each game mints its own random 32-hex id per pet **lazily, on first run**, into `<game>/state/uid_<id>.txt` | `petslike_core.c` (comment: "made once, state/uid_<id>.txt -> wallet id `e` + the first 24 hex") |
| 30 id files over 11 games, 29 distinct values | scan |
| The same name in several games is always a **different** id: `rex` 5 games / 5 ids, `harold` 5 / 5, `ember` 4 / 4, `solvent` 3 / 3, `pip` 2 / 2, `blip` 2 / 2 | scan |
| Two id files are **empty** (`cdda-hq` harold, `tcg-rpgce` pal_1); they are the only "shared" value | scan |
| The id files are **not durable**: `state/` is git-ignored in every game (0 of 30 tracked) | `git check-ignore` |
| Five games (`cdda-hq df-hq dsr-rpg gta-hq mcl-hq`) read the **same** db party row `DsrRpgParty` (Rex Gus Pip Orin Maren Fenn); `rpg-pet` reads `RpgPetParty`; `rpg-space rpg-dust rpg-edit-hq` use a name list in their own `toy.pdl` | `system.pdl`, `toy.pdl` |
| A **master pal list already exists** and is durable: `xyzfs/users/<uuid>/home/livedesk/pals/` = **53 pal folders**, each with `entity_uid.txt`, `asset.pal`, `desktop_pos.txt`, `event_pkg/`, `history.txt` (pets, banks, castles, stores, robots, `hero_01` ...) | `ls` |
| **The games do not use it.** The master list has `terumon_001_ember`; the games mint their own `ember` (3 different ids). `rex`, `solvent`, `harold` are not in the master list at all | scan |
| The pals folder is user data (git-ignored in code branches; data branches `user/<id>` per `USER-DATA-BRANCHES-DESIGN.md`) | AGENTS.md |

So today: **no piece has one identity across the house**, identity is **minted by whoever runs first**, and it is **lost when `state/` is wiped**. For things that carry value that is the wrong shape.

## 2. The model (MINE)
Three different things were being called "a pet":
1. **Actor** = a *template*: a db row (name, species, stats, art). Cheap, reusable, not rare. Many pieces can come from one actor. (`actors.pdl`, per-book layer.)
2. **Piece** = one *owned instance*: it has an **id (`uid`)**, a wallet (`e` + 24 hex of the uid), an owner, a lineage and a value. **Rare things are pieces.** A piece lives in the owner's **Master Pal List**, not in any game.
3. **Appearance** = that piece *in one book* (a game): position, hp, hunger, the cast row that says "this book uses piece X". A board is a **visit**, never a home.

Rules:
- **R1 Only the uid identifies a piece.** A name or a slug never does.
- **R2 Minted once, by one authority, stored durably** (section 4). A game never mints on first run; it **asks** the mint op, and the answer is written where it survives a wipe.
- **R3 Same piece in two books = explicit.** Both cast rows carry the **same uid** and `shared=1`; one book is the **home** for value and wallet; position/hp are per book; a piece is **active in one book at a time** (OPEN, section 9).
- **R4 Different pets = different uids**, always. Two books may use the same actor template and still hold two pieces.
- **R5 Slugs** (the in-book id such as `rex`) are unique **within a book**; the display name may repeat across books.
- **R6 One authority per fact:** the Master Pal List owns identity, owner, lineage, value; the book owns the appearance; the chain owns the money. Everything else is a projection.

## 3. The Master Pal List (Bill's PC)
- **Where:** the existing `xyzfs/users/<uuid>/home/livedesk/pals/<pal>/` folders (READ: 53 today; each has `entity_uid.txt`). A pal is **any entity** (pet, bank, castle, robot, book-stack), so the list is also the one place the db window, the taskbar **Pals menu**, the desk and every game read.
- **Index (new, MINE):** `pals/master_pals.pdl`, one row per piece: `PIECE | uid | slug | kind | template | owner | origin | parent | home_book | minted_at | sig`. The folder `entity_uid.txt` stays the cheap per-pal copy (the two must agree: `piece_check`).
- **Boxes (MINE):** a "box" is just a filter of the list (all, by book, by kind, unplaced) the way Bill's PC shows boxes; a "Pokedex" view is the set of **templates seen/owned** (actors the owner has at least one piece of).
- **Appearance rows** live in the book: `CAST | uid | slug | role | shared | home` in the book's db layer (`@.apps/<game>/db/cast.pdl`), so a book's db window lists **that book's cast** (answering "each db lists the named actors for each model").
- **Durable:** the master list is user data (user-data branch, backed up by `save-data`); the book's cast rows are tracked in the game package.

## 4. Minting is an op and an event (owner direction)
**Op `piece_mint`** (C, in `&.widgits/_shared-lib/ops/`, pal harness with mutation tests), one verb set, used by every caller:
```
piece_mint <house> --owner <user|wallet> --template <actor id> --source <mine|buy|auction|npc|market|fork|copy|grant> [--parent <uid>] [--book <id> --slug <s>] [--key <idempotency key>] [--dry]
  -> stdout  `minted uid=<32hex> wallet=e<24hex> slug=<s> source=<..> price=<n> fee=<n>`      exit 0
     refusals (exit 1, nothing written): unknown template | owner unknown | key already used (returns the SAME uid: idempotent) | slug taken in the book | parent not owned/forkable | price not paid (escrow) | policy forbids the source
```
- **Writes (all or nothing, in this order):** the `PIECE` row in `master_pals.pdl`, the pal folder with `entity_uid.txt`, the book's `CAST` row, one **ledger row** (`MINT | ts | uid | source | price | fee | key`), and, when the chain supports it, a signed **`NFT_MINT`** transaction (CHAIN-ECONOMY section 4 option A; until then the ledger row is the record, `play` unit).
- **Source rules come from data**, not code: `policy.pdl` rows per chain (section 7 of the chain document): who may mint what, price, fee, royalty to the parent's creator on `copy` / `fork`.
- **Randomness is verifiable:** for `source=mine` the rarity roll uses the block hash + the mint-right id, so anyone replaying the chain gets the same piece; a model never decides it.
- **Idempotent:** the same `--key` returns the same uid, so a retried event cannot create two pieces.
- **Event:** an events-hq page `piece_mint` (trigger `ui`/`action`) runs the op; the **registry command** `piece_mint` (`LABEL`, fields `template`, `source`, `book`, `slug`) makes it appear in blueprintz, in `book_check --events`, in menus and for other agents, the same way `rpg_space_verb` does. Rule from the house: events first, C fallback until the harness passes.
- **Callers (all through the same op):** newpal / `+ new pal` menu rows, the db window "add actor", the mining result, the store / auction / exchange settlement, NPC shops, the fork button, dev tools and harnesses.

## 5. Copy, fork, share, transfer, buy (definitions, MINE)
| Verb | uid | Owner | Lineage | Money |
|---|---|---|---|---|
| **share** | same | same | none | none (a second book uses the piece; `shared=1`) |
| **transfer** | same | changes (`NFT_XFER`) | none | per sale |
| **buy** | same | seller -> buyer, via escrow | none | price + chain fee + tax |
| **copy** | **new** | copier | `parent=<uid>` | price set by the chain policy; royalty to the parent's creator |
| **fork** | **new** | forker | `parent=<uid>` + a recorded change | policy price; royalty; the fork may be sold |
Which of copy / fork / buy a dev may use "depends on the chain": a chain's `policy.pdl` lists the allowed verbs per piece kind and their prices (see the chain document).

## 6. The checker `piece_check` (read-only first)
Like `book_check`, over all books and the master list. Findings: empty or missing uid; uid only in `state/` (not durable); two books with the same uid but not `shared`; `shared` with no `home`, or active in two books; same display name with different uids across books (warning, so renames are deliberate); cast row with no master-list piece; master-list piece whose `entity_uid.txt` disagrees; slug collision in a book. Output is one line per finding plus a count, exit code for CI, **no writes**.

## 7. Reuse surfaces (so other agents and players can use all of this)
| Surface | What it gets |
|---|---|
| **op CLI** (`piece_mint`, `piece_check`, later `piece_bind`) | scripts, harnesses, other agents; stable stdout and exit codes |
| **events-hq pages + registry commands** | blueprintz, the event editor, `book_check --events`, game event pages |
| **taskbar menus** (`pals_menu_pre/post`, `toolbar.pdl` rows) | the Pals menu lists the Master Pal List; "+ new pal" calls `piece_mint` |
| **db window** | actors tab shows the open book's cast (layer) and a Master List view; "add actor" calls `piece_mint` |
| **store / auction / exchange HQ** | settlement calls `piece_mint` / transfer; listings read `master_pals.pdl` |
| **harness** | pal cases with mutation tests for each op |

## 8. Migration of today's data (non-destructive, MINE)
1. `piece_check` read-only: the table of section 1 as a repeatable report.
2. Fix the two empty uid files by minting properly (owner OK: it changes ids).
3. **Bind** the games' pets that already match a master-list pal (`ember` ~ `terumon_001_ember`) through an explicit mapping the owner approves; mint master-list rows for the rest (`rex`, `solvent`, `harold`, ...) from the **current** game uids, so no existing id changes.
4. Games read the uid from the book's `CAST` row first; `state/uid_<id>.txt` becomes a cache and is never the source.
5. Give each of the five games that share `DsrRpgParty` its **own** cast rows (own uids, own slugs) unless the owner marks a piece shared.
Every step: a pal harness, a negative test, a before/after hash of the owner's `pals/` folder (backup under the home folder first).

## 9. Open questions for the owner
1. The five games that share the `DsrRpgParty` cast: **different pieces** (my recommendation: they are dev clones) or one shared cast?
2. A shared piece: **active in one book at a time**, or in several at once?
3. Is a template (actor) ever a **piece itself** (the first minted piece of a species)?
4. Where does the **master index** live when the owner has several user accounts (one list per user, as `pals/` is today)?
5. Names: keep `terumon_001_ember` style ids for master-list folders, or slugs?

## 10. Not verified
Nothing here is built. The scan counts 30 id files in `@.apps/<game>/state/` only; other apps' pets (`pet-trainer`, `rpg-pvp-hq` ...) were not scanned. I did not read the chain ops' replay code, so `NFT_MINT` is a design from CHAIN-ECONOMY section 4, not an existing transaction type (grep found none).

## 11. Step 1 BUILT: `piece_check`, the read-only audit (2026-10-28)
`&.widgits/_shared-lib/ops/piece_check.c` (+ `build_piece_check.sh`). `piece_check <house>`: reads `@.apps/<folder>/state/uid_<slug>.txt` for every folder that has a `toy.pdl`, and `xyzfs/users/*/home/livedesk/pals/*/entity_uid.txt`; writes nothing. Findings: `ERROR empty_uid`, `ERROR dup_uid`, `WARN name_collision`, `WARN not_durable`, `WARN no_master`, `WARN master_bad_uid`, `WARN mixed_uid_formats`; exit 1 only when an ERROR exists.
**Found while building it (RAN):** the Master Pal List holds **64-hex** uids, the games mint **32-hex** ones (section 1 said "32 hex" for the master list by assumption; wrong). One format everywhere is part of "uniformity", so the checker accepts both and warns `mixed_uid_formats`; which length wins is OPEN (the wallet id `e` + 24 hex works with either).
**Live run today (read-only, the owner's house):** `games=25 pets=32 master=53 errors=2 warnings=42` = the 2 empty ids (`cdda-hq harold`, `tcg-rpgce pal_1`), 7 name collisions, 12 folders whose uids sit under an ignored `state/`, 22 pets with no master-list pal of that name, and the mixed-format warning (30 games32 / 53 master64). It counts 32 pets, not 30: the 2 extra are in `rpg-pvp-hq` (counted per folder: the 11 petslike games hold 30 uid files, `rpg-pvp-hq` 2; the 25 folders it reports are every app folder with a `toy.pdl`, most with no pets).
**Evidence:** harness `piece_check.pal` **22/0** (compiled `-Wall -Wextra -Werror`); 13 mutations each fail it (after section 12: uid length 8-64 hex, mixed-length warning removed; live: errors=2 warnings=41). Two of my first mutants were invalid (one never applied; two failed only the compile gate) and one real gap was found and closed (no case covered a wrong-length hex uid or the `terumon_001_ember` -> `ember` match); the final run is the table above.
**Not done:** step 2 onward (fixing the empty ids, the master index `master_pals.pdl`, `piece_mint`) waits for the owner's answers in section 9.

## 12. Owner answers of 2026-10-28 (to section 9) and what follows
| # | Question | Answer (SAID) | Consequence (MINE) |
|---|---|---|---|
| 1 | The five games sharing `DsrRpgParty`: different pieces or shared? | "Sure, both. Flexible." | Support both: the default for dev clones is a **separate piece per book** (own uid); `shared=1` on a cast row makes it the same piece. `piece_mint` and the cast rows take either. |
| 2 | A shared piece active in two books at once? | "I don't know? Should it? Mechanics of that?" | **Recommendation, not yet decided:** one book at a time, by **check-out / check-in** (Bill's PC): the Master Pal List row has `active_book`; entering a book checks the piece out, leaving checks it in; a second book that lists it sees it as "away" and cannot change it. Reason: one wallet, hp, inventory and mining income per piece; two writers conflict or duplicate income. A per-piece `multi=1` flag could later allow stateless things (tools) in several books. OPEN until the owner says yes. |
| 3 | Which uid length? | "Even 8, 16 are fine." | Any length of **8 to 64 hex** is valid; `piece_check` no longer warns about mixed lengths. The wallet id must therefore be **derived by hash of the uid** (`e` + first 24 hex of the hash), not by cutting the uid, which breaks for an 8-hex uid. The current rule in CHAIN-ECONOMY section 3 ("`e` + first 24 hex of the uid") has to change for short uids; OPEN: keep it for uids of 24+ hex and hash only the short ones, or hash always. |

## 13. Step 2 BUILT: `piece_mint` v1 (2026-10-28, paper)
`&.widgits/_shared-lib/ops/piece_mint.c` (+ `build_piece_mint.sh`, which also builds the sibling `hash_op`). Sources grant | copy | fork; `--adopt-uid` keeps an existing game id (migration, section 8 step 3); `--key` makes it idempotent; `--dry` writes nothing; every check runs before the first write and a failed write is rolled back. Writes the pal folder (`entity_uid.txt`), the index `pals/master_pals.pdl`, the book's `db/cast.pdl` (with `--book`) and `pals/piece_ledger.txt`. The wallet id comes from `hash_op wallet` by fork/exec (no shared header). **Evidence:** harness `piece_mint.pal` **68/0** (both ops compiled `-Wall -Wextra -Werror`; unknown template, layer-only template, each refused source, parent rules, slug taken in the index / on disk / in the book, adopt rules, uid length, bad user / book / slug / key, dry run, missing `hash_op`, two rollback paths); 30 mutants each fail it on a real expectation (an earlier fixture of mine overwrote the index instead of appending and produced one false failure; fixed). Live: `--dry` against the owner's house adopted the df-hq Rex uid unchanged and left `pals/` byte-identical (53 entries, 2313 files, same listing hash). **Not built:** the events-hq page + registry command, prices / fees / chain, share / transfer / check-out verbs, a fork change note. See the handoff, section 7.

### 13.1 `--pal-name` (added before the first real adoption)
The pal FOLDER name must be unique in the whole Master Pal List, but the in-book slug (`rex`) repeats across games, so `piece_mint` takes `--pal-name <n>` (default = `--slug`): the folder and the index entry use the pal name (`df-hq_rex`), the book's cast row keeps the in-book slug (`rex`, unique within that book). Harness now **79/0**, **35** mutants each fail it (one fixture of mine used an uppercase pal name and the op rightly refused it; one mutant text went stale after the edit and was updated, nothing left unapplied).

## 14. Step 3 DONE: the games' existing ids are adopted into the Master Pal List (2026-10-28, the owner said "go for it")
**What ran (live, on the owner's house):** a backup first (`/home/no/scratch-gab/pals-backup/pals-20261028-212738.tgz` + `.sha256`, 2313 files on disk = in the list = in the tarball), a rehearsal twice on a scratch copy (26 minted, then 26 `existing`), then the real run: `piece_mint --source grant --adopt-uid <the game's own uid> --slug <slug> --pal-name <game>_<slug> --book <game> --key adopt-<pal>` for **26 pieces** (df-hq 3, dsr-rpg 6, rpg-space 4, tcg-rpgce 4, rpg-dust 3, rpg-edit-hq 2, cdda-hq 1, gta-hq 1, mcl-hq 1, rpg-pet 1).
**Result (observed):** 79 pal folders (53 + 26); index 26 rows, ledger 26 rows; **all 2313 original files byte-identical** (`sha256sum -c`, exit 0); 2341 files in total as predicted; no id changed (each new `entity_uid.txt` equals the game's `state/uid_<slug>.txt`); ten `@.apps/<game>/db/cast.pdl` files created (new, tracked, durable). `piece_check`: errors 2 (unchanged), warnings 41 -> 23, `no_master` 22 -> 4.
**Not adopted (4):** `cdda-hq/harold` and `tcg-rpgce/pal_1` (empty ids: need a real mint, owner OK), `civ-rpgce/blue` and `gta-hq/nico` (no actor of that name in the trunk db, so `piece_mint` refuses: they are characters, not db actors; OPEN what they are).
**Decisions kept (MINE, from the owner's "separate by default"):** same-named pets stay separate pieces (five `rex`, each with its own uid, pal names `cdda-hq_rex` ... `mcl-hq_rex`); NOT bound to the owner's older `terumon_001_ember` etc. (that mapping needs the owner's approval, section 8 step 3).
**Found:** (1) `piece_check` counted the new index and ledger files as pals (`master=81`, two false `master_bad_uid`): fixed, harness 25/0 (14 mutants; the old bug fails 3 cases). (2) A game's `db status` now says "has its own layer: cast.pdl overridden"; cast.pdl is not an override of anything (the trunk has none). It changes no behavior (`db trim` keeps it; checked in df-hq, rpg-space, mcl-hq); fixing the wording means editing `petslike_core.c` `lp_status` (Claude only, 11 games rebuild). **Limits:** the games do not read the Master Pal List or `cast.pdl` yet (their uid still comes from `state/uid_<slug>.txt`; the next slice makes the core read the cast row first); the 10 cast files and `master_pals.pdl` are the only new durable copies of these ids.
