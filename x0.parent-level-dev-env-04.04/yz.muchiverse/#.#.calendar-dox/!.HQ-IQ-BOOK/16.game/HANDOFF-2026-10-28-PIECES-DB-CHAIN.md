# Handoff 2026-10-28: pieces, the per-book db and the chain (Claude to the next agent)

Read this first, then `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md`, `CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md` and `PER-BOOK-DB-2026-10-28.md` (all in this folder). Labels: **SAID** owner's words, **RAN** I ran it, **MINE** my proposal, **OPEN** not decided. Branch `claude`, nothing pushed.

## 1. Where we are (RAN, one line each)
| Item | State | Commit |
|---|---|---|
| Per-book db = empty layers over one shared trunk (1.4 MB removed) | DONE | `630a29268` |
| The db window follows the OPEN book (`db_publish`, run from `db-hq-pal/button.sh`) | DONE for a window opened after the switch; harness 17/0; **no live window launched** | `b1df0c327` |
| Designs: pieces + Master Pal List; chain as master bank / Fed / contracts | WRITTEN, not built | `3c500396f` |
| `piece_check` (read-only audit of piece identity) | DONE, harness 22/0, 13 mutants | `3ab6acd5e`, `ffdf1ccbc` |
| `hash_op` (SHA-256 + the wallet id, one tiny op) | DONE, harness 13/0, 8 mutants | `2ae9d4077` |
| `piece_mint` v1 (the mint OP; paper, grant / copy / fork) | **DONE as an op**, harness 79/0, 35 mutants; the event page + registry command are NOT built; never run for real on the owner's `pals/` (dry run only) | the commit titled `piece_mint v1` |
| `master_pals.pdl` index | written by `piece_mint` (header + rows); no reader besides it yet; the check-out / check-in verbs for `active_book` are NOT built | with `piece_mint v1` |
| Per-record db layer (`cast.pdl`), db window shows the book's cast | NOT STARTED | none |
| Chain: NFT_MINT, fees, tribute, bonds, contracts | DESIGN ONLY; chain hardening comes first | none |

## 2. Decisions (SAID unless marked)
- A **piece** is rare and valuable and has ONE id; boards are visits, pieces live in the user's **Master Pal List** (`xyzfs/users/<uuid>/home/livedesk/pals/`, 53 pals today). Minting is an **op + event**, reused by devs, who buy / copy / fork depending on the chain.
- Dev clones are **separate pieces by default; sharing is also supported** ("both, flexible").
- A uid may be **8 to 64 hex** ("even 8, 16 are fine"). The wallet id is **derived by hash always**: `hash_op wallet <uid>` = `e` + first 24 hex of SHA-256("wallet" 0x00 uid). This replaces "e + first 24 hex of the uid" for pieces; `CHAIN-ECONOMY-DESIGN.md` section 3 still says the old rule (not edited).
- **Master tax AND master tx fee** on every purchase (percentages in `policy.pdl`, placeholders only).
- **House style: no shared headers** ("prefer ops/ events/ pals"). Something needed by several programs is a small op they call (see `hash_op`), not a header or a copy.
- Owner said yes to MY proposals (2026-10-28): a piece is **active in one book at a time** by check-out / check-in (`active_book` on its master-list row, other books show it "away"); **tribute = payment for a mint right at a tier**, the roll uses the hash of a block after the payment is confirmed. Both are still labelled MINE in the designs until the owner restates them in their own words.

