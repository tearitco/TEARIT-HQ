# Companion to the Grok handoff: THE AI SIDE (2026-10-27)

Companion to `HANDOFF-2026-10-27-RPG-SPACE-TO-GROK.md`. A reviewer pointed out, correctly, that the handoff says nothing about the AI
side (weights, word/concept bank, attrition, judge, tomom, terumon). That is a real hole: Grok would build mechanics without knowing
the AI harness exists and would flatten it into ordinary game code. This page fixes that. **Read it before the handoff's work order.**

Every claim below says where it comes from. "Read" = I opened it this session. "Observed" = I ran a command and saw it. "Not checked" =
I did not verify it; do not rely on it.

## 0. Sources, and how much of each I actually read

| Source | How much I read |
|---|---|
| `17.ai/HARNESS-AS-GAME-BRAIN.md` (94 lines) | all |
| `17.ai/00-INDEX.md` | all |
| `#.#.calendar-dox/1-1.HARNECIENT.SMOL/NIGHT_30_THE_ATTRITION_MODEL.txt` (the class script, 16.9 KB) | about the first 140 lines, which is most of it |
| `08-roadmap/design-docs/robot-chat/ROBOT-CHAT-BLUEPRINT.md` (317 lines) | the first 45 lines only |
| the rpg-space and core code (pets, life, party/voices/pals data, decision-ledger references) | grep and short reads, observed |
| `17.ai/AI.md`, `AUTO-AND-AUTO-STUDIO-DESIGN.md`, `16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md`, `TERUMON-SPEC.md`, `HARNESS-BEHAVIOR-BANK-DESIGN.md`, `LLMUD-HACK.md`, the tomom and concept-bank source | **not read** |

Vocabulary, as the sources use it: the **attrition model** is the house name for the whole effort of wearing down the need for an outside
API (NIGHT 30). **Maxine, Tomo and Rahweh** are the three voices of the NIGHT class scripts (characters, not programs). **tomom** is the
hand-built local model, dormant (`#.Z.HUMAN_LLM/3.stage.llm.tomom...`, UI in `&.hq-apps/tomom-hq`). **terumon** are the creature pals
(`party.pdl` art ids `terumon_001_ember`...; spec `TERUMON-SPEC.md`, not read). **TEARIT** is the describe-then-decide learning loop.

## 1. What the AI side is (NIGHT 30 + HARNESS-AS-GAME-BRAIN, read)

Four stacked pieces: **TEARIT** (the loop), the **Concept Bank** (weighted nodes the loop reads and writes), **tomom** (local model, waits for
something worth learning from), and **FSM/GOAP** (what actors use to choose). The loop:

1. **Watch**: something happens (an actor acts, a state changes, an owner gives thumbs up or down); it is written as a plain record. No interpretation.
2. **DESCRIBE** (the house law: *describe, never classify*): a small model (Gemma, on the LAN Mac per `#.desktop/ai_backend.pdl`; never the
   local Ollama) writes ONE line in a fixed shape. It may not output a decision token, set a number or flip a switch. Free-form output was
   tested and invented a fact (a thirst level) that was never in the input; the fixed shape did not.
3. **Code, not a model**, turns the line into a structured edit record (target, slot, proposed delta, the model's reasoning kept as text for the
   audit trail). **A fixed lookup table picks the number.**
4. **Validator**: deterministic, small, dumb on purpose; real target and slot, number in a safe range; rejects the whole record on any fault, never a partial apply.
5. A validated edit **lands in a review file; a human reads it. There is no automatic promotion.** NIGHT 30 calls that gap the real ceiling on how
   fast the house learns, and says do not close it until more edit types exist and real review entries have piled up.
6. Only **one of four edit types is built** (a weight nudge on an existing concept). Inventing a concept node, describing a state-machine
   transition and describing a planner action are named but not built.
7. **Outside models are workers, not the runtime** (free providers only; the house has no paid credits). They produce demonstrations. Reading a
   file or listing a directory is automatic; writing, editing or running a command needs a human's explicit approval, every time.
8. Honest limit (NIGHT 30): the models' tool use is mechanically reliable; their reasoning quality has had one real graded test (two questions, one model).

