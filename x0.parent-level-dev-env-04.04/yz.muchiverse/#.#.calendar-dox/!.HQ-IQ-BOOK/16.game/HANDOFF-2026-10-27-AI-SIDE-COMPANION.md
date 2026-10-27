# Companion to the Grok handoff: THE AI SIDE (2026-10-27, second edition)

Companion to `HANDOFF-2026-10-27-RPG-SPACE-TO-GROK.md`. First edition written after a review said the handoff was silent on the AI side; this edition is
written after reading the AI docs properly and after the owner's answers of 2026-10-27 (section 5). **Read this before the handoff's work order.**

Labels used throughout: **FACT** (read in a doc or observed in code/files this session), **PROPOSAL** (mine; not house doctrine; the owner decides),
**OPEN** (needs the owner or Grok), **NOT CHECKED**.

## 0. What changed since the first edition (corrections to what I told you before)

1. **The decision ledger is a shared text-include, not something to "copy from df-hq."** `&.widgits/_shared-lib/game_decisions.c` (`dec_append`, `dec_review`,
   `dec_bank_update`, `dec_weight`) is used by df-hq, cdda-hq and mcl-hq through an `ai_step` wrapper. It already closes the loop for ONE choice: a pet's
   "pick a new target" chance is `45 + (0.5 - weight("walk")) * 60`, clamped 15..85. (`DECISION-LEDGER-AND-DAILY-REVIEW.md`, read.)
2. **The weights I polluted are the harness's SCRATCH bank** (`harness/play/bank/live/`), not the Concept Bank, and they measure *reliability* ("does the phase pass"),
   which the design says is simply counted from ledger rows. My failing development runs still add noise to those counts (p4_voxels 22/7 etc.), but nothing
   about a pet's personality or the real Concept Bank was touched. The design keeps **two numbers** apart (section 2.3).
3. **Promotion is tighter than the 10-07 note said.** `^.hai-horn/ops/halo_chat_validate.c` now needs a policy line AND a ledger score >= 0.90 AND N >= 20 AND "promotion is a
   person's act"; a missing file, line or ledger entry queues for review (code read, lines 84-94 and 308). Default is queue. The `learning_limits.pdl` tiers
   (`preschool | elementary_hs | associate_bachelor | master_phd`) gate who may auto-promote.
4. **`ai_describe` IS a registered event command** (since 2026-09-28); `ai_fsm_transition` and `ai_goap_plan` are not (HARNESS-BEHAVIOR-BANK-DESIGN 4c, memory).
5. **"dual-yes" appears in no house document** (searched `#.#.calendar-dox`, `@.apps`, `^.hai-horn`; the only hit is my own first edition quoting a reviewer). Treat it as unknown.
6. The rest of the first edition's observations stand: pets move by `rand()` in `pick_target` (32% toward a pet, 15% auto door, else a random free cell), no decision ledger in rpg-space or the
   core, and `pals.pdl` / `voices.pdl` hold no rows.

## 1. Sources: what I read this time

Read in full or nearly: `17.ai/HARNESS-AS-GAME-BRAIN.md`, `17.ai/AI.md`, `17.ai/AI-BACKENDS-DISAMBIGUATION.md`, `17.ai/AUTO-AND-AUTO-STUDIO-DESIGN.md`,
`16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md`, `08-roadmap/design-docs/HARNESS-BEHAVIOR-BANK-DESIGN.md`, `DUSTOPIA-HACK.md` (about 200 of 322 lines),
`robot-chat/ROBOT-CHAT-BLUEPRINT.md` sections 1-4 (lines 1-45 and 183-262), `NIGHT_30_THE_ATTRITION_MODEL.txt` (most), `terumon-dev/TERUMON-SPEC.md` (first 60 of 431 lines),
`concept-bank/AUTO-PROMOTION-RULE.md` (first 25 lines), the memory notes on the attrition model, ecosystem soul, pet world, chain economy, delegation flywheel and entity learner.
**Not read:** `LLMUD-HACK.md`, `PALCRAFT-DESIGN.md`, the rest of TERUMON-SPEC, `A-TEARIT-IS-ALL-YOU-NEED.md`, `AUTO-TOYS-AND-AGENT-LEVERAGE.md`, `NIGHT_20/21/22/26/31`, the tomom and concept-bank source.

## 2. The AI side on one page (FACT)

