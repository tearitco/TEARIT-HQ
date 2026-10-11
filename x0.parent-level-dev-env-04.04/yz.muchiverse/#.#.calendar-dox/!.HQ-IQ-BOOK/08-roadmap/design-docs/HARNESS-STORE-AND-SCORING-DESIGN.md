# Harnesses as scored, customizable, reusable, sellable store items (design)

Status: DESIGN ONLY, 2026-10-07, claude. Owner: "we can score, customize, reuse, sell harnesses in the store etc." Builds on `AUCTION-SCREEN-DESIGN.md` (NIGHT 33: auction/marketplace rows and the settlement gate), `HARNESS-BEHAVIOR-BANK-DESIGN.md`, NIGHT 31 (a test is a Watch record), `GAME-BUILDING-BLOCKS-AND-TOOLS-DESIGN.md`.

## 1. What a harness item is
A **harness** = a pal + a cases `.pdl` + (optional) a scratch fixture, with a header naming what it proves, the files it reads, and what it does NOT cover. Already true of every harness in `&.widgits/_shared-lib/harness/`. Item = that folder subset + a `harness.pdl` manifest row set: `HARNESS | id | version | proves=... | needs=<blocks/ops> | fixture=... | license=...`.

## 2. Score (reuse the attrition model; no new scoring math)
Every case run leaves a Watch-style row (case id, verdict, counts). A harness's score is **counted, derived, never typed** (NIGHT 31): reliability = (passes + 1) / (passes + fails + 2) per case (Laplace), plus: **can-fail proof** (a recorded negative run), **coverage notes** (the "not covered" lines), **who ran it** (and on what version of the thing under test), **usage count**. Concept tags: each case carries Concept Bank words (`kind:`/`action:`) so a harness is searchable by what it tests and weightable by tomom/TEARIT. A harness with no recorded failing run scores as **unproven** (a test that cannot fail).

## 3. Customize and reuse
A buyer **forks** a harness: copies the case file and edits rows (expected values, paths, fixtures) with the same audit-row-plus-undo discipline as tomom-hq; the fork keeps a `FORK_OF | id | version` pointer, and its own score starts fresh. Reuse is **by reference** to blocks and ops (a harness lists what it needs; it never bundles another block's code).

## 4. Sell (gated, like everything that moves value)
A harness is another listing kind in the auction/marketplace ledger (`ITEM | kind=harness | id | version | price | unit`), found by the same search (concept words). **Same gate as NIGHT 33:** play money and test chains first; cones only after signing/escrow exist; the escrow built in alpha (`PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md`) is the delivery rail (lock the price, release when the buyer's copy verifies, refund otherwise) but its `agent` field is an honor field until signing exists, so trusted only locally. **Verification at delivery:** the buyer's copy is checked against the listed hash and run once in a scratch root; a harness that does not run or cannot fail is refused/refunded. Revenue split to the author, with an optional rake sink (play-economy doc).

## 5. Safety rules
Harnesses run in **scratch roots only**; a purchased harness is **untrusted code** (pal and ops): it never runs against live data without a copy and a backup, never gets network or `xyzfs/users` access by default, and its ops must be compiled from reviewed source, not binaries. Licensing: a harness that embeds third-party assets carries their licenses (CC-BY-SA attribution).

## 6. Build order
1. `harness.pdl` manifest + a catalog op listing harnesses with derived scores from existing result rows (read-only viewer).
2. Fork/customize tool (tomom-hq style editor over a cases file).
3. Store listing kind + search by concept; play-money settlement only.
4. Delivery verification run; escrow-backed purchase on a test chain.
5. Cones only after the gate.

## 7. Open questions
1. Is a harness's score public per case or only rolled up?
2. Can a fork be resold, and does the original author get a cut?
3. Which harnesses are first listed: the existing 130+ checks (chain, desk_copy, solar sandbox, pc-hq levels)?
