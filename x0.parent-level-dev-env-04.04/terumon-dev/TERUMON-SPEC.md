# terumon — evolvable pet/entity class: technical spec

Track opened 2026-09-22. Source prompt:
`🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive/!.gemini-llm-sept/8.TERUMON/TERUMON-DEV-PROMPT.md`.

This document is the real spec, written after a confirmation pass
against the actual house files named in that prompt's §1 table — not
a restatement of the prompt. Every claim below is either **CONFIRMED**
(cited to a real file read this session) or **OPEN** (named, not
invented). No third state.

Track location note: the prompt's working name was `XO.terumon-dev`.
Checked against real precedent before finalizing — `x0.parent-level-
dev-env-04.04/xyz-installer-dev/` and `1.TPMOS_c_+rmmp.0103.0001/
projects/{gem-dev,slop-ed-dev}/` are the house's actual dev-track
directories, and none use an `XO.`/`X0.` prefix or uppercase — they're
plain `<name>-dev`, sitting directly under `x0.parent-level-dev-env-
04.04/` (the top-level one) or under a `projects/` folder (the nested
ones). `x0.moke-pet-project-04.04/` is a separate, unrelated pet-sim
project (an existing `x0.` top-level dir, not a naming precedent for
terumon). This track is therefore placed at
`x0.parent-level-dev-env-04.04/terumon-dev/`, matching `xyz-installer-
dev`'s pattern exactly (top-level, `-dev` suffix, lowercase, no
letter+digit prefix).

---

## 0. Canonical name

**Decision: `terumon`.** Aliases in circulation, kept as known aliases
rather than deleted: `fuzzpets`, `dustpets`, `muchipets`.

Reasoning: `terumon` is the name the dev-prompt document itself uses
throughout as the working title, it's the name this track and its
seed files are being filed under, and none of the three aliases has
any existing file, code, or doc reference in the house as of this
pass (checked: no hits for `fuzzpet`, `dustpet`, or `muchipet` under
`x0.parent-level-dev-env-04.04/` or `44.xyz.01.00/` at the time of
this write). Picking the prompt's own working name over an alias with
equally-zero prior art is the smallest possible commitment — it costs
nothing to rename later if one of the aliases turns out to have real
history elsewhere in the house that this pass didn't surface.

---

## 1. Confirmation pass — one row per §1 table entry

### Entity system

**OPEN (not confirmed).** Grepped `44.xyz.01.00/&.hq-apps` and
`#.#.calendar-dox` for `entity_template`, `entity_manager`,
`learner_instance` — no direct hits this pass. NIGHT_20 states plainly
("Each entity gets its own learner instance") that the shape exists as
a *design decision*, but this pass did not locate the C-level entity
declaration mechanism that would show whether a terumon is a plain new
entity type/template or needs its own state subclass. Do not build
against an assumed entity API — the next session on this track should
grep specifically for wherever NIGHT_20's "learner instance per
entity" claim actually landed in code, if it has, before writing any
terumon entity-side C.

### tomom / school model (`NIGHT_20_THE_SCHOOL.txt`, `NIGHT_21_THE_RETURN_PATH.txt`)

**CONFIRMED, with one OPEN sub-item.** Both files read in full this
session.
- Per-entity learner instance, per-skill "class" = a literal curriculum
  folder of examples, pass/fail gating instead of one global fine-tune
  — this is exactly NIGHT_20's school model, and a terumon's "schools"
  learning-limit dimension maps onto it directly: which curriculum
  folders (classes) a given terumon's learner instance is allowed to
  attend.
- NIGHT_21 confirms the *return path* for anything a school-trained
  learner produces: promoted edits land as one new row in a per-entity
  `*_learned_overrides.pdl` file, gated by the same reward-weighted
  promotion ledger as everything else (§2.6/§2.7 below). A terumon's
  physical-trait evolution (color/size) and skill acquisition both
  follow this same return path — no separate mechanism.
- **OPEN**: NIGHT_20/21 never built the curriculum-folder format
  itself ("No curriculum format exists — not one class, not one lesson
  ... written anywhere," NIGHT_20's own honesty checkpoint). So "reuse
  the curriculum-folder shape from NIGHT_21 as-is" (the prompt's own
  phrasing) can't be literally confirmed — there is no existing folder
  shape yet to reuse. What's confirmed is the *design intent* (one
  learner instance, one folder per skill, pass/fail), not a built
  format. Terumon's `schools/` folders (§3.4 below) are this track's
  own first real instance of that format, not a copy of a working one.

