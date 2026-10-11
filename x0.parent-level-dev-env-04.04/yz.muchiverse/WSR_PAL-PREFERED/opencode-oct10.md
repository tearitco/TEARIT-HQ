# opencode-oct10 - what landed, what to do next, and why bonds must be fixed

Written 2026-10-10 by claude (Sonnet 5.5) for the opencode agent and whoever works the WSR / AI lane next.
Scope: WSR economy, the LLM pilot, the entity word bank / chain SCORE / promotion ledger. NOT the browser (spans, tables, nb-render): that work is merged and needs no action beyond the checks in section 2.

House rules that apply (AGENTS.md): commit only your own paths with an explicit pathspec, never `git add -A`, never touch `xyzfs/users`, no stash, no reset with other agents' work staged, every "done" needs a fresh build + fresh run + evidence. Owner rules: harnesses are pal (`&.widgits/_shared-lib/harness`: pal + cases pdl + case/verdict ops), not new `.sh`; free models only; Ollama calls go to the Mac (URL from `ai_backend.pdl`), never this machine; heavy background work under `nice -n 15 ionice -c3`.

## 1. What happened to `origin/opencode` (2026-10-10)

`origin/opencode` (51 commits, 75 files) was merged into `claude` and `main` (merge commit `012c41a89`, parents `ef3b548e2` and `4cdecfcaf`), done in a scratch worktree (`/home/no/staging/merge-opencode`, branch `merge-opencode`, left in place).
- Two conflicts, both resolved:
  - `041.pal-chain/ops/chain_balance.c`: header comment only; kept main's transaction-type docs (TX/FAUCET/LOCK/PAYOUT/REFUND) plus your SCORE paragraph. The code skips SCORE lines (`chain_balance.c` ~156).
  - `^.hai-horn/ops/halo_chat_validate.c`: **a real design difference.** Main already had the policy-driven auto-promote from `e30c59494` (policy file, in-process ledger, score >= 0.90 AND N >= 20). Your side shelled out to `promotion_ledger.+x` with a hardcoded 0.90 and no N gate. **Main's version was kept; yours was dropped from that hunk.** If anything calls `promotion_ledger.+x` expecting the old auto-promote behaviour, check it.
- `xyzfs/users` tracking on your branch (1,766 files) was NOT brought in (0 user paths changed). Live user data was backed up first (`/home/no/backups/xyzfs-users-20261010-184903.tar.gz`, 2,962 files, sha256 list beside it).
- Compiled in the worktree: `chain_balance.c`, `halo_chat_validate.c`, `khtpm_core_render.c`, `dump_frame_png_op.c`, `khtpm_entity.c`. **Nothing else was built or run.** The live programs are still the pre-merge build: run `sh '$.crypts/button.sh' build` before judging any of this.
- Four tracked WSR binaries had local rebuild changes and were replaced by the branch version (backup: `/home/no/backups/wsr-binaries-20261010-184903/`): `WSR_PAL-PREFERED/system/{chtpm_parser_pal,keyboard_input,prisc+x,renderer}`. Their file mode also changed 644 to 755. They are build outputs; rebuild if they misbehave.

## 2. First: make the merged work trustworthy (do this before new features)
1. Build the house, then run the entry points that exist: `nb_all_tests`, the word-bank selftests, the table/spans tests, `chain_balance` with a SCORE line in `data/blockchain.txt` (it must not change any balance).
2. One live pass: `bash run_xod_agent.sh --live --goal survive --model gemma3:1b --cycles 10` (model via the Mac). Record fitness before/after in the pilot doc.
3. Confirm the auto-promote decision above: run the promotion-ledger / halo tests against main's policy-driven `halo_chat_validate`. If your pipeline needs the shell-out variant, say so in this file and reconcile deliberately, do not re-resolve silently.
Evidence rule: a state file or a diff for each step.

## 3. Bonds are broken and must be fixed (owner request): peg them to government lending

