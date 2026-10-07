# wsr-sim-manager-ai.md

**What this is:** a survey of what the house already has for "a sim manager whose weights are
human-tweakable in `.pdl`, trained from readable RL, driven by prisc/pal events, reusable
across games with economies." Written before building anything, so the design starts from
what exists rather than from a parallel invention.

**Read the first section before anything else.** It invalidates the most natural reading of
the request.

**Method:** every claim below is cited `file:line` or quoted. Anything I inferred is marked
**INFERENCE**. Search covered `yz.muchiverse` and the `origin/attrition` branch (via
`.kilo/worktrees/attrition-win` @ `153397a1`).

> **Search trap, worth knowing:** `.git/info/exclude` lists `.kilo/`, and the search tooling
> honours ignore files. A repo-root search for "attrition" returns **zero** matches and that
> is a **false negative** — `RUSSIAN_DOLL_HOUSE_DESIGN/`, `17.ai/`, `NIGHT_24`–`NIGHT_30` and
> `^.hai-horn/` exist only on that branch, inside `.kilo/`. Anyone re-deriving this will
> wrongly conclude none of it exists.

---

## 1. The correction: "the attrition model" is not a model

This is the load-bearing finding, and it changes what we'd be building.

> **The name, confirmed 2026-09-29:** "the attrition-model" is the umbrella term for the whole
> effort — a war of attrition, wearing down API/OpenRouter dependence until entities have their
> own small offline autonomy, sharing trunked intelligence banks.
> — `RUSSIAN_DOLL_HOUSE_DESIGN/ATTRITION_DIAGRAM.md:11-16` *(attrition branch only)*

> **MAXINE:** The attrition-model. Not a pipeline name, not a product name — a strategy name. A
> war of attrition against needing an outside API for everything, worn down piece by piece,
> until something in the house can stand on its own weight.
> — `1-1.HARNECIENT.SMOL/NIGHT_30_THE_ATTRITION_MODEL.txt:12`

Corroborated twice more, including the owner's own words:

- `AGENT_ROADMAP_ANSWERS.md:521-527` — "Confirmed directly by the owner: **'attrition-model'
  is the umbrella term** … This is now settled, not a proposal."
- `12.calendar/2026-09-29/grok-report-s29.txt:17-25` — "🏷️ Official name, owner decision later
  the same day: the **attrition-model**. It is the umbrella over the Concept Bank, A TEARIT,
  the Harnecient hack, IRL, RL, FSM, and GOAP."

**There is no attrition equation.** No attrition-rate mechanic, no personnel/staff-turnover
model, no decay term. Grep for `attrition rate|employee attrition|turnover|quit rate` across
all of `yz.muchiverse` → **zero matches**. The word appears in 61 places, all of them this
strategy name.

**So "train the attrition model's weights" has no target as stated.** The thing that actually
has weights, a reward, and an edit loop is a different document. That's section 2.

---

## 2. What actually has weights and a reward: DESCRIBE → SCORE → VALIDATE → PROMOTE

This is the real mechanism, specified in `08-roadmap/design-docs/A-TEARIT-IS-ALL-YOU-NEED.md`
(519 lines, on both branches). It's the thing usually meant when people say "the model".

### 2.1 One input format, reused everywhere

`A-TEARIT-IS-ALL-YOU-NEED.md` §2.1:

```
OBS | id=<uuid> | target=<action/word/fsm-transition ref> | outcome=<free text>
FEEDBACK | valence=<+1|-1|0> | concept=<z-node name, optional> | intensity=<0.0-1.0>
```

> This is the one, single feedback event format used everywhere below — Bank Layer
> reward/punish, Concept Bank spoke updates, FSM/GOAP promotion scoring all consume the *same*
> record. One format, reused, not three separate schemas.

That reuse is the point. A new consumer costs a parser, not a schema.

### 2.2 One output format, four edit types

§2.3 — the only place natural language becomes state:

```
EDIT | id=<uuid> | type=spoke_weight_delta | target=gravity_constant
    | slot=force | delta=+0.05 | reason="<model's text, kept verbatim, not discarded>"
    | proposer=gemma | status=candidate
```

A **real record on disk** — `44.xyz.01.00/&.widgits/concept-bank/data/candidates/edit_0001_pass.txt`:

```
EDIT | id=e0001-real-smoketest | type=spoke_weight_delta | target=gravity_constant | slot=force | delta=+0.05 | reason="Watch Layer: stunt jump height increased when gravity_constant's force weight was lower" | proposer=gemma | status=candidate
```

Four types: `spoke_weight_delta`, `new_concept_node`, `fsm_transition_describe`,
`goap_action_describe`.

