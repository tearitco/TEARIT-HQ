# A TEARIT Is All You Need
### (the tomom / TEARIT hack — 2026-09-22)

> Named directly against "Attention Is All You Need" (Vaswani et al.,
> 2017). That paper's claim was: one mechanism, brute-forced at scale
> over opaque matrices, replaces the need for hand-built structure.
> This doc's claim is the deliberate inverse: **you don't need to
> re-discover that mechanism from scratch by brute force, because the
> capability it produces — flexible, context-sensitive reasoning —
> already exists, for free, in two forms we already have (Claude,
> Gemma). What's missing isn't a bigger brain. It's a legible,
> persistent, hand-tunable substrate those brains can read from, write
> to, and refine together, inside a real running environment, from
> real feedback — built like Lego, not grown like a black box.**

This is a real, technical architecture proposal, not a metaphor piece.
Every component named below either already exists in TEARIT-HQ (cited
with its real file/path), or is specified concretely enough to build.
Nothing here is vague inspiration — where something is genuinely
undecided, it's marked **OPEN**, not glossed over.

---

## 0. The actual claim, stated precisely

Modern LLMs get their power from a small number of primitive
operations (attention, feed-forward transforms) applied at scale,
trained by brute-force gradient descent over billions of parameters
until useful structure emerges *implicitly*. That structure is real —
but it's smeared across millions of uninterpretable weights
(superposition), which is exactly why it's opaque and exactly why it
requires GPU-scale compute to discover in the first place: brute
force is the *only* way to find structure you can't name.

TEARIT-HQ's actual situation is different from the situation that
paper was solving. We are not trying to discover novel reasoning
capability from nothing. We already have:

1. **A near-AGI-grade general reasoning agent** (Claude, this session
   and others) — capable of language understanding, planning, code
   authorship, and structured judgment, callable on demand.
2. **A free, fast, local, always-available narrow model** (Gemma
   270M) — cheap enough to run constantly, already restricted by
   house law to DESCRIBE-only (never CLASSIFY, never route) —
   `HARNECIENT-HACK.md`'s existing rule.
3. **A house-wide convention of real, file-backed, legible state** —
   every existing subsystem (Bank Layer, FSM tables, Events, desk
   layout, window state) is already a real file, already auditable,
   already the opposite of a hidden weight matrix.

Given (1)-(3), the actual bottleneck was never "we need a bigger
model." It's: **nothing yet lets (1) and (2) jointly read, propose,
and refine a shared, persistent, legible memory of what the house has
learned — gated by real feedback, promoted only when proven, never a
black box.** That shared substrate is what this document specifies.
Once it exists, "training" stops meaning gradient descent over opaque
matrices and starts meaning: Gemma/Claude/tomom propose small, legible
edits; the house validates, tests, and promotes them the same way a
human editor would, just automated. That's the "Lego" framing — small,
named, swappable, inspectable pieces, snapped together, instead of one
enormous, unnamed block.

---

## 1. The Lego bricks that already exist (nothing here is new)

| Brick | What it does | Where it lives |
|---|---|---|
| **Bank Layer** | Sparse, named, Laplace-smoothed weighted lookup (Synonym/Relation/Behavior/Sentence Banks); reward/punish → `(reward+1)/(reward+punish+2)` | `LLMUD-HACK.md` §5, existing Bank files |
| **Watch Layer** | Observes action sequences + outcomes + feedback; files results into Behavior Bank | `LLMUD-HACK.md` §2, `DUSTOPIA-HACK.md` |
| **FSM Sequencer** | Deterministic path selection from Bank matches; no model in the loop | Q5 diagram, `ai_fsm_transition` primitive |
| **GOAP planner** | Precondition/effect-based action planning | referenced, `ai_goap_plan` primitive |
| **Events pipeline** | Human-authored `event.ir.pdl` → compiled `cmd_N.sh`, via `khtpm_events_hq_manager.c` | `&.widgits/events-hq/ops/` — real, working, confirmed by direct read tonight |
| **DESCRIBE law** | Model generates raw material; owned deterministic code makes every discrete decision; model never routes | House-wide, restated in Q5 |
| **tomom (3-stage LLM)** | A real, working, hand-built pipeline: `vocab_model → trainer → chatbot_moe_v1 → meta_rl` | `#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/` — audited directly tonight |
| **Concept Bank** (new, specified, not yet built) | Hub-and-spoke, named, fixed-slot, hand-tunable substrate replacing tomom's opaque attention/MLP matrices | Q9, `AI-TRACK-BRAINSTORM-QUESTIONS.md` |