### Bank Layer / Concept Bank / promotion gate (`A-TEARIT-IS-ALL-YOU-NEED.md` §2)

**CONFIRMED.** Full doc read this session, §2 in particular (the
Watch → Gemma DESCRIBE → deterministic scorer → candidate edit →
validator → replay → promotion ledger → live state loop, §2.1-2.7).
This is a general pipeline, not scoped to behavior alone — §2.3 names
four candidate-edit types (`spoke_weight_delta`, `new_concept_node`,
`fsm_transition_describe`, `goap_action_describe`), and §3.5
explicitly extends the same single promotion gate to *any* proposer
("one promotion pipeline, multiple kinds of proposer, never two
mechanisms"). A terumon's physical-trait evolution (color/size/other
visible traits) is exactly the shape of edit this loop already covers:
a `spoke_weight_delta`-shaped candidate edit on a trait-slot,
Gemma/trainer-proposed, validated, replayed, promoted through the same
ledger, written as a real file diff (per §2.7 — "Every promoted edit
remains a real file diff"). No second, parallel evolution mechanism is
needed or should be built. This confirms the prompt's own §1 claim
holds for physical-trait evolution, not just behavior.

### Watch Layer

**CONFIRMED (design-level).** `A-TEARIT-IS-ALL-YOU-NEED.md` §2.1: the
Watch Layer already observes action sequences + outcomes + feedback
and this pass's job was only to confirm reuse, not build a new
observer. In-game terumon learning (mode 1, §0 above) is a Watch Layer
consumer like any other entity — a terumon's `OBS`/`FEEDBACK` records
use the exact same format already specified in §2.1
(`OBS | id=... | target=... | outcome=...` /
`FEEDBACK | valence=... | concept=... | intensity=...`). No new
observer designed or needed.

### Chatbot / isolation mode

**Resolved as: second, clearly-labeled input channel into the same
per-terumon learner instance — not a genuinely separate mode.**
Reasoning: §3.5 of `A-TEARIT-IS-ALL-YOU-NEED.md` already establishes
the precedent this decision follows — a `trainer_bp` proposer (classic
forward/backward-pass training) submits candidate edits through the
*exact same* validator → replay → promotion-ledger gate as
Gemma-DESCRIBE-proposed or hand-authored edits, "never a second,
parallel path that writes live state directly." Isolation-mode
learning (a terumon as standalone chatbot, distilling from bigger
chat LLMs, searching the web) is the same shape of problem: it's just
a different *proposer* of candidate edits (call it `proposer=isolation_
chat` in the same `EDIT` record format, §2.3), sourced from
conversation/web content instead of Watch Layer observation. It still
has to clear the same validator (bounded deltas, hub-and-spoke
enforcement) and the same promotion ledger before anything it
"learns" while off the clock touches that terumon's live state. This
is the choice the house's own `A-TEARIT-IS-ALL-YOU-NEED.md` §3.5
precedent argues for directly, not a fresh design decision — building
isolation mode as a genuinely separate learner/state tree would
recreate exactly the "two-authoritative-writers bug" §3.5 names as
already solved once and warns against reintroducing.

### Chemistry system (`chemistry_tiles🏆.csv`)

**CONFIRMED, format only.** File read directly at
`x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.ref/menu/
palletes/chemistry_tiles🏆.csv` (also a shorter companion file with
fewer columns in the same dir). Real header row:
`emoji,compound_name,formula,category,hint,color_hex,state,
melting_point,boiling_point,density,toxicity,reactivity,icon_tile,
animation_frames` — a flat, one-row-per-compound CSV, no special
casing for compound *source*. Nothing in the schema distinguishes a
compound from a real-world periodic-table entry versus one a game
mechanic (a terumon being "milked") produced — a terumon-produced
molecule is representable as an ordinary new row (with a
`terumon-produced` provenance note in `hint`, or a new optional
column if the house wants that tracked structurally — not decided
here, flagged as a follow-up, not OPEN in the "unresolved" sense since
the row-level mechanism itself needs no schema change to work). This
confirms the prompt's §1 claim: the existing tile format can represent
a terumon-produced molecule as-is.

### Events pipeline (`&.widgits/events-hq/`)

**CONFIRMED.** Directory listed directly:
`44.xyz.01.00/&.widgits/events-hq/ops/` exists, matching
`A-TEARIT-IS-ALL-YOU-NEED.md` §2.6's own citation of it as "real,
working, confirmed by direct read." §2.6 already states the exact
mechanism terumon behaviors must use: an `fsm_transition_describe` /
`goap_action_describe` candidate edit is never allowed to write a
transition table or action definition directly — its DESCRIBE text is
compiled through the *same* deterministic `event.ir.pdl` →
`khtpm_events_hq_manager.c` → `cmd_N.sh` pipeline a human-authored
event already uses. Terumon fighting/reproducing/evolving-as-gameplay-
action all compile through this path, no new authoring surface.

### Economy apps (`chain-hq`, `myne-qrypto`)

**CONFIRMED (existence), OPEN (which one, or both, for "the store").**
Both apps are real and running: `44.xyz.01.00/&.hq-apps/chain-hq/`
(has `chain.seq`, `chain_action.txt`, `chain_ui.txt`, an `ops/` and
`audit/` tree, `open_chain_hq.sh`) and `44.xyz.01.00/@.apps/myne-
qrypto/` (has `MYNE_QRYPTO_DESIGN.md`, `net/`, `qtc/`, `system/`,
`pieces/`). Neither directory contains anything named `store`,
`marketplace`, or `listing` as of this pass — no existing in-house
storefront app was found under either. So "sellable on a store" is
real infrastructure-adjacent (a wallet app and a crypto/token app both
exist and could plausibly host listings/trades) but the storefront
surface itself — where a terumon listing actually lives, how a buy/
sell transaction clears — was not found built anywhere. Left OPEN per
§5 of the prompt, now with the specific negative evidence recorded
(checked both dirs directly, no store app present) instead of an
unchecked assumption.

---

## 2. `learning_limits.pdl` schema

One `learning_limits.pdl` file per terumon, sitting next to that
terumon's other state files (see §3 seed layout below). Written as a
plain key:value `.pdl` file, matching the house's existing "real file,
not hidden state" `.pdl` convention used throughout Events/tomom
override files. Exact numeric bounds are **OPEN** per the prompt's own
instruction (§2); the fields themselves, and their rough shape, are
this track's real design decision:

```
# learning_limits.pdl — owner-set cap on one terumon's learner instance
# Every field here gates a candidate-edit PROPOSAL (A-TEARIT §2.3) before
# it ever reaches the validator. This file is a proposal-side filter,
# not a replacement for the validator's own bounds (A-TEARIT §2.4) —
# both layers apply; this one is owner-authored, the validator's is
# house-authored and universal.

terumon_id: <uuid>
owner_id: <player/account ref>

# direction — which named concept/skill domains this terumon's learner
# instance may propose candidate edits toward. A list of Concept Bank
# master-node names (A-TEARIT §3) or curriculum/class names (NIGHT_20
# school model), never a free-text domain — enforced the same
# hub-and-spoke way the validator already enforces target/slot
# existence (A-TEARIT §2.4).
direction:
  allow: [<concept-or-class-name>, ...]
  deny:  [<concept-or-class-name>, ...]   # explicit block list, checked first

# size — ceiling on this terumon's own learner-state file size (its
# personal Concept Bank subtree / curriculum corpus on disk), not a
# count of promoted edits. OPEN: exact byte/row ceiling.
size:
  max_state_bytes: <OPEN>
  max_corpus_examples_per_class: <OPEN>

# strength_of_training — caps how aggressively candidate edits may be
# proposed for THIS terumon specifically. Maps onto A-TEARIT §2.3's
# `delta` field and §5's bootstrap tiers: a low-strength terumon is
# pinned to preschool-tier edit types/bounds regardless of its actual
# promotion-ledger track record; a high-strength terumon is allowed to
# earn its way up the same tier ladder A-TEARIT §5 already defines.
strength_of_training:
  max_delta_per_edit: <OPEN, bounded per A-TEARIT §2.4's own open item>
  max_tier: preschool | elementary_hs | associate_bachelor | master_phd
  edits_per_day_cap: <OPEN>

# schools — which curriculum folders (NIGHT_20 "classes") this
# terumon's learner instance may attend. A list of real paths under
# this terumon's schools/ dir (see §3.4) or a shared house curriculum
# path, once one exists (OPEN, per §1's tomom/school-model finding —
# no curriculum format is built house-wide yet).
schools:
  enrolled: [<class-name-or-path>, ...]
  pass_required_before_next: true | false

# environment — proximity/social learning: which OTHER terumon (by id)
# this one's environment-mode learning is allowed to be influenced by,
# and how strongly. Distinct from `direction`/`schools` because it's
# not curriculum-gated, it's who's nearby.
environment:
  proximity_sources: [<other-terumon-id>, ...]
  influence_weight: <OPEN, 0.0-1.0 range presumed, not confirmed against
                      any existing bound>

# mode — which of the two §0 modes (in-game / isolation) this terumon's
# learner instance currently accepts candidate edits from. Both may be
# true at once (see §1 "Chatbot / isolation mode" resolution above —
# both feed the SAME learner instance as separate proposers).
mode:
  in_game_watch_layer: true | false
  isolation_chatbot:   true | false
```

Every field above gates a *proposal*, matching the house's real
promotion architecture: `learning_limits.pdl` is read at candidate-
edit-proposal time (before `A-TEARIT-IS-ALL-YOU-NEED.md` §2.3's
deterministic scorer even emits an `EDIT` record), not at promotion
time — promotion-time bounds stay the validator's job (§2.4) and are
not duplicated here.

---

## 3. Track structure and seeds

```
terumon-dev/
  TERUMON-SPEC.md          (this file)
  seeds/
    terumon_001_ember/
      dustball_state.pdl
      learning_limits.pdl
      schools/              (empty class-folder placeholders, per §1 OPEN note)
    terumon_002_glacine/
      dustball_state.pdl
      learning_limits.pdl
      schools/
    terumon_003_murmur/
      dustball_state.pdl
      learning_limits.pdl
      schools/
    terumon_004_solvent/
      dustball_state.pdl
      learning_limits.pdl
```

### 3.1-3.4 The four seeds (summary — full state in each seed's own files)

| id | starting state | mode | learning-limit profile |
|---|---|---|---|
| `terumon_001_ember` | dustball | in-game only | narrow-direction, high-strength (single class enrolled, `master_phd` tier ceiling, high `max_delta_per_edit`) |
| `terumon_002_glacine` | dustball | in-game only | broad-direction, low-strength (many classes allowed, `preschool` tier ceiling, small deltas, tight `edits_per_day_cap`) |
| `terumon_003_murmur` | dustball | in-game only | environment-driven (near-zero direct `direction`/`schools`, high `environment.influence_weight`, proximity to 001 and 002) |
| `terumon_004_solvent` | dustball | isolation only | `isolation_chatbot: true`, `in_game_watch_layer: false`, moderate direction/strength, no `environment` proximity possible (never in-game) |

This gives four genuinely different experiment conditions rather than
four copies: one control-style tight/deep learner, one broad/shallow
learner, one that ignores curriculum and only reacts to who's nearby,
and one that never touches the Watch Layer at all and only learns via
the isolation-mode proposer resolved in §1 above.

---

## 4. What's OPEN (carried forward, not resolved here)

Restating the prompt's own §5 list, plus this pass's additions —
nothing below is silently answered anywhere in this document:

- Entity-system declaration mechanism for a terumon (new type vs.
  subclass) — not located in code this pass.
- Curriculum-folder format itself (NIGHT_20/21 name the shape, neither
  builds the format) — terumon's `schools/` dirs are a first attempt,
  not a copy of an existing built format.
- `learning_limits.pdl`'s exact numeric bounds (sizes, deltas,
  influence weights, day caps).
