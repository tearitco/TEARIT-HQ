# AI track — open questions to brainstorm (item 3, deferred)

**Status: NOT a plan, not started.** A holding pen for the user's real
curiosities/concerns about "what AI work should we actually build,"
raised 2026-09-21 while explicitly deferring item 3 (the IRL/AI slice)
until items 4 (piececraft-hq board bug) and 2 (events from inside
Inventory) land. Read before starting item 3's real design.

## The core uncertainty, in the user's own words

> "so sure def push into ai but wut ai? chat? useful automation? irl?
> im not sure what works, etc. so we are going to brainstorm with
> claude on actually building useful tools provably useful (and they
> should probably use events)"

Not a request to pick one now — a request to have this conversation
properly once items 4/2 are done, and to make sure the eventual answer
is **provably useful**, not just architecturally interesting, and
**events-based** where possible (matches §2b's house-wide rule already
established this session: never hand-write C for what an event can do).

## Question 1: a Gemma "codebase Q&A" assistant, through Cursword

> "could we have a gemma ai (thru cursword) that answered questions
> about the codebase based on book + some reading / word storing,
> re-reread word store etc. stuff that is verifiable w/o touching
> code & also useful."

Real, concrete idea: Cursword gets a chat mode backed by Gemma that
answers questions about the house using `!.HQ-IQ-BOOK/` (the docs
already written specifically to onboard an agent) plus some kind of
persistent "word store" it re-reads/re-reinforces over time, rather
than a fresh context window every time. Two things make this
attractive as a REAL first build, not just a nice idea:

- **Verifiable without touching code.** A Q&A tool's output can be
  checked against the docs by a human, cheaply — no runtime risk, no
  "did this silently break something" question. Matches the DESCRIBE-
  not-CLASSIFY discipline (`HARNECIENT-HACK.md`) naturally: the answer
  IS the describe-output, there's no discrete decision to get wrong.
- **Reuses real, already-scoped house infrastructure**: the Synonym
  Bank concept (`LLMUD-INTEGRATION-DESIGN.md`'s own smallest-first-
  step) is close to "a word store that gets re-read/reinforced" already
  — this idea may just be Synonym Bank's first real consumer, not a
  separate system.

**Open, for the item-3 conversation:**
- Is "the book" (`!.HQ-IQ-BOOK/`) read fresh each query, or ingested
  once into a real word store Gemma re-reads (cheaper, but staleness
  risk when docs change)? If a store, how does it know the book
  changed?
- Does this compete with or complement the existing chat-hai/Cursword
  Chat button (real, already wired to a general LLM chat — see
  `13.agent-coms/KILO/claude-2-kilo-9.17.md` §11's decision to add a
  SECOND context-menu option rather than touch the first)?
- Scope: house architecture questions only, or also "how do I do X"
  task-help? The former is much safer/more verifiable to ship first.

## Question 2: what kind of AI work is actually worth building?

The user named three candidate shapes, unsure which is right:
- **Chat** — a Q&A/conversational surface (Question 1 above is a real
  instance of this).
- **Useful automation** — AI doing a real task end to end (the robot/
  puzzle-piece entity idea from item 2's bridge, and the `ai_describe`
  event primitive named in earlier design docs, are both this shape).
- **IRL** (inverse reinforcement learning / the watch-and-learn track)
  — `LLMUD-HACK.md`/`DUSTOPIA-HACK.md`'s own subject, watching real
  action sequences to build Behavior Banks.

These aren't mutually exclusive — Question 1 (chat) could be the FIRST
real, low-risk, verifiable proof that the underlying Gemma-DESCRIBE
mechanism works at all, before trusting it inside "useful automation"
or IRL, which both have real runtime/behavioral risk if the mechanism
turns out to be unreliable. Worth deciding in the item-3 conversation:
does chat-first-as-proof, then automation, then IRL make sense as a
real build order — matching the house's own existing bias toward
cheap, verifiable, reversible first steps?

## Question 3: "famous LLM" / background training discipline

Direct question, 2026-09-21: is anything training Gemma into a smaller
"famous" model in the background right now, "as was originally
intended," so it's making progress while other work happens?