## 3. Next slices (MINE, smallest first; each ends with evidence)
1. **`piece_mint` v1, paper, no chain. DONE as an op (see section 1); what is left of this slice: the events-hq page + registry command `piece_mint`, and a `--note` for fork (the recorded change).** Original spec: C op in `&.widgits/_shared-lib/ops/`, calls `hash_op wallet` by fork/exec (do not copy hash code). Sources v1: `grant`, `copy`, `fork` (parent must exist); `mine` / `buy` / `auction` / `npc` / `market` are REFUSED with "source not built" until the chain work exists. Arguments and refusals: design doc section 4. Writes, all or nothing, after every check: pal folder with `entity_uid.txt`, `pals/master_pals.pdl` row (`PIECE | uid | slug | kind | template | owner | origin | parent | home_book | active_book | minted_at | wallet`), the book's `db/cast.pdl` row when `--book`, and `pals/piece_ledger.txt` (`MINT | ts | uid | source | price | fee | key`). Idempotent by `--key`. Template = a name in the trunk `actors.pdl`. `--dry` writes nothing. **Proof:** a pal harness in a scratch house (never the owner's `pals/`), every refusal negative-tested, mutants that must still compile.
2. **Extend `piece_check`** to read `master_pals.pdl` (it now exists once `piece_mint` has run): cast row with no master piece, `active_book` set in two books, `shared` without a home, folder uid vs index disagreement.
3. **Adopt the games' existing ids and fix the 2 empty ids.** `piece_mint --adopt-uid <the game's uid> --source grant --book <game> --slug <s>` brings an existing id into the Master Pal List WITHOUT changing it (dry run on the live house works today: it printed the df-hq Rex uid `9cb5a95c...` unchanged). It writes the owner's `pals/`: back up and hash first, scratch copy first, owner OK. The 2 empty ids (`cdda-hq` harold, `tcg-rpgce` pal_1) need a real mint (a new id): ask the owner first; back up and hash `pals/` first.
4. **Per-record db layer.** A book's `db/cast.pdl` and its `system.pdl` party row override the trunk **by record**, not by file (today a layer file is a full copy of a 777-line `actors.pdl`). Then the db window's actors tab lists the open book's cast. Changes `petslike_core.c` (11 games share it): Claude only.
5. **The open db window follows a book switch** (re-run `db_publish` when `active_session` changes, e.g. from Synch). Touches the taskbar monad: rehearse in a staging copy, taskbar restart needed.
6. **Row writers branch into the layer** (copy-on-first-write): `entity-cli` `act_row.sh` / `entity_cli_commit.sh` / `two-facts/apply_range.sh`.
7. **Chain** (later, in this order): hardening (balance check at inclusion, signatures, conservation rule) -> `NFT_MINT` / `NFT_XFER` -> `policy.pdl` reader -> `chain_fee_quote` / `chain_settle` -> tribute + mint right -> contracts -> bonds. Paper `play` units until the owner says go.

Suggested split (MINE): Claude keeps 1, 2, 4 (shared core); 3 needs the owner; 5 and 6 are careful shared-surface work; a cheaper agent can take 2 once 1 exists. Do not hand out 7.

## 4. Rules and traps for this work
- `AGENTS.md` applies: own branch, `git add <path>` per file, `git commit -- <paths>`, read `git diff --cached --name-only` first, never `git add -A`, never merge / push / stash / reset unprompted; runtime files are noise.
- **Owner user data** (`xyzfs/users/...`, `pals/`, `sessions/`) is live: back up with a tarball + sha256 outside `/tmp` (`/home/no/scratch-gab/desk-backup/`), verify by hash, test only on a scratch copy. Never `git add` anything under `xyzfs/users`.
- No window launch unless the machine is cool; `nice -n 15`, one heavy process at a time. The 2026-10-28 15:21 freeze is unexplained.
- Harness method (`&.widgits/_shared-lib/harness/README.md`): pal + cases pdl, op compiled `-Wall -Wextra -Werror` from the repo source, **mutation-test everything**. Traps hit today: a mutant that never applied reads as a pass (assert the replace count); a mutant that fails only the compile gate proves nothing (keep it compiling); a missing test for a boundary (65-hex) let a mutant survive; `ls build_*.sh | head -1` built the wrong op; `harness_case_op` ignores unknown verbs silently.
- A piped `| head` can kill a build or hide its error: build, then read the result.
- Quote paths with `&` and `@`. A scripted `rm -rf` loop is blocked by the safety check; do not route around it, use a tested verb.
- Never report a number you did not observe: the "32 pets" in `piece_check` is 30 in the 11 games + 2 in `rpg-pvp-hq` (checked).

## 5. Open questions for the owner
1. Restate the check-out rule and the tiered tribute in your own words (or change them).
2. `hash_op wallet` replaces the old wallet rule for pieces: edit `CHAIN-ECONOMY-DESIGN.md` section 3 and Q005 now?
3. Master tax / fee percentages and floors; who may buy bonds; which chain is "the" master chain.
4. Is a template (actor) ever a piece itself?
5. Where does the master index live with several user accounts?
6. May the 2 empty ids be re-minted (slice 3)?
7. Live test: may I open the db window (niced, one process) to look at the open book's db?

## 6. How to check
`piece_check <house>` (read-only); `db_publish <house> --dry` (writes nothing); `hash_op wallet <uid>`; harness: `cd &.widgits/_shared-lib/harness && /home/no/scratch-gab/bin/prisc <name>.pal`, then `results/<name>.txt.verdict.txt` (cases: `db_publish` 17, `piece_check` 22, `hash_op` 13, `piece_mint` 79); `piece_mint <house> --user <uuid> --template <actor> --source grant --dry` (writes nothing).

## 7. `piece_mint` v1: what it does NOT do (so nobody assumes it)
No price, fee, chain call, signature or `NFT_MINT` (price and fee are written as 0, paper). `mine buy auction npc market` are refused with "source not built". No share / transfer / check-out / check-in verb (`shared` is always 0 in the cast row; `active_book` is set to the book at mint and nothing moves it). Template is checked against the trunk `actors.pdl` (or the book's layer) by name only. `fork` records the parent but not the change. The idempotency key is looked up in `piece_ledger.txt`; the same key with different arguments returns the first piece. It was run for real only in the harness's scratch house; against the owner's house only with `--dry` (pals folder hashed before and after: unchanged).