> `reason="…"` is kept **verbatim, not discarded** — every weight nudge carries the sentence
> that justified it.

### 2.3 Exactly ONE reward formula in the house

`LLMUD-HACK.md` §5:

```
weight = (rewardCount + 1) / (rewardCount + punishCount + 2)
```

Laplace-smoothed. A fresh observation (0 reward, 0 punish) starts at **0.5** — "genuinely
uncertain, not falsely confident at 1.0."

And `DUSTOPIA-HACK.md` §2 makes it an explicit no-duplication rule:

> The reward/punish → weight step is not a new mechanism — it's `LLMUD-HACK.md` §5's own
> Laplace-smoothed formula, reused verbatim. **Don't invent a second weight formula for this
> hack; there is exactly one in this house**, and it lives in the sibling document.

**Any WSR "train its weights" work reuses this formula. It is not to be re-derived.**

### 2.4 The named bottleneck: there is no promotion ledger

The loop does not close. Everything stops at human review.

> The house does not currently have a way to go from "a model proposed an edit" to "the house
> believes it" — that gate is entirely human-review-shaped right now.
> — `ATTRITION_DIAGRAM.md:96-102`

> `ai_describe`, `ai_fsm_transition`, and `ai_goap_plan` are the only new C the kilo note names
> for AI. They are not in the event registry. Tomom is the learner in the design. **IRL is not
> a loop.**
> — `17.ai/AI.md:19-23`

> Every validated EDIT record still lands in `pending_review.txt` for a human to review — there
> is no auto-promotion path. This is the actual gate.
> — `AGENT_ROADMAP_ANSWERS.md:36-43`

`AUTO-PROMOTION-RULE.md` sketches the intended gate and is honest that its numbers are guesses:

| Rule | Threshold | Source |
|---|---|---|
| Human-eligible | `>= 0.70` | `AUTO-PROMOTION-RULE.md:28-32` |
| Auto-promote | `>= 0.90` **AND** `N >= 20` observations | `:33-39` |

> The `0.90` auto-promotion bound and `N>=20` observation floor are both first real guesses, not
> measured against any real data (none exists).
> — `AUTO-PROMOTION-RULE.md:100-102`

### 2.5 HORN is the intended seat for this

`^.hai-horn/README.md:20`:

> Build a terminal-based CLI chat harness using OpenRouter API models, reusing chtpm primitives
> from gem-dev, **as the foundation for a mini attrition-model pipeline**.

`HORN_CHAT-HANDOFF.md:11`:

> …as the foundation for a mini attrition-model pipeline (**HALO_CHAT → IRL learning →
> curricula generation**).

Two build decisions from `^.hai-horn/dox/03-HORN-CHAT-BUILD.md` that matter for WSR reuse:

> **Chat history persists; it is not fed back as model context.** Each request carries only the
> current turn. Persistent context changes model behaviour turn over turn and makes the
> transcript disagree with what the model actually saw — which would poison the HORN-vs-HALO
> comparison the IRL harness exists to make.
> — `:250-255`

> **`tools/horn_tools.json` is the entire security boundary.** A tool the model asks for that is
> not in the `ops` map is refused, never dispatched — a hallucinated tool name must not become a
> process launch.
> — `:70-73`

---

## 3. "RL" is three different things in this house. Conflating them is the main hazard.

### 3a. RL as a per-decision tier — `decision_mode=rl`

`#.nnedox/❤️‍🔥️.⚛️.wrai-dust.fable-0.0.md` §12 gives the chassis:

```
projects/<sim>/pieces/<agent>/
├── piece.pdl
├── state.txt            # current_state=idle, decision_mode=preset|weighted|rl|llm
├── fsm/states.txt        # idle, deciding, acting, ...
├── fsm/transitions.txt   # idle -> deciding (always), deciding -> acting (chosen), ...
└── events/on_tick.asm     # PAL script: dispatch on current_state, call bot::* ops
```

- **Tier 2 — `decision_mode=weighted`**: "options are picked from a small `weights.txt`
  (`attack=0.5, defend=0.3, flee=0.2`) instead of uniformly … **now tunable without touching
  PAL code** … A real, if simple, form of reinforcement learning — no LLM required."
- **Tier 3 — `decision_mode=rl`**: "the weight lookup is replaced by a small learned policy
  keyed on `(state.txt snapshot, available options) → chosen option`, trained offline from the
  `(state, action, outcome)` tuples the FSM already produces for free … **No bespoke telemetry
  needed; the training step just reads the piece history files that already exist.**"
- **Tier 4 — `decision_mode=llm`**: "the model's only output is one of the FSM's already-enumerated
  option names — it never gets direct write access to state."