### 3.1 What the code does today (verified in the source)
- `ops/corp_action.c:131-137` `issue_bonds`: adds the amount to `bonds_outstanding`, hardcodes `bond_rate = 0.06`, and **credits the corporation's cash out of thin air**: nobody lent the money.
- `ops/corp_action.c:139-147` `buyback_bonds`: pays from corporate cash to **nobody**.
- `ops/corp_apply_finances.c:102-108`: every pass subtracts `bonds_outstanding * bond_rate` from the corporation's cash. **The interest goes to no holder**; the money just disappears.
- `bank_loan_op.c` loans use a separate hardcoded 0.08.
- Governments have no bonds at all (`docs/ROADMAP.md` Phase 2: "No government bonds exist at all", no treasury, no yield curve, no borrowing action in `gov_decide.c`). `gov_decide.c` makes fiscal decisions from `debt_to_gdp` and `net_operating / gdp`, but nothing ever changes `debt_to_gdp`, and `gdp` is a static template seed.
- A bond is therefore an accounting number, not an asset anyone can own. Money is created on issue and destroyed on interest.

### 3.2 What it should be (the owner's model)
Bonds are lending, and the price of lending is set by the government:
1. **Government lending is the benchmark.** Governments issue treasury bonds (that is how they borrow: a deficit becomes debt and now costs interest). The sovereign yield (policy rate plus a premium that rises with `debt_to_gdp`) is the risk-free rate of the economy.
2. **Corporate bonds are corporate borrowing pegged to it.** `corporate_rate = sovereign_yield + credit spread` (spread from leverage, cash, fitness). The hardcoded 0.06 (`corp_action.c`) and 0.08 (`bank_loan_op.c`) become spreads over the sovereign yield. This is the roadmap's item 2.5 ("rate benchmark").
3. **Every bond has a holder.** Issuing a bond is a trade: a holder (the player, a pet through a holder folder, a bank, another corporation, a government) pays cash and receives the bond; the issuer receives that cash. Coupons are paid by the issuer **to the holder**, principal is returned at maturity, early buyback pays the holders.
4. **Money is conserved.** Every cash movement is a +/- pair in a ledger; a check proves the sum of all cash (corporations + governments + banks + player + pets + burn) is unchanged by a bond issue, coupon, buyback or default.
5. **Governments lend, too.** A government with a surplus can buy corporate bonds or lend to banks; a deficit government issues treasuries. The existing `gov_decide.c` modes (weighted, LLM, human) get a real borrowing action ("issue_treasury") next to cut/raise taxes.

### 3.3 Order of work (matches `docs/ROADMAP.md` 2.3-2.6; do these in this order)
1. **Bond ledger.** An append-only `bonds` ledger: `ISSUE|id|issuer|holder|principal|rate|maturity|ts`, `COUPON|id|ts|amount`, `REDEEM|id|ts|amount`, `DEFAULT|id|ts`. State is replayed from it, like the house ledger rule (a lost cache never loses a bond).
2. **Holder side.** Player and (later) pet holder folders hold bonds the same way they hold shares (`holdings` file shape, see `SOCIETY-ECONOMY-ARCHITECTURE.txt`). A bond appears in the portfolio with a yield-to-maturity column (the original WSR had that: `financing.c:79-97`).
3. **Fix corporate issuance** so `issue_bonds` finds a buyer (at minimum the bank/government bid; otherwise the issue fails) and money moves; fix coupons and buyback to pay holders. **Hard constraint from the roadmap:** interest is computed on the dollar `bonds_outstanding` field, never on `shares_outstanding` (that is a millions-scaled profile figure).
4. **Government bonds** (2.4): treasury issuance, yield curve driven by `debt_to_gdp` and a policy rate, annual coupons; this makes `debt_to_gdp` live because debt finally costs something.
5. **Peg corporate rates to the sovereign yield** (2.5): bond spreads and loan spreads, not constants.
6. **Put the finance pass on the clock** (2.6): `corp_apply_finances` still runs per End Turn while the clock is wall-clock driven; coupons need one cadence.
7. **Do NOT fudge GDP.** The roadmap warns that tax amounts and the GDP basis must be reconciled deliberately (a static GDP of 426 makes `gov_decide` raise taxes forever once real money moves). Build GDP from the auction, do not hide the mismatch.

