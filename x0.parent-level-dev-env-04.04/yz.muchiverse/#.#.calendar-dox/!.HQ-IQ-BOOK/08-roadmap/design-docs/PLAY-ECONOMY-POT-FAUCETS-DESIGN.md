# Play economy: Elo, the play pot, and where play money comes from

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built (except what "exists" names).
Owner brief (2026-10-07): "elo and play pot to win; players can go to faucets, watch ads, try to mine blocks, do quests, etc. Doc it."
Part of the house session pattern: `GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md`. Currency gate and screens: `AUCTION-SCREEN-DESIGN.md`.

## 1. What exists (read, not assumed)
- **Faucet precedent:** `01.muchi-pals` plans "Main -> Faucet (tokens) -> Store (free starter / buy clone @20)", clones "bought with faucet tokens (local wallet in xyzfs for now)", and says "pal-chain currency vs local faucet tokens: local for now" (`0.a-z-pets-plan/a-z-pets-plan-midterm.md`); its `generate_egg.c` is the faucet's generate verb.
- **Mining is real:** `041.pal-chain/ops/chain_miner.c`: SHA-256 proof of work, block reward `INITIAL_REWARD_MILLICONES = 10500` halving every `HALVING_PERIOD_BLOCKS = 1000`, `TOTAL_SUPPLY_MILLICONES = 21000000` (these are real cones; transactions are unsigned, so cones cannot settle for real: see the auction gate).
- **Quests exist** (`^.grave/quests/Q001..`, each with a deterministic `verify.sh` scorer; the server's `quest.score` records the verdict). **No reward field exists** in the quest files I checked.
- **Ratings:** Elo per game kind from TSC_ELO (new = 1000, K=32).
- **No ad system exists anywhere.**

## 2. Two currencies
- **Play money:** no gate, its own append-only ledger, balances derived by replay (the chain's `chain_balance` pattern). Free to mint through the sources below, with caps. Used for pots, testing, DSR, the auction screen's `unit=play`.
- **Cones:** the real chain. Stay behind the gate (signing, escrow, cross-machine, provable identity); a pot in cones waits for chain escrow.

## 3. Elo and the pot are separate
**Rating = skill** (derived from results, never bought). **Pot = stakes.** A session may declare a `pot` (default 0 = off). Each seat pays an **entry stake** into a **session escrow** at join; at the result the escrow pays out by a placement table, minus a small **rake** that is destroyed (a sink): ranked 2-seat = winner takes the pot; FFA = tunable split (e.g. 50/30/20). Play-money escrow is trivial (own ledger, single writer, rows `STAKE | session | player | n`, `PAYOUT | session | player | n`, `RAKE | session | n`). A cone pot needs the chain's missing escrow transaction. The rating changes the same whether or not there was a pot.

## 4. Where play money comes from (sources), and where it goes (sinks)
Every mint is one ledger row tagged `src=`; per-source **daily caps** and cooldowns live in `play_economy_tunables.pdl` (named joints, not hardcoded).
1. **Faucet:** claim N coins per cooldown per identity (`src=faucet`); a cooldown row is the only state. Generalizes the muchi-pals faucet into the house play-money wallet (decision: reuse that wallet or start the shared ledger fresh; the plan says local tokens are separate from pal-chain currency).
2. **Ads:** no ad system exists. Proposed closed loop that needs no outside network: an **in-game store or civ pays a marketing budget** (DSR marketing line) and a viewer who watches the ad (a short, skippable-after-N-seconds screen, one per interval) gets coins from that budget (`src=ad`, `AD_VIEW | ad | viewer | ts`), while the advertiser sees view counts. So ads are not free money: the advertiser's budget funds them. Real external ads are out of scope and not designed.
3. **Mining:** the real miner mints real cones, which are gated. For play money run a **play chain**: the same miner code on a separate ledger with **low difficulty** and a play reward schedule (`src=mine`), so "try to mine blocks" is real proof of work without touching cones. It burns CPU on a weak machine: run at low priority with a per-day cap.
4. **Quests:** add a `reward` row to a quest/task `.pdl` (event-shaped task data, `TASKS-AS-EVENT-DATA-DESIGN.md`); a passing `verify.sh` (via `quest.score`) pays out once (`src=quest`).
5. **Others, same mechanism:** school grade passed (`ENTITY-SCHOOL-YEARS-DESIGN.md`), daily login, match wins, DSR achievements.
**Sinks** (to hold inflation down): pot rake, entry/lobby fees, DSR rent, taxes and purchases, marketplace/auction fees. A weekly report of minted vs burned per source (derived) is how the owner tunes the caps.

## 5. Why this fits the house
Single writer per ledger; balances derived by replay; every mint/sink a row with a source tag; caps are tunables; no model decides payouts; play-money steps rehearse in beta and each op gets a pal harness (cap enforced, cooldown, escrow conserved: stakes in = payouts + rake out, replay equals balance).

## 6. Build order
1. Play-money ledger + `playmoney_balance` op + harness (mint, spend, caps, conservation).
2. Faucet claim op + cooldown rows.
3. Pot escrow for sessions (stake/payout/rake) on the session ledger; first consumer TSC_ELO (finish its winner + Elo write-back proof).
4. Quest `reward` row and payout hook after `quest.score`.
5. Play chain (low-difficulty miner on a separate ledger) with CPU cap.
6. Ad view loop funded by a marketing budget (needs DSR stores' budgets).
7. Weekly mint/burn report; cones pot only after the chain gate.

## 7. Open questions for the owner
1. Reuse the muchi-pals faucet/wallet as the house play-money wallet, or a fresh shared ledger?
2. Default faucet size/cooldown and daily caps per source?
3. Pot default off; entry stake sizes; rake percent; FFA payout split?
4. Ads: the closed-loop (advertiser-funded) idea acceptable, or did you mean real outside ads?
5. Should play-chain mining coins convert to cones one day, or never?

## 8. Owner decisions (2026-10-07, later the same day)
- **Chain escrow and a faucet: build them.** Spec `PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md`; build in the alpha branch (report appended to that spec when done).
- **Three kinds of chain:** `cones` (the real one, no faucet), **test chains**, and **user-created chains** ("lab around and figure stuff out"); **each test chain has its own faucet** (so the section 4 "play chain" becomes just a test chain).
- **Ads: fake for now.** A placeholder ad screen that records `AD_VIEW` rows and pays from a test budget; the advertiser-funded loop stays the later design.
- **Mining: real, with a daily cap** (per wallet per day, in the chain's `chain.pdl`; the cones chain gets no cap until the owner picks a number).
- **Quests pay in three flavors:** (1) **house-sponsored tutorial rewards** (the house funds a reward for finishing a tutorial step); (2) **in-game quests** (issued by a game, e.g. a DSR store or castle, paid from its treasury); (3) **user quests** (a user posts a quest and a reward). **User and in-game quests use the new escrow:** the poster's reward is **locked** when the quest is posted, **paid out** when the deterministic check (`verify.sh` via `quest.score`) passes, **refunded** if the quest is cancelled or expires. So the escrow is not only for pots; it is the quest board's payment rail too. Needs a `reward` row in the task `.pdl` (`TASKS-AS-EVENT-DATA-DESIGN.md`) with fields `reward_amount`, `chain`, `sponsor` (house/game/user), `escrow_id`.
- **Inflation control stays as designed** (caps, sinks, rake), per chain.
