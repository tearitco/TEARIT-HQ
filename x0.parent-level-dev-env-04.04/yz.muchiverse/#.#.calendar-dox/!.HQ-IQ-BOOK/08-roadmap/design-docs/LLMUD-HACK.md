# LLMUD HACK — the Harnecient Hack, mutated: watching, banking, and bootstrapping actions instead of text

**Status: RESEARCH/DESIGN, not started.** Written 2026-09-18, in
response to a direct live request to fold in `XO/LLMUD_CODE/4.qwen-
harnextend++.txt`'s proposal, resolve its real gaps, and produce a
technically detailed document for the AI-research/ML-PhD side of the
team to theorize from — not just a build spec for the dev side.

**Read `HARNECIENT-HACK.md` first.** Everything here is that same
recipe — plain API calls, persona-constrained simple questions,
tolerant extraction, **the harness always routes, never the model**,
real visible artifacts, hand-hold + fallback everywhere, and the
bonus rule DESCRIBE-don't-CLASSIFY — applied one level up: not to
generating case arguments or FDA verdicts, but to **watching real
action sequences (yours, or Claude Code's own) and turning them into
reusable, weighted, classifiable behavior.**

## 0. Why this needed its own document, not just a new section

`HARNECIENT-HACK.md` is deliberately simple — six components, one
measured lesson, done. What follows is a real mutation, not a
restatement: a new *object* (the observed action sequence, not a
prompt-and-response pair), a new *storage shape* (a weighted,
rewardable bank, not a single real-time API call), and a real,
concrete bridge to a SEPARATE existing spec
(`IRL-BOOTSTRAP-RECURSION-SPEC.md`) that this document's own §6 makes
explicit for the first time. That's enough new surface area to earn
its own file, per direct instruction — keep the base hack legible as
the simple case, put the advanced one here.

## 1. The four shapes (from the qwen doc, verified and extended)

`4.qwen-harnextend++.txt` independently proposes four shapes without
citing `HARNECIENT-HACK.md` — real convergent validation, the same
pattern `NIGHT_14_THE_PICKER_INSTINCT.txt` already noted for a
different feature landing on the same UX instinct three separate
times in one house. Its own core-principle line (verbatim, worth
keeping as the north star for this whole document): *"Claude builds
the harness, Gemma classifies the patterns, the banks remember the
workflows, the FSM executes deterministically, and the user rewards
or rollbacks — never does the model pick the tool, never does it
route, never does it auto-commit."*

| Shape | What it does | Real house precedent it maps onto |
|---|---|---|
| 1 — Original | keyword match → run tool → fold result | `mylawyer_case_worker.c` (component 4, verbatim) |
| 2 — Watch Mode | observe an action sequence → Gemma classifies it → store in a Behavior Bank | **new** — this document's real focus |
| 3 — Behavior Bank Slotting | synonym lookup → slot the stored sequence → Gemma classifies only the *variables* → FSM executes | **new** — extends Shape 2 |
| 4 — Meta-LLMUD | the hack used ON ITSELF to build its own Synonym/Behavior Banks | **new** — recursive application of the same recipe |

Shapes 2-4 are the real subject of this document. Shape 1 is already
built (my-lawyer) and needs nothing further here.

## 2. What "watching" actually requires — and the real, biggest gap

**Direct finding: nothing in any of the five existing Harnecient-Hack
mutations, and nothing in the qwen doc, actually watches Claude
Code's own actions.** All five existing mutations harness *other
small LLMs'* text output (Gemma, Qwen). Watching your own coding
session (`CURSWORD: WATCH`) is genuinely new territory — but it is
**not** new *infrastructure*. Two real, already-existing watch
surfaces exist in this house today; the real gap is that no consumer
reads either of them as an observation stream yet:

1. **The nav/interact relay** (`#.desktop/entity_menu_history/<pid>.txt`,
   `khtpm_core_render.c` ~line 8128/8435/8555/9141) — every real
   keypress/click a window receives is already written here, one line
   per event, append-only, exactly the shape component 5 (real
   artifacts) requires. This already captures real UI-driving action
   sequences, including agent-driven ones — it's how this session's
   own live testing works.
2. **Claude Code's own tool-call transcript** — this session's own
   sequence of tool invocations (Read/Edit/Bash/etc.) is itself a
   real, ordered artifact, no new capture mechanism required to
   produce it.

**Resolution**: Shape 2's real first build step is a converter, not a
new capture system — a small op that reads a real relay/transcript
window (a bounded slice: "the last N actions since the previous
FSM-marked boundary") and hands it to the classification step below.
This reframes "watching Claude Code" from "build new infrastructure"
to "wire a consumer to infrastructure that already exists" — a
materially smaller, more honest first step than either source
document implied.

## 3. Shape 2, resolved: the classification step MUST be DESCRIBE-shaped

The qwen doc's own example — `Gemma classifies: "code-edit-test
workflow"` — is ambiguous in exactly the way `HARNECIENT-HACK.md`'s
bonus rule exists to prevent. Is that a free-text description, or a
forced pick from a closed label set? The doc doesn't say, and this is
the house's single most concretely *measured* lesson (my-biotech:
2/3 wrong on direct classify, 6/6 correct on open-ended describe) —
it does not get to stay ambiguous in this document.

**Resolved, explicitly**: the classification prompt is always framed
as an open-ended description —

> "In one short phrase, describe what this sequence of actions
> accomplished: `<action1>, <action2>, <action3>, ...`"

— never "pick one of: [refactor, debug, test, ...]". The resulting
phrase is then run through the SAME kind of deterministic,
hand-authored scorer `mylawyer_judge_worker.c`'s `classify_comparison()`
already uses to turn free text into a verdict — here, a keyword/
synonym match against the Synonym Bank (§5) to file it under a real
bank key, not asked-of-the-model directly.

## 4. Shape 2/3, resolved: fallback and artifact-visibility, component 5/6 made explicit

The qwen doc's JSON schema (reproduced below, verified against the
source) has no fallback path and never states whether a user can see
or edit an entry — both are required by the base hack, not optional
extras:

```json
{
  "id": "behavior_001",
  "keywords": ["edit", "code", "test"],
  "synonyms": ["modify", "write", "debug", "change"],
  "sequence": [
    {"action": "FileOpen", "params": {"file": "$1"}},
    {"action": "TypeCode", "params": {"code": "$2"}},
    {"action": "FileSave", "params": {}},
    {"action": "RunTest", "params": {"command": "$3"}}
  ],
  "weight": 1.0,
  "source": "observed",
  "timestamp": "2026-09-18T10:22:00Z",
  "observationCount": 3,
  "rewardCount": 2,
  "punishCount": 0
}
```

**Fallback, resolved** (component 6): if the DESCRIBE step returns
empty, unusable, or fails the API call entirely — the SAME
non-negotiable rule as every other Harnecient-Hack fallback — do not
silently discard the observation (real data is expensive to lose) and
do not block. Deterministically derive a label from the literal
action-type names instead: join the sequence's own `action` fields
(`FileOpen+TypeCode+FileSave+RunTest` → `file-edit-test`, lowercased,
de-duplicated). This is the same "tool beats model" fallback
philosophy as `mylawyer_judge_worker.c`'s own argument-count → length
→ default tiebreaker chain — a real, owned, auditable rule, not
another LLM call.

**Artifact visibility, resolved** (component 5): a Behavior Bank is a
real, plain-text/pdl file, not opaque JSON state — and the real UI
surface for a human to inspect, edit, or delete an entry already has
a home: `AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md`'s own proposed db-hq
tab (the canvas-craft-shaped bench/recipe/inventory UI for authoring
`event_commands.registry.pdl` rows). A Behavior Bank entry is
architecturally the same shape as that doc's own "function recipe"
row — `name, input_vars[], output_var, gemma_call_shape` — the two
documents should very likely converge on ONE registry format, not two
parallel ones. Flagging this explicitly as a real open design
question for whoever builds either first: **should the Behavior Bank
and the AI-function-recipe registry be the literal same file format?**
They already look like the same idea observed from two different
entry points (one via live-watching, one via manual crafting).

## 5. Shape 2/3, resolved: a real weight-update rule

The schema tracks `rewardCount`/`punishCount` but the qwen doc never
defines how `weight` derives from them. Left undefined, this is not
buildable. Proposed, explicit, and revisable default — a **Laplace-
smoothed (add-one) success-rate estimator**, the same real, standard
tool as a Beta-Bernoulli conjugate prior used anywhere reward/punish
counts need to become a bounded [0,1] confidence score without
needing a minimum sample size to avoid divide-by-zero or
over-committing on one early observation:

```
weight = (rewardCount + 1) / (rewardCount + punishCount + 2)
```

