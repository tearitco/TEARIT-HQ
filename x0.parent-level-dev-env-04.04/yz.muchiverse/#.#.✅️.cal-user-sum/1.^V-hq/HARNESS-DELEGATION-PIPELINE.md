# Harness Delegation Pipeline — Strategy & Survey

**Purpose**: (1) survey every real "harness" (deterministic tool-
dispatch / test-scoring / relay-driven verification system) already
built in this house, so nothing gets recoded by hand that already
exists; (2) name the concrete gap between what's proven and a general,
sellable "delegation pipeline" for extended multi-step logic; (3) give
a real design for closing that gap.

**Direct instruction (2026-08-13)**: "look into creating advanced
delegation pipelines and algorithms to hand off extended logic with
deterministic tool calls, because we can use these artifacts to
repackage as products sell to our customers in our 'store'. harnesses
are our business and saving tokens by delegating to local models is
the point of this infrastructure... always favor making a harness to
do work that we can reuse over recoding something by hand. this should
be a real habit and thing to look out for going forward."

See also [[$.claude-hai-budget.md]] — the token-delegation habit this
doc's pipeline is meant to make automatic instead of manual.

---

## 1. What already exists (don't recode this)

| Harness | Path | Mechanism |
|---|---|---|
| **Harnecient Hack** (the core pattern) | `HARNECIENT-HACK.md`, realized in `code-tools-harness/run_native_tools.c`'s `run_harnecient()` (~line 510) | Never sends a `tools` field. Persona forbids structure. App code parses the user's literal text DETERMINISTICALLY and picks the tool — the model's output only ever fills a plain-text slot, never decides control flow. |
| **code-tools-harness** | `&.widgits/ai-cell/code-tools-harness/` | Benchmarks 3 competing dispatch strategies against real Ollama models (native `tool_calls`, text-JSON "post_hack", and Harnecient) — machine-scored (compile/run/grep), never LLM-judged. Proof: `api-test-results.md` (9/9 on 3B/1b/270m). |
| **h-ai `detect_tool()`** | `&.widgits/ai-cell/ops/khtpm_ai_cell_render.c` (~line 536) | Keyword-match on chat text → one of 6 tools. Single-shot. Read-only tools pre-execute; mutating tools (write/edit/cmd) require nav approve/deny. Confirmed backend-mode-agnostic 2026-08-13 (works identically whether the model is Harnecient or native-tools). |
| **Relay harnesses** (7+ instances) | `%.harnesses/*`, `#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh`, `205.ttg-tactics/scripts/harness_0{1,2,3}_*.sh`, various `xyzfs/users/*/harnesses/` | Inject ASCII keycodes into a polled relay file to drive real windows/binaries, assert on resulting state/frame files. Fixed, hand-written sequences — no runtime branching. |
| **Phase 4 relay-harness design** (not yet built) | `13.AUG.13-HAI-2do.txt` Phase 4 | Already-designed (not built) generalization of the relay-harness shape: `run_all.sh` orchestrator, pluggable `scenarios/*.sh`, C assert ops (`assert_file_contains`, `assert_receipt_field`, `assert_no_tools_field`), `proof/SUMMARY.md` N/N scoring. This is the closest existing design to a real reusable pipeline skeleton — read it before building anything below from scratch. |
| **harnecient-fsm** (early, real, momentum-only) | `%.harnesses/harnecient-fsm/` | First real piece of §3's FSM design: `run_plan.sh` (generalized DELEGATE-step driver, read-only tools only, unit-syntax-checked, not yet run against a real plan) + `nav_intent_to_index.sh` (deterministic resolver for delegated navigation, unit-verified against real live model trials — see §6). Built 2026-08-13, explicitly not finished — see its own README.txt for real-vs-pending status. |

### The common reusable shape (repeats everywhere, this IS the reusable core)

1. **Idempotent setup** — restore fixture/app to a known state before each scenario.
2. **Deterministic dispatch** — keyword-match or scripted keycodes; the model never routes.
3. **Machine-verifiable scoring** — compile+run, grep, diff. House law: "the model saying done means nothing" — only a receipt/file/exit-code counts.
4. **Durable artifacts** — every run's transcript/receipt/payload dumped to `results/`/`proof/<timestamp>/`, never deleted, so a customer (or a future agent) can audit exactly what happened.

Where an LLM is used at all, it produces raw text material that's tolerantly parsed and never trusted for pass/fail. This is the sellable part: a customer isn't buying "an AI that does X," they're buying "a machine that PROVES X happened, using an AI as one interchangeable component."

---

## 2. The real gap: no adaptive multi-step chaining exists yet

Searched specifically for: does anything here let step N's *verified*
output determine step N+1's tool AND its parameters at runtime,
without that sequence being hardcoded per-scenario?

**Answer: no, not as a working product mechanism.** The one candidate,
`run_native_loop()` (`code-tools-harness/run_native_tools.c:351-420`,
`MAX_TURNS=6`), is a genuine state machine — tool output feeds back
into history, re-queried — but it exists as a PROBE measuring how
unreliable native models are at self-directed looping, not as the
adopted approach. `HARNECIENT-HACK.md` and the adopted
`run_harnecient()` are explicit that the model's role stays narrow:
set a boolean/fill a text slot, never choose the next step.