Note the phrase **"the piece history files that already exist."** That is the same
append-only-file-plus-cursor idea as section 5.

### 3b. RL as an offline training process — a hard house rule

`NIGHT_08_GOAP_RL_AND_THE_DECISION_MODE.txt:32-40`:

> **MAXINE:** RL is a fundamentally different KIND of thing than the other three. Weighted-formula
> and GOAP are both live, per-decision computations. **RL is a TRAINING process — it runs
> offline, over many, many simulated episodes … before the game ever ships that policy.**
>
> **TOMO:** Gemma is a language model, not an optimizer — asking it to DO reinforcement learning
> in real time, per tick, is the wrong tool for the job.
>
> **MAXINE:** … once an RL policy is trained and shipped, it becomes a FIFTH `decision_mode` — call
> it "policy" … The policy itself is a small, fast lookup or a tiny trained network, not a model
> call.

So: **`decision_mode=policy` is the missing tier.** Weighted → policy → llm, all behind one
interface.

### 3c. Inverse RL — learning the reward from recorded behaviour

`NIGHT_11_INVERSE_RL_AND_FAMOUS_LLM.txt:10,16,20,24`:

> **MAXINE:** Inverse RL runs backward — you're given real, recorded BEHAVIOR, and you infer the
> reward function that behavior was implicitly optimizing.
>
> **MAXINE:** So the actual training signal for Inverse RL isn't hypothetical — it's every agent
> session this house has ever run, especially the ones that got corrected. **A revert IS a
> negative reward signal. A commit that stood unchanged for weeks IS a positive one.**
>
> **TOMO:** […] what comes out isn't "the reward for touching events-hq" as one number — it's a
> real, per-ACTION-TYPE reward … **The reward function IS this house's own accumulated
> pitfalls-and-praise, made numeric.**

`IRL-BOOTSTRAP-RECURSION-SPEC.md` (both branches) builds a six-layer version and defines a
correction signal needing no new sensor:

> If the `U|` line immediately following an `A|` line restates the question, says "no I meant," or
> otherwise re-asks, that's a real, cheap, textual NEGATIVE signal. If the next `U|` line moves
> on to a new topic, that's a real POSITIVE signal.

And it names its own failure mode:

> **Hop 2 in particular is a closed loop** (Gemma picks its own curriculum slice AND judges its
> own correctness) — **a textbook reward-hacking condition with nothing watching from outside.**

---

## 4. Weights: hand-tweakable is explicit doctrine, not a preference

The prohibition, stated directly — `AI-TRACK-BRAINSTORM-QUESTIONS.md` §9a:

> Direct, explicit house decision: **no opaque weight matrices, anywhere, ever, in tomom.** […]
> It's **precise, hand/fine-tuned adjustment via meta-harness techniques** … "hand-tunable,
> modular, auditable LLM track". Dense matrices are opaque by construction, not by accident —
> gradient descent distributes each concept across many weights and packs multiple unrelated
> concepts into the same weight (superposition).

And it's argued from a **real audit of real numbers** — which is the strongest form this house
takes:

> Confirmed directly during this same audit: `mlp_model.txt`'s real current values include
> `13075.6`, `17331.1`, `-861.1` — unlabeled, uninspectable, and quite possibly a recurrence of
> the exact weight-explosion bug the Aug 1 session believed it had fixed. **That's not a
> hand-tunable file; nobody could look at row 3 column 47 and know what editing it would do.**

**Crucially, gradient descent is explicitly *allowed* — with a different job:**

> **relevant, but its job changes.** […] Once a relation already exists and is *named*
> (`gravity → force`), refining exactly how strong that weight should be, using real
> corpus/replay data as a loss signal, is a different and legitimate problem […] a classic
> forward/backward pass over a curriculum corpus is still a real, useful thing to run — it just
> computes a suggested delta for an **already-existing slot**, and submits that delta as one more
> `spoke_weight_delta` candidate edit, with `proposer=trainer_bp` […] **one promotion pipeline,
> multiple kinds of proposer, never two mechanisms allowed to silently overwrite the same weight.**
> — `A-TEARIT-IS-ALL-YOU-NEED.md` §3.5

> **OPEN:** the exact loss function and replay-batching scheme for this `trainer_bp` proposer
> haven't been specified.

### Real weight files, on disk, with real numbers

`44.xyz.01.00/&.widgits/concept-bank/data/spokes/gravity_constant.pdl`:

```
NODE | gravity_constant
KIND | spoke
N_SLOTS | 8
# SLOT | <idx> | POINTS_TO=<master name — MUST exist under data/masters/> | WEIGHT=<float>
SLOT | 0 | POINTS_TO=force | WEIGHT=0.60
```

