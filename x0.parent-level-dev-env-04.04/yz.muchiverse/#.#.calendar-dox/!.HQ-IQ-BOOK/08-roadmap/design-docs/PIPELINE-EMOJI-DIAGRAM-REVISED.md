# 🧠⚙️ TEARIT / Concept Bank Pipeline — REVISED (2026-09-27)

**Supersedes:** `2.BOTTLE_PIPE_VISUALIZED/PIPELINE-EMOJI-DIAGRAM.md`
**Also filed at:** `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/PIPELINE-EMOJI-DIAGRAM-REVISED.md`
**Why revised:** the original diagram's examples were invented, never run against a real model. This revision is grounded in an actual test against the actual production LAN model (see "Evidence" below) before locking in the I/O contract.

---

## What Changed From the Original Design

**Kept, unchanged — these were correct:**
- House law: **DESCRIBE, never CLASSIFY** — no model ever sets a weight number or flips live state directly.
- Deterministic C code (scorer, validator) owns all numeric decisions.
- Laplace-smoothed promotion ledger as the gate between "candidate" and "live."
- The real EDIT record schema, already implemented in
  `&.widgits/concept-bank/ops/concept_edit_validate.c`:
  ```
  EDIT | id=<uuid> | type=spoke_weight_delta | target=<spoke name>
      | slot=<master name> | delta=<float> | reason="<text>"
      | proposer=<name> | status=candidate
  ```
  (`target`/`slot` = the hub-and-spoke terms; `delta` bounded
  `[-0.2, +0.2]`, deliberately tight — see that file's own header
  comment on why.) **This validator does not change.** It already only
  accepts structured, constrained-vocabulary records — the fix below is
  entirely about how the scorer PRODUCES a record for it, not the
  validator itself.

**Changed — this was the actual flaw:**
- ❌ **Old:** Gemma writes a free paragraph; a scorer regex/keyword-matches the prose to guess `target`/`slot`/`delta`.
- ✅ **New:** Gemma is given the real, existing candidate node list in the prompt and forced into a fixed, constrained-vocabulary output line. There is no prose to parse for the fields that drive control flow.

**Why the change:** verified by running both shapes against the real
production model (see Evidence). Free-form output hallucinated a fact
not present in the input and never produced the requested structure.
The constrained-format prompt produced a clean, on-list, correctly
enum'd, trivially parseable line on the first try.

---

## 🗺️ The Revised Pipeline

```
👁️ 1. WATCH LAYER          → raw observation
         ↓
🤖 2. GEMMA (constrained)  → pick from REAL node list, fixed format
         ↓
🧮 3. SCORER (C code)      → split fixed-format line → EDIT record (no NLP)
         ↓
🛡️ 4. VALIDATOR (C code)   → unchanged: schema + bounds check
         ↓
📊 5. PROMOTION LEDGER     → unchanged: Laplace-smoothed evidence
         ↓
✅ 6. LIVE STATE           → unchanged: Concept Bank + FSM / GOAP / Events
```

Steps 1, 4, 5, 6 are unchanged from the original diagram. Only steps 2
and 3 change.

---

## 2️⃣ 🤖 Gemma — Constrained Selection, Not Free Description

**Model:** `gemma3:270m` via the LAN Mac Ollama instance — same model
already in production use by `my-lawyer`/`my-biotech` (see Evidence for
why this choice is empirically justified, not arbitrary).

**Prompt shape (real, tested):**
```
OBSERVATION:
actor_id: ember_terumon_5
action: consume_food
object: mushroom_red
before_state: hunger_level=8_high
after_state: hunger_level=2_low
duration_seconds: 3
feedback: thumbs_up
feedback_intensity: 0.9

EXISTING CONCEPT NODES (pick ONLY from this exact list, never invent a new name):
hunger, satiation, food_types, chemistry_reaction, movement, combat

Respond with EXACTLY this format, one line per concept you think applies (1-3 lines), nothing else:
TARGET: <node from the list> | STRENGTH: high|medium|low | REASON: <one short phrase>
```

**Real output (gemma3:270m, this exact prompt, unedited):**
```
TARGET: hunger | STRENGTH: high | REASON: to achieve food scarcity and manage hunger.
```

**What this buys:**
- `TARGET` is drawn from the real, live node list passed IN to the
  prompt — the model cannot hallucinate a node that doesn't exist,
  because the only way `concept_edit_validate.+x` accepts a record is
  if it resolves to a real spoke/master file anyway. Constraining the
  prompt just means the common case doesn't waste a round-trip on a
  candidate that was always going to be rejected.
- `STRENGTH` is a fixed 3-value enum → the scorer maps it to a fixed
  `delta` (see below) with a lookup table, not a heuristic guess.
- `REASON` is the ONLY free-text field, and it is never parsed for
  control flow — it's stored verbatim into the EDIT record's own
  `reason="..."` field, exactly like the original design intended for
  audit/corpus purposes. **This is where the "creative word bank" the
  house wants lives** — it accumulates real, varied, model-generated
  phrasing over time, purely as inspectable/searchable text, with zero
  parsing risk because nothing downstream ever branches on its content.

---

## 3️⃣ 🧮 Scorer — Now a Trivial Split, Not NLP

**Old scorer job:** guess structure out of prose (fragile, unbounded
failure modes, un-auditable failure rate).

**New scorer job:** split a known-format line on `|`, then `:` — the
same class of parsing `concept_edit_validate.c` itself already does for
its own pipe-delimited `EDIT |` records. No keyword matching, no
synonym tables, no hallucination-recovery logic.

