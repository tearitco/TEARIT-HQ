# AI-Function-Crafting — a db-hq tab that crafts event commands, not chemicals

**Status:** design · **Date:** 2026-09-17 · **Nothing built yet**
**Owner request, verbatim:** "we may create more db-hq header tabs for
hai-lab, such as the way canvas craft combines elements and
ingredients, but for ai tools, and other functions/fsm frameworks, by
having ai in the background or something to construct new code
'functions' for users given certain input/variable names etc, kind of
like a light weight deterministic coding agent for events."
**Direct continuation of:** `H-AI-LAB-DESIGN.md` Part 5 (AI bricks as
real events-hq command types). Full narrative walkthrough:
`1-1.HARNECIENT.SMOL/NIGHT_15_THE_BANK_AND_THE_BRICK.txt`.

---

## 0. What this is, in one sentence

A new db-hq tab, sitting next to Common Events and (once built) h-ai-lab,
that lets a human **craft a new events-hq command TYPE** — not just
fire an existing one — by combining real "ingredients" (a house AI
instance, an input variable name, an output variable name, a bounded
Gemma call) on a bench, the same three-panel recipe/bench/inventory
shape `CANVAS-CRAFT-DESIGN.md` already built for chemistry, pointed at
FSM fragments instead of atoms.

---

## 1. Why this is Part 5's real next step, not a new idea

`H-AI-LAB-DESIGN.md` Part 5 already establishes the whole mechanism
this tab needs to plug into:

- **The registry it draws FIELD1 from**: `ai_instances_registry.txt`
  (Part 1) — every AI-function-crafting recipe's first real ingredient
  is "which AI instance," looked up the same way `ai_fsm_transition`
  already does.
- **The place a crafted function lands**: `event_commands.registry.pdl`
  — Part 5's own real, zero-recompile command registry. A crafted
  function is not a new file format; it is a NEW ROW in this same
  file, same `TEMPLATE exec "$D/.../+x/some_op.+x" "$ENT" '{param}'`
  shape every command already uses, same real 2-field ceiling Part 5
  already solved (instance name carries KIND/PATH/IFACE lookup, one
  field stays free for the per-call thing).
- **The op it wraps**: Part 5's own table already lists real, either-
  built-or-clearly-scoped AI command types (`ai_fsm_transition`,
  `ai_irl_judge`, `ai_propose_weights`, `ai_attention_chat`,
  `ai_goap_plan`, `ai_rl_policy_choose`). This tab is not inventing a
  seventh kind of magic — it's the **UI that constructs one more row
  in that same table**, generated from a human's real input/output
  variable names instead of hand-written by whoever built Part 5.

So: Part 5 says "AI bricks are ordinary event commands." This doc says
"here is how a human, with no C and no hand-editing of
`event_commands.registry.pdl`, builds a NEW brick" — the crafting
bench is the missing front end for authoring the rows Part 5 already
made legal.

---

## 2. The canvas-craft analogy, mapped concretely

Read `CANVAS-CRAFT-DESIGN.md` first — this section maps its real,
already-designed mechanics one for one, not by vague resemblance.