`.../data/masters/force.pdl`:

```
NODE | force
KIND | master
N_SLOTS | 8
SLOT | 0 | POINTS_TO=motion | WEIGHT=0.70
SLOT | 1 | POINTS_TO=energy | WEIGHT=0.50

# ---- MIRROR TABLE — DERIVED, GENERATED. DO NOT HAND-EDIT. ----
MIRROR_GENERATED_AT | 2026-09-22 16:16:19
MIRROR_FROM | gravity_constant | WEIGHT=0.60
```

Single-source-of-truth rule, `AI-TRACK §9b`:

> **Weights live in exactly one place: the spoke's own forward record.** […] **The master's own
> record holds a mirror table … but that mirror table is a derived, regenerated index, never a
> second authoritative copy.**

Documented tunables:

| Parameter | Value / rule | Source |
|---|---|---|
| Slot count | "start at 4-8, room to grow to 32+" | `AI-TRACK §9b` |
| Weight range | `WEIGHT=<float, -1.0..1.0>` | `force.pdl:21` |
| Slot trim | "zero a low-weight slot (pruning)" | `AI-TRACK §9b` |
| Delta bound | bounded, **exact ranges OPEN** | `A-TEARIT §2.4, §6` |
| Time decay | **OPEN** | `LLMUD-HACK §5, §7.2` |

**And the WSR-shaped one already exists.** ~110 real `weights.txt` files, e.g.
`MarS.StreetRace.wsr]Q]k32/corporations/generated/AFL/weights.txt`, entire content:

```
risk 12
```

`NIGHT_09_THE_FINAL_BOSS_ATTENTION_WEIGHTS_FROM_GEMMA.txt:18`:

> the house already has the exact SLOT this fits into. … `corp_decide.c`'s own real "weighted"
> decision_mode reads its risk-bias numbers from a real file — `weights.txt`. **Someone, or
> something, still has to WRITE that file.**

> Every Gemma-proposed word, weight, or category is a real, inspectable line in a real file
> before it ever reaches a runtime `weights.txt` — **a pull request, not an auto-merge.**
> — `:32`

---

## 5. `.pdl` as the tweak surface — and the real zero-recompile precedent

### 5a. There is no single `.pdl` format. There are at least five sharing one extension.

| # | Family | Separator | Example |
|---|---|---|---|
| 1 | Canonical PDL/PDLO — 3-col pipe | `SECTION \| KEY \| VALUE` | `WSR_PAL-PREFERED/project.pdl` |
| 2 | Extended PDLO METHOD — 4–6 col, `lang:path` | pipe | `pieces/ui_components/button/button.pdl` |
| 3 | INI-ish hybrid — `[META]` **or** `META \| K \| V` | pipe or `=` | `projects/*/project.pdl` |
| 4 | Plain `key=value` | none | `#.desktop/hq_ui.pdl` |
| 5 | YAML-like | nested | `curriculum.pdl`, `learning_limits.pdl` |

**Practical consequence: "the `.pdl` format" is only well-defined per consumer.**

Canonical grammar (`piece_manager.c:79-143`):

```c
// Parse PDLO format: SECTION      | KEY                | VALUE
if (sscanf(line, "%49[^|]|%99[^|]|%199[^\n]", section, key, value) == 3) {
```

- Separator is literal `|`; whitespace trimmed.
- **CRLF and LF both parse identically** (`piece_manager.c:98` strips `\n\r`).
- `#` comment **at column 0 only**; no inline comments; no quoting; no escapes.
- Hard caps, **silent truncation**: section 49, key 99, value 199; `MAX_LINE_LENGTH 500`.
- Section names honored by `piece_manager.c` (`STATE`/`METHOD`/`EVENT_IN`/`RESPONSE`) and
  **ignored entirely** by every chtpm parser, which matches the literal `METHOD` prefix alone.

### 5b. `.pdl` already holds tunables — real examples

| File | Rows |
|---|---|
| `pieces/registry/fonts/ascii/99/piece.pdl` | `META \| width \| 8`, `META \| height \| 16`, `META \| baseline \| 12`, `META \| advance \| 8` |
| `#.desktop/desk_grid.pdl` | `GRID \| cell_px \| 80` |
| `#.desktop/hq_ui.pdl` | `win_top_y=96`, `click_two_step=1`, `font_scale=1.25` |
| `#.desktop/livedesk_theme.pdl` | `COLOR \| bg`, `COLOR \| opacity \| 1.00` |
| `_.START_BUTTON/config/start_button.pdl` | `STATE \| scan_depth \| 2`, `STATE \| max_entries \| 64` |
| `projects/lsr/project.pdl` | `STATE \| year \| 2026`, `STATE \| pop_count \| 2` |