```
STRENGTH → delta lookup (scorer-owned, fixed, not model-set):
  high    → ±0.15
  medium  → ±0.08
  low     → ±0.03
```

(Sign follows whether `feedback` was thumbs_up/thumbs_down — scorer
logic, not Gemma's.)

If Gemma's output doesn't match the fixed format at all (rare, but
possible — small models occasionally drift), the scorer's fallback is
to **reject the whole observation as a candidate**, log it to a
`malformed_gemma_output.txt` ledger for later review, and move on — not
attempt partial recovery. Same "no partial apply" discipline the
validator already enforces at its own layer.

---

## When Should the Full DESCRIBE Path (Free Prose) Still Exist?

It shouldn't be the primary path, but it isn't deleted as a concept —
per the original diagram's own "can the validator skip Gemma for
obvious cases" section (kept, still true, just inverted in emphasis):

- **Default path (constrained, above):** handles the case where the
  observation maps cleanly onto existing nodes — this is the common
  case and should be nearly all real traffic.
- **Escalation path (free-form, judge pattern):** reserved for
  observations that are genuinely novel — e.g. the scorer's fallback
  above fires repeatedly for a pattern that looks real, or a human
  flags a `pending_review.txt` entry as interesting. In that case, and
  only that case, a SECOND Gemma call in free-prose DESCRIBE mode (the
  original diagram's step 2) is worth the extra cost — and a
  Gemma-as-judge third call can sanity-check the resulting candidate
  before it even reaches the deterministic validator, catching a
  malformed `new_concept_node` proposal before a human wastes time on
  it. This is the "second Gemma as judge" idea from the practicality
  conversation — real, valid, but explicitly NOT the hot path.

**Do not build the escalation path yet.** Per the original diagram's
own recommended order (kept): ship the constrained hot path, run it,
measure how often the scorer's malformed-output fallback actually
fires. If it's rare, the escalation path may not be worth building at
all yet.

---

## 🧪 Evidence (real test, this repo, 2026-09-27)

Ran both prompt shapes against the actual production LAN endpoint
(`http://10.0.0.144:11434`, the same Ollama instance `my-lawyer.c`/
`my-biotech.c` already call in production).

**Free-form prompt** (asked for a paragraph + concept list, same shape
as the original diagram's step 2 example):
> "The creature simulation is currently experiencing hunger **and
> thirst levels** of 8 high... The animal's visual appearance is also
> indicative of a state of hunger, **as the mushroom red color suggests
> it's full of nutrients**..."

Two real problems, not hypothetical: it invented a "thirst" level never
present in the input, and produced a non-sequitur about mushroom color
implying nutrition. It also never produced the requested concept-list
at all. This is exactly the failure mode the practicality conversation
worried about — not "might happen at scale," but reproduced on the
very first real call.

**Constrained prompt, `gemma3:270m`:** clean, correct, on-list, on
first try (shown above).

**Constrained prompt, `gemma3:1b`** (bigger model, tested for
comparison): WORSE at the exact task — dropped the `TARGET:`/
`STRENGTH:`/`REASON:` labels entirely, ignored the "only list ones that
apply" instruction, and dumped all 6 nodes including obviously
irrelevant ones (`combat`, `movement`) at uniform low confidence. This
is real, useful, counterintuitive signal: **bigger is not automatically
better for exact-format instruction-following at this scale.** The
production apps' existing choice of `gemma3:270m` is empirically
justified by this test, not just inherited/arbitrary.

---

## ⚠️ Separate, Real Finding: Hardcoded Endpoint

`GEMMA_LAN_URL` (`http://10.0.0.144:11434`) is hardcoded as a C string
constant in FOUR separate files:
- `@.apps/my-lawyer/ops/mylawyer_case_worker.c`
- `@.apps/my-lawyer/ops/mylawyer_judge_worker.c`
- `@.apps/my-biotech/ops/mybiotech_research_worker.c`
- `@.apps/my-biotech/ops/mybiotech_fda_verdict.c`

`my-biotech`'s own design doc already flags this IP as "NOT guaranteed
stable." Any new Concept Bank Gemma-calling op should NOT add a fifth
hardcoded copy. Before building the real scorer-side Gemma call:

- [ ] Move `GEMMA_LAN_URL` into a shared `.pdl` config (house
      convention — same pattern as `hq_ui.pdl`/`desk_grid.pdl`), read
      once at startup by any op that needs it.
- [ ] Update the four existing hardcoded call sites to read from that
      config instead, so this gets fixed house-wide in the same pass,
      not just for the new Concept Bank op.
- [ ] Keep the "not guaranteed stable" warning as a runtime check (curl
      `/api/tags` with a short timeout before relying on it, same
      pattern `my-biotech`'s own test scenarios already use) rather
      than assuming it's always reachable.

---

## Real Files This Touches (once implementation starts — not done yet)

- **New (not yet built):** the actual Gemma-calling scorer op for
  Concept Bank observations — the design above is the contract it must
  follow. `ai_lab_concept_bank_propose.sh`'s own header comment already
  notes "Gemma's own real /api call is a separate, later step... not
  yet built" — this doc is that step's design, written before writing
  the code, not after.
- **Unchanged:** `&.widgits/concept-bank/ops/concept_edit_validate.c`
  (the validator), the promotion ledger math, `pending_review.txt`
  staging convention.
- **To fix alongside:** the four `GEMMA_LAN_URL` hardcodes above, into
  one shared `.pdl`.

---

## 🧭 One-sentence summary

**Same house law (Gemma describes, code decides), but "describe" now
means "pick from the real list in a fixed format," not "write a free
paragraph and hope the parser finds it" — tested against the real
production model before locking this in, not guessed.**