### 2.1 The attrition model
The house's name for the whole local-AI effort, "a war of attrition against needing an outside API for everything." Four stacked pieces: **TEARIT** (describe-then-decide loop),
the **Concept Bank** (hub-and-spoke weighted nodes), **tomom** (hand-built local model, dormant), and the **decision layer** (FSM for the known, GOAP for the new).

### 2.2 The loop and its laws
Watch record (with owner valence) -> **Gemma DESCRIBES in one fixed line** (`TARGET: <existing node> | STRENGTH: high|medium|low | REASON: <phrase>`; never classifies, never sets a number) ->
**code** maps strength to a delta with a fixed table (high 0.15, medium 0.08, low 0.03) -> candidate **EDIT** (`spoke_weight_delta`, bounded to [-0.2, +0.2]) -> **validator** (real target and slot, in range;
rejects the whole record on any fault) -> **review file** -> a person promotes (or, for high-tier entities, a policy line + ledger score >= 0.90 + N >= 20) -> tomom eventually learns.
Only one of four edit types is built (a weight nudge). Free-form model output was tested and invented a fact; the fixed line did not. **Never** let a model output a decision token or a number.

### 2.3 Two numbers, never one doing both jobs (HARNESS-BEHAVIOR-BANK-DESIGN 4d)
* **Reliability** ("does this behaviour work"): counted from ledger rows, Laplace `(reward + 1) / (reward + punish + 2)`, a fresh entry 0.5, nobody edits it.
* **Aboutness** ("how much is this behaviour about concept X"): slot weights; they change **only** through a candidate EDIT -> validator -> human review.
A failure is therefore not a reason to lower an "about" weight; it lowers reliability.

### 2.4 Backends (AI-BACKENDS-DISAMBIGUATION)
(1) `gemma3:270m` on the LAN Mac (`#.desktop/ai_backend.pdl`, `10.0.0.144:11434`): stock, constrained DESCRIBE and free chat; **never the local Ollama**. (2) **tomom@qroq**: from-scratch C net, dormant, not wired.
(3) OpenRouter free models via open-hai: the only one with real tool use. Free providers only; the Chat-gemma button feeds none of the pipeline.

### 2.5 Built vs design
| Built | Design only |
|---|---|
| phase-table harness, scratch bank, RESULT ledger (`harness/play/`) | a **caller**: no pet, tomom or `auto` calls `playthrough_op run` |
| `game_decisions.c` ledger + review + bank update + one read-back (pick chance), in df/cdda/mcl | per-actor weights; a `why`/`score` column; more choices reading weights; a judge that checks a changed weight helped |
| `ai_describe` command, validator, promotion ledger op, halo/horn pipeline | **promotion** (undesigned on purpose until reviews accumulate), tomom learning, the three other edit types |
| `auto` A0-A3 on sfx-hai (34/34 proofs): `auto_ctl`, `auto_act`, `auto_observe`, `auto_plan`, manifests | A4-A8: tiers T1/T2 as choosers, studio scheduler, sub training, `auto-hq` window |
| entity learner (`entity_grade`: check/advance/use/rest/tick, harness 44/0), curriculum + skillbook rows | wiring it to a game's pets; the Grade Tick through the real clock |
| robot-chat: Gemma round trip, registered `ai_chat`, `robot_chat_001` pal | the live chat window, personality bank, template/delta propagation |
| Dustopia: nothing built (research/design) | the whole world-generation loop; Relation and Sentence banks |

## 3. What rpg-space and rpg-dust have today (FACT, observed)
* Pets: `rand()` wandering in `pick_target` (core); templated chat lines; per-pet voices. **No weights, no ledger, no model calls.** Life is `life.pdl` + C.
* The scripted gate (`rpg_space_planet_map.phases.pdl`, 83 checks, 7 phases) **is a phase table in the AI format**; `p7_worlds` is a worked example of judging generated content in a scratch game.
* Content generation today is deterministic C (`sk_world_switch`: per-planet asteroids from `hash(planet id, seed)`; `sk_nugget` shapes) plus the `gen:<name>` seam for externally made models. No model is involved; no confirmation step exists (and none is needed for deterministic seeds).

## 4. What the owner said on 2026-10-27 (the direction; RECORDED, not interpreted away)
1. **First AI thread:** "explore the possibility / data / fx and ui for all those options, both to use, and also to create games. This can easily be used as a **voxel game making engine** and will be by some."
   So: all four threads (pets learn from outcomes, pets as plan-proposing actors, the world/asteroid generation loop, entity skills and grades), each looked at as a **play feature AND a creator tool**.
