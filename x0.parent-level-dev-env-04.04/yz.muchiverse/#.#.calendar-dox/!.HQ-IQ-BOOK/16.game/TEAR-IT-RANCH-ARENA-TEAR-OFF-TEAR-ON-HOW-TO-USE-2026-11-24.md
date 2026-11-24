# TEAR-IT Ranch and Arena: the web page, tear-off, tear-on (how to use, how it fits, what is not built) 2026-11-24

One game, one rule set, three faces. Labels: RAN = run and seen 2026-11-24; NOT BUILT = no. Paths from the house `H` = `.../yz.muchiverse/44.xyz.01.00`.

## 1. The three faces (the chain file is the only authority)
| Face | Where | What it is | Harness |
|---|---|---|---|
| **web page** | https://tearitco.github.io/tearit-web/ (house copy `H/&.hq-apps/tearit-web/docs/`, rules in `chain.js`) | landing (hatch from words, upload a save), ranch (2D / tilted 3D canvas), market, arena on the real RPG Maker map Map063, the chain | `web_chain` (97 assertions) |
| **tear-off** | taskbar > network > `tear-off` (was `tear-offline`; internal names keep `tear-offline`) | a C window, text grid, same rules ported to `H/&.hq-apps/tear-offline/ops/tear_chain_op.c` | `tear_offline` (152) |
| **tear-on** | taskbar > network > `tear-on (rpg-edit board)` | a BOOK in the rpg-edit HQ board (real 2D/3D), pages `ranch` and `arena` drawn from the chain; Play runs the driver | `tear_on` (51) |
The C op and the JS are graded byte for byte against each other (a chain mined by the JS must give the same pets, arenas and wallet options in C). A rule changes in `chain.js` AND `tear_chain_op.c` together, never one.

## 2. Using tear-on (normal use)
1. **Open it:** network menu > `tear-on (rpg-edit board)`. The op `H/&.hq-apps/tear-on/ops/+x/tear_on_op.+x --open` projects both pages, saves tear-on as the editor's last book, then switches a RUNNING editor through its inbox (`CONFIRM_START_MAP:tear-on`, `CONFIRM_SET_DESK:ranch`) or starts the editor (`button.sh run`, which opens the last book). RAN on a running editor (3D ranch and arena seen); the start-from-closed branch is NOT verified live.
2. **Empty chain?** `tear_on_op demo <H>` seeds five pets and an arena as ROWS (refuses a chain that already has rows). Reset = delete `H/@.apps/tear-on/ledger/web_chain.txt`.
3. **Play** (Player > Play in the editor): starts `@.apps/tear-on/ops/+x/tear_on_driver.+x`, which draws a game bar with buttons: Hatch a pet, Next pet, Pet faucet, Feed, Train x3, Rest, Next ranked fight, Walk (4), Open 1v1 / 2v2 / free for all, Join, Fill with default fighters, FIGHT, Leave, Next arena, wallet buttons, Close. Every click is ONE ROW mined into the chain; pages are re-projected. The editor's `running` switch is the only pause (commands wait while it is 0). Verified by `tear_on` case `driver-plays-the-book-like-the-editor-does` with a scratch editor; the live Play click is NOT done yet (see section 5).
4. **Wallet options per pet** (rows `WOPT|pet|option|value|owner|ts`): `cap` (most one spend may cost, 0 = none), `reserve` (coins the pet must keep), `sale` (0 = locked from sale, clears a listing). Enforced at every spend: feed, train, upgrade, items, cup entry, arena entry, buying a pet. The game bar cycles cap through 0/10/25/50/100 and reserve through 0/50/100/250/500.
5. **Who you are:** `@.apps/tear-on/play/owner.txt` (one word, default `you`).

## 3. The arena (rows ARENA JOIN LEAVE FILL FIGHT)
1v1 (2 fighters), 2v2 (4, two teams), free for all (3 to 8). Entry 20 from the pet into a pot, 10 percent rake to the ranch pool, free for all pays the top three 50 / 30 / 20. FILL lets the arena owner put default fighters in empty chairs (strength = a hash, never a roll at fight time). Score = power - tired/2 + a roll from the pet seed and the arena name, so replaying the chain replays the fight. A fight tires the pet and ages it one week.

## 4. Files and state (no ghost data)
`@.apps/tear-on/state.pdl` classifies every file: the LEDGER is `ledger/web_chain.txt`; the two page folders are PROJECTIONS (`tear_chain_op desk`, safe to delete); the atlas is CONFIG; `play/action.txt` (capped 64 KiB), `play/driver.lock`, `play/owner.txt` are declared too. `book_check` on the package: B1 to B4 pass, A1/A2 fail (no page file, no event pages) exactly like hai-pal.

## 5. Not built / open (do not claim these)
- A live Player > Play click on tear-on: the editor was already running another game (the owner's Teario-Play popup, `state/game_ui.txt` gm_title Teario-Play); Play would replace its game bar, so it needs the owner's OK. Restore data: `/home/no/tear-live-backups/2026-11-24/` (MD5SUMS).
- Whether the board re-reads `events.txt` / `cells.txt` after a re-projection without a page switch (unproven; `cells.marker` is a constant 1 byte). If it does not, bump the marker or re-send `CONFIRM_SET_DESK`.
- Pals sidebar for the party and wallet options; rules as `COMMAND` rows with event pages (D92); signatures; market price discovery; multi-round tournaments.
- Backup of the earlier state: git tag `tear-back-2026-11-24` and `/home/no/Desktop/github/work/tear-back/` (source archive + sha256).