The font-metrics file is the cleanest proof: pure numeric tunables, no `piece_id`, no `version`.

**There is no `DEFAULT` or `FIELDS` section anywhere in the repo** — searched, zero matches.

### 5c. The important find: `event_commands.registry.pdl`

`44.xyz.01.00/#.ref/menu/event_commands.registry.pdl` — 747 lines, ~50 `COMMAND` blocks. This is
the strongest existing precedent for "tweakable in `.pdl`, zero recompile":

> Adding a new SIMPLE command (one that wraps a single real op with substituted string params —
> which is what almost every event command actually is) requires editing ONLY this file.
> **Zero recompile, zero C changes**, in either events-hq or event-ez.
> — `:8-11`

Shape (`:13-20`):

```
COMMAND <type>
  LABEL ...
  FIELD1 ...
  PARAMS ...
  TEMPLATE exec     # or: PAL   (raw prisc+x lines)
END
```

Hot-reload is real: `khtpm_events_hq_manager.c:164-167` — "Re-reads the registry only when its
mtime changes."

**This is the mechanism to copy for the WSR sim manager.** Not a new config language — the
house already has the pattern, it's just never been applied to WSR.

### 5d. The trap: `.pdl` edits are not hot-reloaded

`chtpm_parser_pal.c:1442-1451` and `chtpm_parser.c:687-696` are identical:

```c
const char* active_id = get_var("active_target_id");
const char* existing_methods = get_var("piece_methods");
if (strstr(current_layout, "playrm/layouts/loader.chtpm")) { load_dynamic_methods("loader"); }
else if (strlen(active_id) > 0 && (strlen(existing_methods) == 0 ||
         strcmp(existing_methods, "[No Methods]") == 0)) { load_dynamic_methods(active_id); }
```

Once a menu renders, `piece_methods` is non-empty and **editing the `.pdl` does nothing** until
a fresh process start. **This is not a file watcher.** If WSR wants live-tweakable config, it
needs the `hq_ui.pdl` marker-file pattern or the registry's mtime check — otherwise "tweak the
.pdl and restart the process" is the loop, and that's a worse experience than the user is
asking for.

---

## 6. prisc/pal events: what exists, and the one seam worth building

### 6a. The VM

`WSR_PAL-PREFERED/system/prisc+x.c` (1520 lines) is a 16-register int32 machine with a separate
string-register bank. Event-relevant opcodes:

```c
OP_READ_HISTORY, OP_EXEC, OP_HIT_FRAME, OP_READ_STATE, OP_READ_ACTIVE_TARGET,
OP_READ_ENV_KEY, OP_SLEEP, OP_READ_LAYOUT, OP_READ_POS, OP_ECALL,
OP_SFOPEN, OP_SFAPPEND, OP_SWRITE, OP_SFCLOSE
```

**`OP_READ_HISTORY` is the event primitive.** It `fseek`s to a register-held byte offset,
`fscanf`s one int, then **writes the new offset back into the register the instruction itself
advanced** (`prisc+x.c:1246-1268`). The queue *is* a file cursor.

The loop, `pal/main_loop_chtpm.pal:13-28`: read persisted `history_cursor` → `read_history` →
dispatch each key → persist cursor → recompose frame → sleep 30ms.

### 6b. The ledger is already event sourcing — and has zero readers

This is the highest-value finding for "reusable for financial research tools."

`market_settle.c:252-255` writes double-entry rows:

```c
fprintf(g_ledger,
        "Time: %s | Debit: %s | Credit: %s | Amount: %.2f Dollars | "
        "Event: market_settle.+x | Ticker: %s | Price: %.4f | Shares: %.2f\n", …);
```

Provenance, `market_settle.c:228-231`:

> The original's own row format, `MSR-DEPRACATED/financing.c:256`. One line carries both sides
> of the entry, so one line IS the balanced record; this is a double-entry ledger, not a pair of
> half-entries.

**Three writers** (`market_settle.c:237`, `goods_settle.c:195`, `corp_payroll.c:154`), all
`fopen(…,"a")`. **Zero readers.** The only consumer anywhere is legacy
`MSR-DEPRACATED/master_reader.c:24-47` — a line-cursor tailer that dispatches on
`Event: incorporation.+x` — **and it was never ported.**

> **INFERENCE:** the house has event sourcing *in shape but not in practice*. WSR today is
> *state*-sourced: each op reads `state.txt` and writes `state.txt`, and the ledger is an audit
> trail beside it rather than the authority. The information needed to reconstruct balances is
> present and self-contained; it is simply never read. **Event sourcing without the sourcing.**

