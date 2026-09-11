# HARNECIENT NIGHT track — horizon items pulled into the real roadmap

**Status: ON THE HORIZON, not started.** The 10-lesson NIGHT track
(`1-1.HARNECIENT.SMOL/NIGHT_01..10_*.txt` + matching `.mp3`) is
onboarding/export content — training material for students/agents
learning this house, not itself a dev-dox roadmap entry. But several
of the NIGHTs surfaced real, concrete, buildable proposals while
researching them (each one grounded in real code found live, not
invented), and those deserve a real pointer here so they don't only
live inside audio lessons. This doc is that pointer — terse, one entry
per real proposal, no re-narration of the lessons themselves.

## The items

1. **pc-hq trigger layer** (NIGHT_05) — events-hq/db-hq's Common
   Events integration is real and done; pc-hq's own game board never
   fires one. Proposed: a new append-only `board_events.txt` the
   engine writes on real triggers (`touched_npc:NAME`, etc.) + a small
   separate watcher process that maps trigger strings to Common Event
   IDs and fires them through events-hq's existing dispatch. Smallest
   provable version: one NPC, one trigger, one Common Event, checkable
   end to end. See item 1 above (events/db-hq registry) — this is the
   real trigger-side half of that same gap, not a separate one.

2. **Wager-chess / cross-chain exchange on the REAL chain** (NIGHT_06)
   — `pal-chain` (`041.pal-chain⛓️`) already has real wallets (real
   SHA-256 auth, `chain_create_wallet.c`) and a real block-processed
   chain (`last_processed_block` confirmed non-zero on a real wallet
   read off disk). Proposed: read `PAL-CHAIN-STANDARD.txt` before
   building anything — confirm whether an escrow transaction type and
   a cross-chain exchange rate already exist, partially exist, or are
   explicitly out of scope, and design wager-chess's stake/payout
   against whatever's actually there instead of a new ledger.

3. **`cursword_say()` — finish the real stub** (NIGHT_07) —
   `*.monads/*.cursword/ops/cursword_fsm.c` already documents the
   real FSM-first/Gemma-optional pattern (model optional, canned-
   string fallback always present) but `cursword_say()` itself is
   still a stub. Proposed: implement it for real, then PROVE the
   fallback path (kill the local model process mid-run, confirm the
   canned line still plays) before building any new FSM on top of it.

4. **`decision_mode` extension — a GOAP-candidate mode and a trained-
   policy mode** (NIGHT_08/09) — `014.wsr-pal/ops/corp_decide.c`
   already has a real, working `decision_mode` dispatch (weighted
   formula / rule / llm-via-gemma3:270m / human). Proposed: add a
   `goap` mode (a deterministic planner generates candidate plans;
   Gemma is the optional chooser between them, same call shape as the
   existing llm mode) and, later, a `policy` mode (a small RL-trained
   lookup/network, trained fully offline against simulated GOAP-plan
   runs — never a live model call at runtime).

5. **Gemma-authored `weights.txt`** (NIGHT_09) — the same
   `decision_mode=1` weighted formula reads its risk-bias numbers from
   a real `weights.txt` a human currently hand-tunes. Proposed: a
   small offline tool that hands Gemma a real corpus/term list, asks
   for a per-term weight proposal (one pass, short-span judgment —
   the same shape as `corp_decide.c`'s own real mode-3 call), and
   writes a draft `weights.txt` a human reviews line-by-line before it
   ever reaches the real file `corp_decide.c` reads. Extend, later, to
   proposing new terms/categories with draft weights — same review
   discipline, no auto-merge.

6. **Swap `rand()` cold-start for Gemma-authored priors in the real
   attention chatbot** (NIGHT_10) — `#.Z.HUMAN_LLM/3.stage.llm.
   tomom@qroq.fame]921🐋️/attention.c`'s `struct VocabEntry` has a real
   per-word `weight`/`embedding` field, currently `rand()`-initialized
   every training run. Proposed, smallest-risk first: swap the random
   init for a Gemma-authored starting weight per word (item 5's own
   tool, pointed at this vocabulary) — zero architecture change,
   `trainer.c`/`forward_prop.c`/`backward_prop.c` untouched. Only if
   that's insufficient: extend the existing `chatbot_moe_v1.c`
   Mixture-of-Experts "curriculum" system (add an LSTM layer for
   turn-to-turn memory; route curricula via item 4's own GOAP/policy
   modes). Custom architecture from scratch is explicitly the last
   resort, not the first instinct, per NIGHT_10's own ranking.

## Not resolved by this pass

"Agent 45"/"Iqabella," named directly in the request that produced
NIGHT_10, were not independently confirmed as their own distinct
chatbot architecture — flagged honestly in NIGHT_10 itself rather than
guessed at. If they're real and separate from the qroq/MoE project
above, they need their own real file-path pointer before any of this
roadmap assumes they're the same thing.

## Related

- The NIGHT lessons themselves: `1-1.HARNECIENT.SMOL/NIGHT_05..10_*`.
- `08-roadmap/OPEN-ITEMS.md` item 1 (events/db-hq registry) — item 1
  above is the trigger-side half of that same gap.
- `TILESETS-EVENTS-AND-GAME-CLONES.md` — the wider game-clone catalog
  item 1's pc-hq trigger layer feeds into.
