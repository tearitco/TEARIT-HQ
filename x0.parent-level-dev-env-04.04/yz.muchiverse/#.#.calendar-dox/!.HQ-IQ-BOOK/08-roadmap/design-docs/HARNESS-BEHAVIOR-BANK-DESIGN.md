# Harness behavior bank (weights, synonyms, sentences, concept slots)

Written 2026-10-07 (claude). **v1 built** for the ten pal harnesses (`&.widgits/_shared-lib/harness/bank/`, op `harness_bank_op`); the AI-layer wiring is **not** done. Owner words that drove it:

> "do these harnesses get .pdl associated weight / bank / synonyms for use with the ai layers yet? i'd like that if you know how, infer based on dox"
> "they are like a behavior bank + 3d associated synonyms for the behavior" / "sentences, words, etc in the bank related to how it works. seo" / "maybe under 'hidden layer'"

Answer to the first question: **no, they had none**; this is the first version. Related: `OPS-BANK-DICTIONARY-DESIGN.md` (the dictionary these entries live in), `LLMUD-HACK.md` sections 4-5 (Behavior Bank schema + Laplace weight), `AI-TRACK-BRAINSTORM-QUESTIONS.md` 9b (Concept Bank, z-nodes, hub-and-spoke), `A-TEARIT-IS-ALL-YOU-NEED.md` (Bank Layer).

## 1. What the docs say (read, 2026-10-07)

- **Behavior Bank entry** (`LLMUD-HACK.md` section 4): `id`, `keywords[]`, `synonyms[]`, `sequence[]` (the actions), `weight`, `source`, `timestamp`, `observationCount`, `rewardCount`, `punishCount`. A plain-text/pdl file, human-visible and editable, not opaque state.
- **Weight rule** (section 5): `weight = (reward + 1) / (reward + punish + 2)`, Laplace-smoothed, a fresh entry starts at **0.5** (the doc explicitly corrects the example's 1.0). The doc leaves open whether time decay is needed.
- **Concept Bank** (`9b`): **z-nodes are named abstract concepts** (`force`, `motion`, ...); **hub-and-spoke**: a word/spoke points at **master** concepts only, via slots `SLOT | idx | POINTS_TO=<master> | WEIGHT=<-1..1>`; masters may point at masters; **weights live in exactly one place** (the spoke); a **second axis** of **corpus-level meta-weights** (`(Mathematics, Physics) -> 0.8`) is explicitly requested; the Synonym Bank in the source doc had no weight field (open question 4 there).
- "Hidden layer": **the term does not appear in any doc** (searched). The structure that plays that role is the z-node layer: it sits between the words people type and the behaviors that run, and nothing outside the bank addresses it directly.

## 2. My inference (owner: confirm or correct)

"Behavior bank + 3D associated synonyms" read as **three association axes** around one behavior (a harness):

| Axis | In the sidecar | Meaning |
|---|---|---|
| 1 words | `KEYWORDS`, `SYNONYM \| word \| weight`, `SENTENCE` | what a person (or a search) would say to mean this behavior; sentences describe how it works, so free text finds it ("SEO") |
| 2 concepts (the hidden layer) | `SLOT \| i \| POINTS_TO=<master> \| WEIGHT=<w>` | the z-nodes it is about (`play-mode`, `map-access`, `process-lifecycle`, ...), weighted; two harnesses that share masters are related |
| 3 context / corpus | `CORPUS \| name \| WEIGHT=<w>` | the second axis from 9b: which area it matters in (`game-engine`, `debug-build`, `ai-layers`) |

plus the **behavior** itself: `SEQUENCE | n | exec ...` (what the pal runs) and the **measured** part: `COUNTS | reward | punish | cursor` and `WEIGHT`.

## 3. What is built (v1)

- **Seeds**: `harness/bank/<case>.behavior.pdl` for all ten harnesses (tracked). Keywords, synonyms, sentences, slots and corpus weights are **hand-authored seeds** (marked `SOURCE | hand-authored`), not learned.
- **Live weight, the one measured number**: after each run the pal's last line `exec harness_bank_op <cases.pdl>` reads the harness's own results ledger and adds its **PASS rows to reward and FAIL rows to punish**, using an **append-only cursor** (byte offset; the house marker rule, never mtime), so a ledger row is counted once. The entry weight is then the Laplace value above. Live state is written to `bank/live/<case>.behavior.pdl` (git-ignored) so the tracked seed never changes on a run.
- **`find`**: `harness_bank_op find bank <word>...` ranks entries: per word the best of keyword **1.0**, weighted synonym **its weight**, z-node name **0.6 x slot weight** (hyphenated names split), word in a sentence **0.25**; summed over the words, **times the entry weight**. Examples run 2026-10-07: `restart` -> close-listed 0.952, proc-ledger 0.909; `teleport` -> transfer-map-access 0.947; `save game` -> game-slots 0.786; `minimize` -> hotbar-minimize 0.909.
- **Tests**: the pal harness `harness_bank.pal` (18 checks: counts, Laplace value, cursor idempotence, appended rows only, shrunk-ledger restart, seed untouched, every ranking rule).

## 4. What it is NOT (honest limits)

- It does **not** feed any AI layer yet. Nothing reads the bank except `find`. Wiring to `ai_describe` / the Synonym Bank / the promotion loop is the next step and needs the owner's go (house rule: read `ROBOT-CHAT-BLUEPRINT.md` and the latest `2do.md` first; stay in the existing bank format).
- Synonym, slot and corpus **weights are guesses** until the validated promotion loop (DESCRIBE -> SCORE -> VALIDATE -> PROMOTE) or the owner confirms them. Only `WEIGHT` is measured, and it measures "does the harness pass", which is evidence the behavior works, not that the words match.
- The sidecar is a **flattened pdl form** of the Concept Bank spoke (`SLOT` rows are exactly that format; `SYNONYM`/`SENTENCE`/`CORPUS`/`SEQUENCE` are my row names). Not yet reconciled with `concept-bank/data/spokes/*.pdl` or `AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md` (the doc itself asks whether Behavior Bank and the AI-function recipe registry should be one format).
- The bank is a **separate scratch bank** (`harness/bank/`), not written into `concept-bank/data/` (my own rule in `OPS-BANK-DICTIONARY-DESIGN.md` section 4).

## 5. Open questions for the owner

1. Is my reading of "3D" (words / concepts / corpus) right, or is the third axis something else (for example a 3-coordinate position of the behavior in the concept space)?
2. "Hidden layer": is it the z-node/master layer (as I assumed), or a separate named layer to add?
3. Should weights decay with time (the doc's open question)? v1 is cumulative Laplace.
4. Same bank as the ops dictionary (`OPS-BANK-DICTIONARY-DESIGN.md`), or separate? Recommendation: one bank, harness = behavior spokes that point at the ops they exercise.
5. Who validates promoted synonyms/weights: owner, or the headstone/official-LLM chain?
