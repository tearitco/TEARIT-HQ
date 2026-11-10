# Onboard: skyland blueprint, the crew, and where this lane stops (2026-11-10)

**Decision D69.** One pet life, two kinds of verb. `world` is the skyland (mine, place, and open haul and farm). `table` is Teropoly (talk, offer, buy, build, and build is a house on a street). The rows are `data/crew.verbs.pdl`. `bind` refuses a table verb owned by the crew and a world verb owned by deskopoly. Add a life as a row. Do not add a C branch, and do not exec Teropoly to lay a stone. The owner copy is `XO/31.Nov/nov-10/grok/4.the-choice.md`.

Read this after `ONBOARD-GROK.md` and `HANDOFF-2026-11-10-DF-MINECRAFT-EVENTS-META-HARNESSES-AND-CENTROID.md`. Those two still hold the editor, the camera, and the older task list. This file is the lane that started the same day. Labels: **RAN** means a command in this note was run again on 2026-11-10 before the note was written. **READ** means the file was opened. **NOT DONE** means do not tell the owner it exists.

## Where the code is

Branch `grok`, commits `a51df7607` then `7261a1efc`. The worktree is

```
/home/no/Desktop/github/work/NNEST-12.00/.kilo/worktrees/grok
```

The shared checkout at the repo root stays on `claude`. Do not switch it. Commit from this worktree with `commit_scoped` and `--branch grok`. Do not push.

House root, quote it:

```
/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```

## What the owner asked for

The whole skyland gets built. A chapel by itself is not the product. It is acceptable for pets to build the rest df-style, if they use the same blueprint the harness uses. One pipeline:

```
skyland.blueprint.pdl → CELL columns → JOB rows → a harness crew
                      → the same JOB rows → df-style pets, in stages
```

A pet with its own idea of where stone goes is a second copy of the plan. Do not add one.

True Swords Clash is a different harness (a duel and Elo in `@.apps/TSC_ELO`). It is not this builder. The skill `tsots-game-map` says missions can use a blueprint viewer later. Do not start TSC unless the owner points at it.

## The files

| File | What it is |
|---|---|
| `@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl` | The plan. Islands, zones, water, `BUILD` masses, 16 waypoints. Not cells. **READ** |
| `@.apps/centroid-hq/ops/bp_op.c` | `check`, `list`, `gen`, `verify`, `break` |
| `@.apps/centroid-hq/ops/crew_op.c` | One pet. States idle, claim, place |
| `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md` | Reconciled 2026-11-10. Test = the job list. Live = df-style pets on that list. The old eat-and-sleep run is `centroid-needs` |
| `&.widgits/_shared-lib/ops/game_core.c` verb `gridpng` | Snake picture from the replayed grid. Not the editor window |

`bp_op` and `crew_op` are source. The `+x` binaries were not installed. A fresh agent compiles them with `gcc -O2` under `nice -n 15` (or `cpu_gate`) into `/tmp`, and does not drop them on the live desktop.

## What a fresh run just printed (RAN)

`bp_op check` on the real blueprint:

```
BLUEPRINT islands=12 buildings=12 waypoints=16 zones=11 scale=4 grid=72x52
```

`bp_op gen` of `chapel`:

```
PROJ build=chapel cells=73 jobs=73
```

`crew_op run` then `audit` on that projection:

```
CREW done placed=73
AUDIT placed=73 fsm=place
```

An extra `PLACE | p1 | 1 | 1 | 99 | stone` then `audit` exits 2: `place is not a job`.

Snake, from an earlier run the same day, not re-run for this note: `GRID cols=16 rows=12 len=3 food=12,2`. The PNG is `XO/31.Nov/nov-10/grok/1.snake-grid.png`. A lobby before `start` refuses `gridpng`.

## How the crew ticks

Copied from df-hq's dig loop in `@.apps/df-hq/ops/df_hq.c` (`job_pick`, then `CLAIM`, then the work). Here the work is `PLACE`, and the cell has a z. There is no walk, so the next job is the next row in the file.

Ledger rows the crew appends:

```
CLAIM | p1 | x | y | z | material
PLACE | p1 | x | y | z | material
FSM | tick | p1 | claim|place
```

The projection is the authority for what may be placed. The ledger is the authority for what the pet did. Regenerating the projection and replaying the ledger must agree. A hand-added place fails.

## What is not done (do not skip this)

- The other 11 `BUILD` rows are not generated. `gen <id>` is the same verb. Run it per building. Do not invent a second generator.
- Island undersides (the taper under each ellipse) are not cells yet.
- df-hq's live tick still only digs `DESIGNATE` rows in `jobs.txt`. That file is 2D and caps at 64 jobs. `place` in `verbs.pdl` is `item x y` with no z. The pets on the board do not read `crew_op`'s ledger yet. Wiring that tick to this ledger is the next code change.
- The harness case exists. Current line, commit `ab810b647`: `VERDICT|PASS|passed=42|failed=0`. An older note that says `passed=37`, or that no verdict exists, is the earlier run. Do not quote it.
- Chutes was not opened. Snake was not shown in the editor. No nested X server was started. Human play of the games is still undone.
- `SCALE | cell_blocks | 4` is recorded and not applied. `gen` emits one plan cell, not a 4 by 4 block stamp. Say so if you change it.
- About 30,000 columns for the whole land at that scale may be too heavy to draw at once. Walk the plan in stages. Do not load the volume into the owner's live board.

## Commands a new agent can re-run

From the worktree, with `HOUSE` set to the house root above:

```
gcc -O2 -o /tmp/bp_op   $HOUSE/@.apps/centroid-hq/ops/bp_op.c -lm
gcc -O2 -o /tmp/crew_op $HOUSE/@.apps/centroid-hq/ops/crew_op.c
/tmp/bp_op check  $HOUSE/@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl
/tmp/bp_op gen    $HOUSE/@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl chapel /tmp/chapel.pdl
/tmp/crew_op run   /tmp/chapel.pdl /tmp/crew-ledger.txt
/tmp/crew_op audit /tmp/chapel.pdl /tmp/crew-ledger.txt
```

`break` writes a mutant and `check` must exit 2. Kinds: `void` (waypoint 9 off the plateau), `close` (small island S1 onto the forest lobe), `zone` (a zone on S1).

## How agents talk

The room is Co-lab-h-ai, app `&.hq-apps/co-lab-hai/`. The contract is `&.hq-apps/co-lab-hai/onboard-co-lab.txt`. A message sits in the pending queue until the owner clicks Approve. Nothing reaches another agent before that. Agent id for this lane is `grok`. This note does not send a room message.

Older questions for Claude stay in `QUESTIONS-FOR-CLAUDE.md`. Add a new question there instead of a new channel.

## Next code, in order

1. The harness case is done (`passed=42`). Do not rebuild it to "get a verdict."
2. Point df-hq's pet tick at this ledger: claim, walk, place, and refuse a cell that is not a job. Keep the 2D dig designations working. A pet on a board is still not done.
3. `gen` the next `BUILD` id with the same verb. Then an island underside as another slice.
4. Only after two slices have used that loop, shape `mech_new_op` from it.

Do not start a trained policy, a vehicle, or TSC in this lane.