### 6c. Determinism is deliberate — with exactly one hole

By design, and the stated reason is diagnostic:

> Dispersion is a deterministic hash of the piece id, NOT a random number: the same participant
> must hold the same view on every run, or the market would be unfalsifiable and a divergence
> could never be diagnosed.
> — `market_quote.c:414-418` (FNV-1a, `:423-434`)

Same in `goods_quote.c:280-284` — "or **a scenario could not be replayed**" — and
`goods_sink.c:45-46` — "breakthroughs come from an accumulated threshold, never `rand()`, so a
run replays exactly."

**The one hole**, `corp_attempt_merger.c:130-131`:

```c
srand((unsigned int)(time(NULL) ^ getpid()));
float roll = (float)rand() / (float)RAND_MAX;
```

That decides whether real cash moves in a merger. A grep for `srand|rand\(\)` across all
`WSR_PAL-PREFERED/**/*.c` returns **exactly these two lines and nothing else**. Any RL training
that replays runs must fix this first, or the training data is irreproducible.

### 6d. Snapshot capability exists and is dead code

`prisc+x.c:832-847`:

```c
void load_mem(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    int addr, val;
    while (fscanf(f, "%d %d", &addr, &val) == 2)
        if (addr >= 0 && addr < MEM_SIZE) mem[addr] = val;
```

Wired to `argv[2]` at `:1232`. Grep `pal/*.pal` for `prisc+x|\.mem|save_mem|load_mem` → **zero
hits.** `save_mem` has **no call site at all**.

> **INFERENCE:** `MEM_SIZE 4096` of int32 is far too small to snapshot a world living in ~100
> `state.txt` files plus a growing ledger. The VM's snapshot is a register dump, not a world
> dump. Real replay needs a world-level snapshot/restore, which does not exist.

### 6e. The IPC surface is file-based and bidirectional

- **Inject:** the house calls it the "AI-injection power relay channel"
  (`mr_move_to_entity.c:15-28`) — `interact_relay.txt`, consumed by `khtpm_core_render.c`.
  `mr_show_text.c:54-58` writes it as a **truncate-write single-slot mailbox**, not a queue.
  Two racing writers lose one write.
- **Inject (generic, in the registry):** `send_input`, `:198-219` — "wraps the real
  `SYS_OPEN(append)+SYS_WRITE_LINE+SYS_CLOSE` chain into one command so a harness author writes
  one line per keypress instead of three."
- **Observe:** `current_frame.txt`, and the ledgers.

Pipeline shape, `44.xyz.01.00/#.DOX/CHTPM_ARCHITECTURE_GUIDE.txt:10-37`:
`keyboard_input` → `history.txt` → `chtpm_parser_pal` → `interact_relay.txt` → `prisc+x` →
`ops/*` → `master_ledger.txt` → `compose_frame` → `current_frame.txt` → `renderer_pulse.txt` →
`system/renderer`.

That guide opens `:7-8` with "**THIS IS THE CORRECT ARCHITECTURE TO USE GOING FORWARD** …
Template for all future apps/games" (fixed 2026-07-23).

### 6f. The real event VM already exists — in a sibling widget, unused by WSR

`yz.muchiverse/44.xyz.01.00/&.widgits/events-hq/`. Package shape
`event_pkg/pages/page_N/` with `condition.pdl`, `event.ir.pdl`, `event.pal` (generated),
`cmd_N.sh` (generated).

- `condition.pdl` is `COND | trigger | <name>`; vocabulary fixed by `event_commands.registry.pdl:111`.
- `ops/play_event.sh:13-22` runs the **highest-numbered** matching page — "same semantics RPG
  Maker MV itself uses."
- Manager loop, `khtpm_events_hq_manager.c:1228-1256`, and `:401`: "`event.pal` is ALWAYS fully
  regenerated from `event.ir.pdl`, never" patched.

**WSR uses none of it.** No `condition.pdl`, no `event.pal`, no `play_event.sh`. Adopting
events-hq is the path — not extending WSR's ad-hoc marker files.

---

## 7. Dustopia — real, and a good fit for "what they're made of"

`#.nnedox/❤️‍🔥️.⚛️.wrai-dust.fable-0.0.md`, 322 lines, on both branches.

Four rules (§2): **Size = Time** (small things run fast clocks, big things slow);
**Fuzz Pets** (particles as resonant creatures on a spectrum); **fractal zoom**
(voxels↔particles); **4-state chemistry** borrowed from transistor logic.

The chemistry is the usable part — a truth table, O(1), no orbitals:

```
0 + 0 -> 0   (no reaction)
0 + 1 -> 1   (B drives A into a bond — weak, one-directional)
1 + 1 -> 1   (mutual bond — stable molecule forms)
Z + x -> x   (Z is transparent — x passes through unchanged)
3 + x -> 0   (3 forcibly breaks any existing bond back down to 0)
```

States: `0` no bond · `1` bonded/driven · `Z` pass-through/high-impedance · `3`
cutoff/breakdown.

Drug-screening worked example (§11): blocked at membrane (`Z`) → no effect; bonds and breaks
asymmetry → toxic; bonds an interior material back into cycling → therapeutic; triggers `3` →
breakdown.

**Two hard constraints:**

1. **λ is undefined and explicitly off-limits** (`DUSTOPIA-HACK.md:4`):
   > The "spectral flow parameter λ," and the specific λ values used in its worked example
   > (2.5, 2.8) are **imported wholesale from an external source this house has not
   > independently derived, defined, or verified.** […] the λ formalism specifically should not
   > be cited by a future document as if this house has a real implementation or definition of it.

   And `:303-304`: "**Do not let a later doc treat the receipt work as a definition of λ.**"

2. Its own KPI ladder (`:309-322`) is mostly unmet — items 3–7 including "a stored weight
   chooses which fact advances the tick" and "`ai_describe` is not in the registry."

---

## 8. Tech trees and R&D — the mechanism is named and already reused five times

`SOCIETY-ECONOMY-ARCHITECTURE.txt` §4 is the key line:

> crafting, tech trees, ecosystem population, creature evolution, and now manufacturing —
> **worth naming as the throughline it clearly is**

…reusing `craft.c`'s real `composed_of` mechanism. **Not a new tech-tree engine.** That matters:
the user's "employees research breakthroughs → tools, chemicals, what they're made of" is a
`composed_of` graph plus a `decision_mode`, not a new subsystem.

§9 — R&D tiering, directly answering "how does this tie into our goals":

> **Routine/light training** = `ROADMAP-models.txt` §12's grade/subject curriculum ladder,
> running locally via IQABOD, governed by `GAME-AI-SPEED-DOCTRINE.txt`'s own rule that ongoing
> weight-tuning is fine to run as background/between-tick work as long as it never blocks a live
> decision on a network call
>
> **Cutting-edge R&D breakthroughs** … A company or college's R&D department, staffed by
> research-tier-expertise citizens, occasionally (**NOT every tick - rare, exactly per the
> doctrine**) calls a real LLM to propose a genuinely novel research direction; that proposal
> then gets tested/validated LOCALLY (no further API calls), and if it works, becomes a
> permanent local asset - **a new trained curriculum, a new product recipe, a new commodity.**

**That is exactly the "breakthrough" mechanic requested**, already specified. And §8 gives a
checkable example: "Internet" is a tech-tree node; once unlocked a company may register
`ecommerce=1`, bypassing the physical-location requirement — "a real mechanic with a real
trigger condition."

Real chemistry data exists: `44.xyz.01.00/#.ref/menu/palletes/chemistry_tiles_expanded🏆.csv`,
51 rows —
`emoji,compound_name,formula,category,hint,color_hex,state,melting_point,boiling_point,density,toxicity,reactivity,icon_tile,animation_frames`

> chemistry-as-gameplay, already tiled and playable, not just a design doc.
> — `A-TEARIT-IS-ALL-YOU-NEED.md` §8

**One warning to carry forward** — `MY_BIOTECH_DESIGN.md:36`, about its own "RL":

> **IMPORTANT — what NOT to reuse:** IQABOD's own trainable-embedding curriculum/RL system […] is
> a genuinely separate, much heavier system […] My-biotech's own "RL" is a **lightweight,
> non-trained weighted-random selection** […] **a scoring/selection heuristic, not a trained
> model.** […] flagged here explicitly **so nobody conflates the two.**

---

## 9. Honest negatives — cited material that does not exist here

Stated plainly rather than worked around, because several are cited as load-bearing:

1. **No attrition-rate / turnover mechanic.** Zero matches.
2. **No quantitative attrition model.** No formula, no loss function. It is a name.
3. **`MUCHICIVO_BIBLE.txt`** — cited ≥6 times as the source of the Tier 2 tech tree. Globs to
   nothing.
4. **`GAME-AI-SPEED-DOCTRINE.txt`** — cited as the authority for "AI APIs = training/rare-moment
   only". Absent.
5. **`COMPOSABLE-MATERIALS-ARCHITECTURE.md`**, **`GS-23-HQ-TECH.md`** — absent.
6. **`fsm+ai-training-plan-b1/`** — `fsm_bot_programmer.txt`, `ai_plan.txt`,
   `talkable-interest_=fsm.txt` all cited as "already-specified existing files". Absent.