2. **Spawn policy:** "needs based. What do you want / need in this codebase as ops / events that would make generating games / content faster, using fewer tokens, less work, with descriptive UIs." (My answer: sections 9 and 10.)
3. **Weight owner:** "all of the above; let's doc the most flexible framework for this, if none exists." (Section 8: a design exists in part, the robot-chat template/delta split; I extend it.)
4. **Dust vs space:** rpg-dust is the **AI / world-generation testbed**; rpg-space is the hand-authored playable game. (Section 11.)

## 5. Principles for Grok (do / do not)
* **Mechanics are verbs, checks and ledger rows**, never hidden C branches. If a feature cannot be stated as "a verb, an expectation, a row", the loop (and a creator) cannot see, judge or change it.
* **Describe, never classify. Models never output a number or a decision token.** Code and lookup tables decide; a validator refuses; a person promotes.
* **No silent world mutation.** Model-proposed content proposes first (section 9). **No auto-promotion** of weights or banks.
* **Never edit the grader that scores you** (ecosystem-soul rule): an actor may modify its own harness/plan, never the checks that judge it.
* **Data before code**: a new generator, policy, curriculum or event template is rows in a `.pdl` plus a gate phase.
* **Footprint**: every feature gets a measured cost with a browser open (`state/tick_stats.txt`, `cpu_permille`); heavy model calls go to the Mac, niced, budgeted. Free models only.
* **Keep dev runs out of the AI bank** (section 12); keep user data out of code branches (`xyzfs/users`); never push `user/*`.
* Every claim you make to the owner: a PNG, a state file or a ledger row. A clean compile is not evidence.

## 6. The four threads, each as USE and CREATE (the owner's "explore all of them")
| Thread | Player sees (use) | Creator gets (create) | Data | FX / UI | Smallest first slice (PROPOSAL) | Prerequisite |
|---|---|---|---|---|---|---|
| **A. Pets learn from outcomes** | a pet's habits drift; a "why" line (tried 12, worked 9) | pick which choices learn; per-pet / per-species / per-game scopes (section 8) | `state/decisions.txt`, `state/bank/dec_*`, scoped sidecars | a mood/emote beat when a habit changes; a why panel | hook `pick_target` (core) to `dec_append` + the existing read-back, for rpg-space | add `why`/`score`; per-actor key |
| **B. Pets as plan-proposing actors** | pets doing chores (feed, mine, ferry) and a visible plan queue with verdicts | author a behaviour as a phase table; test it in the scratch judge | `play/proposed/*.phases.pdl`, `play/bank` | ghost plan overlay; verdict badge | a hungry pet proposes "feed" (two phases), judged, then acts through a verb | a caller op; judge on a snapshot of live state (not wired) |
| **C. World / asteroid generation** | new asteroids and terrain appear, sometimes asking first | write a generator pack; see its preview and its budget | `asteroids/<name>/model.txt`, `worlds.pdl`, `policy.pdl` (section 9) | ghost preview + proposal rows in the bottom bar | `content propose/confirm/undo` for `gen:` asteroids | the pack validator (section 10) |
| **D. Skills and grades** | pets gain levels, MP, skills by exams | author a curriculum folder and exam | `curriculum.pdl`, `skillbook.pdl`, `entity_grade` | a level-up beat; a grade card | run `entity_grade` on one rpg-space pet | wire it to the game's clock; `approval=person` for tiers that may auto-promote |
All four share one spine: **Watch record -> describe (fixed line) -> code decides -> validate -> propose -> confirm/promote -> ledger.** Build the spine once; the threads differ only in the rows they put on it.

