# Events/Pal Command Buildout — Delegation Plan

**Purpose**: plan building out the RPG-Maker-style event command
vocabulary (`.pal` scripting + the "pal" entity/event backend) as
harnecient-delegated work through `%.harnesses/harnecient-fsm/`, not
hand-coded by Claude one command at a time.

**Direct instruction (2026-08-13)**: "next steps will be building the
rpg maker events backend 'pal' logic, before then building out our
games in 'events' thru injection (hopefully thru harnecient, esp
learning with weights/joints and supervised learning)... wanna start
strategizing how to plan this task to be as harnecient as possible?"

See also [[HARNESS-DELEGATION-PIPELINE.md]] (the pipeline this plan
runs on) and [[EVENT_AI_VISION.md]] (the longer-range design intent
this plan implements a slice of).

---

## 1. Real current state (verified, not assumed)

**Architecture, confirmed correct** (direct question answered: "is
change gold in .pal or erroneously hardcoded in C?"): NOT hardcoded.
The pattern already in production, per-command:

```
event-ez (visual editor)
  -> event.ir.pdl        (SECTION|KEY|VALUE; NODE lines: id=N type=change_gold|amount=10)
  -> event.pal            (compiled opcodes: exec cmd_1.sh / halt)
  -> cmd_N.sh              (one wrapper per NODE, calls the generic op binary)
  -> mr_<command>.+x       (generic C binary: state-change logic ONLY, args from cmd_N.sh)
```

`mr_change_gold.c` is genuinely generic (`<package_dir> <delta>`, zero
entity-specific logic) — a correctly-scoped VM-opcode-style op, not a
hardcoded special case. `play_event.sh` (the page-dispatcher/loader
that picks which page's `.pal` to run) is a different architectural
layer, closer to a process loader than to event logic — reasonable to
leave as engine code (user's own framing: "tho not as important").

**What's real and working today**: Change Gold (party stat change),
Show Text, Show Choices (message commands). Multi-page/multi-trigger
dispatch works (`play_event.sh`, highest-numbered matching page wins,
RPG Maker MV semantics). Common events (session-level, not just
per-entity) work with ZERO new runtime code — same package shape.
event-ez doubles as the common-events authoring UI already.

**What's vision, not code** (`EVENT_AI_VISION.md`): Player
Touch/Event Touch/Autorun/Parallel triggers (need real tile/collision
movement — none exists), entity AI/FSM, movement/collision, network
events. Out of scope for this plan's first phase — this plan is about
the COMMAND VOCABULARY (the ~90-command menu below), not movement/AI.

## 1b. MAJOR CORRECTION (2026-08-13, same session): most commands need NO new C code at all

Direct questions that caught this: "why c why not .pal? its just riscv
assembly?" — checked `prisc+x.c` directly rather than assume C was
required, and it wasn't. **`prisc+x` is a real RISC-V-shaped VM**, not
just an "exec a script" sequencer: real opcodes (`ADDI`, `BEQ`, `LW`/
`SW`, `JALR`, `J`, real registers), and — the load-bearing part —
**`ecall` syscalls `SYS_GET_KV_INT` (6) and `SYS_SET_KV_INT` (7)** that
already implement the EXACT read-modify-write-preserving-other-lines
pattern `mr_change_gold.c` reimplemented from scratch in C
(`prisc+x.c` lines ~718-816). Change Gold's compiled `.pal` just
happens to use the simplest available pattern (`OP_EXEC`, spawn a
script) — that's what a first proof-of-concept event reached for, not
a demonstration that C is the required shape for this class of work.

**Verified live, not just read**: wrote a real `.pal` snippet by hand
and ran it against a real `items.txt` fixture via the real `prisc+x`
binary:

```
li x15, 6
li x13, 0
ecall "items.txt" "item_3"
addi x12, x12, 5
li x15, 7
ecall "items.txt" "item_3"
halt
```

`item_3=10` → `item_3=15` after running it. **Zero new C code, zero
compilation, zero new binary artifact.** This is a ~7-line text file,
not an 80-line C program.

**What this means for the whole plan**: every Party/Actor command that
is "read an integer stat, apply a delta, write it back" (Change Gold,
Change Items✅-provable-now, Change HP/MP/TP/EXP/Level — most of the
22-command cluster from §2 below) needs ONLY a parameterized `.pal`
TEMPLATE (3 blanks: state file path, key name, delta-or-source-of-
delta), not a new compiled op per command. This is the "fill in the
blanks... bank of mini prompts" shape the user asked for directly —
see §5c.

**Where C op binaries (the OLD plan's default) are still genuinely
needed**: anything `ecall`'s current syscall set can't express —
non-integer state (Change Name/Nickname/Profile are strings, not
ints — `SYS_GET_KV_INT`/`SET_KV_INT` are integer-only per their own
names), multi-field structured changes (Change Equipment touches
several slots at once), or logic needing loops/branches beyond simple
arithmetic (Change Party Member — add/remove from a list). Check each
command against `ecall`'s real syscall set (`SYS_OPEN`/`CLOSE`/
`WRITE_LINE`/`WRITE_INT`/`READ_INT`/`GET_KV_INT`/`SET_KV_INT`,
`prisc+x.c` lines ~695-719) BEFORE defaulting to "write a new C op" —
that default was the actual mistake this correction fixes.

## 2. The real target: the full RPG Maker MV command vocabulary

From `#.ref/menu/event.commands.{1,2,3}.txt` (the actual reference the
`event-ez` UI is modeled on). ~90 commands total, grouped by category:

| Category | Commands | Count | State-change shape |
|---|---|---|---|
| Message | Show Text, Show Choices, Input Number, Select Item, Show Scrolling Text | 5 | UI-driven, no persistent state file |
| Party | Change Gold✅, Change Items, Change Weapons, Change Armors, Change Party Member | 5 | Single state-file arithmetic (Change Gold's exact shape) |
| Game Progression | Control Switches, Control Variables, Control Self Switch, Control Timer | 4 | Key=value state file writes |
| Actor | Change HP/MP/TP/State/EXP/Level/Parameter/Skill/Equipment/Name/Class/Nickname/Profile, Recover All | 13 | Per-actor state file, same shape as Change Gold |
| Flow Control | Conditional Branch, Loop, Break Loop, Exit Event, Common Event, Label, Jump to Label, Comment | 8 | **Control-flow, not state-change** — needs real `.pal`/`prisc+x` VM support, not just a new op binary |
| Movement | Transfer Player, Set Vehicle/Event Location, Scroll Map, Set Movement Route, vehicle | 6 | Needs real tile/position system (unbuilt, `EVENT_AI_VISION.md` gap) |
| Character | Transparency, Followers, Show Animation/Balloon, Erase Event | 5 | Needs a real sprite/rendering layer per entity |
| Timing | Wait | 1 | Needs `.pal`/`prisc+x` timing support |
| Screen | Fade, Tint, Flash, Shake, Weather | 6 | Needs a real screen-effects layer |
| Audio & Video | BGM/BGS/ME/SE/Movie/Picture ops | ~15 | Needs real audio/picture-layer infra |
| Scene Control | Battle, Shop, Name Input, Menu/Save screens, Game Over, Title | 8 | Needs those subsystems to exist first |
| Map | Map name, Tileset, Battle BG, Parallax, Get Location | 5 | Needs map-metadata infra |
| Battle | Enemy HP/MP/TP/State, Recover, Appear/Transform, Animation, Force Action, Abort | 8 | Needs a battle system (doesn't exist) |
| System Settings | Battle BGM, Victory/Defeat ME, Vehicle BGM, Save/Menu Access, Encounter, Formation, Window Color, Actor/Vehicle Images | ~10 | Needs those subsystems |
| Advanced | Script, Plugin Command | 2 | Escape hatches, lowest priority |

**Real, load-bearing observation**: roughly 22 commands (Party + Actor,
minus Recover All's minor extra logic) are STRUCTURALLY IDENTICAL to
Change Gold — single state-file, single signed-integer (or small
struct) arithmetic operation, differing only in which file/field they
touch. This is the highest-leverage, lowest-risk delegation target:
the exact same 5-step template repeated ~22 times with different
parameters, zero new architecture needed.

## 3. Why this is the right task to make maximally harnecient

Every criterion from `HARNESS-DELEGATION-PIPELINE.md` §1/§9's "what's
worth delegating" lines up:

- **Repetitive, well-specified, low architectural risk** — 22 near-
  identical Party/Actor commands, same template, different fields.
- **Machine-verifiable by construction** — every op is "does the
  right state file end up with the right value," exactly the kind of
  assertion `run_plan.sh`'s STEP+APPROVE+strict-exit-code discipline
  (§8/§10) already proves reliable.
- **Genuinely high trial volume** — ~22-90 real, independent
  implementation attempts is EXACTLY the data volume
  `HARNESS-DELEGATION-PIPELINE.md` §7.1 said was missing before Stage
  1 (heuristic reweighting) becomes worth building. This task, not a
  contrived test, may be where `observations.log` first earns its
  keep for real.
- **`CHOOSE_MODEL` has a genuine job here**: Party/Actor commands
  (SIMPLE, single-state-file arithmetic, §10's `gemma3:1b` win) vs.
  Flow Control commands (COMPLEX, real control-flow semantics —
  `stable-code:latest` or better) is a REAL complexity split, not a
  contrived test case, and `complexity_signal_count()` (§12's fixed,
  DESCRIBE-based resolver) can be validated against dozens of real
  commands instead of 2 synthetic ones.
- **"Supervised learning"**, concretely, not aspirationally: every
  delegated command-implementation attempt logs
  `(task, resolved_label, resolved_idx, verdict)` to
  `observations.log` already. Once ~20-30 real commands have been
  attempted, that log IS a labeled dataset (task description → which
  model handled it → did it pass) — the actual substrate Stage 1
  needs, generated as a byproduct of doing the real work, not built
  as a separate exercise.

## 4. The proven per-command template — TWO shapes now, not one

**Shape 1 — native `.pal` (default, prefer this, §1b)**: for any
"read an integer, apply a delta, write it back" command:

1. **event-ez UI**: add the command to the visual editor's command
   list, with whatever fields it needs — additive, `event-ez` is
   already generic.
2. **IR node type**: `event.ir.pdl` gains a new `type=<command>` NODE.
3. **`.pal` compiler**: emits the 6-7 line GET_KV_INT/ADDI/SET_KV_INT
   snippet (§1b) directly, filling in the state-file path and key name
   from the node's fields — NOT `exec cmd_N.sh`, no wrapper script, no
   C binary. Verified working live (§1b).
4. Nothing else. No new compiled artifact per command.

**Shape 2 — C op binary (fallback only, when `ecall`'s real syscall
set genuinely can't express the logic — §1b's list)**: the ORIGINAL
5-step template (event-ez UI → IR node → `.pal` emits `exec cmd_N.sh`
→ generated `cmd_N.sh` wrapper → new `mr_<command>.c`) still applies,
but check against `ecall`'s syscalls FIRST — Shape 2 should be the
exception, not the default.

## 5. Delegation plan, staged

### Stage A — pilot Shape 1 on 2-3 real commands (do this first, small)

Before templating all ~20 GET/SET_KV_INT-shaped commands, prove the
delegated version of what was already proven by hand in §1b:

1. Write ONE real plan (`plans/subplans/build-op-<command>.plan`)
   that: `CHOOSE_MODEL`s based on the command's description,
   `NAVIGATE`/`STEP`s the model into producing the 3 real blanks
   (state-file path, key name, delta source) for a KNOWN, FIXED
   `.pal` template (not asking it to write the whole snippet from
   scratch — see §5c, this is the actual fill-in-the-blank shape),
   then a real assertion: run the filled-in `.pal` through `prisc+x`
   against a fixture state file, check the real resulting value.
2. Run it via `run_plan.sh` directly (not queued) for the first 2-3
   commands, inspect the actual filled-in `.pal` text by hand each
   time — same "watch it work before trusting the queue" discipline
   as everywhere else in this pipeline.
3. Only once 2-3 real commands pass this way, promote into a reusable
   generator (parameterized by command name / field list) and queue
   the rest through `run_queue.sh` (§9). Because the fill-in-the-blank
   task is now ~3 short values instead of a full file, this should be
   BOTH more reliable (smaller generation surface, per §10/§12's
   "shorter output = more reliable" finding) and cheaper than the
   original C-generation plan.

### Stage B — the remaining ~19 GET/SET_KV_INT-shaped commands, via queue

Once the Shape 1 template/generator is proven, queue the rest through
`run_queue.sh`. Expect some commands to need Shape 2 instead (string
fields, multi-field changes — check each against §1b's list before
assuming Shape 1 fits) — that's real, expected variance, not a
plan failure.

### Stage C — Flow Control (harder, different shape, do NOT template from Stage A/B)

Conditional Branch/Loop/Common Event/Label are control-flow, not
state-change — they need real `.pal`/`prisc+x` VM support (the
compiler and the tiny VM itself need new opcodes, not just a new
generic C binary). This is architecturally different work, likely
needs Sonnet-level design before ANY delegation, matching
`$.claude-hai-budget.md`'s own line: "keep architecturally-risky work
in Claude; push mechanical/repetitive work to Harnecient." Don't
template Stage C from Stage A/B's shape.

### Stage D+ — Movement/Character/Screen/Audio/Battle/Scene/System

All genuinely blocked on infra that doesn't exist yet (tile/collision
movement, a rendering/audio layer, a battle system) — `EVENT_AI_VISION.md`
territory, not this plan's near-term scope. Revisit once Stage A-C are
real and Stage D's actual prerequisites are separately built.

## 5b. Stage A+: does a Harnecient model write AND test its own harness? (2026-08-13, direct instruction — not previously explicit as a near-term priority)

Direct question: "let's see if it can even write and test its own
harnesses, let's be building/training harnecient models for that as
well. does dox explain this is also desired priority?" Honest answer
when asked: NOT explicitly, before this — the vision existed
(`13.AUG.13-HAI-2do.txt` Phase 5: "h-ai runs instances of itself to
write and verify new harnesses") but was framed as a LATER stage, not
something to try alongside Stage A. Corrected here: **for each Stage A
pilot command, after the op binary itself is delegated and verified,
delegate a SECOND, separate request asking the model to write a small
verification script (its own mini test/harness) for that exact op** —
a fixture-setup + run + check-the-result script, same shape as this
project's own `run_plan.sh` assertions, just written by the model
instead of by Claude. Claude still reviews and actually RUNS the
generated harness for real before trusting its verdict (same
"never trust the model saying done" law as everywhere else) — this
tests whether the model can produce something Claude would otherwise
have hand-written, not whether to trust its self-reported result.

## 5c. The "ops bank" / fill-in-the-blank prompt bank (mid-term, documented per direct request)

Direct instructions: "u can even give it the ops names or a
library/bank, do so harniciently?" / "then it could be smaller
chunks" / "we could eventually have prompts that are more like fill
in the blanks (exactly like how events work) and we could have our
own bank of mini prompts/harnesses or w/e to send for specific chains
of tasks (doc this even if for use more midterm)."

**The idea**: instead of pasting a full reference file (§1b's original
approach — 1800+ chars of stripped C source just for context) or
asking a model to generate a whole program from scratch, give it a
compact BANK of known-working primitives — op/ecall names, their
argument shapes, and 1-line descriptions — and ask it to compose from
that bank, or (even better, per §1b's fill-in-the-blank realization)
just fill in the 2-4 real blanks in an ALREADY-COMPLETE template. This
is structurally identical to how events themselves work (a fixed
command shape with a few real fields — Change Gold's "amount", Change
Items' "item_id + delta") — so the delegation prompts for BUILDING
event commands should look like the event commands they're building.

**Concrete mid-term shape** (not built yet, real design for later):

```
%.harnesses/harnecient-fsm/ops_bank/
  prisc_ecalls.txt        # one line per ecall: name, args, one-line description
    GET_KV_INT | path key default | reads an int, returns default if missing
    SET_KV_INT | path key value | writes an int, preserves other lines
    WRITE_LINE | fd text | appends a line to an open file
    ...
  pal_templates/
    get-add-set.pal.tmpl   # the exact §1b snippet, with {{PATH}} {{KEY}} {{DELTA}} blanks
  house_ops.txt            # existing house-wide op binaries (mr_change_gold.+x etc.) - name, args, one-line description, for the Shape 2 fallback cases
```

A `FILL_TEMPLATE` plan-step kind (not built) would read a `.tmpl` file,
ask a Harnecient model ONLY for the blank values (not the surrounding
structure — the structure is already correct and fixed), substitute
them in deterministically (never trust the model to reproduce the
boilerplate correctly, only the 2-4 real decisions), and hand the
result to the existing STEP/APPROVE + real-verification machinery.
This is the natural evolution of `CHOOSE_MODEL`'s DESCRIBE-then-
resolve discipline (§12) applied to CODE GENERATION instead of
model-selection — smaller model output surface, same "never trust a
raw structured/classification-shaped answer" law.

**Why midterm, not now**: Stage A (§5) only needs 2-3 real pilot
commands to prove the fill-in-the-blank shape works at all — building
a whole bank/template infrastructure before that would be exactly the
"plausible-looking plumbing that doesn't survive contact with the real
requirement" trap (`feedback-verify-architecture-survives-before-
building`). Do Stage A by hand-writing the template inline in each
pilot plan first; extract the reusable bank/`FILL_TEMPLATE` mechanism
once 2-3 real pilots confirm what the bank actually needs to contain.

## 6. Concrete first 3 commands to pilot (Stage A)

Chosen for maximum similarity to the §1b-proven Change Items shape
(all Shape 1, native `.pal`, no C — lowest risk for the FIRST real
delegated implementation attempts):

1. **Change Items** (Party) — literally the exact snippet already
   hand-proven live in §1b (`item_3=10` → `item_3=15`). First
   delegation target: have the model fill in the 3 blanks (path, key,
   delta) instead of Claude hand-writing them, verify identically.
2. **Change HP** (Actor) — same GET_KV_INT/ADDI/SET_KV_INT shape,
   different state file (`actor_<id>/stats.txt` instead of
   `items.txt`), tests a different file-naming convention with the
   SAME template.
3. **Control Switches** (Game Progression) — boolean ON/OFF instead of
   signed-delta arithmetic (`li x12, 1`/`li x12, 0` then straight to
   `SET_KV_INT`, no `GET`/`ADDI` needed) — first real test of whether
   the template family generalizes beyond pure add-a-delta.

## 7. Open questions before starting Stage A (real, not rhetorical)

- Where should new/generated `.pal` templates and any Shape-2 fallback
  `mr_*.c` binaries live — same `xyzfs/bin/muchi-pet/ops/` as Change
  Gold, or does the house-wide op-sharing convention need a new
  namespace per game/command family? (Check `EVENTS_RUNTIME.md`'s
  ops-vs-events sharing model before Stage A, don't assume.)
- Does `event-ez`'s command list need a UI change per new command, or
  can Stage A skip the UI layer entirely and hand-author
  `event.ir.pdl`/compile via CLI for the first few commands (faster
  pilot, defer the UI work to a later, separate pass)? Recommend the
  latter for Stage A specifically — proves the Shape 1 pattern works
  before spending delegation effort on UI wiring.
- Does the `.pal` COMPILER (event-ez's IR→pal step) need to learn the
  GET_KV_INT/ADDI/SET_KV_INT emission pattern, or can Stage A's first
  pilots write/test the `.pal` snippet directly (bypassing the
  compiler, same as §1b's hand test) and defer compiler integration to
  Stage B once the pattern is proven? Recommend the latter — same
  "prove the mechanism before wiring the UI" ordering as the point
  above.
- Confirm `run_plan.sh`'s STEP+APPROVE path can handle writing a real
  multi-line `.pal` file (today's proven write_file tests were
  one-liners; a 7-line snippet is much smaller than the ORIGINAL
  plan's 80-line C file concern, but still untested through the actual
  delegation pipeline, only proven by Claude hand-writing it in §1b)
  — check before trusting Stage A #1's delegated write.

---

## 8. Stage A pilot #1, run for real (2026-08-13) — three real findings, one real fix, one real course-correction

Direct instruction: "try it. maybe the pipeline can create/run the
test harness as well." Ran the full pipeline live, not just planned.

### 8.1 Real bug found and fixed in `run_plan.sh` itself

`delegate_step()`'s poll loop stopped on ANY `n_msgs` change from
baseline — correct by accident for fast paths (`submit_composer()`
persists the user's own message SYNCHRONOUSLY, so a fast/synchronous
reply landed before the difference mattered) but WRONG for a slower
generation: the user-message increment landed first, the poll declared
victory, and `capture_last_output()` grabbed the STALE greeting
instead of waiting for the real (async) reply. Confirmed live with
`stable-code:latest`'s slower generation. **Fixed**: wait for
`n_msgs >= baseline+2` (both real messages), not just "changed."
`POLL_TRIES`/`POLL_INTERVAL_S` joints also bumped (7.5s window was
sized for instant reads, not real generation time).

### 8.2 First full-harness-generation attempt: broken (stable-code:latest)

`CHOOSE_MODEL` correctly classified the task and routed to
`stable-code:latest`. The generated bash test harness, once actually
run (not just trusted): wrapped in markdown fences, wrong arithmetic
(checked for 14 instead of 13), unfilled placeholder text instead of
real arguments, and a mysterious `u003e` where `>` should be.
`bash test_harness.sh` → real error, `exit 127`.

### 8.3 Second attempt (gemma3:1b) surfaced a REAL, separate, cross-cutting ai-cell bug

Retried the same task on `gemma3:1b` (§10's earlier precedent). Still
broken (unmatched brace, malformed redirect) — but it also produced
`u0026` where `&` should be. **Two independent models producing the
exact same corruption pattern is a strong signal the bug is in OUR
pipeline, not the models** — checked `khtpm_ai_cell_render.c`'s
`extract_response_field()` and found it: the hand-rolled JSON
unescaper handled `\n`/`\t` but had no `\uXXXX` case, so Ollama's own
standard JSON-escaping of `>`/`&` (as `>`/`&`) silently
corrupted into literal `u003e`/`u0026` text — in EVERY model response
containing those characters, not just this test. **Fixed**: added
proper `\uXXXX` → UTF-8 decoding (BMP range). Rebuilt, re-ran the same
gemma3:1b task — confirmed `>` now renders correctly. The harness
itself was STILL logically broken (separate finding from §8.2/8.3 —
generation reliability, not encoding), consistent with every prior
finding that full multi-line generation is unreliable on both models
tested so far.

### 8.4 Course-correction (direct instruction): "we maybe asking the harnecients for too much? what about just giving it a template and asking it to fill in the blanks? we'd prefer a realistic starting point to something that doesn't work at all"

Exactly right, and §8.2/8.3 just proved it empirically. Rebuilt the
approach around a REAL, Claude-hand-verified template
(`%.harnesses/harnecient-fsm/templates/get_add_set_test.sh.tmpl`,
sanity-checked once with known values before trusting it with
anything) plus a bounded EXTRACTION task instead of free generation:

1. Hand-verify the template works with known values (state file, key,
   start, expected) — confirmed: `item_7 = 6` (2 + 4), real `prisc+x`
   run, exit 0.
2. Delegate ONLY extracting 3 short facts from a natural-language
   scenario ("increase item_9 by 5, starting from 1") — a bounded
   comprehension task, not generation. `gemma3:1b`'s reply didn't
   follow the requested strict 3-line format, but DID contain the
   correct facts (`item_9`, start=1, delta=5) — a far more forgiving
   failure mode than §8.2/8.3's structurally broken scripts.
3. Extract those facts tolerantly (regex, not strict parsing — same
   discipline `HARNECIENT-HACK.md` prescribes), substitute
   deterministically into BOTH the `.pal` snippet and the test-harness
   template (the model never touches the boilerplate), compute the
   expected value (`start+delta`) in shell, never ask the model for
   arithmetic it might get wrong.
4. Ran the fully-assembled result for real: **`PASS: item_9 = 6
   (expected 6)`, exit 0.** Genuinely correct, genuinely delegated
   (the model's own words drove which key/values were used),
   genuinely verified by actually executing the output — not a
   self-reported verdict.

**This is now the proven Stage A pattern, replacing §5's original
"delegate the whole file" framing**: Claude (or a human) writes and
verifies the template ONCE per command shape; delegation only ever
fills bounded blanks extracted from natural language, never generates
free-form structure. Matches §5c's fill-in-the-blank design exactly —
confirmed by real failure-then-success, not assumed. `Change HP` and
`Control Switches` (§6's remaining pilots) should follow this SAME
corrected pattern, not the original full-generation one.
