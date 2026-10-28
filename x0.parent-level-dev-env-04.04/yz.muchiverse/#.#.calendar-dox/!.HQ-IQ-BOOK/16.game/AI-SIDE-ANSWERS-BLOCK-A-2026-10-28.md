# AI-side answers (Block A of the companion brief), from Claude, 2026-10-28

Answers to `GROK-COMPANION-BRIEF-AI-SIDE-AND-PUNCH-UP-QUESTIONS` (the owner's copy is in `XO/30.rpg-dust-work/oct-28/`; the book's original is `HANDOFF-2026-10-27-AI-SIDE-COMPANION.md`). The owner asked me to answer them as a doc. I answer **Block A** (connect rpg-space to the AI and pets) in full, because it can be grounded in code I could read; Blocks B, C, D need a driven board or a design pass and I list in section 9 what I would need. Labels as in the brief: 📖 **READ** (file or grep), ▶️ **RAN** (I executed it and saw the result), 📝 **PROPOSED**, 🧠 **JUDGEMENT**. Everything was read on 2026-10-28 from branch `claude`.

## 0. Two corrections to the brief, found while grounding it
1. **"No pet learns from play today" (brief 2.6) is not quite true.** 📖 `rpg-space` pets write one `DEC` row per autonomous choice (`dec_row`, `rpg_space.c` ~line 190), the op `dec_op` digests them, and the game reads a result back. ▶️ `state/decisions.txt` has **5532 rows**; `state/bank/dec_rpg-space_walk.behavior.pdl` says `reward=4691 | punish=516 | WEIGHT 0.9007`; `state/bank/dec_rpg-space_pick_pct.txt` holds **21**; and 45 + (0.5 − 0.9007) × 60 = 20.96, which rounds to 21, so the file matches the formula in `dec_op.c`. The game re-reads it every 10 s (`dec_pick_pct`, `rpg_space.c` ~line 217) and uses it as the chance that a target-less pet picks a new target (`PET_SECTION 37`). So **one loop is closed for one choice**: *how often a pet picks a target*. What is **not** learned is *what* it picks, or anything about needs.
2. 🧠 **That one loop probably optimises the wrong thing.** The formula lowers the pick chance as walking succeeds more often, so a world where pets walk without being blocked makes them **idle more** (21 % now, clamp 15..85). "Retarget sooner when blocked" is sensible; "be lazier when things go well" is probably not what anyone wants. It is a single constant pair (`45`, `60`) that `dec_op.c`'s own header says should become `POLICY` rows, so the fix is data, not a redesign.
Also stale in the brief (2.5): "nothing generates yet" for the asteroid pipeline. The step *pictures → model* now exists and is gated (`views_to_voxel`, see section 6); what does not exist is any provider call.

## A1. Where could a pet or actor propose a plan that the existing gate scores?
* **Smallest hook (📖):** `ai_step` is a thin wrapper around `ai_step_inner` (`PET_SECTION 35`): it records the pet's before/after state and writes **at most one** `DEC` row (`pick`, `walk`, `door`, or `walk`+`blocked`). The place a *choice* is actually made is `pick_target()` (`petslike_core.c:729`), called at `PET_SECTION 37` when `rand() % 100 < dec_pick_pct()`.
* **Verb / page / state files (📖):** verbs `review` (writes `state/review.txt`) and `decpct` (prints the current chance); state `state/decisions.txt` (ledger), `state/bank/*` (digest); the digest op is `&.widgits/_shared-lib/ops/dec_op.c` (`bank`, `review`, `weight`).
* **What "plan" and "gate" mean today (📖):** a *gate* is a phase table run by a pal harness in a scratch copy (`harness/play/phases/*.phases.pdl`, e.g. `play_rpg_space_planet_map`); it scores a **whole run**, not a plan. The roadmap itself says thread B (plan-proposing pets) "needs a snapshot judge", which does not exist.
* 📝 **Proposal, smallest first:** inside `pick_target`, generate **k = 3 candidate targets** (the three existing branches already are candidates: approach a pet, take a door, random cell), score each with a **pure function** (distance, door, recent `blocked` rate per cell from the ledger), pick by the weights, and record all three plus the choice in a sibling ledger (A6). No new gate is needed for step one; the test is the replay harness (A6). A *plan* in the sense of several steps comes later and needs the snapshot judge.

