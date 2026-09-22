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

**7f. `FILE:DESK` naming.** The user noted the `FILE` half of
`FILE:DESK` causes real confusion (collides with the OS sense of
"file" constantly) and proposed renaming it, floating `ROOM` and
`WORLD` as options, landing on `WORLD`. Agreed and recommended:
`WORLD` is the better fit — `ROOM` undersells the scope (implies one
bounded space, when entities/pieces are meant to scale up to whole
game boards), while `WORLD` matches the game-engine vocabulary this
track is already heading toward and reads unambiguously next to
`ENTITY`/`PIECE`. **Not yet done**: this is a naming recommendation
only — no rename has been performed anywhere in code, docs, or the
`FILE:DESK` term's existing usages. Given `10. .xhtpm -> .xhtm rename
- CANCELLED` in `12.calendar/2026-09-20/2do.md` §10, any actual rename
must be treated as a deliberate, separately-scoped decision, not
silently folded into an unrelated commit.

**Not yet done, deliberately**: no Corpus/Training Layer has been
designed or built; no school/curriculum format has been defined; no
`FILE:DESK` rename has been executed. This section is the record of
the conversation, for item 3 and any future architecture-diagram or
renaming work to start from.

## Grounding

`HARNECIENT-HACK.md`, `LLMUD-HACK.md`, `DUSTOPIA-HACK.md`,
`LLMUD-INTEGRATION-DESIGN.md`, `IRL-BOOTSTRAP-RECURSION-SPEC.md`,
`13.agent-coms/KILO/claude-2-kilo-9.17.md` §5/§11,
`12.calendar/2026-09-20/2do.md` §8/§10,
`XO/LLMUD_CODE/8.0.JEV=class-4-describe.md`,
`XO/LLMUD_CODE/8.1.harn+jev-diagram.md`.
