# Harness behavior bank (weights, synonyms, sentences, concept slots)

Written 2026-10-07 (claude). **v1 built** for the ten pal harnesses (`&.widgits/_shared-lib/harness/bank/`, op `harness_bank_op`); the AI-layer wiring is **not** done. Owner words that drove it:

> "do these harnesses get .pdl associated weight / bank / synonyms for use with the ai layers yet? i'd like that if you know how, infer based on dox"
> "they are like a behavior bank + 3d associated synonyms for the behavior" / "sentences, words, etc in the bank related to how it works. seo" / "maybe under 'hidden layer'"

Answer to the first question: **no, they had none**; this is the first version, **built before reading the NIGHT lessons: see section 4b for what that got wrong**. Related: `OPS-BANK-DICTIONARY-DESIGN.md` (the dictionary these entries live in), `LLMUD-HACK.md` sections 4-5 (Behavior Bank schema + Laplace weight), `AI-TRACK-BRAINSTORM-QUESTIONS.md` 9b (Concept Bank, z-nodes, hub-and-spoke), `A-TEARIT-IS-ALL-YOU-NEED.md` (Bank Layer).

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

## 4b. Reconciliation with the NIGHT lessons (added 2026-10-07, after the owner said "i dont think u read enough, did u read the night lessons?")

I had **not** read them (`#.#.calendar-dox/1-1.HARNECIENT.SMOL/NIGHT_01 .. NIGHT_30+`, plus `DAY_01..26`). Read now, in full: NIGHT 22 (the Concept Bank), NIGHT 15 (the bank and the brick), the first half of NIGHT 26 (everything is an event); grepped the whole series for 3D / SEO / hidden layer. **Not yet read: NIGHT 05 (trigger layer), 07-09, 11-12, 16, 20-21, 23-25, 27-30 and the DAY series.** What they say, and where v1 above is wrong or off-pattern:

**Confirmed**
- **"Hidden layer" = the z-node layer.** NIGHT 22 says it in so many words: a shared lower-dimensional layer of named latent directions that many words route through is "an embedding dimension, a hidden layer ... the same math, opposite legibility": done **explicitly, named, by hand** instead of by gradient descent. My inference (section 2, axis 2) holds.
- Hub-and-spoke (pointers resolve only to masters), weights in exactly one place (the spoke), master mirror derived: my `SLOT` rows follow that shape.

**Wrong or off-pattern in v1 (to fix before anything depends on it)**
1. **I used non-existent masters.** The real rule (validator, `concept_edit_validate.c`): a slot's `POINTS_TO` must be a **real master file** under `data/masters/`. My `play-mode`, `map-access`, ... have no master file, so a real validator would reject them. Fix: create the masters (a scratch bank folder, never the real `concept-bank/data` yet) and let the existing `concept_mirror_rebuild.sh` derive the mirrors.
2. **Wrong file shape.** Real spokes are `NODE | name`, `KIND | spoke`, `N_SLOTS | 8`, sparse `SLOT | idx | POINTS_TO=<master> | WEIGHT=<w>` (`data/spokes/*.pdl`); fixed-width slots (4-8, growable to 32). My sidecar adds `KEYWORDS`/`SYNONYM`/`SENTENCE`/`CORPUS`/`SEQUENCE` rows of my own naming.
3. **Wrong synonym format.** The house's own named first step for the Synonym Bank (NIGHT 15) is `ai_synonym_bank.txt`, one row per alias: `CANON=<name>|ALIAS=<phrase>|WEIGHT=<0-1>|SOURCE=<user|learned>`, "same convention as every other house state file". My `SYNONYM | word | weight` rows should become exactly that (CANON = the harness id).
4. **Weights were hand-set.** The lessons' rule: **a model or a script never freely decides**; weights change only through a **candidate `EDIT` record** (`type=spoke_weight_delta`, `delta` bounded to **[-0.2, +0.2]**, with `reason` and `proposer`) checked by the **real validator**, then lands in a **review file for a human** (the tiered auto-promotion in `AUTO-PROMOTION-RULE.md` is only a stub, see 4c item 2: do not rely on it). My seeds bypass that gate. Fix: seed values stay only as the *initial* spoke file, and every later change goes through a candidate edit.
5. **Reward went only to the entry.** NIGHT 22: reward/punish "updates the Bank entry it matched **and also propagates to the concepts that entry touches**", and feedback carries `FEEDBACK | valence=<+1|-1|0> | concept=<z-node> | intensity=<0..1>` (`A-TEARIT` section 2.2). v1 only moves the entry weight; concept propagation is missing.
6. **Gemma proposes only inside a fixed line** (NIGHT 26, tested for real): given the real candidate node list, one line `TARGET: <node from the list> | STRENGTH: high|medium|low | REASON: <phrase>`; `gemma3:270m` works, `1b` did worse. So the way to fill synonym and slot weights for harnesses is that DESCRIBE step with my master list, not my guesses.
7. **Order.** The lessons say "build `ai_fsm_transition` first, then the bank, then the bench" (NIGHT 14/15). I built a bank sidecar ahead of that; it stays a **scratch bank** and must not be mistaken for the real Synonym Bank.
8. **Code-shape rule (NIGHT 26):** one consumer = inline; 2+ pure = text-include; 2+ stateful = op + fork/exec + IPC; **never a header + link split**. `khtpm_game_setup.c` (text-include, pure, 2+ consumers) and the ops-as-processes harness follow it.