| Canvas-Craft (chemistry) | AI-Function-Crafting (this doc) |
|---|---|
| **Recipe registry** (`elements]new=RECIPEZ...txt` / proposed `canvascraft_recipes.pdl`) — `name, protons, neutrons, electrons, parentA_idx, parentB_idx` | **Function-recipe registry** (new `ai_function_recipes.pdl`) — `name, ai_instance_kind_required, input_vars[], output_var, gemma_call_shape, parentA/B (composable sub-functions)` |
| **LEFT panel — Recipes/POE**: search + tiered scrolllist of craftable items | **LEFT panel — Bricks**: search + list of the SAME real AI command types Part 5's table defines (`ai_fsm_transition`, `ai_irl_judge`, ...), grouped by KIND the way Canvas-Craft groups by `tier` |
| **MIDDLE panel — bench + ingredient rows** (`have/need <parent name>`, highest-common-denominator resolver) | **MIDDLE panel — bench + slot rows**: instead of `have/need` counts, each slot is a REQUIRED INPUT the selected brick's op signature needs — an AI instance name (from the registry), a variable name, a literal, or another already-crafted function's OUTPUT plugged in as this one's input |
| **Formula bar** (`ƒ(...)`, `oxygen*5 + tree*2`, parses to bench moves) | **Signature bar** (`ƒ(...)`, e.g. `ai_irl_judge(instance=cursword, transcript=$slice) -> $verdict`) — same "typed fast-path that stages the bench" idea, grammar swapped from quantities to named parameters |
| **CRAFT button** — enabled when every ingredient row is satisfied | **BUILD button** — enabled when every required input slot is filled with a real registry-valid value; writes ONE new row into `event_commands.registry.pdl` (or a NEW recipe combining existing rows — see §4) |
| **RIGHT panel — inventory** (stacks with stable `[]N` handles) | **RIGHT panel — Variables & Instances**: the real, currently-known variable names (from the Common Event this tab was opened against) and AI instances (`ai_instances_registry.txt`), filterable, draggable onto bench slots the same way an inventory item is dragged onto a bench slot |
| **3D atom inspector** (Phase 4, deferred) | **FSM-fragment inspector** (deferred, §6) — a small visual preview of what the crafted brick's state-graph touch looks like, analogous, not built in v1 |
| **`canvascraft_manager.c`** — owns parse/resolve/bench-state/craft-execution | **`ai_function_crafting_manager.c`** (or a `<module>` in the shared renderer, matching db-hq's own embed pattern — see §3) — owns the same jobs, pointed at `event_commands.registry.pdl` instead of an inventory file |

The one real structural difference: Canvas-Craft's "ingredients" are
counted quantities of interchangeable stuff (5 oxygen). This tab's
"ingredients" are NAMED, TYPED slots (one AI instance, one input
variable, one output variable) — closer to filling in a function
signature than filling a quantity bar. That's a real, deliberate
divergence from the literal analogy, not an oversight: the owner's own
phrasing ("given certain input/variable names") is about naming, not
counting.

---

## 3. Where it lives (db-hq tab shape, reusing the real pattern)

Per `H-AI-LAB-DESIGN.md`'s own grounding section, db-hq is not a
separate manager+projector binary — it's a `<window class=
"database-window">` mode of the shared `khtpm_core_render.c`, sidebar
of categories + panel of content, and selecting "Common Events"
INJECTS the events-hq editor into the panel
(`dbhq_ce_open()`/`dbhq_ce_inject_panel()`). h-ai-lab (already built,
Part 3) reuses this same sidebar→embedded-panel pattern for AI
instances. This tab is a THIRD row in that same family:

```
db-hq sidebar:
  Actors / Classes / Items / ... / Common Events   ← events-hq embed
  AI-Lab                                            ← h-ai-lab embed (built)
  AI-Function-Crafting                              ← THIS tab (new)
```

Selecting "AI-Function-Crafting" injects the same generic
`<repeat>`/`${var}`/`action=` vocabulary Canvas-Craft and h-ai-lab both
already use — no new per-app C in `khtpm_core_render.c`
(CENTROID_GOLD_STD rule: generic tags, manager owns the brain). The
manager is a `<module>` the same shape as `canvascraft_manager.c` and
`ai_lab_scan.sh`, publishing a state file the renderer draws and
polling an action file for verbs — the same convention every other
house crafting/inspection surface already uses.

**Real open question, not decided here:** whether this tab's manager
is a NEW small C `<module>` (`ai_function_crafting_manager.c`,
Canvas-Craft's own precedent) or extends `ai_lab_scan.sh` (h-ai-lab's
existing shell-based scan/publish script) with new verbs. Canvas-Craft
chose a C manager because its resolver math (§3 there) is real
arithmetic; this tab's "resolver" is closer to registry lookups and
string templating — probably closer to `ai_lab_scan.sh`'s own
complexity than to a full C manager, but this is genuinely
underspecified until Part 5's own smallest-first-step
(`ai_fsm_transition`, still not started per `H-AI-LAB-DESIGN.md` §
"Smallest real first step" item 4) actually lands and this tab has a
real single command type to build against.

---

## 4. What "the AI in the background" means, concretely, bounded

The owner's own phrasing — "having ai in the background... to
construct new code 'functions'... kind of like a light weight
deterministic coding agent for events" — is the one part of this
request that most needs the house's own existing discipline applied to
it, per `H-AI-LAB-DESIGN.md` Part 4's own hard-won rule: **never a
freeform generator, always a bounded, structured call.** Concretely,
what Gemma (or a future `famous` instance) is allowed to do here is
narrow and explicit, matching Part 4's Propose/Score/Create button
shape exactly:

- **Suggest a signature** — given a human's own typed intent ("judge
  whether cursword's last exchange was accepted"), Gemma proposes
  which existing brick type fits (`ai_irl_judge`) and which real
  variable names from the RIGHT panel's known list should fill its
  slots. This is a SUGGESTION into the bench, exactly like Canvas-
  Craft's formula bar stages ingredients — never an auto-commit.
- **Suggest a composition** — given two already-crafted bricks, Gemma
  proposes whether chaining them (`ai_fsm_transition -> ai_irl_judge`)
  is a sensible new named recipe, matching Part 5's own "legos" framing
  (an ordered command list in a Common Event). Still a suggestion, a
  human still clicks BUILD.
- **Never**: Gemma does not write raw C, does not edit
  `event_commands.registry.pdl` directly, does not invent a NEW op
  binary. Every "function" this tab can produce is a NEW ROW composing
  EXISTING ops (Part 5's table) or wiring EXISTING variable names —
  the "deterministic coding agent" the owner asked for is deterministic
  specifically because its output space is the closed set of
  already-built ops + already-known variables, the same closed-set
  discipline that made events-hq's own "+ Add Command" picker (NIGHT
  14) the right shape instead of a typed field.

This keeps the tab honest with the owner's own "deterministic" word —
determinism here comes from Gemma choosing among a small, real,
enumerable set of moves, not from Gemma being asked to generate
arbitrary code and hoping it's safe.

---

## 5. What's genuinely new vs. reused

**Reused, zero invention:**
- The whole db-hq sidebar→embedded-panel mechanism.
- The whole Canvas-Craft bench/recipe/inventory UI shape and its
  formula-bar-as-fast-path idea.
- `event_commands.registry.pdl` as the real destination file — no new
  registry.
- `ai_instances_registry.txt` as the real source of "which AI" —
  same Part 1 registry h-ai-lab and Part 5 already use.
- The Propose-never-auto-merge discipline from Part 4 / NIGHT 9-12,
  applied to Gemma's suggestions here.
- The picker-not-typed-list instinct from NIGHT 14 (h-ai-lab's own
  "New State" rebuild) — this tab's slot-filling should use the same
  toggleable-list-of-real-choices shape for anything with a closed set
  (which AI instance, which existing brick type to compose), typed
  `cli_io` only for genuinely free text (a new brick's own name).

**Genuinely new:**
- `ai_function_recipes.pdl` (or equivalent) — no house file today
  describes a *crafted composition* of AI command types as a
  reusable, named, re-craftable "recipe" the way Canvas-Craft's own
  recipe file describes atoms. This is real new schema work.
- The signature-bar grammar (§2's `ƒ(...)` row) — named-parameter
  parsing is a different grammar than Canvas-Craft's quantity grammar
  (`item*count`), even though the UI slot is the same.
- Gemma-suggests-a-signature / Gemma-suggests-a-composition (§4) — no
  existing house feature does either; Part 4's buttons are Gemma
  acting ON a corpus/curriculum, not Gemma proposing NEW event-command
  rows.
- The tab itself and its manager — new UI surface, new (probably
  small) manager process.

---

## 6. Real open questions (not guessed answers)

1. **Manager shape** (§3) — new C `<module>` vs. extending
   `ai_lab_scan.sh`. Decide once Part 5's `ai_fsm_transition`
   smallest-first-step is actually live and there's a real command
   type to build a crafting UI against — building this tab before that
   exists would mean crafting rows for ops that don't exist yet.
2. **Where crafted recipes live relative to `event_commands.registry.
   pdl`** — does a "recipe" (a saved combination of bricks + filled
   slots) get flattened into ONE new registry row per craft, or does
   it stay a separate, named, re-invokable unit that itself appears as
   ONE command type (closer to a macro)? Canvas-Craft has no exact
   analogue (a crafted Water doesn't become a new craftable "recipe"
   itself, unless #7 in its own open-questions list — "recipe-
   definition language" — is ever built; that item is explicitly
   marked "not for now" there too, worth keeping consistent).
3. **Validation before BUILD is enabled** — Canvas-Craft's CRAFT button
   gates on `have >= need`. This tab's BUILD button needs an
   equivalent real gate: does every filled slot actually type-check
   against what the op script expects (an instance name that exists in
   the registry, a variable name that's actually in scope for the
   Common Event this was opened from)? Real validation logic, not
   designed here.
4. **Does Gemma's suggestion step require a live Ollama call every
   time the tab opens**, or only on an explicit "Suggest" button click
   (matching Part 4's own "never auto-run on open" discipline)?
   Leaning explicit button, not decided.
5. **Scope of "other functions/fsm frameworks"** — the owner's own
   phrasing explicitly widens this past AI bricks ("and other
   functions/fsm frameworks"). Whether this tab should be generic
   enough to craft NON-AI event commands too (e.g. a plain
   `show_text`/`change_gold`-style row) is unresolved — Part 5's whole
   premise is that AI bricks are ORDINARY event commands, so a fully
   generic version of this tab may end up being "a visual composer for
   any events-hq command," with AI bricks as just one category. Worth
   revisiting once the AI-only version proves the bench pattern works,
   same incremental discipline as every other doc in this chapter.
6. **FSM-fragment inspector** (§2 table, deferred) — Canvas-Craft's
   own atom inspector is Phase 4, explicitly deferred past the working
   bench. Same call here: a visual preview of the state-graph a
   crafted `ai_fsm_transition` touches is real and useful but not
   part of a v1 that just needs BUILD to write a correct registry row.

---

## 7. Smallest real first step

Following the same discipline as `H-AI-LAB-DESIGN.md` and
`LLMUD-INTEGRATION-DESIGN.md`: do not build this tab before Part 5's
own smallest-first-step (ONE new command type, `ai_fsm_transition`,
per `H-AI-LAB-DESIGN.md` §"Smallest real first step" item 4) is live.
Once it is:

1. A read-only version of the LEFT panel only — list the real command
   types that exist in `event_commands.registry.pdl` today (AI and
   non-AI alike), no bench, no BUILD, no Gemma. Proves the sidebar can
   enumerate the registry the way Canvas-Craft's Phase 1 proved it
   could enumerate the recipe file before any crafting existed.
2. Only once that's confirmed: ONE real bench, hand-filled (no Gemma
   suggestion yet), for `ai_fsm_transition` specifically — a human
   picks an AI instance and types a target state, BUILD writes ONE new
   registry row, verified against a real Common Event the way NIGHT 14
   verified its picker against cursword's real states.
3. Gemma-suggests-a-signature (§4) is a real later step, not part of
   the smallest first proof — the bench has to work by hand first.