A freshly-observed entry (0 reward, 0 punish) starts at weight=0.5 —
genuinely uncertain, not falsely confident at 1.0 (the qwen doc's own
example schema shows a fresh entry at `weight: 1.0`, which this
document is explicitly correcting). This is a real, principled,
literature-standard choice — **not the only defensible one**, and
deliberately left open for the ai-research team: does reward/punish
noise in this domain (a human clicking approve/reject on a replayed
action sequence) actually behave like independent Bernoulli trials,
or is there real temporal drift (a behavior that was correct last
week may not be correct after a refactor) that a plain Laplace
estimator can't see? A time-decayed variant (recent observations
weighted higher) is a real, live open question, not resolved here on
purpose — this is exactly the kind of granular, theory-level question
this document exists to hand to the team that can actually chase it.

## 6. Shape 4 (Meta-LLMUD) and the real bridge to the famous-LLM work

Shape 4 — using the hack recursively to build its own banks (e.g. "I
say 'archive', I mean 'save'" → FSM appends a row to the Synonym
Bank) — is genuinely elegant and directly answers "can it help create
the synonym banks?": yes, same recipe, one level up, no new mechanism
needed beyond what §3/§4 already resolve.

**The qwen doc does not address "handing off to a famous LLM by
training it and hand-tuning weights" at all** (checked directly,
zero mentions of training/fine-tuning/famous anywhere in the source
file). This house already has a real, separately-scoped answer to
that exact question, predating this document by six days:
`IRL-BOOTSTRAP-RECURSION-SPEC.md` (2026-09-12, status
RESEARCH/DESIGN, companion narrative `NIGHT_11_INVERSE_RL_AND_
FAMOUS_LLM.txt` / `NIGHT_12_THE_BOOTSTRAP_RECURSION.txt`) — Claude
bootstraps a process that teaches Gemma to bootstrap its own smaller
"famous-llm," two real recursion hops as the proof bar.

**The real, new connection this document draws** (not previously
written down anywhere): that spec's own "Layer 0" data source today
is `khtpm_open_hai_manager.c`'s real, already-logged open-hai
transcripts (`persist_msg()`, one `U|`/`A|` line per turn). **A
Behavior Bank, once real observations accumulate, is a second,
genuinely different Layer-0-shaped curriculum source** — not
conversation turns, but real, weighted, human-reward-scored ACTION
sequences. A famous-llm bootstrapped partly from "here is a sequence
of real coding actions a human rewarded" is a materially different
and arguably richer training signal than one bootstrapped purely from
chat transcripts. This is a real, concrete, theorizable extension to
hand to the IRL-bootstrap spec's own future work — not built here,
named here so it doesn't get lost.

## 7. Real open questions for the ai-research/ML-PhD side, not resolved here on purpose

These are deliberately left open — granular, architectural, and
genuinely uncertain, the kind of question this document exists to
surface rather than answer unilaterally:

1. **Reward-signal shape**: is a bare reward/punish click enough
   signal, or does the FSM need to capture a real post-hoc outcome
   (did the replayed behavior's OWN result get accepted downstream)
   as a second, independent reward channel — closer to real IRL than
   a single explicit rating?
2. **Time-decay vs. Laplace-only** (§5) — does confidence need to
   decay for behaviors untouched for N days/observations, given real
   codebase drift?
3. **Bank format unification** (§4) — should Behavior Bank entries and
   `AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md`'s function-recipe rows be
   the literal same registry, or genuinely separate with a real,
   principled reason to keep them apart?
4. **Synonym Bank weighting** — the qwen doc's Synonym Bank has no
   weight field at all (only the Behavior Bank does) — should synonym
   confidence be tracked the same way, given a wrong synonym match
   would silently misfile an entire observation?
5. **Graduation threshold** — at what `observationCount`/`weight`
   should a behavior stop requiring confirmation before auto-running
   (Shape 3's own "slot and execute" step) — and is a single global
   threshold right, or should higher-risk action types (anything
   touching `khtpm_core_render.c`, per `CENTROID_GOLD_STD.md`'s own
   real caution) require a structurally higher bar than low-risk ones?

## 8. Grounding / further reading

- `HARNECIENT-HACK.md` — the base recipe this whole document extends.
- `4.qwen-harnextend++.txt` (`XO/LLMUD_CODE/`) — the external proposal
  this document verifies, resolves the gaps in, and extends.
- `IRL-BOOTSTRAP-RECURSION-SPEC.md` + `NIGHT_11`/`NIGHT_12` — the
  real, separate famous-llm/weight-training answer, now bridged (§6).
- `AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md` — the real UI surface
  candidate for Behavior Bank visibility/editing (§4).
- `LLMUD-INTEGRATION-DESIGN.md` — the earlier fold-in of the LLMUD/
  Bank-Systems architecture this document's §1-5 make concrete.
- `entity_menu_history/<pid>.txt` (`khtpm_core_render.c`) — the real,
  already-existing watch surface named in §2.