- Concept Bank subtree-per-terumon bound — whether "infinite evolution"
  needs the Concept Bank's own promotion-threshold/validator-bound
  opens (`A-TEARIT-IS-ALL-YOU-NEED.md` §6) resolved first. Not resolved
  here; flagged as a real dependency, not assumed clear.
- Store/economy integration — confirmed chain-hq and myne-qrypto both
  exist; no storefront app found in either; which hosts terumon
  listings (or a third, new app) is undecided.
- Reproduction mechanic (inherit/fork learner state vs. fresh dustball
  with inherited limits only vs. other) — not specified.
- terumon-produced molecule provenance tracking in the chemistry CSV
  (new optional column vs. `hint`-field note) — format works either
  way as confirmed above, which convention to use is undecided.

## 6. Organs, body simulation, and skills (2026-09-22, direct request, real design)

Direct instruction: terumon should have basic organs (brain, stomach,
bones, blood, nervous system, cardiovascular system, lungs, other
organs as needed). Real clarifying answer given when asked whether
these are Concept Bank nodes or simulated body-state: **both** —
hand-coded to start, evolvable later, and the design below is how
those two roles stay one real system instead of two things that can
drift out of sync.

**The split, precisely** (same single-source-of-truth discipline as
the Concept Bank's own spoke/mirror normalization, Q9):
- **`body_state.pdl`** (new, per-terumon, alongside
  `dustball_state.pdl`/`learning_limits.pdl`): the actual current
  numeric values — `blood_volume`, `lung_capacity`, `bone_density`,
  `nervous_activity`, etc. This is state, hand-editable, the thing
  gameplay (feeding, fighting, milking) directly changes.
- **Concept Bank master nodes** (`brain`, `stomach`, `bones`, `blood`,
  `nervous_system`, `cardiovascular_system`, `lungs`, ...): the named,
  weighted RULES for how those values change and relate to each other
  (e.g. how strongly `eating` affects `stomach` affects `blood`) — the
  same propose→validate→replay→promote loop (A-TEARIT §2) governs
  these exactly like any other Concept Bank relation. Hand-coded
  starting weights, same as the Concept Bank's other seed relations
  (`force`/`motion`/`energy`).

`body_state.pdl` is the value; the Concept Bank is the behavior. One
real file owns the number, one real substrate owns the rule for how it
moves — never two independently-hand-edited copies of the same fact.

**Game-time sync**: organs tick once per real house "turn" — a real,
existing concept (`@.apps/my-chara-txt/test-harn-same/scenarios/
demo_end_turn.sh` confirms a genuine turn-advance mechanism already
exists in this app family). **OPEN**: the exact hook a terumon's
`body_state.pdl` update should attach to on each turn-advance hasn't
been confirmed against real code yet — named as the real next
investigation, not assumed.

**Audit channels — three real, different interfaces onto the same
state, not three separate systems:**
1. **Chat query** (isolation-mode channel, already resolved as a
   second proposer into the same learner instance) — "are you
   hungry" reads real `body_state.pdl` values and phrases a real
   answer; the model describes real state, it doesn't invent one
   (same DESCRIBE-not-CLASSIFY law as everywhere else).
2. **Stats view** — a real dashboard panel (natural fit: extend
   h-ai-lab's own per-instance viewer, since terumon are already
   registered there per tonight's drop-wiring) reading
   `body_state.pdl` + the relevant Concept Bank spoke weights
   directly.
3. **Special items** (e.g. a "stethoscope") — **the in-game
   counterpart to h-ai-lab's out-of-game dashboard**, same underlying
   loop, different interface: a real inventory item that, used on a
   terumon, is itself an Event (same Events-pipeline mechanism as any
   other item/action, A-TEARIT §2.6) which reveals or lets a player
   hand-weigh a specific organ/skill weight. **Not yet built** — no
   stethoscope-shaped item exists; this names the real mechanism
   (item = Event, same compiler) it should use once built, not a new
   one.

**Skills — real correction, not a restatement of "weighted traits":**
direct clarification given: a skill is **not** just a Concept Bank
relation. A skill is **an actual, functional, compiled Event** — "a
moving/growing/learning event page" — the same real
`event.ir.pdl → cmd_N.sh` artifact any other Event already is (Q8
8a's resolved DESCRIBE+compiler decision, never direct emission),
stored as a real page in the terumon's own `event_pkg`. What makes a
skill different from an ordinary hand-authored Event: it has its own
Concept Bank weight(s) governing how reliably/powerfully it fires, it
can be "leveled" via repeated successful use (the same reward-weighted
promotion ledger as everything else in this house), and — the real
extension — its own DEFINITION can change over time, not just its
weight, through the same propose→validate→replay→promote loop. A
skill growing is therefore not a metaphor: it's a real Event being
re-proposed and re-compiled as it earns promotion, exactly the
mechanism A-TEARIT §2.6 already specifies for any FSM/GOAP
self-authoring, now named explicitly as what "a terumon learning a
skill" concretely means.

**Not yet done, deliberately**: `body_state.pdl`'s real field list and
starting values; the turn-advance hook (OPEN above); the stethoscope
item; any organ Concept Bank master nodes beyond tonight's seed set
(`force`/`motion`/`energy` — none of the organs named here have real
master node files yet). This section names the shape, not a finished
build.