**What this means concretely**: every "harness" in this house today
answers a single yes/no or drives a FIXED, hand-authored sequence
(scenario 01, 02, 03...). Nothing here yet answers "given this goal
and this tool registry, figure out the right sequence of 2-5 tool
calls, verify each step before committing to the next, and produce a
scored proof artifact at the end" — without a human writing that exact
sequence into a `.sh` file first.

That's the product gap. Closing it is what makes a harness reusable
ACROSS tasks instead of built fresh per task — which is the actual
lever on "saving tokens by delegating to local models," since a
one-off hardcoded scenario still needed Claude (or a human) to write
the sequence by hand every time.

---

## 3. Proposed design: a general deterministic delegation pipeline

Not built yet. This section is a real design, sized to be buildable in
stages (Haiku/Harnecient-scoped where possible per
[[$.claude-hai-budget.md]]'s own delegation rule), reusing every piece
from §1 rather than replacing any of it.

### 3.1 Core loop (an FSM, not an LLM-driven loop)

```
state = INIT
plan = [] # ordered list of (tool, args) - built once, not re-decided per step
while state != DONE and state != FAILED:
    case state:
        INIT:
            plan = build_plan(goal, tool_registry)   # deterministic, see 3.2
            state = EXECUTE
        EXECUTE:
            step = plan[cursor]
            result = dispatch_tool(step.tool, step.args)   # existing
                                                             # Harnecient/
                                                             # detect_tool()
                                                             # machinery,
                                                             # unchanged
            verified = assert_ops.check(step.assertion, result)  # existing
                                                                   # Phase-4
                                                                   # C assert
                                                                   # ops
            state = VERIFIED if verified else NEEDS_REPLAN
        VERIFIED:
            cursor += 1
            state = DONE if cursor == len(plan) else EXECUTE
        NEEDS_REPLAN:
            # the ONE place an LLM (Harnecient-persona, plain text) is
            # consulted mid-run: "step X failed, here's the verified
            # failure reason, suggest ONE alternative tool+arg" -
            # tolerantly parsed (T2.4-style), app still dispatches
            # deterministically from that suggestion, never trusts
            # free-form control flow
            suggestion = ask_harnecient_for_fix(step, result)
            plan = splice_plan(plan, cursor, suggestion)
            state = EXECUTE if suggestion else FAILED
    checkpoint(state, plan, cursor)   # resumability - see 3.4
DONE -> write proof/SUMMARY.md (existing Phase-4 shape)
```

The important property: **the model never picks the next tool from
open-ended reasoning mid-chain.** `build_plan()` is deterministic
(keyword/goal-template matching, same DESCRIBE-not-CLASSIFY discipline
`HARNECIENT-HACK.md` already established for single-shot dispatch,
just applied to a whole plan instead of one call). The model is only
ever consulted for narrow, verifiable repair suggestions, and even
then the app decides whether to accept it (does the suggested tool
exist in the registry? does the arg look pathish/valid?) before
splicing it in — same guard shape as `detect_tool()`'s existing
pathish-token check.

### 3.2 Pluggable tool registry (generalizes today's hand-written tool sets)

Today, each project (ai-cell, code-tools-harness, my-lawyer) hand-
codes its own small tool set in C. A reusable pipeline needs ONE
registry format multiple harnesses can share:

```
tool_registry/
  read_file.tool      # name, arg-shape, assertion-template, approval-class
  write_file.tool
  edit_file.tool
  cmd_exec.tool
  <project-specific>.tool
```

Each `.tool` file is a plain key=value spec (same PDL-flavored format
already used house-wide for `.pdl` configs — not a new syntax), naming:
- the deterministic keyword/pattern that triggers it (matches
  `detect_tool()`'s existing pathish-token discipline)
- its approval class (pre-execute / needs-approve-deny, matching the
  existing `tool_requires_approval()` split)
- which C assert op (from Phase 4's `ops/+x/`) verifies its result

This is additive, not a rewrite: `detect_tool()`/`start_tool_job()`
stay exactly as they are; the registry just makes their tool set
data-driven instead of hardcoded per project, so a NEW project can
reuse the same dispatcher by dropping in `.tool` files instead of
writing new C.

### 3.3 Declarative scoring (generalizes Phase 4's per-scenario grep code)

Phase 4 already names the right primitives (`assert_file_contains`,
`assert_receipt_field`, `assert_no_tools_field`,
`assert_taskbar_state`). The gap is that today each scenario's PASS/
FAIL logic is hand-written per `.sh` file. A declarative layer:

```
scenario.assert:
  step_1: assert_file_contains <out> <substring>
  step_2: assert_receipt_field <receipt> tool=write_file
  step_3: assert_no_tools_field <payload-dir>
```

...read by the FSM's VERIFIED state, means a new scenario is a data
file, not new bash logic. This is the single highest-leverage piece
for the "sell as a product" framing: a customer's own use case becomes
a `.assert` file + a goal string, not a bespoke engineering project.

### 3.4 Checkpoint/resume (doesn't exist anywhere yet)

Every existing harness restores fixtures only at SCENARIO boundaries —
a mid-chain failure loses all progress. The FSM's `checkpoint()` call
(state + plan + cursor, written to a plain file after every step, same
tmp+rename atomicity as `strip_state.txt`'s own write pattern) means a
5-step chain that fails on step 4 can resume from step 4, not restart
from step 1. This matters specifically because delegation to a small
local model is where step-level flakiness is expected — Harnecient
models are proven 9/9 reliable at SINGLE deterministic tool calls,
individually verified between steps.

---

## 4. Business framing (why this is worth building, not just interesting)

- **Every harness artifact is already sellable as-is.** The Phase 4
  proof shape (`proof/SUMMARY.md`, timestamped run dirs with receipts/
  payloads/transcripts) is a customer-facing deliverable: "here is
  proof our system did X, on real local models, with a fully audited
  trail" — that's the product, not a side effect of testing.
- **The token-saving lever is real but currently manual.** Right now,
  delegating a task to Harnecient still requires Claude (or a human)
  to hand-write the dispatch sequence every time — the FSM above is
  what turns "delegate this one task" into "delegate this CLASS of
  task, forever, via a reusable pipeline." That's the actual
  token-budget win [[$.claude-hai-budget.md]] is tracking.
- **Habit to enforce going forward** (direct instruction): before
  writing one-off code for a new capability, check (a) does an
  existing harness/tool already cover this, (b) if not, is this
  GENERAL enough to build as a reusable harness piece instead of a
  bespoke script. Log the decision in
  [[$.claude-hai-budget.md]]'s delegation log either way, so the
  pattern of "we keep building the same thing by hand" becomes visible
  over time instead of invisible.

---

## 5. Concrete next steps (staged, not all-at-once)

1. **Build Phase 4 as originally designed first** (`13.AUG.13-HAI-2do.txt`
   §Phase 4) — the C assert ops + `run_all.sh` shape are section 3.3's
   prerequisite and already fully speced, just not built. Don't design
   a parallel system; finish this one.
2. **Extract the tool registry format (§3.2)** from `detect_tool()`'s
   existing hardcoded set as a proof of concept on ai-cell alone,
   before generalizing to other projects.
3. **Build the FSM (§3.1) as a thin layer ON TOP of Phase 4's assert
   ops and the registry** — not a rewrite of either.
4. **Checkpoint/resume (§3.4) last** — lowest priority, only matters
   once chains are long enough to make restart-from-scratch expensive.

Each stage is independently useful and independently sellable —
this doesn't need to ship as one big system to start paying for
itself.

---

## 6. Real finding (2026-08-13): delegating NAVIGATION to a Harnecient model works, but only in the proven shape — not as a new pattern

Direct question from the user: "i bet it would be easy to delegate
navigation? did u ever think of that?" Tested live against
`gemma3:1b` on the LAN Ollama host rather than assumed:

**Attempt 1 — ask the model to pick a raw index number from an
enumerated list** (e.g. "1. New Chat / 2. Session.../ 4. Model...
Which number do you press?"): **2 wrong out of 4 trials.** Confirms
PITFALL 69's existing finding (`#.haiku+/!.xyzos-pitfalls+1.txt`) —
small local models cannot reliably self-classify against a raw
enumerated list. Do not build a navigation-delegation feature on this
shape.

**Attempt 2 — ask for plain-text intent, naming the real item by
label, no index involved**: **3 out of 3 correct.** This is not a new
pattern — it's `HARNECIENT-HACK.md`'s existing DESCRIBE-not-CLASSIFY
discipline applied to navigation instead of tool dispatch. The model
never picks a number; it only ever produces a plain sentence naming
what it wants, and the APP resolves that sentence to a real index
deterministically (substring match against the CURRENT real labels,
never a hardcoded list — labels/order both drift per
`_.0.aigent-testing-k9.txt` Rule 4/Rule 7).

**Built (real, not aspirational)**: `%.harnesses/harnecient-fsm/nav_intent_to_index.sh`
— the deterministic resolver half of this pattern. Takes the model's
free-text reply + the real current labels, returns the matching index
or fails closed (exit 1, no guess) on no match. Unit-verified against
the actual live trial transcripts above.

**Gap CLOSED 2026-08-13 (same session, direct instruction: "nav labels
gap must be fixed")**: added `nav_label()` + `dump_nav_labels()` to
`khtpm_ai_cell_render.c` — writes
`ai-cell-frame.png.nav-labels.txt` (one REAL current label per line,
`<1-based-index>|<label>`) alongside the existing PNG receipt on every
`'p'` dump, using the EXACT same text `draw_sidebar()`/
`draw_close_button()`/`draw_composer()` already draw on screen (not a
separate guess). Compiled clean, relay-verified live: dumped a real
running session, got back real labels including the "* " current-
session marker and real session snippets — `nav_intent_to_index.sh`
now has a real live input source.

**Full loop tested end-to-end against real live labels — two honest
findings, not just a clean success**:

1. **Real labels are noisier than the earlier synthetic test.** A
   session snippet label ("08-13 03:00 howdy , how are u ?") confused
   `gemma3:1b` into replying "Howdy" (echoing noise instead of naming
   an item) when asked to switch models. `nav_intent_to_index.sh`
   correctly **failed closed** (exit 1, no match) rather than guessing
   — this is the resolver's fail-safe working exactly as designed, not
   a bug. Real implication: session-snippet labels are too noisy to
   safely offer as delegation targets as-is; a future pass should give
   sessions a CLEAN type-name for this purpose (e.g. "Session N")
   separate from their human-facing snippet label, or exclude
   snippet-bearing nav kinds from delegated-navigation candidate sets
   entirely.
2. **Resolver correctness ≠ semantic correctness.** Asked (with clean,
   non-noisy labels) to "type a new message," the model replied "New
   Chat" — a REAL label, correctly resolved to the real index — but
   arguably the wrong item (a human would mean Composer, not New
   Chat). The resolver did its job; the model's intent-mapping did
   not. **This means resolving to *a* valid nav index is not
   sufficient verification on its own** — any real delegated-
   navigation harness must still check the POST-ACTION receipt state
   (did focus/mode/screen actually change to what the goal implied),
   the same "never trust the model saying done, verify the real
   result" law every other harness in §1 already follows. Don't relax
   that discipline just because navigation felt like a smaller,
   safer surface than tool dispatch — it still needs the same
   verification step.

**Why this matters for the North Star** (h-ai running instances of
itself, `13.AUG.13-HAI-2do.txt` Phase 5): self-navigation is a
prerequisite for a meta-agent (or h-ai itself) to drive its OWN relay
programmatically.

**Both remaining gaps CLOSED, same session (direct instruction: "clean
up and wire it up")**:
(a) session labels now have a clean, content-free delegation-safe
form (`nav_label_delegate_safe()` — "Session N", human-facing label
unchanged), relay-verified live.
(b) `run_plan.sh` gained a real `NAVIGATE` step kind with mandatory
pre/post-action receipt capture — `STEPn_TARGET`/`STEPn_LABEL` for
what was resolved, `STEPn_PRE`/`STEPn_OUT` for the receipt before/
after, so an assertion can verify the ACTUAL landed focus, not just
"a valid label resolved." First real end-to-end run:
`plans/navigate-to-model.plan` — delegated "switch to a different AI
model" to `gemma3:1b`, it correctly named the Model item by label,
resolver correctly picked the real current index, dispatch landed
exactly there, assertion verified `nav=` in the post-receipt equals
the resolved target. **PASS**, real artifacts in
`proof/run_20260813-082848/`. (One real assertion-writing bug hit and
fixed along the way: a naive `grep -oP '(?<=nav=)\S+'` also matched
inside `n_nav=` since "nav=" is a literal substring of "n_nav=" —
fixed with a preceding-space lookbehind. Worth remembering for anyone
writing receipt-field assertions against THIS receipt format.)

---

## 7. Theory: weights, RL, attention — where they'd fit, and why to wait

Direct question (2026-08-13): "what about having weights? rl/
observation learning from how ai uses harnesses and harness chains...
what would help these be more capable instead of wasting tokens?"
Real answer, not hype: **build the data substrate now (cheap, always
useful); defer actual weight/attention machinery until there's either
real trial volume or a richer natural environment to learn from.**

### 7.1 Why not now — the honest reasoning, not just caution

- **Trial volume**: as of this session, the FSM has run TWICE, total.
  Any RL or weighted-scoring approach needs hundreds-to-thousands of
  labeled trials before a learned signal outperforms the current
  deterministic baseline (which is already 3/3 on clean labels, 1/1
  on the full NAVIGATE+verify loop). Training on 2 data points isn't
  learning, it's memorizing noise.
- **House law conflicts with jumping to neural approaches early**:
  `HARNECIENT-HACK.md`'s whole thesis is that deterministic app-side
  logic + a narrow, verifiable model role beats trusting the model
  with more autonomy — the same discipline argues for exhausting
  cheap deterministic/statistical approaches (§7.2) before reaching
  for weights/attention.
- **A richer, more natural environment already exists in the
  roadmap and hasn't been built yet**: `EVENT_AI_VISION.md` already
  names entity AI (movement/interaction/decision-trees via FSM/BT) for
  the demo games (desk-civ/desk-shop) as a target. Many entities,
  many turns, real win/loss signal from gameplay — that is a FAR
  better-fit environment for RL than nav-clicking or PDL-authoring,
  which don't naturally generate high trial volume on their own.
  **Recommendation: if/when RL work starts, start it there, not
  here.**

### 7.2 What to build now — Stage 0, cheap, no ML commitment required

An **observation log**: append one line per STEP/NAVIGATE decision to
a durable, plain-text, pipe-delimited file (same convention as every
other house config) — goal text, what was resolved/dispatched, and
the VERIFIED outcome (pass/fail, from the same real assertion
mechanism already built, not a separate judgment). This is valuable
regardless of which future approach (if any) gets built on top of it:
a human auditing harness reliability, a future heuristic reweighting
pass, or eventual RL all need the SAME underlying dataset. Building it
now costs almost nothing and is never wasted work.

```
TS | KIND | GOAL | RESOLVED_LABEL | RESOLVED_IDX | VERDICT
1786634899 | NAVIGATE | switch to a different AI model | Model: gemma3:1b | 8 | PASS
```

### 7.3 Staged plan (theory, not commitments — revisit once Stage 0 has real data)

- **Stage 0 (build now)**: observation log, §7.2. Zero ML, pure
  bookkeeping.
- **Stage 1 (once the log has real volume, e.g. dozens-to-hundreds of
  rows)**: a simple FREQUENCY-WEIGHTED heuristic, not a neural net —
  e.g. "this goal-phrase pattern has historically resolved correctly
  to label X 9/10 times" biases `nav_intent_to_index.sh`'s matching
  order, but the resolver still fails closed on no match and every
  result is still independently verified post-action. Still fully
  explainable, still deterministic given the same log state, no
  training loop, no gradient anything. This is the "attention-
  mechanism-shaped" idea in its cheapest honest form: a lookup table
  that weights candidates, not a learned embedding space.
- **Stage 2 (defer until entity-AI/gameplay work reaches FSM/BT
  territory per `EVENT_AI_VISION.md`, OR the Stage 1 log shows the
  heuristic approach hitting a real ceiling)**: actual learned
  weights/attention — likely scoped to the entity-AI decision-tree
  work first (richer signal, natural trial volume from gameplay), with
  harness-chain delegation reusing whatever infra that produces,
  rather than the other way around. Not designed further than this
  paragraph today — there isn't enough data yet to design it for real,
  and designing it now would be exactly the "plausible-looking
  plumbing that doesn't survive contact with the real requirement"
  trap this house has hit before (see the khtpm-parser lesson,
  internal Claude memory `feedback-verify-architecture-survives-
  before-building`).

**Bottom line for "what would help these be more capable instead of
wasting tokens" right now**: Stage 0's observation log plus the
existing deterministic verification loop IS the token-saving lever
today — every VERIFIED pass is a task that didn't need Claude to
babysit it token-by-token. Weights/attention are a future multiplier
on top of that, not a prerequisite for it.

### 7.4 "Joints" — built real, same session (direct instruction: "even
if we had to set 'weights' nodes by hand it would be nice to know that
they were being used as tunable 'joints'")

Not just theorized — `%.harnesses/harnecient-fsm/tunables.conf` now
exists: every magic number `run_plan.sh` used (poll timeout/interval,
relay-code delay, post-navigate settle time, post-dump settle time,
which model answers NAVIGATE goals, launch settle time) is a named,
documented, hand-editable joint with defaults in the script and
override capability in the conf file — sourced, not hardcoded.
Re-ran `navigate-to-model.plan` after the refactor: still PASS,
confirms the joint-extraction didn't change behavior, only made it
adjustable.

This is what makes Stage 1 (§7.3) buildable without a rewrite later:
a heuristic reweighting pass would read `observations.log` and could
write directly into `tunables.conf` (or a sibling per-goal-pattern
weights file using the same sourced-config shape) — the "slot" already
exists, only the writer doesn't yet.

`Stage 0` is also real now, not just designed: `run_plan.sh` calls
`log_observation()` after every step (PASS, FAIL-TIMEOUT, FAIL-CLOSED,
or FAIL-ASSERT), appending to `%.harnesses/harnecient-fsm/observations.log`
— real data starts accumulating from the very next run onward.

---

## 8. Write-approval gap CLOSED (2026-08-13, direct instruction: "fix gap if not fixed!")

Real bug found while closing this out, not just an untested path:
`delegate_step()`'s original poll logic watched `n_msgs` alone to
decide a STEP was "done" — but a mutating tool's approval-request
banner ALSO increments `n_msgs` (it's a real assistant message,
`add_and_persist(0, banner)` in `khtpm_ai_cell_render.c`). So the
original driver would have silently captured "Tool request:
write_file... awaiting approve/deny" as if it were the tool's actual
output — a **false PASS**, not a timeout, the worst kind of bug for a
verification layer whose entire point is "never trust a claim, verify
the real result."

**Fixed**: `delegate_step()` now checks the receipt's `tool_pending`
field after any reply lands. If `1` (a mutating tool is genuinely
awaiting approval), the step **fails closed by default** — nothing is
approved or denied, clear stderr reason given. A plan opts IN
explicitly per-step via a 4th pipe-delimited field, `| APPROVE`; only
then does the driver locate the real "Approve: `<tool>`" nav row (via
the live `nav-labels.txt` dump — never a hardcoded index, same
discipline as everything else in this pipeline), dispatch it, and wait
for the REAL post-approval tool result before returning.

**Proven both directions live, not just one**:
- `plans/write-unapproved-should-fail.plan` — same write task, no
  APPROVE field. Correctly FAILED, and the target file was confirmed
  NOT created (`ls` → "No such file or directory").
- `plans/write-approved-should-pass.plan` — same task, `| APPROVE`.
  Correctly PASSED, found the Approve row dynamically (nav index 2 in
  that run), the target file was confirmed created with the exact
  real content (`hello-from-fsm`), not a guess or the banner text.

Both runs logged to `observations.log` alongside the earlier NAVIGATE
trial — three real rows now, one clean PASS, one intentional FAIL, one
approved PASS, giving Stage 1 (§7.3) its first non-trivial dataset
variety instead of a single repeated success.

**Both follow-ups closed same session (direct instruction: "close
those too")**:

- **DENY wired**: `| DENY` is now a valid 4th field alongside
  `| APPROVE` — `find_deny_nav()` locates the real "Deny" row live
  (never hardcoded), dispatches it, waits for the real
  `[tool denied]` note. Proven live: `plans/write-deny-should-pass.plan`
  — dispatched a real write request, denied it, confirmed the target
  file was NOT created AND the transcript really contains the denial
  note (not silently ignored either way). Found the Deny row at a
  DIFFERENT nav index than Approve had been at in an earlier run
  (index 3 vs 2) — confirms this is genuinely reading live state each
  time, not a cached assumption.
- **NEEDS_REPLAN wired**: new `MAX_RETRIES` joint (default 0 = off,
  opt-in). When a STEP dispatches successfully but its OWN assertion
  fails, and `MAX_RETRIES>0`, the driver asks Harnecient for ONE
  alternative plain-text phrasing of the SAME request, then retries
  through the identical `delegate_step()`/assertion path — the model's
  role stays narrow (suggest phrasing only, same discipline as every
  other use of Harnecient here), never picks a tool or shortcuts
  verification. Each retry is its own captured artifact and its own
  `observations.log` row, nothing overwrites the original attempt's
  evidence. Proven live: `plans/retry-demo.plan` with an
  intentionally-unsatisfiable assertion — the driver asked for and
  received two genuinely different real alternative phrasings from
  the model, retried both for real, and correctly FAILED once
  `MAX_RETRIES` was exhausted (not a false pass, not a silent hang).

`observations.log` now has 7 real rows across 4 distinct outcome types
(PASS, FAIL-TIMEOUT-OR-UNAPPROVED, FAIL-ASSERT, plus STEP-RETRY rows)
from this session alone — real variety for whenever Stage 1 (§7.3)
becomes worth building.

**What's still genuinely open** (not hidden): retry suggestions aren't
themselves verified for plausibility before trying (a nonsense
suggestion just burns a retry and fails normally, which is safe but
not smart); NAVIGATE steps have no retry path (only STEP does);
there's no cross-plan memory (each `run_plan.sh` invocation starts
fresh, doesn't consult `observations.log` to bias anything yet — that
IS Stage 1, deliberately not built per §7's reasoning).

---

## 9. Task execution & automation (2026-08-13): the actual token-save mechanism

Direct question: "would u like to move on to task execution and
automation strategies for delegated plans? if u plan surely a harness
can execute and delegate massive token save." Real answer: yes, and
this is the bigger lever right now — every plan built in §6-8 still
required a human (or Claude) to manually invoke `run_plan.sh` once per
task. The actual token-saving multiplier only exists once ROUTINE work
runs without that manual invocation.

**Built**: `%.harnesses/harnecient-fsm/run_queue.sh` — processes every
`*.plan` file sitting in `plans/queue/` through `run_plan.sh`
sequentially (never parallel — only one ai-cell instance can safely
exist, PITFALL 72), files each into `plans/done/` or `plans/failed/`
by its REAL exit code, writes an aggregate summary per pass. This is
what turns "run one plan by hand" into "drop a plan file in a folder,
it runs" — the actual queue/batch mechanism recurring delegation
needs. Proven live: queued one plan designed to pass and one designed
to fail, ran one queue pass, got exactly 1 PASS/1 FAIL, correctly
filed into `done/`/`failed/`, correct aggregate exit code.

**Deliberately NOT done, and why**: `run_queue.sh` is not wired to any
scheduler — no cron entry, no `$.crypts/autostart.pdl` row, no
self-looping/daemon mode. Wiring recurring execution is a genuinely
separate, more sensitive step: it touches boot-time/host-level
automation (`autostart.pdl` launches real windows on every session
start) or the system crontab, both of which warrant their own
explicit go-ahead rather than being silently bundled into a harness
build. `run_queue.sh` is ready to be called BY such a scheduler once
one is deliberately set up — it doesn't set one up itself.

**Real next step, not started**: an actual scheduling decision
(cron entry vs. a periodic call from an already-running process vs.
something else) — ask before building, since it changes what runs
automatically on this machine.

---

## 10. Composition primitives + first real self-coding attempt (2026-08-13)

Direct instruction: "branching, sub-plans and loop/until sound good...
we actually want it for 'self coding' not healing right now." All
three built on ONE mechanism (`run_plan_file()`, called recursively),
not three separate implementations:

- **INCLUDE** (`INCLUDE | <sub-plan path>`) — runs a sub-plan, takes
  its overall PASS/FAIL as this step's verdict. Proven live.
- **BRANCH** (`BRANCH | <condition> | <true-plan> | <false-plan>`) —
  evaluates a real deterministic shell condition (never a model
  choice), runs the matching sub-plan. Real bug found and fixed while
  proving it: the field-splitting logic double-counted the condition
  (tried to pull 3 fields out of what was already 2), always resolving
  the false branch regardless of the real condition. Fixed, re-proven
  correct both directions.
- **LOOP_UNTIL** (`LOOP_UNTIL | <max iters> | <sub-plan>`) — re-runs a
  sub-plan fresh each iteration (own artifacts, own observation-log
  rows) until it PASSes or the cap is hit. Proven with a real counter
  file that only passes on its 3rd genuine execution — stopped at
  exactly iteration 3, not earlier or later.

### Self-coding: 4 real attempts, 2 real findings, both resolved practically not theoretically

Built `plans/self-code-demo.plan` (wraps `subplans/self-code-attempt.plan`
in `LOOP_UNTIL`): ask a model for a one-line shell snippet, write it to
a file via the tool-approval path, verify by actually RUNNING the
script (stdout content AND exit code, not just grepping for expected
text — a script can print the right thing and still error out after).

1. **`stable-code:latest`, casual phrasing**: task text containing
   "write" near a sentence-ending period, and separately "command"
   (itself a `cmd_exec` trigger keyword), both accidentally fired
   `detect_tool()` on what were meant as plain chat steps — the
   harness correctly failed closed both times (no APPROVE granted,
   nothing ran), but it took two rounds of task-rephrasing to avoid
   detect_tool()'s real keyword set entirely. Real lesson for anyone
   writing STEP tasks: avoid write/create/save/edit/modify/append/
   search/grep/list/show/dir/run/execute/command/cmd/exec/read/open/
   cat/view + "file" in a task meant to stay plain chat.
2. **`stable-code:latest`, explicit no-explanation instructions,
   strict verification**: FAILED 3/3 real independent attempts. The
   model reliably appended trailing content into the SAME response
   used as file content — first plain prose, then (after strengthening
   the instruction) a markdown code fence — both broke the script when
   actually run. A looser assertion (grep stdout only, `2>/dev/null`
   discarding errors) had let attempt #1 silently PASS despite the
   file being broken - fixed by checking the exit code too, which is
   the real lesson: **verify execution, not just output content.**
3. **`gemma3:1b`, same exact task/prompt, same strict assertion**:
   PASSED on the FIRST attempt, output was EXACTLY
   `echo "OK-CODE-TEST"` with zero trailing pollution, confirmed
   clean by running the script standalone (exit 0, correct stdout).
   Direct user suggestion, tested rather than assumed: "u can try 2
   use a lower power model like gemma270/1b for less verbose answer?
   just do what works, practice beats theory." Confirmed true for THIS
   task — the smaller, terser model outperformed the larger "code"
   model specifically because it doesn't over-elaborate.

**Real, generalizable finding**: for narrow, single-line/short code-
generation tasks where verbosity is the failure mode (not
capability), a smaller Harnecient model can be MORE reliable than a
larger one, not less. Don't default to the biggest available model for
self-coding tasks — test the smallest one first when the target output
is short and strictly formatted.

**Not yet done**: this was one task, one working example. No
systematic model comparison across task types/complexity levels exists
yet - that's real future work, not a conclusion to generalize from a
single data point beyond what's stated above.

---

## 11. Composer wrap/scroll fix (2026-08-13, unrelated to the pipeline itself, same session)

Direct report: "for the gui text input i want it to wrap new line and
user input can scroll up instead of dissapearing off the side of the
screen." Fixed in `khtpm_ai_cell_render.c`: the composer box now grows
(up to `COMPOSER_MAX_LINES`=6 visible lines) as typed input wraps to
more lines, using the same `wrap_text()` helper already proven for
transcript messages — not a new wrapping implementation. Beyond the
cap, it auto-follows the bottom (cursor's line always stays visible)
instead of running text off the edge. Relay-verified live with a real
230-character message: rendered as 6 correctly wrapped lines, composer
box grew, transcript area correctly shrank to make room, nothing lost
off-screen. (Testing note: `'p'` only triggers the frame dump when
NOT armed — while armed it types a literal 'p' character instead, by
design, documented in the code itself — cost some real debugging time
mid-session before being remembered; verify via Escape, which disarms
without clearing the buffer, when you need to inspect composer state
mid-typing.)

---

## 12. Automatic model choice: SET_MODEL + CHOOSE_MODEL (2026-08-13)

Direct question after §10's finding (gemma3:1b beat stable-code:latest
on a real self-coding task): "have u somehow implimented a bt/fsm or
weights/joints for gemma3 vs sc choice? or how will that be
automatic?" Honest answer at the time: no — the model switch for that
test was done by hand (editing `sessions/model.txt` before launch,
which only takes effect at process STARTUP, not live). Closed the same
session, two real pieces:

### SET_MODEL — deterministic, explicit switch

`SET_MODEL | <model-name>` cycles ai-cell's REAL, LIVE model selector
via the same nav+Enter action a human clicking "Model" would use
(`cycle_model()` in `khtpm_ai_cell_render.c`) — never edits
`sessions/model.txt` directly, since an external edit to that file has
no effect on an already-running process. Cycle count is computed from
a hardcoded `G_MODELS_ORDER` array that MUST mirror the app's own
`g_models[]` order — **a real, named, unclosed gap**: if they drift
apart, `SET_MODEL` silently cycles to the WRONG model. Not fixed this
pass (would need ai-cell to export its own model list live, mirroring
how `dump_nav_labels()` solved the same class of problem for
navigation) — flagged, not hidden. Proven live: 3 real switches
(gemma3:1b, llama2:latest, stable-code:latest), each with a correctly
computed cycle count (1, 3, 1) and each verified against the real
post-switch live state, not assumed.

### CHOOSE_MODEL — meta-delegation: a model chooses the joint

Direct instruction: "maybe can choose based on complexity (can even
call 2 gemma 2 choose joint? get it?)... this is a good idea imo and
how it should be used." The idea: a cheap classifier model
(`MODEL_CHOOSER_MODEL`) looks at the upcoming task and picks which
worker model (`SIMPLE_TASK_MODEL` / `COMPLEX_TASK_MODEL`, both real
tunable joints, informed by §10's comparison) should handle it —
`CHOOSE_MODEL | <task description>` in a plan.

### First attempt: a naked test, not actually Harnecient (caught live, worth recording exactly why)

The first implementation asked the classifier to directly output one
of two enum tokens — `"Reply with exactly one word: SIMPLE or
COMPLEX."` **This is precisely the CLASSIFY anti-pattern
`HARNECIENT-HACK.md` exists to warn against**, and it showed:
unreliable across BOTH `gemma3:270m` and `gemma3:1b`, on the same two
test tasks, through three separate prompt-engineering attempts:

1. Plain "SIMPLE or COMPLEX" instruction — both models called a
   genuinely complex task ("design and implement a multi-file build
   system with dependency tracking") SIMPLE.
2. Few-shot examples (raw text completion, no proper prompt/system
   split) — both models went completely off-script: one produced
   unrelated Python code, one echoed the instructions back, one
   attempt's response wasn't even valid JSON.
3. An explicit rubric + a real Ollama `system` field pre-prompt (a
   genuine improvement in structure, at the user's direct suggestion
   — "u should have more specific criteria... or give it a pre
   prompt") — better, but `gemma3:1b` still answered "MULT ISTIME," a
   non-word, on the complex task.

Direct question that caught the actual root cause: **"is this using
the harnecient hack or a naked test? any way to make it more
harnecient?"** It wasn't. `HARNECIENT-HACK.md`'s DESCRIBE-not-CLASSIFY
discipline had already been correctly applied to navigation (§6, ask
for a plain-text description, resolve deterministically) but was
never applied here — the classifier was being asked to self-report a
category, exactly the shape every other successful use of a
Harnecient model in this house deliberately avoids.

### The fix: DESCRIBE the task, count real structural signals, never ask for a category

Reworked to match the proven pattern exactly: ask the model to
describe the task's shape in one plain sentence (no classification
words offered or allowed), then `complexity_signal_count()` counts
real structural signals in that description — commas and `" and "`
occurrences, a genuine proxy for "how many parts did the model itself,
unprompted, enumerate" — and compares against `COMPLEXITY_THRESHOLD`
(a tunable joint, default 2) to pick the worker model. The classifier
model never sees or outputs SIMPLE/COMPLEX/MULTISTEP/anything - it
only ever describes, same as `nav_intent_to_index.sh`'s model input.

**Verified 4/4 before trusting it**: both classifier models
(`gemma3:270m`, `gemma3:1b`), both test tasks — the simple task's
description ("Print OK-TEST" / "...involves inputting and outputting
a single string") scored 0 signals every time; the complex task's
description ("...file structure organization, configuration files,
compilation commands, and integration of dependency analysis tools")
scored 3-4 signals every time. Then proven end-to-end through the real
plan (`plans/choose-model-demo.plan`): simple task → 0 signals →
switched to `gemma3:1b`; complex task → 4 signals → switched to
`stable-code:latest`; both switches independently verified against
real live state, both correct.

### Lesson for future agents, stated plainly

**Before trusting ANY new use of a Harnecient model in this pipeline,
ask the question the user asked here: "is this the Harnecient hack or
a naked test?"** The tell is simple — if the model is ever asked to
output a category/label/enum/index directly, it's a naked test,
regardless of how good the prompt wording sounds. The fix is always
the same shape: ask for a plain description of the thing, then resolve
that description deterministically in the app. This isn't a
theoretical rule kept for its own sake — it produced a measurable
difference on the exact same two test tasks, with the exact same two
models, going from unreliable/broken to 4/4 correct.

**Real next steps, not started**: (a) `COMPLEXITY_THRESHOLD=2` and the
comma/"and"-counting signal set are a first-pass heuristic validated
on exactly 2 tasks — a real Stage 1 candidate (§7.3) once
`observations.log` (which `CHOOSE_MODEL` already logs to) has more
real rows to tune against; (b) if `SET_MODEL`'s `G_MODELS_ORDER` gap
(above) is ever hit in practice, fix that before trusting
`CHOOSE_MODEL` in anything unattended.
