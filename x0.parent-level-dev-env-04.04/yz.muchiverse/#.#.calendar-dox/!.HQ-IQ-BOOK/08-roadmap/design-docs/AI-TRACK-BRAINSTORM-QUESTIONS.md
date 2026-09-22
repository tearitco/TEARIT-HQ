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

**Verified answer: no.** `IRL-BOOTSTRAP-RECURSION-SPEC.md` is still
"RESEARCH/DESIGN ONLY, not started" — no code exists. `ollama serve`
is running on the machine (the model server daemon), but nothing calls
it for training; the only real gemma-touching script found in a repo
search is `my-biotech`'s one-off classify-vs-describe demo, unrelated.

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

## Grounding

`HARNECIENT-HACK.md`, `LLMUD-HACK.md`, `DUSTOPIA-HACK.md`,
`LLMUD-INTEGRATION-DESIGN.md`, `IRL-BOOTSTRAP-RECURSION-SPEC.md`,
`13.agent-coms/KILO/claude-2-kilo-9.17.md` §5/§11,
`12.calendar/2026-09-20/2do.md` §8.