Nothing above requires inventing a new kind of intelligence. It
requires wiring six real, already-proven pieces (plus one newly
specified piece, the Concept Bank) into one loop.

---

## 2. The loop, specifically

```
 ┌─────────────┐   ┌─────────────┐   ┌──────────────────┐   ┌────────────────────┐
 │ WATCH LAYER │──▶│   GEMMA     │──▶│ DETERMINISTIC     │──▶│  CANDIDATE EDIT     │
 │ (observe)   │   │ (DESCRIBE)  │   │ SCORER (owned C)  │   │  (staged, unproven) │
 └─────────────┘   └─────────────┘   └──────────────────┘   └────────────────────┘
                                                                        │
                          ┌─────────────────────────────────────────────┘
                          ▼
                 ┌──────────────────┐   ┌───────────────────┐   ┌──────────────────┐
                 │ VALIDATOR (C)    │──▶│ REPLAY / SIMULATE  │──▶│ PROMOTION LEDGER │
                 │ schema+bounds    │   │ against real logs  │   │ Laplace-smoothed │
                 └──────────────────┘   └───────────────────┘   └──────────────────┘
                                                                        │
                                                pass threshold?         │
                                          ┌─────────────────────────────┘
                                          ▼
                                 ┌──────────────────────┐
                                 │  LIVE STATE (promoted)│
                                 │  Concept Bank / Bank  │
                                 │  Layer / FSM / GOAP   │
                                 └──────────────────────┘
```

Every arrow in that diagram is a real file write, not an in-memory
handoff — matching the house's own "real artifacts, never hidden
state" law. Below, each stage, specified.

### 2.1 Watch Layer → observation record (exists, extend it)

Already real (`LLMUD-HACK.md` §2). Extend its output schema to write
the Q9 feedback shape explicitly, not just a raw log line:

```
OBS | id=<uuid> | target=<action/word/fsm-transition ref> | outcome=<free text>
FEEDBACK | valence=<+1|-1|0> | concept=<z-node name, optional> | intensity=<0.0-1.0>
```

This is the one, single feedback event format used everywhere below —
Bank Layer reward/punish, Concept Bank spoke updates, FSM/GOAP
promotion scoring all consume the *same* record. One format, reused,
not three separate schemas.

### 2.2 Gemma DESCRIBE (exists as a law, extend its target)

Gemma reads an `OBS` record and produces a natural-language
description of what happened and why it might matter — never a
classification, never a discrete decision (house law, unchanged).
Example output (not code, just text): *"the player's stunt jump
increased height when the physics primitive's gravity constant was
lower; this seems related to the `force` and `motion` concepts."*
Gemma's own inference cost stays flat regardless of house size — it's
already local, free, and fast; this is why it's the right brick for
the "always-on, constant" half of the loop, versus Claude which is the
"occasional, deep-judgment" half (see §4).

### 2.3 Deterministic scorer → candidate edit (new, small, C)

Turns Gemma's raw text into a structured **candidate edit** record —
the *only* place natural language gets converted into anything
resembling state, and it happens through owned, deterministic
pattern-matching against the Concept Bank's own name list (not free
text parsing at the point of application):

```
EDIT | id=<uuid> | type=spoke_weight_delta | target=gravity_constant
    | slot=force | delta=+0.05 | reason="<Gemma's text, kept verbatim, not discarded>"
    | proposer=gemma | status=candidate
```

Four `type`s cover everything this document needs:
- `spoke_weight_delta` — adjust one (word/action, master-concept) slot weight.
- `new_concept_node` — propose a brand-new master z-node (rare, high-bar).
- `fsm_transition_describe` — a natural-language description of a
  desired new FSM transition (see §2.6, NOT the transition itself).
- `goap_action_describe` — same, for a GOAP action's
  preconditions/effects.

### 2.4 Validator (new, small, deterministic C — this is where hallucination gets clamped)

Rejects, on sight, before anything touches real state:
- `target`/`slot` referencing anything that doesn't already exist as a
  real master concept (**hub-and-spoke enforcement** — a candidate
  edit can never invent a spoke pointing at a non-master, per Q9's
  resolved schema).
