# Game mechanics as trainable events

2026-11-06. Second pass after `SELF-CODING-HARNESS-PLANS-2026-11-06.md` and `WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md`. This page is how each mechanic is implemented so it can be trained and can evolve. Decision D47. Not built on this pass.

The rule from the sprint still holds (`GAME-MECHANICS-SPRINT.md`): one ledger, one verb list, a screen appends a row.

```
EVENT | time | actor | verb | target | at | result
```

What this pass adds is two files beside that row, not a second ledger.

- **Concepts.** A verb page carries `KEYWORDS` and slot weights (what the verb is about). The material gate may fill `target` when the verb is about a stuff: wall, door, crop, molecule. Aboutness changes only by a candidate edit a person promotes.
- **Trainable.** Each page has `COUNTS` and `WEIGHT = (reward+1)/(reward+punish+2)`, written by `harness_bank_op` from PASS and FAIL. A new attempt is another phase row or another page. `find` ranks them. It does not run the winner (D45). Evolution is a new row that outranks an old row, then a person promotes it. It is not a mutation of the C verb.

A mechanic is done for this pass when three things exist: the verb in `verbs.pdl` with `page=yes`, a harness phase that can fail, and a behavior-bank seed whose keywords a `find` can hit. The C verb remains the rule that writes the ledger. The page is what the UI fires (`EVENT-VERB-PAGES-DESIGN.md`).

## Shared shape for every mechanic

1. Manifest row: `verb`, `page=yes`, the concepts it is about.
2. Generated page: `exec` the verb op, then halt. Hand edits are kept. `verbgen` does not overwrite them.
3. Seed `bank/<game>__<verb>.behavior.pdl`: keywords, slots, weight 0.5 before any run.
4. Phase table: one attempt. Scratch house. Expect the ledger row and the result row.
5. After the run, the bank counts. A second attempt with different concepts competes only through `find`.

Local learning stays the tools already named. Look-table means for stuff. Laplace for "did this page work". Gemma may describe a candidate keyword row. It does not set the weight. No new FSM. No diffusion in the verb.

## Mechanics, and the concepts each one trains

| Mechanic | Games | Verb | Concepts on the page | What a failed attempt proves | How it evolves |
|---|---|---|---|---|---|
| Move | all | `walk`, `wait` | place, cost, blocked | the actor did not change cell, or changed when the cell was wall | a new phase with a different stand tile, not a new mover |
| Needs | df, cdda, centroid-live | `eat`, `drink`, `sleep` | hunger, thirst, fatigue, food item | the need number did not fall, or fell with an empty stock | keyword `hunger` must rank `eat` above `dig` |
| Gather | df, cdda, mcl, centroid pets | `take`, `dig`, `haul` | stone, wood, food, element | stock count unchanged, or the wrong item | a phase that gathers H then O before a water molecule |
| Craft | df, cdda, mcl | `craft` | recipe id, tool, skill, inputs | output missing, or inputs still in stock | two recipe rows. `find craft door` ranks the door recipe first. Already observed once for bandage versus scavenge |
| Build | df, centroid | `build`, part `apply` | material from the gate, part name | cells lack the wall, the door event, or the bridge air gap | a new part row (stair, tower) after its harness is green |
| Body | df, cdda | `strike`, `wear` | part, wound, armor | the named part has no wound row, or wear did not change the hit | phrases for coat and hide, later, through the look table. The wound row stays the authority |
| Map | df, cdda | `gen_map`, `embark` | biome words, z-column | the chunk has no stone under soil, or the house chunk has no door | a candidate map row. The player does not hand a second generator |
| Molecules | centroid, chem palette | `craft` plus Plan 5 | formula, element ids 1..118 | contact list wrong, or colour far from that element's Drude colour | a new placement proposal. Formula is fixed until a person edits it |
| Talk and trade | mcl, rpg | `say`, greet pages | who, topic | the page did not append a result row | keyword tune only. Do not put the material gate on a greeting |
| Hostile | df, cdda | `spawn_actor`, `strike` | hostile, sound | no actor, or the actor did not step | a weight on the spawn page. Schedules and factions stay later, as the sprint already says |

Centroid-test does not run the needs loop. The campus is prebuilt (D40). Centroid-live is where `eat`, `drink`, and `sleep` join the same verbs. The skin toggle is not a concept weight. It stays the backslash copy of a sheet.

## What "evolvable" is not allowed to mean

- A page does not rewrite `centroid_parts_op` or the renderer.
- A higher weight does not auto-fire the page in the live house. A person, or a phase table a person shipped, chooses the attempt. Rank is a suggestion.
- Slot weights do not move because a test failed. Failure is the occasion for a person to propose an aboutness edit (`HARNESS-BEHAVIOR-BANK-DESIGN.md`). Reliability moves on its own.
- Element ids, the autotile table, and the camera are not evolvable rows.

## Order for the implementation pass

1. Pick one verb that already has a C body and no page: `craft` on the game that already ranks bandage. Add the manifest row, the page, the seed keywords, and a phase that fails if the output item is absent.
2. The same shape for centroid `build` of a door, scratch desk only, gate words `wood door`, expect a door event and `TOP2 timber`.
3. One need: `eat` lowers hunger in scratch, and `find hunger` ranks `eat`.
4. One molecule: water, from Plan 5, as a `craft` whose result is three cells. No pet yet.
5. Only then a second attempt at the same verb, so `find` has two rows and the worse one sorts lower.

Stop if a mechanic needs a new ledger or a new neural runtime. It is the same verb with worse data, and the fix is a row.

## External references

- This file. D47 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`.
- Verb list and loops: `16.game/GAME-MECHANICS-SPRINT.md`.
- Pages: `16.game/EVENT-VERB-PAGES-DESIGN.md`.
- Learner contract: `16.game/SELF-CODING-HARNESS-PLANS-2026-11-06.md`.
- Where weights already sit: `16.game/WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md`.
- Playthrough bank: `06-testing/GAME-PLAYTHROUGH-HARNESSES.md`.
- Owner copy: `/home/no/Desktop/github/work/XO/31.Nov/nov-6/0.game-mechanics-trainable-events-2026-11-06.md`.
