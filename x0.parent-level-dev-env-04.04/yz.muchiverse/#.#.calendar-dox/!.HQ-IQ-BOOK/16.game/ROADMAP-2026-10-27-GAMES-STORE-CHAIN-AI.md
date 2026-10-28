# Roadmap, 2026-10-27: games on the engine, Store, chain and courts, social/p2p, AI side

One page over the four docs written today (all in this folder): `HANDOFF-2026-10-27-RPG-SPACE-TO-GROK.md`, `HANDOFF-2026-10-27-AI-SIDE-COMPANION.md`, `GAMES-AS-PACKAGES-AND-STORE-DESIGN.md` (sections 1-17), and the existing `XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md`. Labels: **DONE** (built and observed), **MOCK** (runs, mock data), **DESIGN** (doc only). Order is a PROPOSAL; the owner reorders.

## 0. Where we are (FACT)
| Area | State |
|---|---|
| rpg-space / rpg-dust | DONE: playable, both gates 83 PASS, life layer, ambient asteroids, tick cost file. rpg-dust = AI/world-generation testbed (owner). Common skin is still a sed copy (DESIGN: extract it). |
| AI side | DESIGN mostly. DONE: phase-table gate, decision ledger in df/cdda/mcl, `ai_describe`, validator. Not built: a pet caller, per-actor/scoped weights, promotion. The scratch bank holds my dev-run noise (reset needed). |
| Games as packages | DESIGN: `game.pdl` manifest, parts/slots, demos, mods, shared dev folder + `access.pdl`, screening. Nothing enforced. |
| Store | MOCK: `store-hq` window opens from taskbar cell 13 (tested). Catalog has 12 rows, nothing installable. |
| Events for chain/store | DONE (tested by hand): `ev_chain`, `ev_store`, 9 registry commands. Free `get` only; test chains only; unsigned. |
| Courts / REP / disputes | MOCK: `court-hq` under Network (tested). DESIGN: dispute ledger, REP, jail/bail, my-lawyer reuse. |
| Social / p2p / user-owned data | DESIGN only. No upload exists; hq-ftp is a placeholder; `palnet_peer` is same-machine. |
| Safety rules decided | data-only for outside creators; agent proposes + owner confirms; opt-in insights; no bias in courts; 7-day window; small-amount rule-resolve; all numbers in `.pdl` per chain/game. |