## A2. Which pet behaviours are rows today, which are C?
| Rows / files (editable, no rebuild) | Hard-coded in C |
|---|---|
| 📖 `life.pdl` `LIFE` rows: `enabled`, `hunger_ms` 60000, `starve_hp` 1, `drop_pct` 50, `death_mode`, `respawn_room/x/y`, `take_range` | 📖 the **branch odds** in `pick_target`: ~32 % approach another pet (if more than one), then up to 47 % an auto-walk door, else a random passable cell (`petslike_core.c:729-735`) |
| 📖 `life.pdl` `ACTOR` rows: id, position file, max hp (harold 450, pets 20) | 📖 the pick formula constants **45 / 60 / clamp 15..85** (`dec_op.c`; its header says "not yet a .pdl policy") |
| 📖 `state/life_weights.txt` `hunger_rate_pm` (per-mille, **clamped 100..4000 in `life_op.c`**): the one existing hook for an evolved need rate | 📖 the death/respawn *effects* when no event page exists (C fallback; pages `events/death`, `events/respawn` override) |
| 📖 `tuning.pdl` `TUNE` rows: `step_min_ms`, `step_jitter_ms` (how often a pet steps on its own) and many CPU-safety timers | 📖 the **decision rows themselves** (the `dec_clean` truncation to 40 chars and the 8-field shape) |
| 📖 `state/bank/dec_*_pick_pct.txt` and `*.behavior.pdl` (measured, "not hand-authored") | |
| 📖 `pals.pdl` for rpg-space is **empty** on purpose (comment: "none for rpg-space"); the party comes from `party.pdl` | |
**The finding that matters most for the AI side (▶️ grep):** `hunger` appears **0 times** in `petslike_core.c` (the file that chooses targets) and only in the life layer of `rpg_space.c`. **No pet decision reads a need.** Hunger rises, hp drains, the pet dies and respawns, and nothing in `pick_target` ever steers toward food. Needs and choices are two separate systems that do not touch. That is the real "single highest-value next step": connect them (A3).

## A3. What would it take for a pet's choices to be weighted rows a check can refuse?
Two walls, as the house rule "laws beat weights" asks:
1. **A runtime clamp (law, in code):** already the pattern: `hunger_rate_pm` is clamped 100..4000 in `life_op`; `pick_pct` clamped 15..85 in `dec_op`. A floor like "a pet may never ignore hunger" is the same kind of line.
2. **An edit check (refuses a bad weight file before it lands):** 📝 `weight_check <weights_file> <law_file>`, an op in the style of `concept_edit_validate` (📖 exists in the concept bank): law rows such as `LAW | seek_food | min_pm | 300 | when hunger >= 60`. A candidate edit that sets the seek-food weight below its floor is **refused with the rule named**, logged, and the old file kept.
Needed first (📝): a `seek_food` *candidate* in `pick_target` (nothing to weight yet; see A2), reading `hunger` from `life_<id>.txt`; then rows `POLICY | scope | need_floor.seek_food | 300` on the scope chain (engine → game → world → kind → entity; caps flow down). **Test that shows it is wrong:** (a) mutate the clamp out and the harness's starve scenario must fail (a hungry pet in a room with food starves); (b) a weight edit below the floor must be refused; (c) with the floor honoured the pet reaches food before hp < 50 % within N ticks, measured from the ledger.

## A4. How does a player see a pet's readable "why", from data?
📖 Verbs already return a text line to the UI (`snprintf(msg, cap, …)`, e.g. `decpct`), and `state/chat.txt` is the chat surface. 📝 A `why <pet>` verb reads the **last row of the why-ledger** (A6) for that pet and returns one line: `ember: picked cell 4,9 (door branch, 3 candidates, walk weight 0.90, blocked there 0 of last 8)`. It needs a verb row in `verbs.pdl`/the C verb table and **no renderer change**, since the line goes through the message path every verb uses. The same row feeds the daily `review.txt`.

