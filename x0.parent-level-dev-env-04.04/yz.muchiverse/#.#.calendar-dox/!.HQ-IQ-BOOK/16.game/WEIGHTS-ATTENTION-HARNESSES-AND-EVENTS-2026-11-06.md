# Where concepts and weights already land, and the next hand tune

2026-11-06. The five campus look phrases train a material score. The rest of the game does not read that score yet. This page says what already has a weight, what can take a small hand tune now, and that an event page can call the gate.

## Already counting a weight

- A harness that ends in `harness_bank_op` stores reward and punish and writes `WEIGHT = (reward+1)/(reward+punish+2)` in `bank/live/<case>.behavior.pdl`. Playthrough phases do this per phase (`06-testing/GAME-PLAYTHROUGH-HARNESSES.md`). `harness_bank_op find` ranks. Decision D45: find does not run the winner.
- HORN versus HALO grades move concept-bank spokes (`08-roadmap/design-docs/HORN-HALO-IRL-IMPLEMENTATION.md`). That is chat, not the map.
- The material gate (`@.apps/generator-hq/ops/gate_op.c`) and the look table score a material. The five campus phrases (`look_assoc.campus.*`, harness `campus_look.pal`, 9/0 on 2026-11-06) name stone, roof_red, grass, timber, and stone. Raising wear on the wall moves the second pick from brick_brown to rust. The stamp, the door, and the bridge do not read `TOP2`.

## Worth a small hand tune now

- Harness seeds. The counted weight is already real. The hand tune is the keyword and slot rows in `bank/<case>.behavior.pdl`, so a find for craft keeps craft first. That is the mine, craft, and place loop getting concepts. No new learner.
- One event page. A page is already an `exec` of an op (`EVENT-VERB-PAGES-DESIGN.md`). A page can run `gate_op` and write the top material to a file. The useful page is a campus or generator event: these words, which material. Greet and shop dialogue stay fixed pages. The stamp still places the voxels. The page only names the skin.
- The next phrases, for parts you can see: stair, tower, plant, animal, a person. Same apply, train, and gate. The tenth phrase is not a new program.

## Not yet

Dialogue events, pet wandering, and a diffusion draft wait until one event has written a material name and a harness has failed when that name is wrong. The campus stamp does not read the gate. Wall slot 36 stays until a second rock is real. `0.good/` stays empty.

## Owner copy

`/home/no/Desktop/github/work/XO/31.Nov/nov-6/0.weights-harnesses-events-2026-11-06.md`