- `delta` outside a bounded range (prevents a single edit from
  swinging a weight implausibly, same discipline as Bank Layer's
  existing Laplace smoothing bounding runaway confidence).
- Malformed records (parse failure = reject, never a partial apply).

This is literally Track C's own already-written plan
(`#.Z.HUMAN_LLM/^.2DO.aug01_2026.txt`: *"Validator: parse-check lines,
numbers in range, no dropped tokens"*) — not a new idea, just finally
wired to something real.

### 2.5 Replay / simulate → promotion ledger (new, reuses existing math)

A candidate edit is applied to a **staged copy** of the Concept
Bank/Bank Layer (never live state) and re-evaluated against the
*specific* observed sequence that motivated it, plus a small recent
window of similar past sequences (cheap — this is score arithmetic
over already-logged data, not new inference). Each replay outcome
(did the edit's prediction match what actually happened / did the
FSM/GOAP choice improve) becomes one more (reward, punish) count in a
**promotion ledger**, using the *exact* Laplace-smoothed formula the
Bank Layer already uses — `(reward+1)/(reward+punish+2)` — no new
statistics to invent, no new formula to trust.

### 2.6 FSM/GOAP self-authoring — reuses the Events compiler, doesn't reinvent it

This is the specific, technical answer to "program its own FSMs and
GOAPs," and it's already resolved (Q8 8a, confirmed by direct code
read tonight): an `fsm_transition_describe`/`goap_action_describe`
candidate edit is **not** allowed to write a transition table or
action definition directly — house law (`DESCRIBE, never CLASSIFY`;
model never routes). Instead, once such a candidate clears the same
promotion ledger as everything else, its DESCRIBE text is fed through
the *exact same* deterministic compiler a human author's events-hq
session already uses: `event.ir.pdl` → `khtpm_events_hq_manager.c` →
`cmd_N.sh` (confirmed real, working, read directly tonight in
`&.widgits/events-hq/ops/`). **Zero new code-generation risk surface**
— an LLM-authored FSM/GOAP action is compiled by literally the same
pipeline, with the same validation, as a human-typed one. This is the
whole point of the Lego framing: reuse the brick that already exists
instead of building a second, parallel, less-trusted one.

### 2.7 Promotion → live state

Once an edit's ledger score crosses a threshold (same shape as any
Bank Layer promotion — not a new mechanism), it's applied to the real,
live Concept Bank / Bank Layer / FSM table / GOAP action set. Every
promoted edit remains a real file diff — inspectable, revertible,
git-history-able, same as any other change in this house.

---

## 3. The Concept Bank's role in this loop, specifically (not a repeat of Q9 — this is its *function* inside the loop above)

The Concept Bank is what makes step 2.3's candidate edits *legible
targets* instead of blind coordinates into a matrix. When Gemma says
"this seems related to `force` and `motion`," those are real, named,
existing master nodes the scorer can look up — not floating-point
indices into an opaque embedding space. This is the entire reason
Section 1's brick list can include a hand-editable substrate instead
of a trained one: **the loop above only works at all if its target
state is nameable.** Dense matrices can't be a validator's target
(*"is delta in bounds for row 3, column 4187?"* is meaningless to a
human or an LLM); named concept slots can (*"is +0.05 to the `force`
slot of `gravity_constant` plausible?"* is a real, answerable
question).

---

## 3.5 Is classic FF/BP/QKV-style training still relevant here, or obsolete?

Real question, real answer: **relevant, but its job changes.** The
opacity problem with dense attention was never gradient descent
itself — it's that gradient descent is *simultaneously* discovering
topology (which dimensions matter, what gets superposed with what)
*and* fitting magnitudes to data, in one undifferentiated process.
Only the topology-discovery half is what has to stay out of a
hand-tunable system. Once a relation already exists and is *named*
(`gravity → force`), refining exactly how strong that weight should
be, using real corpus/replay data as a loss signal, is a different
and legitimate problem — this is not a new idea, it's how real
**knowledge-graph embedding methods** (TransE, RotatE, and similar)
already work: fixed, hand-authored entities/relations; gradient
descent only ever adjusts an already-named edge's *magnitude*, never
invents a new one.