## A5. Can the asteroid pipeline reuse draft → describe → dual-yes → bank row?
Yes. Describe checks for a voxel model, by what exists:
* ▶️ **built and run** (`views_to_voxel`, harness 60/0): the three source pictures parse; **consistency = 1.0** (the views describe one solid thing, otherwise exit 4 and no file); dimensions fit the engine slot (**`fits_engine`**: X ≤ 32, Y ≤ 32, Z ≤ 16, matching `bv_gpu_raymarch.h`); voxel count ≤ `--max-vox`; non-empty hull.
* 📝 **not built:** **connectivity** (one 6-connected component; a free model's text grid can pass consistency and still be dust, 🧠 the most valuable missing check, one flood fill), palette budget, a source hash for replay.
* **Dual-yes:** owner AND Claude, and the proposer cannot promote its own (img-hai's two-judge precedent). **Bank row (📝):** `ASTEROID | name | model=<path> | source=<hash> | consistency=<n> | verdicts=<owner>,<claude>`; the existing seam that loads `asteroids/<name>/model.txt` is `rpg_space.c` ~line 929 (📖 I read its header and parse loop, not its range checks end to end). Full pipeline: `TECH-TREES-AND-RENDER-RESEARCH-PIPELINE-DESIGN.md`.

## A6. What should be logged per decision so it can be replayed?
**Why today's row is not replayable (📖):** `DEC|<unix>|<stamp>|<actor>|<state>|<action>|<target>|<outcome>` records *what happened* but not *why*: no candidate set, no weights used, and the choice uses `rand()` seeded once per daemon start from `time(NULL) ^ getpid()` (📖 `rpg_space.c:331`), so every run differs and the same inputs never reproduce. The doc itself says `why`/`score` columns are "not covered yet".
📝 **Proposal: a sibling ledger, not wider `DEC` rows** (readers such as `dec_op` expect 8 fields; widening would break them). One row per decision, same append-only `O_APPEND` write, same size-growth cursor:
```
WHY|<unix>|<actor>|<action>|rng=<seed>:<draw>|cands=<n>|pick=<i>|w=<name>:<per-mille>;<name>:<per-mille>|rule=<id>|in=<hunger>,<hp>,<room>
WHY|1793162499|ember|pick|rng=7731:12|cands=3|pick=1|w=walk:900;door:500|rule=pick_target.v1|in=22,20,bridge
```
* `rng=<seed>:<draw>` makes the draw reproducible (seed once per actor from the world seed + actor id; log the draw index). `w=` is the weights **as used** (copied from the bank at decision time, so a later weight change does not rewrite history). `rule=` names the code version of the scoring function. `in=` is the few inputs the rule read.
* **Replay harness (pal):** run the same ledger + seed in a scratch copy and require **identical target sequence**; mutate (change one weight, or the seed) and require a difference; this is the "same seed, same bytes" claim made testable for pets. Cost: one more append per decision (~100 bytes; the `DEC` write is the existing hot path), idle cost unchanged.

## A7. Which verbs map cleanly to voice, which need a focus concept first?
📖 from the verbs I read in the board-driving skill and `rpg_space.c`: **clean (no target argument):** `helm yes|no|direct`, `int` (a toggle: read the flag first), `review`, `decpct`, `life status`, `hold status`, `collision`, `mine here` (key `m`). **Need a focus first** (they take an id, a room or cells): `life hurt|give|take|die|respawn <id>`, `mine <kind> <room> <x> <y>`, `destroy …`, `setpos <room> <x> <y>`. 📝 The focus concept is the **selector** (the xelector the arrows already move): "select next/previous pet", then a verb that defaults its `<id>` / cell to the selection. 🧠 Build the "selection → default argument" rule once in the verb dispatcher, not per verb.

## A8. What does possession mean mechanically, and where is the swap recorded?
📖 Today: the controlled character is `ctl_idx()` (`petslike_core.c:382`: `player` → hero index −1, else the `active` pet); `end_turn` skips `ai_step` for the controlled one (`!(in && i == c)`, `PET_SECTION 39`); the `flag_ctl.txt` / `flag_int.txt` files carry the state; arrows move the controlled character. So mechanically possession is **already "AI suppressed for one actor + arrows routed to it"**; there is no record of who took over or when beyond `ctl_trace.txt` (a debug trace).
📝 Define it as three separable parts and record each: (1) **AI suppression** (exists); (2) **command-set swap** (which `keybinds.pdl` / verbs apply to the possessed actor, data); (3) **camera follow** (optional, `camera_mode 2` anchors on the selector, a separate setting). Record in the game ledger `state/events.txt`: `POSSESS|<unix>|<actor>|by=<player|agent:id>|mode=<…>` and `RELEASE|…`. In `DEC`/`WHY` rows set `state=possessed` and `by=`; the learner then **excludes human-driven moves from the pet's weights** (the ledger doc already says a person's input is not an actor decision) but **includes agent-driven ones as that agent's decisions**.

## 6. What I changed in other documents because of this
* Roadmap `ROADMAP-2026-10-27-GAMES-STORE-CHAIN-AI.md`: one update line (the two corrections, the A2 finding, the A6 sibling-ledger proposal, new build-order items).
* The terumon design (`TERUMON-…`) now says form changes append `FORM` rows **and** feed the same `WHY` ledger once it exists, so a shapeshift choice is explainable like a walk.
* Not edited: the owner's copy of the companion brief and the original handoff.

## 7. New items for the build order (cheapest and most central first)
1. **Connect needs to choice:** `seek_food` candidate + `hunger` input in `pick_target` (A2/A3), harness: a starve scenario that fails without it. 2. **`WHY` sibling ledger + seeded rng + replay harness** (A6). 3. **`POLICY` rows for the pick formula** (45 / 60 / 15..85) and a look at the "lazier when it goes well" direction (section 0.2). 4. **`weight_check`** (A3). 5. **`why <pet>` verb** (A4). 6. **Connectivity check** for generated models (A5). 7. **Possession rows** (A8).
Each needs its own harness with a mutation that makes it fail (the rule this session reinforced: a check I never saw fail is not a check; see the `EXPECT_OUT_HAS` finding in the roadmap).

## 8. Not verified
* Nothing here was **run live**: I did not drive the board. The ledger and bank numbers (5532 rows, 4691/516, weight 0.9007, pick chance 21) are from the files on disk today and the arithmetic matches the formula, but I did not watch the game consume them.
* `hunger` grep: 0 hits in `petslike_core.c`; I did not read every line of the core, so "no pet decision reads a need" means *no textual reference*, which I think is conclusive but is a search result, not a trace.
* I read the first lines of `dec_op.c`, `life_op.c`, the ledger doc and parts of `rpg_space.c`/`petslike_core.c`; the verb list in A7 is from the skill and the sections I read, not from `verbs.pdl` (which lists only the zoom verbs).
* Proposals A1, A3, A4, A6, A8 are designs; no code was written for them today.

## 9. Blocks B, C, D (not answered here, and what each needs)
* **B (RPG Maker gap table, three example events):** needs a careful read of the events engine (`events/` folders, `EVENT-VERB-PAGES-DESIGN.md`, the handoff's section 2); good for Grok, who was asked to focus on events. 🧠 Habit most harmful here: hidden state in a plugin or a switch that lives only in memory.
* **C (settings menu, first 10 minutes, accessibility):** a design pass plus a driven walk-through; the menu part is natural once `tuning.pdl` generates its own settings window (roadmap item).
* **D (play-test):** needs a launched board, screenshots and the owner's machine not otherwise busy; the board-driving skill is the how-to.
* **Section 7 of the brief (fixes to the handoff: merged order, gate counts, not-verified-live table):** a read-through of the 204-line handoff; I have not done it.
**End:** (1) RAN: arithmetic and file counts above, grep counts; (2) READ: the code and docs named; (3) JUDGEMENT: section 0.2, A3's test, A5's connectivity priority, A7's rule; (4) single next smallest step: **add the `seek_food` candidate and its starve-scenario harness** (it is the first thing that makes a need change a choice).
