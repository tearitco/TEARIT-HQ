# OpenRouter free-model competency: what we actually know, and how to find out more

**Written 2026-09-29** after the real OpenRouter delegation work landed
in open-hai (tool calls, approval gate, the real round trip). Companion
to `13.agent-coms/GROK/2026-09-29/open-hai-api-delegation-guide.md`
(the mechanism) - this doc is about how much to actually TRUST what
comes back, and how to keep finding out as new models/tasks come up.

---

## 1. Can we delegate small tasks to a free OpenRouter model? Yes, mechanically - as of today, for real.

Confirmed real, live, end to end (not assumed from reading code):
- Tool calls (`list_dir`/`read_file`, auto-run; `write_file`/`edit_file`/
  `cmd_exec`, approval-gated) execute correctly and extract arguments
  correctly, via a real, generic JSON parser (not fragile string
  matching - see `12.calendar/2026-09-29/2do.md` for that whole story).
- **The real round trip works**: the model sees its own tool's real
  result and gives a real follow-up answer, not just a raw tool-output
  banner. Verified live: asked a model to read a file containing a
  planted fact it could not have guessed (`"The secret number is
  7429"`), and its follow-up answer correctly stated `7429` - proof
  the follow-up genuinely used the tool's real output, not a guess.
- The approval gate survives the real human-timescale gap correctly -
  approving a `write_file` request an arbitrary amount of time later
  still produces the model's correct follow-up ("Done! I created...").

**One hop only, deliberately** - a follow-up that itself tries to
request a second tool call is not (yet) re-detected; it falls back to
plain content extraction. Multi-step tasks need to be either scoped to
fit in one tool call, or broken into separate `SEND`s by whoever is
delegating (see the delegation guide's own §4 on statelessness).

---

## 2. Confidence level - be honest about sample size

**Mechanical reliability (does the plumbing work): HIGH.** Live-tested
across 3 different models (nemotron, dots-studio, poolside) with
genuinely different JSON serialization styles - the parser and round
trip handled all of them correctly once built right.

**Reasoning/comprehension competence (is the ANSWER any good): tested,
encouraging, but on a small sample - don't over-trust yet.** One real
quiz run so far (§3below), 2/2 correct, including a genuinely tricky
question (an off-by-one-shaped boundary check, `sz > MSG_LEN * 2`)
that a shallow pattern-matcher would likely get wrong. That is a real,
positive signal, not a coin flip - but it's one data point on one
model on one piece of code. Do not generalize it to "the model is
good at code review" yet.

**Recommendation until more data exists**: keep delegated tasks
small, single-file, and — crucially — **verifiable** (the human or
Grok can cheaply check whether the answer/edit was actually right).
Don't delegate anything where a wrong answer would be expensive or
hard to catch. Run the competency quiz below before trusting a NEW
model, or before trusting the model with a NEW kind of task (e.g. it
being good at "what does this function do" doesn't tell you anything
about it being good at "write a correct patch for this bug").

---

## 3. The competency quiz protocol (real, repeatable - run this before trusting a new model or task type)

Direct owner idea: "have it make a set of documents it makes
openrouter read first, and a set of k9-relay-style tests to display
competency, answer quiz questions about the codebase."

**Real constraint that shapes the protocol**: the round trip is one
hop. A tool-based "read this file, then answer" quiz works for
single-file questions (proven, §1). For a quiz spanning MULTIPLE files
or requiring the model to hold several pieces of context at once,
paste the material directly into the prompt instead of relying on
tool calls - simpler, and avoids hitting the one-hop limit.

**Procedure:**
1. Pick a real, non-trivial piece of this codebase - not a toy
   example. A function with a real subtlety (an edge case, a size
   limit, an ordering dependency, a resource-lifetime rule) is a much
   better test than a simple getter/setter.
2. Either (a) put the code directly in the prompt and ask factual
   questions about it (works today, no round-trip needed), or (b)
   point it at a real file via `read_file` and ask a question whose
   answer requires having actually read it (tests the real round trip
   too - see the "secret number" test in §1).
3. Ask 2-4 SPECIFIC questions with KNOWN correct answers you already
   worked out yourself first - never grade against a vibe. Include at
   least one that requires real reasoning (not just "what is this
   variable named") - a boundary condition, a "what happens if X is
   also true" branch, an ordering/lifetime question.
4. Grade strictly: partially-right on a reasoning question still
   counts as a miss for confidence-tracking purposes (see §4).
5. Record the result in §4 below - date, model, task type, pass/fail,
   and the actual question/answer if it's short enough to be useful
   to a future reader.

## 4. Real results log (append here, don't overwrite)

| Date | Model | Task type | Result |
|---|---|---|---|
| 2026-09-29 | `nvidia/nemotron-3-ultra-550b-a55b:free` | Read a real ~35-line C function (`tool_edit_file`) pasted in-prompt; 2 factual/reasoning questions (single-vs-multi-occurrence replace behavior; an exact off-by-one boundary on a size check) | **2/2 correct**, including the boundary case reasoned correctly (`sz > MSG_LEN*2` → max 16384 inclusive) |
| 2026-09-29 | `nvidia/nemotron-3-ultra-550b-a55b:free` | Real `read_file` tool call + round-trip follow-up on a file containing a planted, unguessable fact | **Pass** - follow-up answer correctly used the real tool result |
| 2026-09-29 | `nvidia/nemotron-3-ultra-550b-a55b:free` | Real `write_file` tool call, approval-gated, round-trip follow-up after a delayed APPROVE | **Pass** - correct file content, correct follow-up confirmation |

**Not yet tested, real open items**: multi-file reasoning, a real bug-
finding task (not just "explain this code" but "is there a bug here"),
`dots-studio`/`poolside` on anything beyond the mechanical tool-calling
test already in `12.calendar/2026-09-29/2do.md`, and `edit_file`'s
search/replace path specifically (only `write_file` has been round-
trip tested so far - `edit_file`'s argument extraction is real and
tested, but its own approval->execute->follow-up path hasn't been run
end to end yet).