7. **`03-ATTRITION-VIA-CHAT.md`, `04-TOMOM-TRAINING-OPEN-QUESTIONS.md`** — cited as *required
   reading* by the HORN handoff, and located at
   `/home/no/Desktop/github/work/XO/10.kilo-attrition/kilo-openrouter-docs/` — **a Linux path
   outside this repo entirely.**
8. **`❤️‍🔥️.⚛️🌆️dustop-aia/`** and `tpm.dust-pia-prompt.md` — the original 19-knob material.
   Cited, absent.
9. **"HALO" as an AI concept only ever means `HALO_CHAT`.** Every other `halo` hit in the house
   is a UI glow ring on cursword.
10. **`EVENT_IN` / `RESPONSE` in `project.pdl` are declared but unimplemented** — no code
    anywhere parses `EVENT_IN`. Aspirational.

---

## 10. What I'd build, given the above

**INFERENCE — my recommendation, not house doctrine.** Ordered by value per unit of risk.

1. **A ledger reader for WSR.** Port `master_reader.c`'s cursor-over-ledger pattern. The
   double-entry rows are already written, already balanced, already self-contained. Nothing reads
   them. This is the single seam that turns WSR into an event-sourced sim — and it's the
   prerequisite for everything else, because a training corpus needs replayable events.
2. **Fix the one nondeterminism hole** (`corp_attempt_merger.c:130`) before anything trains on
   runs. Deterministic replay is the house's own stated requirement for diagnosability; an RL
   loop built on irreproducible runs can't be debugged.
3. **A `weights.pdl` per participant + a `weight_train` op**, using the *existing* Laplace
   formula and the *existing* `OBS`/`FEEDBACK`/`EDIT` record formats. Reads
   `weights.txt`/`weights.pdl`, writes `SLOT`-style rows, emits `EDIT` records to
   `pending_review.txt` — human-gated, matching current house behaviour. This is
   "train its weights in a tweakable `.pdl`" with zero new architecture.
4. **A `decision_mode=policy` tier** between `weighted` and `llm`, per NIGHT_08, so a trained
   lookup and a hand-written `weights.pdl` are interchangeable to callers.
5. **Breakthroughs as `composed_of` graphs**, per `SOCIETY-ECONOMY-ARCHITECTURE.txt` §4/§9,
   with Dustopia's `0/1/Z/3` chemistry as the bond model for "what they're made of."
6. **Adopt `event_commands.registry.pdl`'s zero-recompile pattern** for the manager's command
   surface, and add an mtime check so `.pdl` edits are actually live.

Steps 1–3 are self-contained and don't require the WSR/attrition merge to be settled first.

---

## 11. Questions

Answering these changes the design; I've had to guess on all of them.

**On the name**
1. Given "attrition model" is an umbrella term for API-independence rather than a mechanism, do
   you want the WSR sim manager to *implement* one of its named parts (A-TEARIT's loop, IRL),
   or be a **consumer** of that loop — i.e. WSR supplies `(state, action, outcome)` events and
   weights and something else teaches?

**On training**
2. "Train its weights using readable RL" — do you want **offline training over replayed runs**
   (NIGHT_08's hard rule, needs steps 1–2 above), or **live per-tick updates** from the Laplace
   counter (cheap, no replay, but it *is* the thing NIGHT_08 says not to do)?
3. Is `decision_mode=rl`/`policy` meant to be **per-WSR-corporation** (each corp learns its own
   weights from its own history), or one shared house-level policy?
4. Reward signal for a corporation: what counts as a positive outcome — share price up,
   dividend paid, a goods sale filled, a tech unlocked? Several of these are already ledger
   rows.

**On the `.pdl` surface**
5. Should WSR manager config use the **canonical `SECTION | KEY | VALUE`** form (so
   `piece_manager.c` reads it), or match the **concept-bank `NODE/SLOT/WEIGHT`** form (so
   weights look like the existing spoke files)? Or one file with both?
6. Do you want **live reload** while the game runs (needs an mtime check — `pieces.pdl` edits
   currently do nothing until restart), or is edit-and-restart acceptable?

**On scope**
7. WSR↔attrition merge direction — WSR consumes HORN/HALO, or they coexist? This decides
   whether the sim manager lives in the `^.hai-horn` package or in WSR.
8. Is Dustopia in scope for WSR now, or later? §7's `0/1/Z/3` is cheap and would make
   "breakthroughs" mean something concrete, but it's a second large mechanic.
9. Do you want the ledger reader built **now** as step 1, or is there a reason to sequence it
   later?