**Still undefined in the lessons: "3D".** No document defines it. NIGHT 10 shows real per-word embeddings (`EMBEDDING_DIM` 7), so a **literal 3-coordinate position per synonym** (distance in a 3D concept space = how close two words are) is a live reading next to my three-axes reading (words / concepts / corpus). Not guessing again: owner to say which.

**Existing machinery to reuse instead of mine:** `concept_edit_validate.c` (bounds + hub-and-spoke check), `concept_mirror_rebuild.sh`, `data/candidates/edit_000N_*.txt` (pass / reject examples), `AUTO-PROMOTION-RULE.md`, `A-TEARIT-IS-ALL-YOU-NEED.md` sections 2.1-2.7. My `harness_bank_op` should shrink to: count PASS/FAIL into OBS/FEEDBACK-shaped rows and file candidate edits, leaving validation and promotion to the existing pieces.

## 4c. Where this sits in the "attrition model" (NIGHT 30, read 2026-10-07; owner: "multipurpose, callable thru our from-scratch attrition model: a tearit? tomom?")

**The attrition model is the house's name for the whole local-AI effort**: a strategy, "a war of attrition against needing an outside API for everything". It is four stacked pieces: **(1) a TEARIT**, the DESCRIBE-then-decide learning loop; **(2) the Concept Bank**, the weighted-node data the loop reads and writes; **(3) tomom**, the hand-built local model, **dormant** until there is something real to learn from; **(4) the decision layer**, an FSM for what is known and a GOAP planner for what must be figured out. The loop: **Watch** (a plain record of what happened plus the owner's valence) -> **Gemma DESCRIBEs**, never classifies, in one fixed line `TARGET: <existing node> | STRENGTH: high|medium|low | REASON: <phrase>` -> **real code** (not the model) maps the strength word to a delta with a fixed lookup and writes a candidate `EDIT` -> **validator** (built, one edit type: `spoke_weight_delta`) -> a **review file a human reads** -> promotion (does not exist) -> eventually **tomom** learns from the accumulated accepted/rejected history. Outside models are *workers*, not the runtime (their demonstrations are "evidence in a drawer").

**What that means for the harness bank (corrections to v1):**
1. **A harness run is a Watch record.** `results/<name>.txt` (PASS/FAIL rows with the harness id) is exactly the "plain record, no interpretation" the loop starts from; the PASS/FAIL valence is the feedback. So `harness_bank_op` should not own weights: it should turn ledger rows into observation / `FEEDBACK` rows that point at the harness's concept slots, and let the loop (DESCRIBE -> validator -> review file) propose any weight change.
2. **Promotion is deliberately NOT designed yet** (NIGHT 30: "closing it now means guessing at promotion criteria with almost no real data"; build the other three edit types, let review entries pile up, design promotion from that pile). My section 4b item 4 leaned on the **stub** `AUTO-PROMOTION-RULE.md` (tiers, 0.90 / 20 observations): that file says itself it is a rule to wire "once real replay data exists, not a working feature". **Do not build auto-promotion from harnesses.** Validated edits land in a review file; a human reads them.
3. **Multipurpose, callable through the loop**: a harness is a behavior spoke the loop can name (its `CANON` id, its synonyms, its concept slots), and the thing it runs (a pal of `exec`s) is already shaped like an event command; the registry has **no `ai_*` commands yet** (`ai_describe`, `ai_fsm_transition`, `ai_goap_plan` are "named, absent", the frontier). Until `ai_describe` is a registered command, the bank stays a scratch bank read by `find`.
4. **Tomom waits on the same gap**: nothing to learn from until reviews accumulate. Harness PASS/FAIL history is one honest, cheap, high-volume source of that signal (a deterministic behavior with a real valence); that is the real use of the weights, not a ranking toy.
5. **Delegation limits** (NIGHT 30) apply to anything I hand an outside model here: keep tasks small, single-file, checkable; writes/edits/shell need a human's explicit approval each time.

## 5. Open questions for the owner

1. Is my reading of "3D" (words / concepts / corpus) right, or is the third axis something else (for example a 3-coordinate position of the behavior in the concept space)?
2. "Hidden layer": is it the z-node/master layer (as I assumed), or a separate named layer to add?
3. Should weights decay with time (the doc's open question)? v1 is cumulative Laplace.
4. Same bank as the ops dictionary (`OPS-BANK-DICTIONARY-DESIGN.md`), or separate? Recommendation: one bank, harness = behavior spokes that point at the ops they exercise.
5. Who validates promoted synonyms/weights: owner, or the headstone/official-LLM chain?