Concretely, this means a classic forward/backward pass over a
curriculum corpus is still a real, useful thing to run — it just
computes a suggested delta for an **already-existing slot**, and
submits that delta as one more `spoke_weight_delta` candidate edit
(§2.3), with `proposer=trainer_bp` instead of `gemma`/`claude`. It
goes through the *exact same* validator → replay → promotion-ledger
gate as any hand-authored or DESCRIBE-authored edit (§2.4-2.7) — never
a second, parallel path that writes live state directly. This is what
prevents the two-authoritative-writers bug (the same class of bug
already solved once tonight, for the phymoji sprite cache, and again
for the spoke/mirror normalization in Q9) — one promotion pipeline,
multiple kinds of proposer, never two mechanisms allowed to silently
overwrite the same weight.

QKV specifically, at *inference* time, is superseded, not merely
refined: its job (context-dependent weighting) is already done by the
Concept Bank's own overlap computation (§3 above) — no learned Q/K/V
projection is needed to decide relatedness once relatedness is a
named, looked-up fact. QKV's *training signal* (predict-next-token
loss) is still real and useful — it's just redirected at the small
number of already-named slots a given example actually touches,
never at a dense projection matrix. **OPEN**: the exact loss function
and replay-batching scheme for this `trainer_bp` proposer haven't
been specified — this section establishes its role in the loop, not
its implementation.

---

## 4. Claude's role vs. Gemma's role — division of labor, specifically

- **Gemma**: constant, cheap, local DESCRIBE on every Watch Layer
  observation. High volume, low cost, no API dependency, runs whether
  or not any external agent session is active. This is the "always
  learning" half.
- **Claude** (or, once bootstrapped far enough, **tomom itself**):
  invoked for the harder judgment calls — proposing genuinely new
  concept nodes (`new_concept_node`, the highest-bar edit type),
  authoring FSM/GOAP descriptions for entirely new behavior (not just
  tuning an existing weight), and periodically auditing the promotion
  ledger for edits that are technically passing but look wrong on
  inspection (the same role this session played auditing tomom's real
  code tonight, rather than trusting a summary). This is the
  "occasional, deep, expensive" half — used deliberately less often,
  same as any real engineering team uses a senior reviewer less often
  than a linter.
- **Harnecient Hack** (existing house delegation/automation
  convention): the actual scheduler/conductor running this loop
  unattended — watches for new `OBS` records, triggers Gemma DESCRIBE,
  invokes Claude when a session is live and a harder-tier edit is
  proposed, runs the validator+replay+promotion pipeline, logs
  everything as real files. It is not a new brain. It is the thing
  that calls the other bricks in order and keeps the audit trail —
  the same role it already plays for every other delegated task in
  this house.

---

## 5. Bootstrapping order — how the loop earns more autonomy over time