🔄 **CORRECTION (2026-09-21, same day - my first answer here was
wrong, don't repeat it):** I originally wrote "no, nothing exists" for
this question, based only on checking `IRL-BOOTSTRAP-RECURSION-SPEC.md`
(genuinely still "RESEARCH/DESIGN ONLY, not started" - that part was
right) and a repo grep for gemma-training scripts, which missed the
REAL project entirely because it doesn't have "gemma" in its name.
Direct correction from the user: **it already exists**, real and
substantial, at
`44.xyz.01.00/#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/` (also
referenced as "tomom" / "famous : 3.stage.llm.tomom@qroq.fame"). This is
a from-scratch, hand-built small LLM/training pipeline in C — real
`attention.c`/`forward_prop.c`/`backward_prop.c`/`optimizer.c`/
`mlp_layer.c`, a `chatbot_moe_v1.c`, an `http_server.c`, and real
directories for `curriculum/` (23 entries incl. per-subject Astronomy/
Biology/Chemistry/Economics/Geography train sets), `corpuses/`,
`distil/` (knowledge distillation), `meta_rl/`. It has its own honest
internal status doc (`dox/goals_report.md`): causal attention is
**partially implemented** (works, but a real bug — `causal_attention`
hardcoded to 0 in one call path inconsistently with the `config.txt`
setting) and per that same self-assessment, **not yet implemented**:
full knowledge distillation, Meta-RL as a core component, the MoE
architecture with multiple experts, behavior-trees-with-LLM-proxy, and
an SD-Emoji renderer — despite scaffolding files for several of these
already existing. No recent-activity evidence of it currently training
(nothing found newer than its own reference files) — real, substantial,
but **currently idle**, not actively running right now either. Whether
this project uses (or should use) the SAME recursive bootstrap
mechanism `IRL-BOOTSTRAP-RECURSION-SPEC.md` describes is NOT
established - flagging as open rather than assuming equivalence.

**New, real user request to fold into the item-3 conversation**: test
and confirm tomom's actual capabilities, then give it a real spot in
h-ai (a live surface, not just files on disk) AND in an "h-ai studio"
view where its Banks and training processes can be monitored. This is
a concrete, scoped next step once item 3 starts - not yet done.

**For the item-3 conversation**: the user explicitly wants this
running as a standing background discipline once started ("delegate
to have work going while we're doing other things, this is a core
discipline we want to get going asap"), not a one-shot script. Real,
already-identified cheapest starting point (`LLMUD-HACK.md` §2): watch
Claude Code's own tool transcript / the relay files that already exist
- no new capture infrastructure needed - rather than waiting on a real
gameplay loop from kilo's WSR-CIV/DSR work. Whether that's the right
FIRST watch target, vs. waiting for item 2's robot-entity events to
give it something more game-relevant to watch, is itself worth
deciding explicitly in the item-3 conversation rather than assumed.

## Question 4: the 🤖️ robot/puzzle-piece entity

> "we were gonna give the 🤖️ ..." (message cut off)

Referenced in item 2's own plan (`12.calendar/2026-09-20/2do.md` §8:
"robot/puzzle-piece entities carrying events, dropped into inventories,
methods run from the Inventory right-click") — the vertical slice item
2 is about to build. Flagging here because it's the literal connective
tissue between item 2 (events from inventory) and item 3 (AI track):
a robot entity's event pages are exactly the kind of small, bounded,
inspectable unit that "useful automation" (Question 2) would want to
compose with an `ai_describe`/`ai_*` primitive later. Worth asking the
user directly what the rest of that sentence was, since it may name a
specific first robot use case already in mind.

## Question 5: the JEV docs (2026-09-22) — reviewed, honest take

User asked me to read and react to
`XO/LLMUD_CODE/8.0.JEV=class-4-describe.md` and
`8.1.harn+jev-diagram.md`. Both read as output from an external
session (unclear which model/tool), not house-authored. My real
review, not just agreement:

**What's genuinely useful:** `8.1`'s full-stack diagram (User Input →
Parser/Synonym-Bank-lookup → Bank Layer [Synonym/Relation/Behavior/
Sentence] → FSM Sequencer → Event Executor → Action Manifest → User
Feedback loop, plus a parallel Watch Layer and a Primitive Layer at
the floor) is the first single picture tying together everything this
session already built piecemeal across `HARNECIENT-HACK.md`,
`LLMUD-HACK.md`, `DUSTOPIA-HACK.md`, `LLMUD-INTEGRATION-DESIGN.md`, and
the kilo handoff's `ai_*` primitive plan. It doesn't contradict
anything real: the Laplace-smoothed weight formula matches
`LLMUD-HACK.md` §5 exactly, the Watch Layer matches `LLMUD-HACK.md`
§2's resolved design (relay + tool transcript, no new capture infra),
and the Primitive Layer names match the kilo handoff exactly
(`ai_describe`/`ai_fsm_transition`/`ai_goap_plan`, registry AI category
confirmed empty this session).

**What I won't accept uncritically:**
- **"JEV the product" (TypeSafe AI, $0.042/M tokens, Choice/Score/Noul
  primitives) is asserted, not independently verified by me** — I have
  no confirmation this external product's specs are accurate. The
  value of the "mini-JEV" framing doesn't depend on JEV being real or
  correctly described — it's useful purely as a naming/analogy device
  ("System 1"/Kahneman fast-thinking, calibrated structured decisions),
  not as evidence the house should model anything against a real
  competitor's exact spec.
- **The diagram presents all four banks as equally real** — they
  aren't, per `DUSTOPIA-HACK.md` §3's own honest table: Synonym Bank
  has a real smallest-first-step, Behavior Bank fully reuses
  `LLMUD-HACK.md`'s schema, but Relation Bank and Sentence Bank are
  still just named ideas with zero house-side design work. Any
  house-native version of this diagram should keep that honesty, not
  smooth it over.
- **The restated rule** ("classification must happen in a
  non-autoregressive, owned layer, never inside an LLM call") is a
  clean, correct restatement of `HARNECIENT-HACK.md`'s existing
  DESCRIBE-not-CLASSIFY law — not new, but a good sharper phrasing
  worth reusing.

**Recommendation, not yet done**: once item 3 actually starts, fold a
cleaned, house-native version of the `8.1` diagram (JEV-as-analogy
only, banks table honestly marked partial) into a real design doc as
the canonical "one architecture picture" — this house has needed
exactly this since `NIGHT_16` was first critiqued for lacking
mechanical depth.

## Question 6: transparency, KPIs, and the "teach humans the same tools" product angle (2026-09-22)

Direct, longer user message, real and not yet acted on — recorded in
full spirit here so it isn't lost before the item-3 conversation:

- **Test the famous LLM (tomom) and other pipeline aspects** — already
  a named next step in Question 3 above.
- **Train the FSM/RL/IRL loop to do real minor jobs, iterating and
  stacking "till we are saving real tokens"** — i.e. the watch-and-
  learn loop isn't just a research curiosity, its explicit success
  metric is measurable Claude-token savings over time, via Harnecient-
  Hack-style automation/delegation.
- **Keep parallel human-facing documentation of the same tools/
  harnesses** — explicitly named as a real, intended product/selling
  point of this house: not just "Claude can do X automatically" but
  "here's how a human operator can drive the exact same harness by
  hand," so the automation story is legible and reproducible, not a
  black box.
- **Wants real, transparent KPIs** — to be worked through together,
  not assumed — covering: what's being tested/fixed/trained and why,
  how it drives measurable Claude-token savings, and a path toward
  real independence for (a) understanding the codebase/docs via chat
  and tool-use, (b) eventually even minor bug fixes, (c) game
  programming, (d) agentic user-behavior emulation.
- **A new NIGHT script may be warranted** to address these concerns —
  user's own words, not yet started; would naturally pair with a
  house-native version of the Question 5 architecture diagram once
  that exists, so the NIGHT has a real picture to dramatize rather
  than only prose.

**Not yet done, deliberately**: no KPI list has been drafted, no
training loop started. This is exactly item 3's own conversation,
now with real material (the JEV docs' synthesis + this KPI/product
framing) to start from once items 4/2 are confirmed solid and the
user is ready to have it.

## Question 7: the training-layer gap, the "school" model, growable primitives, and FILE:DESK → WORLD (2026-09-22)

Direct follow-up conversation after Question 5's diagram review, real
and substantial — the user called this "a huge breakthru." Recorded
in full so item 3 has real material to start from.

**7a. The corpus-chunk / retraining / attention gap in the `8.1`
diagram.** Checked the diagram directly before answering (not from
memory). Confirmed: the Bank Layer is deliberately *not* a model — it
is four weighted dictionaries updated by Laplace-smoothed reward
counts, and the LLM Layer (Gemma 270M) is drawn as a **frozen,
inference-only fallback** with no arrow feeding training into it, no
corpus/chunk store, no attention mechanism anywhere in the picture.
That's a real, honest gap, not an oversight in this review — the
diagram never had a training loop in its scope at all.

Where it actually belongs: this is Famous LLM (tomom)'s concern, not
the Bank Layer's and not Gemma's. It needs a **new layer** — a
Corpus/Training Layer, sitting alongside the Watch Layer, feeding
tomom specifically — because tomom is the one component meant to grow
by retraining, as distinct from the Bank Layer's "learning" (which is
only weight updates on existing entries, never new structure) and
Gemma's role (permanently frozen, DESCRIBE-only, never fine-tuned in
this design). Adding new corpus chunks and retraining attention is a
structurally different kind of growth than anything currently drawn.

**7b. Scope confirmed: all four Banks, used everywhere.** Not a single
demo feature — intended for games and any other application as the
house grows. Explicit, lowered bar for now: a working demo doesn't
need to "cure cancer," it needs to (a) be genuinely impressive to a
technical ("greybeard") audience and (b) actually help a user save
real tokens, or show them how to build more FSM/GOAP scaffolding to
grow the token-saving foundation further. This matches Question 6's
own token-savings KPI framing — restated here because it now applies
to all four Banks, not just the Behavior Bank alone.

**7c. Famous LLM (tomom): same bar, plus the "school" model.** Goal is
"basically works" — able to do the most basic real conversation and
math, later taught to code in Events and/or a binary form of prisc as
a bootstrapping curriculum — always augmented by the existing
FSM/IRL/GOAP machinery specifically to make tomom's current weak parts
useful as **growing/learning joints**, not dead weight waiting on a
future rewrite.

The real long-term structure, named explicitly by the user: **school-
based training**. Tomom (and later, one instance per entity) attends
"classes" with curricula authored by a teacher — the user now, Gemma
later — and is tested pass/fail on each one until it passes, rather
than being globally fine-tuned all at once. This reframes training as
discrete, gated, per-skill lessons instead of one undifferentiated
loop.

**7d. Bootstrapping is universal policy, not a one-off.** Every piece
of work that crosses the Events threshold should, wherever possible,
(a) build its own tools in Events rather than only being driven by
Events, and (b) simultaneously train IRL/tomom/attention/whatever
applies — the goal being to find how soon the house can bootstrap
itself and delegate its own actions. This applies to game-building via
relay (users building games should exercise and train the same loop)
and to the house's own tooling work, symmetrically.

**7e. The real long-term goal, stated directly: as few hardcoded game
mechanics as possible.** Every entity gets its own learner instance,
which can attend different "schools" depending on the skill a given
game or context needs (farming, cooking, science, physics, ...) —
mechanics become taught behavior, not hardcoded C. Even the
**primitives themselves should stay flexible enough to grow** — e.g. a
user should be able to "train" a game's physics engine to get more
advanced physics, rather than the physics primitive being a hard,
permanently fixed floor. This reframes "primitive" as *the smallest
thing still allowed to grow*, not a hard architectural floor the way
Question 5's diagram currently draws the Primitive Layer.

**7f. `FILE:DESK` naming — ADOPTED 2026-09-22, `BOOK:PAGE` (superseded
`WORLD` same day).** The user noted the `FILE` half of `FILE:DESK`
causes real confusion (collides with the OS sense of "file"
constantly). First landed on `WORLD` (floated against `ROOM`, which
undersells the scope). Later the same day, superseded again by
`BOOK:PAGE` — a session (a container holding multiple desks) is a
`BOOK`, an individual desk is a `PAGE` — the book-contains-pages
analogy read better than world/desk once actually live on screen.

**Correction to this section's own earlier claim**: `FILE:DESK` was
initially reported here as "never a literal code identifier, pure
vocabulary." That was wrong — a grep for the compound string
`FILE:DESK` missed it because the real code pair is two separate
functions with a shared colon-suffix convention:
`ktb_get_file_label()`/`ktb_get_desks_label()` in
`khtpm_taskbar_manager.c`, which literally render `file:<session>` /
`desks:<desk>` in the live taskbar strip. This WAS real, load-bearing,
user-visible code, not just documentation vocabulary. Both are now
`ktb_get_file_label()` → `book:<session>` and `ktb_get_desks_label()`
→ `page:<desk>` (function/variable names unchanged, only the emitted
label text). Committed `16710365`.

**A real process lesson from this rename** (worth keeping, not just
the outcome): the first pass swapped the wrong half — `desks:` became
`world:` instead of `file:` becoming `world:` — caught only because
the user compared actual on-screen text against what was intended.
Verifying via the text receipt (`strip_ui.txt`) alone wasn't enough
either: a frame dumped immediately after a khtpm restart showed stale
pixels from before the repaint (same X11 window ID persisting across
restart, backing-store lag), which briefly looked like a second,
unrelated bug. Real fix for that class of mistake going forward:
after any khtpm restart, wait for an actual repaint before trusting a
`dump_frame_png_op` capture — a text receipt updating is not proof
the drawn pixels have caught up.

**Not yet done, deliberately**: no Corpus/Training Layer has been
designed or built; no school/curriculum format has been defined. The
naming itself IS done (code-level, not just a doc recommendation) —
see `khtpm_taskbar_manager.c`'s two label functions.

## Question 8: the gameplay injection gap — where does tomom's learned output actually change gameplay? (2026-09-22)

Real gap, caught by a video-prep agent reviewing Question 7/NIGHT_20
before scripting a 20-min technical video
(`🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive/!.gemini-llm-sept/6.JEV-IRL/augment-docs-prompt.md`).
Correct catch: Q7 established that a Corpus/Training Layer needs to
exist and feed tomom, but never traced the **return path** — how
tomom's learned output gets back into the FSM SEQUENCER / EVENT
EXECUTOR loop and actually changes what a player experiences. The
Bank Layer has this return path (reward/punish → weight update →
next lookup uses it); tomom's training loop, as drawn so far, does
not. This section answers it, with real honesty-marked uncertainty —
most of this is DESIGN-LEVEL, not SPECIFIED, and one sub-answer is
flagged UNBUILT with no shape yet, per that agent's own instruction
not to invent an answer where one isn't clear.

### 8a. Injection points

- **FSM path selection — DESIGN-LEVEL.** tomom should not write
  directly into a live FSM table. The house-consistent shape: tomom's
  candidate sequences get filed as new, low-weighted entries into the
  **existing Sentence/Behavior Bank format** (not a separate store),
  and only reach the FSM SEQUENCER through the *same* Laplace-smoothed
  reward/punish promotion mechanism the Bank Layer already uses for
  everything else. This keeps one promotion mechanism house-wide
  instead of inventing a second one. **Unbuilt**: no file format for a
  "candidate, not yet promoted" Bank entry exists yet; the current
  Bank format doesn't distinguish that state.
- **Event parameter tuning — DESIGN-LEVEL, most concrete of the
  four.** Events already read parameters from real `.pdl` files (the
  house's existing "real files, not hidden state" convention). The
  natural extension: tomom writes a per-entity/per-event
  `learned_params.pdl` override row; the Event Executor reads it at
  execution time and falls back to the hardcoded C default when the
  row is absent or the override hasn't been promoted. Cleanest of the
  four because it requires zero new C and zero new FSM-table
  mechanics — it's an ordinary optional-config read, a pattern that
  already exists elsewhere in the house.
- **Event generation — UNBUILT, real open decision, not invented
  here.** Two shapes are in real tension: (a) tomom DESCRIBEs a
  desired Event in natural language, and a deterministic
  compiler/scorer (the same DESCRIBE-then-owned-code-decides shape the
  Parser Layer already uses) turns that into a real, compiled Event
  package — keeps the house's "model never routes, never picks the
  path" law intact; (b) tomom emits Event file content directly — this
  would make the model the author of executable logic, which conflicts
  with that same law as currently stated. **(a) is the only shape
  consistent with existing house rules**, but this is a real decision
  point for item 3, not something decided here.
- **Primitive behavior approximation (e.g. a learned physics
  substitute) — UNBUILT, no shape yet.** The only house-consistent
  starting idea: run any learned approximation as a shadow/candidate
  scored against the real primitive's output or against live user
  reward, using the same promotion-gate pattern as the other three —
  never live by default. Flagged explicitly as unshaped; do not treat
  this as decided.

### 8b. Concrete walkthrough — physics training, start to finish

1. **Initial state**: a game's jump/gravity calc is the hardcoded C
   physics primitive, fixed constants.
2. **User action**: a player does a stunt jump; the existing Action
   Manifest shows before/after state; the player presses one of the
   existing feedback buttons (👍/👎/🔙/✅ Lock In).
3. **tomom learns (design-level, not built)**: the Watch Layer
   observes the same action sequence + outcome + feedback it already
   watches for the Behavior Bank. Instead of (or alongside) filing
   into the Behavior Bank, the sequence becomes a training example
   added to a per-entity, per-skill "physics class" corpus chunk (the
   Corpus/Training Layer named in Q7). tomom retrains/re-attends
   incrementally on that growing, skill-scoped corpus — this is the
   literal mechanism the "school" model's curriculum/class framing
   from Q7 was describing, made concrete.
4. **Next gameplay (unbuilt — this is the actual open injection
   mechanism)**: tomom's updated output would need to write a new
   candidate row into that entity's `physics_learned_overrides.pdl`.
   The real physics primitive's read path checks for a promoted
   override there before falling back to its hardcoded default — gated
   by the same reward-weighted promotion logic as 8a, so a learned
   override doesn't go live until it's earned it through repeated
   positive feedback. What changed, concretely: one new `.pdl` row,
   one conditional read added to the primitive's parameter lookup —
   still a real file, still fully inspectable/overridable/reversible,
   never hidden state.

### 8c. Staying Event-based, not hand-written C

The parameter-tuning path (8a) stays cleanly Event-based — Events
already read `.pdl` parameters, so this needs zero new C. The two
real tension points are FSM-path-selection and Event-generation
(8a): both are only house-consistent if tomom's raw output goes
through a DESCRIBE step and a deterministic compiler/promotion gate
before anything executes — never a direct write into a live FSM table
or a hand-emitted `.c` file. This is the same DESCRIBE-then-owned-
code-decides law the Parser Layer already enforces, applied one layer
further down the stack.

### 8d. Honesty checkpoint

| Mechanism | Status |
|---|---|
| FSM path selection (candidate → Bank entry → promotion) | DESIGN-LEVEL |
| Event parameter tuning (`.pdl` override + fallback read) | DESIGN-LEVEL, most concrete |
| Event generation (DESCRIBE + compiler, vs. direct emission) | UNBUILT — real open decision |
| Primitive approximation (shadow-scored substitute) | UNBUILT — no shape yet |

Nothing in this section is SPECIFIED (fully decided, ready to
implement). Nothing here should be read as built or in progress.

### 8e. The three-layer table, Gameplay Execution row filled in

| Layer | Input | Processing | Output | Stored where? |
|---|---|---|---|---|
| Bank Layer | User reward signal | Laplace-smoothed weight updates | Weight changes only (no new entries) | Synonym/Relation/Sentence/Behavior Banks |
| Corpus/Training Layer | Action sequences from Watch Layer | tomom retrains on new examples | New corpus chunks, improved attention | tomom's Corpus/Training store (Q7, not yet built) |
| Gameplay Execution | tomom's learned output (candidate FSM paths / parameter overrides / DESCRIBE'd Event specs) + existing FSM + hardcoded C | Reward-weighted promotion gate (same pattern as Bank Layer) before anything goes live; DESCRIBE + deterministic compiler for anything Event-shaped | In-game behavior change via a promoted FSM path, a promoted parameter override, or a newly compiled Event | Extended Behavior Bank format (candidate paths) + new per-entity `learned_params.pdl` files + compiled Event packages (if 8a's Event-generation decision lands on shape (a)) |

**Not yet done, deliberately**: no candidate/promotion state added to
the Bank format; no `learned_params.pdl` convention created; the
Event-generation decision point (8a) not resolved; no shadow-scoring
mechanism for primitive approximation designed. This is the honest
shape of the gap, for item 3 to resolve — not a spec to build from
yet.

## Grounding

`HARNECIENT-HACK.md`, `LLMUD-HACK.md`, `DUSTOPIA-HACK.md`,
`LLMUD-INTEGRATION-DESIGN.md`, `IRL-BOOTSTRAP-RECURSION-SPEC.md`,
`13.agent-coms/KILO/claude-2-kilo-9.17.md` §5/§11,
`12.calendar/2026-09-20/2do.md` §8/§10,
`XO/LLMUD_CODE/8.0.JEV=class-4-describe.md`,
`XO/LLMUD_CODE/8.1.harn+jev-diagram.md`,
`🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive/!.gemini-llm-sept/6.JEV-IRL/augment-docs-prompt.md`.

## Question 9: the Concept Bank — hand-tunable weights instead of opaque matrices, and how the whole learning network shares one substrate (2026-09-22)

Real, substantial architecture session, directly requested to be
written up "as detailed as we can" before any of it gets coded — this
is the material NIGHT_22 dramatizes. Grounded in a real, unplanned
finding first: tomom's actual attention/MLP matrices were audited
directly (`chatbot_moe_v1.c`/`trainer.c`/`mlp_model.txt` read in full,
not summarized) and the `chatbot_moe_v1.+x` binary running them turned
out to be a full day stale relative to its own already-fixed source —
rebuilt and verified live (all 10 real subject curricula now show
`Loaded trained model from ...`, confirmed by output changing). That
fix needed zero design work, just a rebuild; not committed since `+x/`
binaries aren't git-tracked. Full separate audit findings (meta_rl
hardcoded to 2 test curricula not the real 10; grade-level curricula
confirmed fully greenfield; `_train` folders are training sandboxes,
not difficulty tiers) are preserved in this session's transcript, not
duplicated here — the load-bearing finding for THIS question is what
came next.

### 9a. Why dense attention/MLP matrices are the wrong substrate here

Direct, explicit house decision: **no opaque weight matrices, anywhere,
ever, in tomom.** Not because dense matrices don't work — they do,
that's the entire premise of GPU-scale training — but because the
house's actual intent for tomom was never "train at scale via brute
force." It's **precise, hand/fine-tuned adjustment via meta-harness
techniques** (Track C, `#.Z.HUMAN_LLM/^.2DO.aug01_2026.txt`, already
states this goal outright: *"hand-tunable, modular, auditable LLM
track"*). Dense matrices are opaque by construction, not by accident —
gradient descent distributes each concept across many weights and
packs multiple unrelated concepts into the same weight (superposition).
There is no version of "make attention/MLP matrices legible" that
doesn't fight the architecture itself. Confirmed directly during this
same audit: `mlp_model.txt`'s real current values include `13075.6`,
`17331.1`, `-861.1` — unlabeled, uninspectable, and quite possibly a
recurrence of the exact weight-explosion bug the Aug 1 session
believed it had fixed. That's not a hand-tunable file; nobody could
look at row 3 column 47 and know what editing it would do.

The house already has the right shape for this goal, just not applied
to tomom yet: the **Bank Layer** (Synonym/Relation/Behavior/Sentence
Banks) is sparse, named, keyed, Laplace-smoothed, auditable by
construction. This question's whole design is: give tomom the Bank
Layer's discipline instead of a transformer's.

### 9b. The Concept Bank — schema, arrived at through real back-and-forth

**Z-nodes are named abstract concepts** (`force`, `motion`, `energy`,
`quantity`, `structure`, `change`, ...), hand-authored to start,
extensible later the same way any Bank grows (by Gemma, by tomom
itself once bootstrapped, by any agent).

**Hub-and-spoke topology, not a maze** — direct correction mid-design,
real and important: pointers do NOT go word-to-word or
concept-to-concept freely. Every pointer resolves to a **master
word/concept** only. This is the same real, proven pattern as WordNet
synsets (every word sense points to one canonical synset, never to
another word sense directly) — bounded fan-out, no chains-of-chains to
untangle when auditing. Concepts themselves can point to OTHER
concepts (`force` → `motion` at 0.7, `energy` at 0.5) using the exact
same record shape spokes use to point at masters — one generic
slotted-pointer record type, reused at both the word level and the
concept level, matching the house's existing "few, flexible, growable
primitives" discipline (Q7) rather than inventing two formats.

**The slot record itself** — fixed-width, N slots (tunable constant,
start at 4-8, room to grow to 32+), each slot = `(pointer, weight)`,
defaulting to zero/empty until actually used:
- **Growable without a format break** — more slots, not a redesign.
- **Trim** — zero a low-weight slot (pruning), same discipline as
  letting a Bank entry's weight decay toward the floor.
- **Beef up** — fill previously-empty slots as new relations get
  authored or observed.
- **Compress/decompress** — on-disk storage writes only the active
  (non-zero) slots (sparse form), expanded to the full fixed-width
  record in memory at load time. Same principle the vocab table
  already uses today (only non-zero bias fields get written per row).

**Normalization — where do weights actually live, forward vs.
mirror**: resolved directly, this was a real open question, not
guessed. **Weights live in exactly one place: the spoke's own forward
record** (a word's own outbound slots to the masters it relates to,
and their weights) — that's the single authoritative fact, the thing a
human/Gemma/tomom actually hand-edits. **The master's own record holds
a mirror table — everything currently pointing at it, and their
weights — but that mirror table is a derived, regenerated index, never
a second authoritative copy.** Storing the same weight in two
independently-editable places is exactly the bug class this house
already got bitten by tonight (the phymoji sprite cache going stale
because nothing regenerated it when `sprite.csv` changed) — same
failure mode, same fix: one real source of truth, everything else
rebuildable from it, never hand-patched. This gives O(1) lookup in
both directions (a word's own outbound relations; "what points at this
master") without any risk of the two copies disagreeing.

**Corpus-level meta-weights** (`(Mathematics, Physics) → 0.8`) — real,
explicitly requested second axis, distinct from word-level z-nodes.
**Still an open decision, not resolved in this session**: hand-authored
directly as its own table, derived automatically from how much two
corpora's vocabularies overlap in shared z-node/concept membership, or
both (hand-authored as an override on top of a derived baseline).
Whichever is chosen, this directly fixes a real, confirmed flaw the
tomom audit found: `meta_rl` currently scores each curriculum
completely independently with zero cross-curriculum structure — a
Physics prompt can't legitimately pull in Mathematics at all right
now.

### 9c. The meta level — how this one substrate strengthens everything else, not just tomom

This is the direct extension asked for: how do these weights get used
across agents, FSMs, RL/IRL harnesses, and GOAP, so autonomy actually
compounds instead of each subsystem needing separate tuning.

- **Shared across agents, for free, by construction.** The Concept
  Bank is a real file-backed structure, same as every other Bank —
  multiple agents (this Claude session, kilo, grok, tomom itself)
  reading and writing it are automatically sharing it, no sync
  protocol to build. Any agent's edit is just a new/updated spoke
  entry, auditable via git/diff exactly like any other house content —
  the same "real files, never hidden state" principle this whole house
  already runs on, just applied to concept weights instead of window
  state.
- **FSM Sequencer.** Today it picks paths purely from Bank matches
  (Q5's diagram). With a Concept Bank, a candidate FSM transition whose
  triggering conditions share high concept-overlap with the current
  context can be preferred over one that doesn't — FSM path selection
  gets the same legible "why" as everything else, instead of being a
  black-box match/no-match.
- **RL/IRL harnesses.** A reward signal (user feedback, or a Watch
  Layer observation) updates the Bank entry it directly matched
  (existing mechanism, unchanged) **and** propagates to the concepts
  that entry touches, when the matched word/action is concept-tagged.
  This is the actual mechanism that makes IRL (inferring the user's
  real intent from feedback) generalize past the single literal
  instance that got rewarded — feedback on one thing meaningfully
  informs everything sharing its concepts, not just its own exact Bank
  row.
- **GOAP.** Action preconditions/effects can carry concept pointers
  too (`increases: force`, `decreases: disorder`), letting a planner
  reason about actions abstractly, the way a person would describe
  intent, instead of only matching literal symbolic preconditions.
- **The actual "meaningful autonomy" mechanism.** Once Bank entries,
  FSM transitions, GOAP preconditions, and tomom's own concept
  membership all point through the *same* named concept substrate, one
  piece of feedback can propagate across every one of these systems at
  once, instead of needing separate tuning passes for each. That's
  concretely what makes "the user mostly just observes and gives
  intent-level feedback" plausible rather than aspirational — the
  concept substrate is the thing doing the generalizing.
- **Feedback needs a real shape to do this — not a bare scalar.**
  "Good, but do more of THIS" / "bad, do more of THIS" is a real,
  richer feedback schema than a single reward number: a feedback event
  is `(target_reference, valence, concept_pointer(s), intensity)` —
  e.g. `(this generated response, positive, force, high)` or `(this
  FSM transition, negative, verbosity, medium)`. The `concept_pointer`
  field is what lets one piece of feedback update the *right* concept
  weight specifically, rather than only the single literal action/word
  it was attached to — directional, targeted reward shaping instead of
  an undifferentiated up/down signal.

**Not yet done, deliberately**: no Concept Bank file format has been
built; the corpus-meta-weight hand-authored/derived/both decision is
still open; no feedback-schema change has been made to any real Bank
Layer code; nothing here has touched tomom's actual files yet. This
section is the honest shape of the design, for NIGHT_22 to dramatize
and for real implementation to start from once the corpus-meta-weight
question is answered.
