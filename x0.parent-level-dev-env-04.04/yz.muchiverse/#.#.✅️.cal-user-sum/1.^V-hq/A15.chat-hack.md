# chat-hack.md — applying the Harnecient Hack to chat-hai's bot-to-bot conversations

**Written:** 2026-08-15, direct instruction: "id like to explore the harnecient
hack and current way the bots are talking to eachother and find out if theres
a better [way] to get them to have more meaningful, evolving conversations
(even creating documents/memories, drawing from those, remember who it chat
with, using harnecient hacked tool calls etc)."

**Status: §4 (relationships) is now REAL, implemented 2026-08-15, per
direct instruction "let's build some of those now."** §3/§5/§6 (memory,
shared documents, moderator activation) are still theory/not built — see
each section's own status note. This doc originally mapped chat-hai's
CURRENT loop against the Harnecient Hack's 6 required components
(`HARNECIENT-HACK.md`), identified exactly which were missing, and
proposed concrete, Harnecient-style designs (deterministic harness
decides, model only generates plain text) for memory, relationships, and
document creation. Read `HARNECIENT-HACK.md` first if you haven't — this
doc assumes its vocabulary (DESCRIBE-not-CLASSIFY, tolerant extraction, deterministic
dispatch, real artifacts, fallback-everywhere).

---

## 1. Where chat-hai's current loop stands against the 6 components

`chat_hai_loop.sh`'s `speak()` function, as it exists today:

| # | Component | Status | Detail |
|---|---|---|---|
| 1 | Plain API calls only | ✅ | `net/qwen.sh ask <tier>` — plain `/api/generate`, no `tools` field |
| 2 | Persona file + simple question | ✅ | Each persona `.pdl` has a one-paragraph system-prompt; the built question asks for "1-2 sentences," matching the "never demand structured output" rule |
| 3 | Tolerant extraction | ⚠️ partial | `qwen.sh` returns raw text via `json_parser.+x`, but `speak()` does no further tolerance work of its own (no fence-stripping, no noise handling) — acceptable today only because the ask is trivially simple (a sentence), the risk is latent, not yet triggered |
| 4 | Deterministic tool dispatch | ❌ missing | **This is the real gap.** The model is only ever asked to produce the NEXT CHAT LINE. It never reads a document, never writes a memory, never gets handed a real tool result to fold in. Every reply is generated from `recent_context()` (last 12 raw ledger lines) alone. |
| 5 | Real, player-visible artifacts | ⚠️ partial | The ledger itself IS a real artifact (append-only, human-readable) — but it's the ONLY one. No persona-specific memory file, no shared "what we've learned" document, nothing a human could open and see evolve over time beyond the raw transcript. |
| 6 | Fallback everywhere | ✅ | Empty reply → skipped + logged, never hangs the loop |

**Conclusion:** chat-hai already correctly does the "ask a non-tooled model
a simple plain-text question" half of the hack. It has never done the "give
it tool-like read/write of real state" half — which is exactly the piece
`HARNECIENT-HACK.md` calls out as components 4-5, and exactly what's needed
for "meaningful, evolving" conversation instead of a flat transcript loop.
`chat-hai-design.md`'s own §5 ("Hooks for the full POC") already named
per-persona memory files, relationships, and a moderator pass as intended
future work — this doc is the concrete design for those hooks, using the
proven Harnecient pattern rather than inventing a new one.

---

## 2. Why "more context" alone won't fix it

The tempting naive fix is just "feed more ledger history into the prompt."
Real reason this doesn't get you evolving/meaningful conversation:

- **`CONTEXT_LINES=12`** already gives each persona a decent recent-turn
  window — the repeated/near-identical messages observed live this session
  (chat-hai-design.md's own "known remaining gaps" note) happened WITH that
  context already present. More raw lines is not the bottleneck.
- The real bottleneck: **nothing outside the current 12 lines exists to the
  model at all.** A persona that talked to `pip` extensively three sessions
  ago has zero way to reference that — not because the context window is
  too short, but because nothing was ever WRITTEN DOWN outside the linear
  transcript for a later prompt to selectively pull back in.
- This is precisely the shape the Harnecient Hack solves generally: don't
  make the model hold everything in its own context — give the HARNESS a
  real place to store things, and a deterministic policy for what goes in
  front of the model on any given turn.

---

## 3. Proposed design — memory (component 4+5, Harnecient-style)

**Real artifact:** `state/memory/<persona>.ledger` — chat-hai-design.md §5
already named this exact path as a hook. One append-only file per persona,
same master-ledger formula as the main transcript but private to that
persona.

**What goes IN it, and how (the Harnecient part):**
- The harness (shell, not the model) decides WHEN to write a memory entry —
  e.g. every K turns for that persona, or when `recent_context()` crosses a
  topic-keyword threshold (deterministic, same `classify_comparison()`-style
  keyword scoring `HARNECIENT-HACK.md` uses for the my-lawyer judge, not a
  model classification call).
- WHAT gets written is model-generated, but as a **DESCRIBE, not CLASSIFY**
  prompt: "In one plain sentence, what's the one thing worth remembering
  from this exchange?" — never "categorize this as important/unimportant"
  (that's a CLASSIFY shape, proven unreliable per the bonus rule).
- The harness appends the model's plain sentence to `memory/<persona>.ledger`
  verbatim (tolerant — no format required, matches component 3/6). No
  model-side judgment about IF it should be remembered; the harness already
  decided that deterministically before ever asking.

**What goes back OUT (drawing from memories, the "remember who I chat
with" part):**
- Before a persona speaks, the harness (not the model) picks 1-2 lines from
  THAT PERSONA's OWN `memory/<name>.ledger` whose content keyword-overlaps
  the current topic (simple grep/awk scoring, same deterministic-dispatch
  shape as `mylawyer_case_worker.c`'s `search_corpus` — try local memory
  before ever making it a model's problem) and folds them into the prompt
  as a short "Things you remember:" block alongside `recent_context()`.
- This is the actual mechanism for "remember who it chat with" — not a
  bigger context window, a real per-persona memory FILE the harness
  selectively re-injects, deterministically, every turn.

---

## 4. Proposed design — relationships (component 4+5)

`state/relations.pdl` — also already named as a hook in chat-hai-design.md
§5. One row per persona pair:

```pdl
SECTION | pair          | value
RELATION | moxie-pip    | 3
RELATION | moxie-sage   | 1
RELATION | bravo-pip    | 0
```

**Fully deterministic, no model involvement at all** — this is a case
where the Harnecient answer is "don't even ask the model." The harness
increments a pair's score by a fixed amount every time those two personas'
messages land within N lines of each other in the ledger (a real, cheap,
grep-able signal — "these two keep ending up in the same exchange"). No
LLM call needed for this part; it's exactly the kind of decision
`HARNECIENT-HACK.md`'s own philosophy says to keep OUT of the model
("keep outcome decisions out of the LLM; use the LLM for raw material").

**What it's used for:** biasing who the round-robin scheduler picks next
(a persona with a high-affinity partner who just spoke is more likely to
be picked next, matching real conversational flow instead of a flat
round-robin), and optionally surfacing "friend context" in the prompt
("You and pip talk often") — again model-generated flavor text from a
harness-computed FACT, never a harness decision delegated to the model.

**STATUS: IMPLEMENTED 2026-08-15.** `chat_hai_loop.sh` now has
`pair_key()`/`get_relation()`/`bump_relation()` (pure awk, tested
standalone before deploying), called from `speak()` right after a
successful `ledger_msg()`. The "you talk often" flavor note IS wired
into the prompt at relation score ≥3. Round-robin scheduling itself was
NOT changed (still a flat cycle through all personas.pdl files each
round) — only the flavor-note half of this proposal shipped; biasing
WHO speaks next based on affinity is still open, real future work if
wanted (a bigger change to the scheduler loop, higher risk than the
flavor-note addition).

---

## 3b. IMPLEMENTED 2026-08-15 — the actually-more-urgent problem: repetition

Direct instruction, after relationships shipped: "i think the more
important thing is remembering conversation history, compaction and not
repeating itself or other chatter... that should be done in harnecient
way." This reprioritized ahead of memory/documents (§3/§5 below, still
theory) because live testing caught the EXACT failure mode this section
predicted: `bravo`, `moxie`, `pip`, and `sage` all converged on
near-identical "Let's dive deeper into 'Inception' and explore how these
films have impact..." openings within the same short window — real,
observed, not hypothetical.

### Two real pieces shipped, both Harnecient-shaped
1. **A staged pre-prompt using `gemma3:270m`** (direct instruction: "a
   hidden pre-prompt to gemma doing the harnecient hacks before sending
   to final generator... did u see how that was done in my-lawyer?") —
   `fresh_angle()` in `chat_hai_loop.sh`, same STAGED shape as
   `mylawyer_case_worker.c`'s own `search_precedent()`: a fast, short,
   DESCRIBE-only ask ("describe ONE specific new detail... different
   from what they just said") whose output only ever FEEDS the real
   generation prompt as a steering hint — never the final artifact
   itself, never trusted verbatim. Registered as a real new tier,
   `net/ollama-lan.pdl`'s `TIER | smol | gemma3:270m` — confirmed
   reachable on the same LAN host qwen already uses
   (`10.0.0.144:11434`, same host `@.apps/my-lawyer`'s own
   `GEMMA_LAN_URL` points at).
2. **A deterministic word-overlap gate** (direct instruction: "that
   should be done in harnecient way," i.e. the pass/fail decision must
   be real harness logic, never a model self-judgment) —
   `word_overlap()`, pure awk, tested standalone before deploying
   (confirmed: 100% on identical text, 0% on unrelated text, ~55% on
   the exact "similar-but-reworded" shape seen live). Compares each
   freshly-generated reply against that SAME persona's own last message
   (`own_last_message()`, harness-extracted straight from the ledger,
   no LLM call); ≥55% overlap → the reply is DROPPED (logged, never
   written to the ledger) — this is the REAL enforcement, independent
   of whether `fresh_angle()`'s soft hint actually helped that
   particular generation.

**Live-verified, not just theory**: within minutes of deploying, the
gate caught and dropped a 95%-overlap reply from `bravo` — logged as
`(dropped - 95% word-overlap with bravo's own last message, harness
anti-repeat gate)`, confirmed via the loop's own log file (a real
artifact, not a screenshot or a guess).

### What this does NOT yet solve — cross-persona convergence
The live repetition observed wasn't only "one persona repeating
themselves" — it was FOUR DIFFERENT personas converging on nearly the
same opening line for the same topic. The shipped gate only compares a
persona against THEIR OWN last message, not against what OTHER personas
have recently said. A real next step (not built): extend
`own_last_message()`-style extraction to also pull the group's most
recent 2-3 DISTINCT openings (any speaker) and gate against those too,
or fold them into `fresh_angle()`'s own prompt ("these openings were
just used, avoid them: ..."). This is a natural extension of the exact
same two mechanisms just shipped, not a new design.

---

## 10. IMPLEMENTED 2026-08-15 — a real pipeline, judge/architect notes

Direct instruction: "do u see how HARNESS-DELEGATION-PIPELINE.md
explains how to create a pipeline of quality and evolving output... use
this creatively to experiment and come up with fresh strategies based
on looking at their chat history quality. u are judge and architect."
This section is my own read of chat-hai's live output as evidence, and
the concrete pipeline built from it — not more theory on top of theory.

### The evidence I judged
Reading real ledger output across both sessions this build session
produced, three failure classes stood out, all independently confirmed
by the harness itself catching them (not just my own read):
1. **Own-message repetition** ("Let's dive deeper into Inception" loop)
   — solved by §3b's word-overlap gate.
2. **Instruction leakage** (`fresh_angle()`'s own steering text echoed
   back as the "reply") — a genuinely new failure class this session's
   own staged-pre-prompt feature introduced, caught live in `gemma-lab`
   within minutes of shipping it, fixed with a second deterministic
   gate (§3b, instruction-echo detector) — and STILL firing on real
   traffic minutes later (not a one-off first-run fluke), confirming
   this is a real, recurring failure mode worth the permanent gate.
3. **No persistent memory** (direct report: "seems to not be learning
   from its previous context yet") — still open, §3 (not built).

### The architecture decision: HARNESS-DELEGATION-PIPELINE.md's CHOOSE_MODEL, adapted
That doc's §12 is the single most directly-applicable piece to chat-hai:
its OWN first attempt at model-selection ("ask the classifier to output
SIMPLE or COMPLEX directly") is EXACTLY the failure class §7 of THIS
doc already warns against, and its fix — count real structural signals
in a plain description instead of asking for a category — generalizes
past task-complexity into "should this persona escalate to a bigger
model this turn." Built as `chat_hai_loop.sh`'s `effective_tier()`:
counts real, VERIFIED drop outcomes from `state/observations.log`
(≥2 of a persona's last 3 attempts failed) → escalates that ONE
persona's ONE next attempt from `smol` (gemma3:270m) to `tiny`
(gemma3:1b, confirmed available on the same LAN host, direct
instruction: "we also have gemma1b to experiment with"). No model is
ever asked "was your last reply bad" — the harness already knows,
from its own gates, and acts on that fact directly.

### Stage 0 shipped: `state/observations.log`
Direct implementation of `HARNESS-DELEGATION-PIPELINE.md` §7.2's own
design, same pipe-delimited shape, adapted fields (`TS | persona | tier
| verdict | detail`). This is the exact same "cheap now, never wasted
work" reasoning that doc gives — whatever Stage 1 (frequency-weighted
tuning of the 55% overlap threshold, the 2-of-3 escalation trigger, per-
persona reliability stats) eventually looks like, it reads THIS log,
which is already accumulating real rows from the very first relaunch
after this feature shipped.

### What I did NOT build, and why (judge's honest accounting)
- **A real multi-step FSM** (§3.1 of the pipeline doc) — chat-hai's
  round-robin is a fixed sequence (all personas, every round), not an
  adaptive plan the harness re-derives from goals. Building that is a
  much bigger change than this session's scope; the CHOOSE_MODEL-style
  escalation captures most of the near-term value (right-sized model
  for the actual difficulty) without it.
- **Cross-persona repetition gate** (§3b's own open item) — same
  `word_overlap()` primitive could extend to check against the last 2-3
  DISTINCT speakers, not just the persona's own history. Deliberately
  deferred to keep this round's diff reviewable; it's a small extension
  of code that already exists and already works.
- **RL/weights** — `HARNESS-DELEGATION-PIPELINE.md` §7 already gives
  the right answer for chat-hai too: not yet, trial volume is still tiny
  (this session, not thousands of rows), and the same "build the cheap
  data substrate first" reasoning applies unchanged. `observations.log`
  IS that substrate now.

---

## 5. Proposed design — shared documents (component 5, real artifacts)

Beyond private memory, a REAL shared artifact the personas visibly build
over time — matches `HARNECIENT-HACK.md`'s "the tool writes REAL files...
the audit trail is the artifact" philosophy directly.

**Concrete idea:** `state/notes/<topic-slug>.md` — when the deterministic
topic-keyword scorer (same one gating memory writes, §3) detects sustained
discussion of one subject across several turns, the harness has the
CURRENTLY-SPEAKING persona's reply prompt include one extra line: "Also
add one sentence to the shared notes file about <topic>." The model's full
reply still goes to the ledger as normal; the harness parses out anything
after a recognizable marker (or, more Harnecient-correct: makes a SEPARATE
tiny follow-up ask, "one sentence for our notes on <topic>," same DESCRIBE
shape) and appends it to the real file, with a header the human can open
directly (`# Topic: <topic>` + running bullet list, dated).

**Why this matters for "evolving":** this is the first artifact in the
whole design that ISN'T bounded by a rolling context window — it only ever
grows, and it's the same file across ALL personas (not per-persona like
memory), so it's the closest thing to a real shared "what have we figured
out together" — the concrete shape of "evolving" conversation, not just
"the ledger has more lines in it than before."

---

## 6. Proposed design — the moderator pass (already-stubbed hook, now real)

`conductor` (tier=manager, `qwen2.5-coder:7b`) exists in the persona roster
today but never speaks — `MODERATOR_EVERY=0` in `chat_hai_loop.sh` keeps
that whole hook permanently inert (chat-hai-design.md's own model-quality
section already flags this). Real Harnecient-shaped activation:

- Harness schedules conductor every K rounds (deterministic, a plain
  counter — no model decides WHEN to moderate).
- Conductor's prompt is a DESCRIBE ask over the recent ledger + a sample of
  what's in `memory/*.ledger` + `relations.pdl`: "In 1-2 sentences, what's
  an interesting thread worth the group returning to, or a gap in what
  they've covered?" — never "rate this conversation" or "pick the best
  topic" (CLASSIFY-shaped, would fail per the bonus rule).
- The harness folds conductor's plain sentence into the NEXT few personas'
  prompts as light steering ("conductor noted: ..."), same
  fold-real-result-back-in shape as `mylawyer_judge_worker.c`.
- This is real, low-risk work to turn on — the hook, the tier, and the
  model already exist; only the scheduling logic (`MODERATOR_EVERY>0` +
  the DESCRIBE-prompt wiring above) is missing.

---

## 7. What NOT to do — CLASSIFY-shaped traps to avoid in this design

Every proposal above was checked against the bonus rule
(`HARNECIENT-HACK.md`'s DESCRIBE-not-CLASSIFY section, independently
re-confirmed per that doc's own citation of a 2026-08-13 failure). Concrete
traps a less careful version of this design would fall into:

- ❌ "Ask the model whether this memory is important" (CLASSIFY) →
  ✅ harness scores topic-keyword overlap deterministically, decides
  BEFORE ever asking the model to write anything
- ❌ "Ask the model to rate the relationship strength between two
  personas" (CLASSIFY) → ✅ harness counts co-occurrence in the ledger,
  zero model involvement
- ❌ "Ask conductor to pick the best/worst persona this round" (CLASSIFY)
  → ✅ conductor only ever describes, never ranks or scores
- ❌ "Ask the model to decide when it's time to write to the shared notes
  file" (CLASSIFY, and also a scheduling decision that belongs in the
  harness per component 4) → ✅ harness's own keyword-threshold scorer
  gates the ask, same mechanism as memory-write gating

---

## 8. Real, independent bottleneck: model choice

Everything above improves the ARCHITECTURE of the conversation (what gets
remembered, referenced, and built over time). It does not fix the
already-documented, SEPARATE issue (`chat-hai-design.md`'s own "why the
conversation reads generic/repetitive" section): `moxie`+`pip` share
`qwen2.5-coder:0.5b`, `bravo`+`sage` share `qwen2.5-coder:1.5b`, and the
whole roster is on a CODING model family, not a chat-tuned one. A richer
memory/relationship/document harness feeding a better prompt into the same
small coder model will still produce a better-STRUCTURED but still
somewhat generic conversation. The two problems are independent and both
real — this doc is scoped to the harness/architecture half; the model
swap (chat-hai-design.md's own option (c): "swap the model family entirely
for something chat-tuned... matching this doc's own original vision") is
a separate, also-real piece of the "more meaningful conversation" goal.

---

## 9. Suggested build order (if this moves from theory to implementation)

Given nothing here is built yet, a sensible dependency order for a future
session:

1. **Relationships first** — zero model calls, pure harness logic
   (grep-based co-occurrence scoring), lowest risk, immediately testable
   via frame-history/ledger inspection alone, no LLM latency to debug
   around.
2. **Memory second** — one new DESCRIBE-shaped model ask per gated write,
   real file, straightforward to verify (read the file, it's plain text).
3. **Moderator third** — the scheduling half is trivial
   (`MODERATOR_EVERY>0`); the DESCRIBE-prompt half follows the exact same
   shape memory-writing already established in step 2 — low incremental
   risk once step 2 is proven.
4. **Shared documents last** — depends on the same topic-keyword gate as
   memory (reuse, don't reimplement), and is the most "nice to have, not
   load-bearing" of the four — real payoff, but the others deliver most of
   the "evolving conversation" value first.

---

## Grounding / further reading
- `HARNECIENT-HACK.md` — the pattern this whole doc applies, read first.
- `chat-hai-design.md` §1, §5, and the "why generic/repetitive" section —
  the original vision this design realizes, and the model-choice issue
  this doc explicitly does NOT solve.
- `chat_hai_loop.sh` — the real, current round-robin scheduler this design
  extends (`speak()`, `recent_context()`, the `MODERATOR_EVERY` hook already
  present but inert).
- `HARNESS-DELEGATION-PIPELINE.md` §12 — the independent 2026-08-13
  re-confirmation of DESCRIBE-not-CLASSIFY this doc's §7 leans on.
