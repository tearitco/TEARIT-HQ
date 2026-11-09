# Wallet conventions and the open calls, decided 2026-11-02

Written by Claude (Sonnet 5.5). Owner instruction: "check for current wallet conventions. push kilo. and do those with your own call first. doc them." Labels: **RAN** = I ran it and saw the result, **READ** = read from a file, **CALL** = my decision, owner can overturn it.

## 1. Current wallet conventions (READ)

| thing | convention | source |
| --- | --- | --- |
| piece / pet wallet id | `hash_op wallet <uid>` = `e` + first 24 hex of SHA-256("wallet" 0x00 uid). uid is 8 to 64 hex | `_shared-lib/ops/hash_op.c`, `piece_mint.c`, `16.game/PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md` |
| old rule `e` + first 24 hex of the uid | **superseded**; still used only for the label `wallet_line=` in `petslike_core.c` (`ent_wallet`) | `pet-trainer/CHAIN-ECONOMY-DESIGN.md` (edited today) |
| user / house wallet | `wallet_id` + password, **unsalted SHA-256** `password_hash`, file `041.pal-chain/wallets/<id>/wallet.txt` | `chain_create_wallet.c` |
| signing | Ed25519 through OpenSSL EVP, BUILT (`sig_op`; corrected 2026-11-08: harness cases `chain_signed` 30, `ev_chain_signed` 55, `chain_escrow_signed` 45 pass); one key per account | `GAMES-AS-PACKAGES-AND-STORE-DESIGN.md` |

## 2. Wallet hashes on the public origin (RAN) - CALL: stop tracking them

- **Found:** `041.pal-chain/wallets/house/wallet.txt` and `.../jb/wallet.txt` were tracked on `claude` and so on the public `origin` (repo visibility PUBLIC, checked with `gh`). Each holds `password_hash=` (unsalted SHA-256) and a balance. No other tracked file holds a real hash (the `chain_escrow_faucet` case uses `password_hash=x`).
- **Done:** `git rm --cached` on both files (files on disk are unchanged, sha256 checked before and after) and `wallets/*/wallet.txt` added to `041.pal-chain/.gitignore`.
- **Cannot be undone by me:** the hashes are already in public history and in other branches. An unsalted hash of a weak password can be guessed offline. **Owner action: change the `house` and `jb` wallet passwords** (a new wallet file, or the chain's change-password path when it exists). I did not rewrite history: that needs a force push to a shared branch and does not remove copies already fetched.
- The three `0.user-pal/**/xyzfs/users/*/home/wallet.txt` files hold only `name`, `type`, `tokens` (no hash). Left as they are; they are old fixtures under a path the data-branch rule says must not be tracked, so a later cleanup should untrack them too.

## 3. Wallet id formula - CALL: the hash rule is canonical; the label changes with the first chain wallet

`hash_op wallet` is the one formula for anything that mints or is checked. I did **not** change the 11 games' display label today: the `pet_cast` harness (24 checks, 9 mutants) proves a cast row wins by looking at the label's uid prefix, so changing the label means re-keying that harness at the same time. Do it in one commit when the first chain wallet is created for a piece.

## 4. Kilo - CALL: push local work under a new name, never force

- **Found (RAN):** local `kilo` (`61f7294d8`) and `origin/kilo` (`d11fa69e4`) have **diverged**: 14 local-only commits (terumon, co-lab clipping, bug-bounty notes), 80 origin-only commits (31 `opencode-windows`, 49 `tearitco`). A plain push is refused and a force push would delete 80 commits.
- **Check before pushing:** the 14 commits touch 25 files under `xyzfs/users/` (terumon pal sprites, menus, meta); none is a wallet or key, and a scan of the added lines for wallet/key/secret words found nothing.
- **Done:** local `kilo` pushed as the new branch `kilo-local-2026-11-02`. `origin/kilo` is untouched. Merging the two histories is the owner's call.

## 5. The rest, one line each

| open item | CALL | why |
| --- | --- | --- |
| `events.txt` 4 MiB cap (`append_line` drops silently) | **Do not rotate yet.** Rotation must write a snapshot of the bags and block table first, because bags are rebuilt by replaying the ledger (`ge_scan`); a plain rename would reset every bag. Spec: snapshot row, then rename to `events.txt.1`, keep one | A blind rotation loses player state |
| 2 empty pet ids (`cdda-hq/harold`, `tcg-rpgce/pal_1`), `civ-rpgce/blue`, `gta-hq/nico` | **Leave unminted.** harold and pal_1 are placeholders; blue and nico are not db actors | Minting a new id on live data needs your OK and I cannot tell what blue and nico are from the files |
| rex identity | **No change.** `cast.pdl` is the identity; `pet_cast` proves the cast row wins | Nothing to decide until a second book claims rex |
| per-record db layer | **Next db step, not started**, after the wallet label re-key | Per-file branching works; per-record is an optimisation |
| R2/R3/R4 for `rpg-pvp-hq` | **Not started** | Needs the R1 skin parity run on a live board, which I held back |
| `RVLV_EMUL` force update; `tear-ix` target and commit `83ce186` | **No call possible.** I found neither string in the book or in this repo's refs | Needs the owner to say where they live |
| live drive of the edit board | **Not needed today**; nothing above needed a window | Machine froze on a board launch 2026-10-28 |

## 6. Still the owner's

Change the `house` and `jb` wallet passwords; merge or retire `origin/kilo` against `kilo-local-2026-11-02`; say what blue and nico are; say where `RVLV_EMUL` and `tear-ix` live.