Directly ties to the grade-level curriculum idea (preschool → PhD,
this session's own real-time design). The **same promotion-gate
discipline applies recursively to the loop's own trust level**, not
just to tomom's subject knowledge:

1. **Preschool tier**: Gemma may only propose `spoke_weight_delta`
   edits on *already-existing* spokes, with small bounded deltas, on
   unambiguous reward signals. Track Gemma's own promotion-acceptance
   rate as a real Behavior Bank entry (Laplace-smoothed, same as
   everything else) — this is a literal, measurable trust score.
2. **Elementary → HS tier**: once acceptance rate clears a threshold,
   Gemma may propose new spokes on existing masters, and simple
   `fsm_transition_describe` edits (parameter tuning only, matching
   Q8 8a's "most concrete" tier).
3. **Associate → Bachelor tier**: `new_concept_node` proposals allowed,
   still gated through Claude review before entering the promotion
   ledger at all (a higher-bar edit type gets a higher-bar reviewer,
   not just a higher ledger threshold).
4. **Master → PhD tier**: full `goap_action_describe` authoring,
   Claude review only spot-checked rather than mandatory — this is
   the tier where "the user only has to observe and give intent-level
   feedback" actually becomes true, because the loop has, by this
   point, a real, measured track record earning that trust — not
   because it was assumed on day one.

This mirrors exactly how a human apprentice earns autonomy — small,
low-risk, closely-supervised tasks first, broader authority only after
a measured track record, never granted by default.

---

## 6. What's OPEN — real, unresolved, not papered over

- **Corpus-level meta-weights** (Q9): hand-authored, derived from
  concept overlap, or both — still undecided.
- **Promotion thresholds**: what specific score, over what replay
  window size, is "proven enough" to promote — needs real numbers,
  not just the formula shape.
- **The validator's exact bound ranges** per edit type — needs
  grounding in tomom's real vocab/weight scale (the `mlp_model.txt`
  audit found values in the thousands where sane values should be
  small; bounds must be chosen with that failure mode explicitly in
  mind).
- **Where the promotion ledger itself lives on disk** — likely a new
  `candidate_edits/` tree parallel to the Concept Bank, format not yet
  written.
- **Whether Harnecient Hack needs new scheduling primitives** to run
  this loop unattended, or whether its existing delegation shape
  already covers it — not yet checked against Harnecient Hack's real
  current code.

## 7. What's NOT open — decided, and why

- No opaque matrices, anywhere, ever, in this loop (Q9, restated).
- The model never writes live state directly — always candidate →
  validate → replay → promote (this doc's whole structure).
- FSM/GOAP authoring reuses the existing Events compiler; no second,
  parallel code-generation path gets built (§2.6).
- One feedback record shape, reused everywhere, not three separate
  schemas per subsystem (§2.1).

---

## 8. TEARIT is not just a game — the real domain breadth this substrate serves

Everything above was framed through game-shaped examples (physics,
stunt jumps, FSM/GOAP). That undersells the point. TEARIT-HQ is a
real, running house/OS-shaped substrate, and the apps built on it
already span domains that normally require entirely separate tech
stacks, each with its own bespoke ML/infra investment. Named
concretely, not asserted vaguely — every category below is a real,
existing app in this house, checked directly, not summarized from
memory:

- **Social**: `forum-hq`, `irc-chat-hq`, `co-lab-hai` (a real, live,
  multi-agent chat room with approve/reject gating and per-agent
  visibility filtering — tested with 4 separate real agents joined
  cold).
- **Blockchain / crypto**: `chain-hq` (a real chain/ledger app) and
  `myne-qrypto` — not a toy: `qtc/` is a full, working, real SHA-256
  proof-of-work engine with real wallet cryptography and a passing
  21-check test harness across two scenarios, wrapped in its own
  player-facing game shell (mining, ASICs, exchange).
- **AI chat agents**: `chat-hai`, `co-lab-hai`, `h-ai-lab` — real
  houses for agent-driven conversation, already the substrate this
  very session runs inside of.
- **Chemistry simulation**: a real tile/combination-based chemistry
  system (`chemistry_tiles🏆.csv`, a periodic-table picker, a
  Little-Alchemy-style combination mockup) — chemistry-as-gameplay,
  already tiled and playable, not just a design doc.
- **Media editing/creation suites**: `media-img-hq`, `media-img3d-hq`,
  `media-vid`, `media-vid-hq`, `media-daw`/`media-daw-hq`,
  `media-canvas`, `media-3d-hq`, `aomorai-editor`, plus a real, working
  **audio synthesis engine** (`lab-audio/synth.c` + `fft.c` + `wav_io.c`
  — actual signal synthesis, not just a player). **Honest distinction,
  not glossed over**: what's real today across image/audio/video is
  editing, composition, and synthesis infrastructure — full
  diffusion-style *generative* image/video is not yet built. That gap
  is exactly what this document's loop is *for*: once the Concept
  Bank and the propose→validate→replay→promote loop exist, the same
  hand-tunable substrate that lets tomom refine a `force`↔`motion`
  weight is the substrate that would let a music-gen or image-gen
  model be steered and corrected the same legible way — one shared
  learning discipline instead of a separate bespoke ML pipeline per
  media type.
- **Game programming from prompts, already real for humans, specified
  for agents**: `&.widgits/events-hq/` is a real, working
  DESCRIBE-then-compile pipeline (confirmed by direct code read this
  same session) — a human author already turns natural-language intent
  into real, compiled, running game logic (`event.ir.pdl` →
  `cmd_N.sh`) without hand-writing C. §2.6 of this document specifies,
  concretely, how an agent does the exact same thing through the exact
  same compiler — not a new, separate, less-trusted code-generation
  path. **`piececraft-hq` (pc-hq) is where this house's actual game
  development happens** — a real board-viewer engine, real terrain
  generation, real entity placement, already wired to the Events
  pipeline — and it's meant to be **highly automated**: the natural
  target for the propose→validate→replay→promote loop once it exists,
  not a hypothetical future app.

The point isn't "TEARIT has a lot of apps." It's that **one shared,
legible, hand-tunable learning substrate (Bank Layer → Concept Bank →
this document's loop) is positioned to serve every one of these
domains at once**, instead of each domain needing its own model,
own training infra, own opacity tradeoffs. A concept relation learned
from tuning tomom's chemistry curriculum is the same kind of object,
in the same Concept Bank, as a relation that steers a GOAP action in
`pc-hq` or a tone-shaping weight in `co-lab-hai` — cross-domain reuse
that's structurally impossible when each domain runs its own opaque
model.

---

## 9. Why this is worth comparing to "Attention Is All You Need" — for two different audiences, honestly

"Attention Is All You Need" mattered because it found **one simple,
general mechanism that, applied at scale, replaced a zoo of bespoke
architectures** (RNNs, LSTMs, CNN-for-sequences) — a single reusable
primitive that unlocked massive capability gains once given enough
compute. It scaled along one axis: more GPU-hours over bigger opaque
matrices, buying more capability.

This document's claim is not "we found a better attention mechanism."
It's structurally the *inverse* bet, and it's worth being precise
about why that's a real, different, and — for a specific set of
real-world constraints — more valuable axis to scale along:

**For AI researchers and investors**: the original paper's axis
(scale + opacity) has a well-known, real cost that the entire industry
is currently fighting — opaque models are expensive to retrain, hard
to audit, hard to correct precisely, and hard to safely deploy across
many different verticals at once without either a separate model per
vertical or an enormous, general, still-opaque foundation model. This
document specifies the other axis: **capability that already exists**
(Claude, Gemma) **gets reused, not retrained, and its outputs
accumulate into a small, named, auditable, hand-correctable substrate**
that gets *better* the more it's used, without ever needing a
from-scratch retrain. That's not a research curiosity — it's a direct answer
to the actual, current, expensive problem of "how do we deploy AI
behavior across many product verticals without either N separate
opaque models or one enormous one we can't audit or fix." Scaling by
accumulated, legible judgment instead of by GPU-hours is a genuinely
different curve, and for anyone trying to ship real, correctable,
multi-domain AI products rather than chase a benchmark, it's arguably
the more commercially relevant one.

**For gamers and developers**: the pitch is concrete, not abstract —
one house, one shared AI learning substrate, powering a game engine, a
chat platform, a real crypto/blockchain app, a chemistry sim, and a
full media-creation suite *at once*, where teaching tomom something in
one context (say, physics) can genuinely inform behavior in another
(say, a crafting system in `pc-hq`) because they share the same named
concept substrate — something structurally impossible in the current
norm of "every app ships its own separate, disconnected AI feature."
Game mechanics themselves become taught behavior instead of hardcoded
logic (Q7's "school model"), authored from natural language through
the same compiler a human already uses (§2.6), and correctable by
anyone who can read a named weight, not just whoever originally
trained the model.

Both pitches rest on the same underlying, already-specified
architecture. Neither is aspirational hand-waving — every mechanism
cited above (the Bank Layer, the Concept Bank's schema, the Events
compiler, the promotion-gate discipline) is either already real and
working in this house, or specified concretely enough in this
document to build. That is the actual basis for the comparison: not
that this document is as *famous* as the 2017 paper, but that it is
attempting to be as *foundational* — one real, reusable, well-specified
mechanism, positioned to change what an entire category of products
can do next, the same way that paper's mechanism did.

---

## Grounding

`AI-TRACK-BRAINSTORM-QUESTIONS.md` Q5–Q9 (same house, same session,
2026-09-22), `LLMUD-HACK.md`, `HARNECIENT-HACK.md`,
`#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/` (audited directly,
not summarized, same session), `&.widgits/events-hq/ops/` (Events
compiler, read directly), `#.Z.HUMAN_LLM/^.2DO.aug01_2026.txt` Track
C, NIGHT_20/21/22 (dramatized companions to Q7/Q8/Q9 — this document
is the technical spec those episodes are based on, not the other way
around). §8's domain citations checked directly: `&.hq-apps/chain-hq/`,
`@.apps/myne-qrypto/MYNE_QRYPTO_DESIGN.md`, `#.ref/menu/palletes/
chemistry_tiles🏆.csv`, `045.muchi-pal-agent🤖️+1++/lab-audio/`,
`@.apps/piececraft-hq/BOARD-CONTROLS.md`.