**Why it matters for Grok:** mechanics must stay **auditable event bricks, verbs and ledger rows**, not hidden C logic, because that is what the loop can
read, replay and judge. A feature that cannot be expressed as "a verb, a check, a ledger row" cannot be learned about, scored or refused.

## 2. The judge IS the gate you have been extending (read, then observed)

HARNESS-AS-GAME-BRAIN describes exactly the machinery the rpg-space gate uses: an actor proposes a **phase table** (data); `playthrough_op compile`
makes a case file; `harness_case_op` plays it in a **scratch copy of the game** with the real binary; checks (`HAS`, `COUNT`, `SAY`...) give
PASS/FAIL; a RESULT ledger row is written; `harness_bank_op update` keeps a per-phase **weight = (reward + 1) / (reward + punish + 2)**;
`harness_bank_op find play/bank <words>` ranks known phases for a goal in words. A phase must state an **expectation**; an actor that cannot
state what it predicts cannot be judged (the runner refuses). So **`rpg_space_planet_map.phases.pdl` (83 checks, 7 phases) is a phase table in the
AI format, and `p7_worlds` is already a worked example of "a generated thing is judged in a scratch game".**

What is built vs design (HARNESS-AS-GAME-BRAIN section 3 and 4, read):

| Built | Design only |
|---|---|
| phase tables, runner, RESULT ledger, bank weights, `find` | **a caller**: no pet, tomom or `auto` calls `playthrough_op run` today |
| decision ledger `state/decisions.txt` (`DEC|unix|stamp|actor|state|action|target|outcome`) in df-hq, cdda-hq, mcl-hq, with a daily review verb | **promotion** (weight -> concept-bank entry / tomom example / a pet's default plan) |
| | a GOAP planner that reads weights (`ai_goap_plan` is named, not in the registry) |
| | judging against the LIVE state (the harness plays a fresh scratch game; a snapshot op exists but is not wired) |

## 3. Observed: what rpg-space's pets actually do today

* Pet movement is **C with `rand()`** in the shared core (`pick_target`, `petslike_core.c`, read): a roll of 0-99; under 32 walk toward another pet (32%); under 47
  walk to an auto-walk door when one is in the room (15%, otherwise it falls through); else a random free passable cell (the rest, about 53%); step timing from `tuning.pdl` rows (`step_min_ms`, `step_jitter_ms`). Chat lines are templated in C (`rand() % 4`), spoken by per-pet voices.
* **No weights, no learning, no decision ledger.** `grep decisions.txt`: 0 hits in `rpg_space.c`, 0 in `petslike_core.c`, 4 in `df_hq.c`.
* **`pals.pdl` (3 lines) and `voices.pdl` (7 lines) hold no rows** (headers only). The earlier statement that they are "listed without explanation" is
  too kind: they are effectively empty. `party.pdl` has the four PET rows (Ember, Glacine, Murmur, Solvent; terumon art ids, spawn cells).
* Life is `life.pdl` rows (rates, mode, respawn place) plus C; it is deterministic and does not learn.
* Nothing in the pets calls a model.

## 4. A hazard I found while checking: the bank weights for our gates are polluted

`harness_bank_op update` runs at the end of every gate pal. Every development run I made (many of them failing while I iterated) counted.
Observed in `&.widgits/_shared-lib/harness/play/bank/live/`: **rpg-space `p4_voxels` reward=22 punish=7 (weight 0.7419), `p5_pilot` 12/3, `p6_life` 8/2,
`p7_worlds` 3/1**; 313 rpg-space ledger rows; 23 rpg-space / rpg-dust entries. Those punish counts are **my development noise, not evidence that a
pet's plan fails**. There is also a stale entry for a gate I retired (`rpg-dust__d1_life`). The bank and ledger are gitignored (local only). Before any
pet uses these weights: reset the rpg-* entries, and separate developer runs from AI-proposal runs (a flag or a different bank folder). I have **not**
deleted anything; it is the owner's bank.

## 5. The reviewer's questions, answered as far as the sources let me (ANSWER / OPEN / NOT CHECKED)

### Connect it to the AI and pets
* *Where could a pet propose a plan that the gate scores, and what is the smallest first caller?*
  **ANSWER (design, nothing built):** the table format and runner exist. Smallest caller: a hungry pet (hunger above a threshold in `state/life_<id>.txt`)
  writes `play/proposed/<pet>-<n>.phases.pdl` (a `GAME` row, a two-step phase such as the feed verb, an expectation that hunger fell), a small op runs
  `playthrough_op run` (fork/exec, niced, about 1.5 s quiet, up to 17 s under load 13 per the doc), reads `harness_bank_op find` weights, and appends a DEC row.
  **Prerequisite:** rpg-space and the core have no decision ledger; copy it from df-hq. **Limit** (doc section 4.5): it is judged on a fresh game, not the live state.
* *Which pet behaviours are rows and which are C? What would make a pet's choices weighted rows a check can refuse?*
  **ANSWER (section 3 above).** To make them rows: enumerate the choices as verbs (visit friend, walk to door, wander, feed), replace the `rand()` branch with
  a chooser that ranks them by bank weight, and let the harness expectations (and the validator) be what refuses a step. **OPEN:** whether that chooser is a core
  hook or a skin function; design it with the owner, and keep the model out of the number (lookup table, per the house law).
* *How would a pet's weights show as a readable "why" in a menu?* **OPEN, and a prerequisite is missing:** the DEC row has **no why or score column yet**
  (HARNESS-AS-GAME-BRAIN 4.2). Add one first. Then a menu line like "Ember visited a friend: tried 12, worked 9" is a read of `COUNTS` / `WEIGHT`.
* *Can the asteroid generate pipeline use the same promote-and-check loop?* **ANSWER (shape fits):** concept -> image -> 3D -> voxelise gives a candidate
  `asteroids/<name>/model.txt`; the **validator** is size caps, palette and no floating voxels; the **check in a scratch game** is a `p7_worlds`-style phase
  (the node loads, dim and voxel count are right, the footprint collides); the result goes to a **review file** and a person promotes it; the bank keeps the weight.
  **NOT CHECKED:** the reviewer's "dual-yes" and "bank row" terms: I did not find "dual-yes" in what I read; ask the owner what it means.

### RPG Maker expertise (Grok, be specific)
I verified only this much of our event system: event folders under `events/<name>/` with `event_pkg/pages/page_N/` (`condition.pdl`, `cmd_N.sh`, `event.pal`),
`area.pdl`, `choice.pdl`, triggers `action` and `parallel` (the helm, death and respawn pages), `hotspots.pdl` rows for Enter-next-to, and verbs the pages call.
**NOT CHECKED by me, so Grok should enumerate them from `EVENT-VERB-PAGES-DESIGN.md` and the pal/event code, not from my memory:** self-switches, variables, conditional
branches, common events, parallel-process limits, move routes, touch / autorun triggers. For each, say verb, page feature or data row, and whether it is built.
The three example events (a shop, a quest giver, a locked door) and "where does it break" are Grok's assignment; the toolbar has a Shop button, whether it is an event is **not checked**.
Blueprint tooling already exists (`&.hq-apps/blueprintz`, `bp_ctl`, `RPG-SPACE-BLUEPRINT-SYSTEMS.md`); a blueprint viewer should start from those, not from scratch.

### Gamification and menus
* *Settings surface for `bar_radius` and the scale-ladder radius, usable by voice and selector, not only mouse:*
  **ANSWER (grounded for keyboard):** khtpm windows number every clickable item (the `[ ]11.` badges) and the relay supports digit-jump and decimal keys, so a menu built on the
  generic renderer is already keyboard-selectable by number; the in-game selector is the xelector cursor. **NOT CHECKED:** any voice input (`voices.pdl` is pet speech OUTPUT).
  The TUNE text column is the help text. Do not add per-project renderer code (see `khtpm-house-standards`).
* *First ten-minute loop, where a new player gets lost:* **OPEN, needs a real play.** I have not played it as a new player; I only drove it as a tester.
* *Menus generated from concept links:* **OPEN.** The bank has `SLOT POINTS_TO=` links (HARNESS-AS-GAME-BRAIN); whether a creature / item / ship part menu can be generated from them is a design question for the concept-bank owner. Not checked.
* *Missing feedback signals (sound, flash, a "you did it" beat):* **OPEN.** Pets have per-pet voices; there is a sfx toy (`AUTO-TOYS-AND-AGENT-LEVERAGE.md`, not read). Grok should list what fires on each event and what does not.

### Make the play-test table stronger
* Before comparing eight games, launch and drive this one and attach evidence; mark what could not run. Practical notes from today: launch with `nice -n 15`, one board at a time,
  read `sh '&.hq-apps/proc-mon/guard.sh' status` first, drive through the relay or `bv_keys` (xdotool last), and **end a board by closing its window or one daemon pid** (the permission layer refused a sweep).
* Add rows for **onboarding, accessibility and moddability**, not only features.
* Add the **footprint** of every proposed feature measured with a browser open: read `state/tick_stats.txt` (`cpu_permille`; rpg-dust at rest is 18) before and after.

### The document itself
* The merged, current work order is in section 6 below; it replaces the lists in handoff sections 3b, 4 and 4b.
* The paragraph for Grok about what the AI side is and why it matters is section 1's last paragraph plus this rule: **do not make a pet, an event or a generator decide by a hidden branch;
  make it a verb with an expectation and a ledger row, so the loop can see it.**

## 6. One merged work order (done items struck through, with the commit that did them)

1. ~~Lift the generic life layer into a shared text-include; rpg-space and rpg-dust on~~ (11d37fd82, d98f08920).
2. ~~Offer the life layer to the other nine games, off by default~~ (abccd2783). **Still to do:** a gate and a live look for each before enabling any.
3. ~~rpg-dust = a copy of rpg-space, both gates equal~~ (6af4ca477). ~~Ambient world~~ (same). ~~Per-planet asteroids and the `gen:` seam~~ (da81812e4). ~~Pilot slice and minimap knobs~~ (827d3bbcc; minimap modes **never photographed**).
4. ~~CPU footprint fix and daemon cost file~~ (1d3924ebb, 5c8d3660b + correction).
5. **AI prerequisites (new, do these before any pet learns):** (a) reset the polluted rpg-* bank entries and separate dev runs from AI runs; (b) add the decision ledger to the core / rpg-space (from df-hq) with a why/score column; (c) the smallest caller of section 5.
6. Settings / db surface for `bar_radius`, the scale-ladder radius, minimap, `world_ambient`, asteroid budgets (keyboard-numbered menu).
7. Objects on every ship room's desk (today the Bridge only), or document why not.
8. The skiff's in-world console and boarding; crew mechanics.
9. The generate pipeline writer + validator + review file for `asteroids/<name>/model.txt` (`RPG-SPACE-USER-ASTEROIDS-DESIGN.md`).
10. Roll `sk_tick_stats` into the other daemons; drops / `take`, mining, build, hold for the other games.
11. The play-test report (section 5) with the stronger table.

## 7. Revised question block to paste into the handoff

1. Where can a pet propose a plan the existing gate scores, and what is the smallest first caller? (needs the decision ledger first)
2. Which pet behaviours are rows today and which are C (`rand()` in the core)? What would make them weighted rows a check can refuse?
3. What should the decision row carry so a menu can show a readable "why"?
4. Can the asteroid generator use the draft -> describe -> validate -> review -> bank loop, and what is the validator?
5. List the RPG Maker event features we lack (self-switches, variables, branches, common events, move routes, trigger types), each as a verb, a page feature or a data row, from the code, not from memory.
6. Build three events (shop, quest giver, locked door) from pages and existing verbs; say where each breaks.
7. Design the settings menu for the bar radius and scale radius (keyboard-numbered, TUNE text as help).
8. Play the first ten minutes as a new player; where do you get lost; what feedback is missing when an event fires?
9. Fill the comparison table with evidence, plus onboarding, accessibility, moddability and measured footprint rows.

## 8. What I did not verify

The unread sources in section 0; whether the decision ledger code in df-hq can be copied as is; that a pet caller fits in the time budget of a tick; any claim about
tomom's or the concept bank's internals; the minimap modes on a live board; the nine OFF life games beyond compiling.