### 3.4 Proof (harness first)
Write a pal harness (cases pdl + case/verdict ops) before the first change, with at least: bond issue conserves total cash; coupon moves cash issuer -> holder and nowhere else; buyback pays holders; a government deficit raises debt and the sovereign yield; a corporate rate moves when the sovereign yield moves; interest uses `bonds_outstanding`. Run it in a scratch copy, never on the real folders (Night 49 did the same: "ran in a scratch copy"). The existing three-round economy tick (1.7 s) is the baseline to compare against.

## 4. Other work for this lane, in order
1. **Tournament harness (the locked judge).** `tournament_xod.sh` and `live_tournament_xod.sh` are on the pilot doc's roadmap; fitness/evolve/chart ops exist. Build the judge as a pal harness. Pets must never be able to edit the judge, so lock it before anything learns against it.
2. **Economy holes beyond bonds** (Night 49 notes `XO/21.blackjain/NIGHT_49_THE_MARKET_GAME_AND_THE_PETS.txt`): pet holder folders so dividends reach pets; a currency bridge (market dollars <-> cones, with a fee and a cap, books must still add up); settle-path bug (helper run from the wrong folder, "dividends will use a stale index") if it still exists after your WSR pipeline fixes.
3. **Real learning in the empty slot.** WSR decision mode 2 ("reinforcement learning") is a stub that falls back to weighted. Fill it: first the plain weighted rule scaled by risk appetite and needs, then RL on play money with the tournament harness as judge, then learning from human trades (inverse RL layer, NIGHT 11). Small models are biased (gemma3:1b defaults to `buy_stock`); your C fallback in `llm_brain.c` is the right pattern: the model is optional, the plain rule always exists.
4. **Word bank to chain to pets.** SCORE records and `chain_bank_query` exist; next is a real consumer (the rpg-pet monsters could use their own bank for replies, `@.apps/rpg-pet`), plus the human review step of the attrition model.
5. **WSR window as a second tab of the exchange window** (the shortest road to a window; the exchange is `&.hq-apps/exchange-hq`). It needs a stocks table, order book, holders and, after section 3, bonds with yields.
6. **Read before coding:** `ROBOT-CHAT-BLUEPRINT.md`, the latest `2do.md`, `docs/PORT-FIDELITY.md`, `docs/ROADMAP.md`, `LLM-HOUSE-PILOT.md`, `SOCIETY-ECONOMY-ARCHITECTURE.txt`. An earlier agent invented a disconnected mechanism by skipping these.

## 5. Where things are
- WSR: `WSR_PAL-PREFERED/` (`ops/` C ops, `pal/main_loop.pal`, `docs/`). Bond code: `ops/corp_action.c`, `ops/corp_apply_finances.c`, `ops/bank_loan_op.c`, `ops/gov_decide.c`, `ops/gov_trade.c`, `ops/corp_ipo.c`.
- LLM pilot: `LLM-HOUSE-PILOT.md`, `ops/xod/llm_brain.c`, `ops/xod/fsm_controller.c`, `ops/xod/wsr_goap_planner.c`, `ops/xod/tom_layer.c`, `ops/xod/wsr_fitness.c`, `ops/xod/wsr_evolve.c`, `run_xod_agent.sh`.
- Word bank: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/ENTITY-WORD-BANK-IMPLEMENTATION-STATUS.md`; chain: `041.pal-chain/`; `chain_balance.c` cites `PAL-CHAIN-STANDARD.txt` sec. 8 for SCORE records but I could not find that file in the tree (searched the house), so the cited spec may be missing or renamed.
- Chain/pet economy design: `@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md` (exchange, auction, pet wallets).