## 7. Guidance on order (PROPOSAL)
1. **A first** (smallest, already half built, shows in a menu), **C second** (the owner's stated product interest: generated content and a generate pipeline), **D third** (RPG-Maker feel, pieces exist), **B last** (needs a caller and live-state judging).
2. Do **not** start with a model. Start with ledger rows and weights from deterministic play; the model only describes, later, into the same rows.
3. Keep every slice **checkable by a gate phase** and **cheap at idle**.

## 8. PROPOSAL A: a scoped weight framework (the "most flexible" one the owner asked for)

**What exists:** the weight rule (Laplace per entry), the decision ledger with an `actor` column, per-game bank entries, and the **robot-chat template/delta split** (one shared TEMPLATE bank per species; each copy holds only a DELTA of slots it personalised; resolution = template unless the delta slot is non-empty; feedback lands in the delta, and only a multi-copy-convergent insight is promoted to the template). **What does not exist:** any scope other than per-game for reliability counts, and nothing joins the template/delta idea to the decision ledger.

**Design (extends, does not replace):**
* **Scopes, general to specific:** `corpus:<name>` (the concept bank's context axis: e.g. space-play, dust-play) -> `game:<app>` -> `kind:<species or terumon type>` -> `actor:<id>`. A world or user scope can sit above `game` later.
* **Reliability (counted) uses shrinkage along the chain.** Each scope keeps its own `reward` / `punish` from the same ledger (one cursor per scope, marker growth, never mtime):
  `w_scope = (reward_scope + K * w_parent) / (reward_scope + punish_scope + K)`. With `K = 2` and the root prior 0.5 this is **exactly the existing Laplace rule**; a scope with no data inherits its parent unchanged
  ("no evidence changes nothing", as `dec_weight` already returns 0.5). `K` is a `tuning.pdl` row (higher K = slower drift = steadier personalities).
* **Aboutness (slots) keeps the template/delta override** and the two-number law: an actor's delta slot changes only through candidate EDIT -> validator -> review; a template changes only when several deltas converge.
* **Storage in the house sidecar shape** (so `harness_bank_op find` works unchanged): `state/bank/dec_<game>_<action>.behavior.pdl` (game scope, exists), `state/bank/dec_<game>_<action>@kind_<k>.behavior.pdl`, `...@actor_<id>.behavior.pdl`.
  `dec_weight(bank_dir, game, action)` stays as the game-scope read; a new `dec_weight_for(chain, action)` walks the chain.
* **Ledger row v2** (PROPOSAL): keep the 8-field `DEC` row readable; add `DEC2|...|kind|why|score` as a new row type so old readers ignore it. `why` is a short code (`blocked-3-of-5`), `score` the weight used.
* **The readable "why" for a menu** is a read of the chain: "Ember: 9 of 12 walks worked (75%); species 62%; this game 58%."
* **Footprint:** one extra tiny file per (action, scope) updated on the existing 200-decision cadence; no per-tick cost.
* **OPEN for the owner:** the value of `K`; whether `kind` = terumon type; who may promote a template; whether a world/user scope is wanted now.

## 9. PROPOSAL B: spawn policy as data (the owner's "needs based")

The Dustopia doc's non-negotiable: model-proposed content that changes the world **proposes, shows what would spawn, waits for one confirmation**, until a specific pattern earns auto-execute by weight and observation count.
"Needs based" means that bar should depend on the **content kind and the proposer**, so make it data:
```
SPAWN | <content kind> | <proposer>   | <mode>        | min_n | min_weight | budget_per_hour | note
SPAWN | asteroid     | person        | auto_undo     | 0     | 0          | 30              | a creator's own asteroid appears at once, one click to remove
SPAWN | asteroid     | generator     | confirm       | 20    | 0.90       | 10              | earns auto after 20 confirmed at >= 0.90
SPAWN | asteroid     | model         | confirm       | 50    | 0.95       | 3               | a model's proposal: stricter
SPAWN | terrain      | model         | confirm_always| -     | -          | 2               | never auto: it changes gameplay
```
Modes: `auto`, `auto_undo`, `confirm`, `confirm_always`. The policy is read by one verb family (section 10, item 2); every proposal, confirm, refusal and undo is a ledger row, so the loop sees it.
**UI that needs no new renderer code:** a proposal is a **bottom-bar row** (the `PET_BAR_EXTRA` mechanism already lists clickable rows) reading `proposed: crystal x4 at 12,-3  [confirm] [no]`, plus a **ghost** voxel piece (the same `veh_N` piece with a distinct tint) so the player sees what would spawn. FACT: bar rows with click targets exist; NOT CHECKED: a translucent/tinted ghost in the engine.

## 10. PROPOSAL C: what I would want in this codebase (answering "what do you need")
Ranked by tokens and work saved, from what actually cost me effort in this session. Each item is a small op, event or row format, not a new subsystem.

1. **A content-pack format + one validator op** (`pack.pdl` rows: `ITEM | kind | id | model | tags | yield | budget`; per-kind limits as rows; `pack_check <pack>` prints PASS/FAIL and the first fault). Today the `gen:` model checks live inside `rpg_space.c`; a pack validator shared by the gate, the generator and the UI means an agent (or a generator) writes rows and runs one command.
2. **One generic verb family**, not a verb per feature: `content propose <kind> <pack-id>`, `content show`, `content confirm <id>`, `content undo <id>`, each writing a ledger row and reading `policy.pdl`. Events call it; creators never touch C.
3. **Generators as data** (`worlds.pdl` / `gen.pdl`): the per-planet asteroid count, kind weights and spacing are hard-coded in `sk_world_switch` today. Move them to rows (like `space.pdl`), so a new generator is rows plus a gate phase.
4. **A one-line judge command**: `judge <phase-table|pack>` = compile + scratch run + a single `PASS n/n` or `FAIL <label>: <seen>` line. The pieces exist (`playthrough_op run`); the missing part is the terse output and a mode that **does not feed the AI bank** (section 12). This is the biggest token saver for any agent iterating.
5. **A skin-sync / new-game op.** I copied rpg-space onto rpg-dust with an ad-hoc `sed` script, then hand-trimmed runtime folders; I generated the dust gate the same way. A `skin_port <from> <to>` op (name swap, exclusion list, data-vs-runtime rules, gate regeneration, compile check) turns "make a new game" into one command and is the backbone of the **voxel game-making engine** idea.
6. **A generated settings window** from any app's `tuning.pdl` (key, value, the TUNE text as help, validation from the default column), numbered for keyboard selection. Answers the owner's bar-radius request and every future knob with zero per-game UI code. Use the generic khtpm renderer (no new per-project C).
7. **The scoped bank library** (section 8) as a text-include `game_scoped_bank.c`, and `sk_tick_stats` lifted from `rpg_space.c` into the core so every game reports its own cost for free.
8. **Event page templates** (shop, quest giver, locked door, door-with-key) as parameterised row files plus a form in blueprintz, so a creator fills fields instead of authoring `cmd_N.sh`.
9. **A voxel interchange op** (`vox_conv`: `model.txt` <-> a common voxel format, palette quantiser, size/voxel caps, connectivity check). It is the receiving end of any image-to-3D generator and of user-made models.
10. **A snapshot judge**: wire `game_snapshot_op` into the harness so a pet's proposal is judged against a copy of the **live** state (HARNESS-AS-GAME-BRAIN 4.5 says it is the wrong test otherwise).
11. **Descriptive UI primitives**: the **why panel** (chain of counts from section 8), the **proposal row** and **ghost** (section 9), the **verdict badge**. All read ledger/state files; none needs a model.
For each: footprint = a read of a small file on demand or on a long cadence; none runs per tick.

## 11. rpg-dust as the AI / world-generation testbed: consequences (PROPOSAL)
* **Experiments land in dust first** (policies, packs, scoped banks, Dustopia-style generation, the scale ladder planets -> molecules), and graduate to rpg-space only when stable. That makes the dust gate the proving ground.
* **The sed-copy cannot stay.** Right now `rpg_dust.c` is `rpg_space.c` with names swapped (about 1,500 duplicated lines). The moment dust diverges, every shared fix must be applied twice. Extract the common skin into a text-include (`rpg_skin_common.c`) that both skins include, with `rpg_space.c` and `rpg_dust.c` shrinking to their own sections (house rule: pure logic with 2+ consumers = text-include). Do this **before** adding dust-only features.
* The scale ladder (`scale.pdl`, 10 levels) and Dustopia's "same loop at every zoom" are the same idea; Dustopia's lambda formalism is **imported and unverified** (DUSTOPIA-HACK section 4): do not cite it as implemented.
* Dustopia's own sequencing insight: build the **watch layer once**, generic over "what stream am I watching" (a player's exploration, a coder's actions, an API log), not once per game.

## 12. Hazard: dev runs counting in the scratch bank (observed, corrected)
Every gate pal ends with `harness_bank_op`, so my development runs, many failing while I iterated, counted: rpg-space `p4_voxels` reward 22 / punish 7, `p5_pilot` 12/3, `p6_life` 8/2, `p7_worlds` 3/1; 313 ledger rows; 23 rpg-* entries;
a stale `rpg-dust__d1_life` for a retired gate. The bank and ledger are gitignored (local). **These are reliability counts of the scratch bank, not personality data**, but they would mislead any pet that read them.
Fix before an AI reads them: reset the rpg-* entries; add a **dev mode** (an env flag or a separate bank folder) so developer runs do not count; keep AI-proposal runs in the real folder. I have not deleted anything (the bank is the owner's).

## 13. Merged work order (done items struck through, with the commit)
1. ~~Lift the life layer; rpg-space and rpg-dust on~~ (11d37fd82, d98f08920). ~~Offer to the other nine, off~~ (abccd2783); **still to do:** a gate and a live look per game before enabling.
2. ~~rpg-dust = copy of rpg-space, gates equal~~ (6af4ca477). ~~Ambient world~~. ~~Per-planet asteroids + `gen:` seam~~ (da81812e4). ~~Pilot slice, minimap knobs~~ (827d3bbcc; minimap modes **never photographed**). ~~CPU fix + daemon cost file~~ (1d3924ebb, 5c8d3660b + correction).
3. **Spine first:** (a) dev-mode for the judge and reset the polluted rpg-* bank entries; (b) hook `pick_target` to `dec_append` + the read-back in rpg-space; (c) `why`/`score` (DEC2) and the scoped bank (section 8).
4. **Extract the common skin** (section 11) so dust can diverge safely.
5. Content spine: the pack format + `pack_check`, the `content` verb family + `policy.pdl`, the proposal row + ghost (sections 9, 10).
6. Settings window generated from `tuning.pdl` (item 6); then the objects-on-every-room-desk question and the skiff console / boarding.
7. Thread D: run `entity_grade` on one rpg-space pet; thread B: the smallest caller after the snapshot judge exists.
8. The generate pipeline writer (image -> 3D -> voxelise -> `model.txt`) via `vox_conv` and `pack_check`, with the review file and a person's promote.
9. `sk_tick_stats` into the core; the play-test report with the stronger table (below).

## 14. Revised question block for Grok
1. Hook `pick_target` to the decision ledger in rpg-space and show a why panel; what does the DEC2 row need beyond `kind`, `why`, `score`?
2. Implement the scoped weight chain (section 8) as `game_scoped_bank.c`; prove with a gate that a scope with no data inherits its parent exactly and that K=2 at the root equals Laplace.
3. Design `pack.pdl` and `pack_check`; port the `gen:` asteroid checks into it.
4. Build `content propose/confirm/undo` and `policy.pdl`; show a proposal as a bottom-bar row with a ghost; log every step.
5. Extract the common skin so rpg-dust can diverge; keep both gates equal.
6. List the RPG Maker event features we lack (self-switches, variables, conditional branches, common events, parallel limits, move routes, trigger types) from the code, each as verb / page feature / data row.
7. Three events from pages and verbs only (shop, quest giver, locked door); say where each breaks. (FACT: pages have `condition.pdl`, `cmd_N.sh`, triggers `action` and `parallel`; the rest NOT CHECKED.)
8. Generate a settings window from `tuning.pdl` (keyboard-numbered, TUNE text as help); measure its footprint with a browser open.
9. Play the first ten minutes as a new player; where do you get lost; what feedback is missing when an event fires?
10. The comparison table (RPG Maker, Unreal, Minecraft, CDDA, Dwarf Fortress, GTA, Pokemon, Palworld) with evidence, plus rows for onboarding, accessibility, moddability, creator tooling, and measured footprint.

## 15. Open questions for the owner (not blocking the spine)
1. **"3D associated synonyms"** (HARNESS-BEHAVIOR-BANK-DESIGN 4b/5): undefined since 2026-10-07. Three coordinates per word, or three association axes (words / concepts / corpus)? Do not bake either in.
2. The value of `K` and who may promote a **template** (species) bank from convergent deltas.
3. `auto` / `auto-studio` (AUTO-AND-AUTO-STUDIO-DESIGN section 9): do pets get `auto` switches, and may the studio act while a human is at the machine?
4. Is a voxel **game-making** surface (creators making whole games on this engine) a product direction to design for now, or a long-term framing only? It changes how much of section 10 is urgent.
5. For user-created content: stored in the user's data branch (private) or shared packs in the house? (AGENTS.md: user data is not in code branches.)

## 16. What I still did not verify
The unread sources in section 1; that `game_decisions.c` text-includes cleanly into the petslike core (the df/cdda/mcl games have their own `ai_step`, the petslike pets use `pick_target`); an engine ghost/tint for pieces; the scoped-bank maths on real data; any claim about tomom's or the concept bank's internals; the minimap modes on a live board; the nine OFF life games beyond compiling.