## 1. Short term (next days): make the foundations real, small and testable
1. **`game.pdl` + `game_check`** on rpg-space and rpg-dust (owner's chosen first build). Then the **common-skin extraction** so parts are swappable and dust can diverge.
2. **Pal harness for `ev_chain` and `ev_store`** (repeatable, with every negative from section 17), and one **events-hq-generated page** played end to end.
3. ~~Reset the polluted scratch-bank entries + a dev-mode~~ DONE 2026-10-27 as **bank lanes** (dev lane default, entries moved not deleted; a learner for the dev lane is still design).
4. ~~Decision ledger in rpg-space~~ DONE 2026-10-27 (pets write rows, pick chance reads the weight; shared code is now the op `dec_op`). Next: the `why`/`score` column, then **move `petslike_life.c` to an op** in five slices (assessment and plan: design doc section 18; first slice `life_op tick` on a slow cadence).
5. **`policy.pdl` reader** with range checks and owner-signed changes (court, store and chain read it at call time); per-game spend caps for `ev_chain`.
6. **Move `store-hq` and `court-hq` onto the same ops** the events use (one path for window and games). Measure both windows' footprint with a browser open.
7. Housekeeping: owner review of the cpu_guard alert; taskbar-owner review of the `store` cell C change and the Network menu row; push-ready commits only.

## 2. Medium term (weeks): the money and trust layer, and the first creator loop
1. **Signed transactions** (owner confirmed "we will do the crypto sigs"; design in GAMES-AS-PACKAGES-AND-STORE-DESIGN section 19: Ed25519 via OpenSSL, a `sig_op`, verification in issue/mining/replay, `SIGNING required` per chain) on the test chain (prerequisite for disputes, REP, social identity). Separate social signing key from the wallet key.
2. **Dispute + resolution ledgers, deterministic small-amount rules, 7-day settlement hold**, REP ledger and its two gates (payout wait, posting reach). Negative tests: stolen-key purchase reversed, false dispute refused, thief cannot reverse back.
3. **`ev_store buy`** (escrow lock -> payout after the window -> refund on failure) and **install**: quarantine -> screening (integrity, deny-list, contract, scratch gate, budget; agent proposes verdict, owner confirms in batches) -> copy into the player's own folder.
4. **Pack builder + screener** for games; the shared dev folder `xyzfs/shared/games/` with `access.pdl` and a reader used by the launcher; first residents rpg-space and rpg-dust.
5. **Content spine** for the voxel game-making engine: pack format + `pack_check`, the `content propose/confirm/undo` verbs + spawn policy rows, generators as data, a one-line judge command, a `skin_port` op, a settings window generated from `tuning.pdl`.
6. **AI threads A and C first:** pets learn from outcomes (scoped weight chain), world/asteroid generation loop (describe -> validate -> review -> bank) with the user-asteroid pipeline. Then D (skills/grades) and B (plan-proposing pets, needs a snapshot judge).
7. **Courts for real:** adapt my-lawyer's docket/case files/describe-only judge to disputes; evidence = ledger rows only; bias = 0 (tested); jail/bail/representation rows; liability share + cap from `.pdl`.
8. **Ledger-to-event watcher** so the Store/chain can notify game pages (OWN recorded, escrow paid, dispute opened).

## 3. Long term (months): open it to outside users and other people's data
1. **Outside users:** role enforcement (`role=` is only a marker today), `access.pdl` enforced everywhere, the public store-items repo (owner publishes only; deny-list + screening before anything is published), per-creator repos as extra sources, capability manifests (`CAP | chain.send | max=...`) for data-only packs.
2. **Social + user-owned data:** images (PNG validation, size caps), `hq-ftp` for two LAN houses (first real transfer), a feed from append-only rows, per-user export to the user's own GitHub with a dry-run plan, then real p2p after the cross-machine networking work. Tokens and keys never leave the user's folder.
3. **Store as a platform:** Steam-like look (capsule art, reviews with rule-based filter, libraries, updates), demos generated from the full manifest, insights back to creators (opt-in aggregates), mods naming their base game; later source adapters for other platforms.
4. **AI attrition model growing up:** more edit types (new concept node, FSM transition, planner action), promotion designed from real review history, tomom learning from the ledgers, `auto` studio for creators' repetitive work. Always: describe never classify, models never set numbers, no auto-promotion.
5. **Hardening:** real money out of scope until a payment provider and legal review exist; external-chain value stays irreversible and out of the reversal model; courts' appeal to the owner stays.

## 3a. Owner's priority call (2026-10-27, after the vision synthesis)
**North star: the learning flywheel** (VISION-CLARIFIED sections 8): micro-delegate tasks to Gemma 270M and free APIs as early as possible, train in-house reusable weights/models/datasets, compact and refine; every feature should feed it. **Showcase milestone: one space with several small games and one central editor** (section 9: pong, space invaders, slots, pet-faucet with a subchain, Elo tactics like BOTS). First flywheel turn: a `exam_taker` micro-task (Gemma 270M answers the chemistry exam, graded by code, worker grade per question kind). Save truth: the shared weights; both fidelity levels are views.

## 3b. Standing principles added later today (owner)
* **Engine grows and compacts** (design doc section 21): every growth slice pays a compaction tax; data over code, ops over copies, one verb family not many commands, retire text includes; measured baseline and a compaction ledger.
* **Life is evolvable** (section 20): needs and rates are data and weights, AI proposes new needs/variables/events through the validated review loop, scored in the dev lane; **anatomy and chemistry are optional AI-built packs**, fidelity opt-in per entity.
* **Life op slice 1 DONE** (section 18): hunger/starvation clock is `life_op`; slices 2-5 next.

* **Dual fidelity over shared weights** (design doc 23.1): abstract and complex levels read the same trainable weights; applies to strength, needs, anatomy, chemistry.
* **Knowledge domains:** chemistry exams (P2) DONE in `chem_label` (seeded, graded by code, shown in Knowledge HQ); next `part_tree_lint` (anatomy v0: brain, stomach, blood, bones, lungs, limbs and fingers), then needs on top (air, thirst, rest, exercise). `sig_op` (signature step 1) DONE.

## 4. Standing risks to watch
* Public GitHub history is permanent: screen before publishing, never publish `xyzfs/users` or `user/*`.
* A 270M judge must never move real value alone.
* Unsigned chain ops are honor fields: test chains only until signing lands.
* Everything must stay cheap beside a browser (idle cost near zero, niced one-at-a-time heavy work, `tick_stats` and `cpu_guard` as findings in our own footprint).

## Update 2026-10-27 (late): genuine house bank started
Old test cones removed (backed up), `house` wallet created with 3 mined blocks, faucet design = seeded game bank with rake and payout floor on a test sub-chain, central editor = rpg-edit-hq, games as books. Next: exam taker (Gemma, graded by code), free-API cap probe, sig steps 2-6, hub shell.

## CTO call, 2026-10-27 (late): order of the next builds and why
1. **Harness cases for what was hand-run today** (exam verbs DONE: `chem_exam.pal`, 21/0, negative-tested; next `sig_op`, `ev_chain`, `ev_store`). Reason: every claim so far rests on my scripted runs; locked cases are the only thing another agent or a cheap worker can re-run, and the flywheel needs judges that can fail.
2. **Signatures steps 2-4** (wallet pubkey, signed TX lines, verification at issue/mining/replay). Reason: the house bank now exists and the faucet, the store and courts all depend on who-signed-what; everything money-shaped waits on it.
3. **`part_tree_lint` + anatomy v0** (generic body). Reason: the next verifiable domain, the cheapest second flywheel family after chemistry.
4. **Hub shell + pong + one central-editor edit** (showcase), then slots + faucet sub-chain once signatures exist.
5. **Router rows + dataset rows for the exam family** (the flywheel plumbing), using the Mac Gemma only; API quota reserved for escalation.
Deliberately NOT next: more design documents (the design is ahead of the evidence), needs layers (air/thirst/rest) until anatomy and weights files exist.

**Update (signatures):** steps 2-3 built (wallet pubkey, `SIGNING` chain row, signed `chain_send` with nonce replay check, harness `chain_signed.pal`); step 4 (miner verifies at inclusion, design section 26) built, harness 30/0. Next: escrow and `ev_chain` signing under the policy cap, key binding on the chain, then the real chain flip (owner).

**Update (see-through walls, 2026-10-27):** GPU shader cutaway built (`view_cut_on/alpha/radius` in view.pdl or hq_ui.pdl, on in the rpg-space and rpg-dust overlays): wall/roof cells in a tube from the camera to the character are blended over what is behind. Headless test `board-viewer/ops/bv_gpu_cut_test.c` (build line in its header) 9 checks, negative-tested; before/after pictures from the synthetic scene only. NOT verified in the live game (engine needs a restart), CPU fallback draws walls solid, sprite-style buildings (`building_draw=sprite`) are boxes and are not blended.

**Update (signed escrow, 2026-10-27):** LOCK / PAYOUT / REFUND are signed and verified at issue time and at mining (design section 27), one nonce space with TX; harness `chain_escrow_signed.pal` 45/0 with three negative mutations. Correction to the order above: the `ev_chain` and `ev_store` harness cases already existed (23/0 and 24/0, committed earlier today); item 1 of the CTO call was stale. Next: `ev_chain` signing through a key file under the POLICY cap, key binding on the chain, then the real chain flip (owner).

**Update (ev_chain signing, 2026-10-27):** `ev_chain` signs through `sig_op` under a per-wallet daily POLICY cap (design section 28); key in the owner folder (never in a chain folder, mode 0600), passphrase from an environment variable, no unsigned fallback; harness `ev_chain_signed.pal` 55/0 with six mutations. Next: check `ev_store` on a signing chain, key binding on the chain, a shared `policy.pdl` reader with owner-signed changes, then the real chain flip (owner).
