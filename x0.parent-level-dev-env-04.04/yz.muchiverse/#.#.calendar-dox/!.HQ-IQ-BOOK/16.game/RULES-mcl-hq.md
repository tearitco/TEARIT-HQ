# mcl-hq rules (sprint)

Hook: `GE_RULE_HOOK` in `ops/mcl_hq.c`. Paint stays the room paint. Do not call `pc_generate_chunk.c`.

Blocks that may be `dig` (break) or `build` (place): `grass dirt stone cobble planks log`. Anything else returns `unknown`.

| verb | who | cost | result text | file |
| --- | --- | --- | --- | --- |
| give_item | the player | none | `ok` | `game_events.c` |
| take | the player | none | `ok` or `none` | `game_events.c` |
| walk | the player, one cell | none for this slice | `todo:walk` until it calls `do_step` | `ops/mcl_hq.c` |
| dig | the player, target is a block id | none | `ok` and `give_item` of that id, or `unknown` | this file |
| build | the player, target is a block id they hold | one `take` of that id | `ok` or `none` or `unknown` | this file |
| craft | the player at a table for 3×3, inventory for 2×2 | none | `ok` or `missing` | table below |

Gravity is **later**. Chest contents are **later** (item rows at `at`, not a new screen). Mobs are **later**.

Craft shape. New file `@.apps/mcl-hq/recipes.pdl`. Grid is nine tokens, `.` for empty, row-major, no spaces. 2×2 uses the top-left four and the rest `.`.

```
RECIPE | id | grid | output | count
RECIPE | planks | log........ | planks | 4
```

`log........` is log in the first cell and eight empties. One example is enough. The hook matches the grid the verb's target names (`craft` target is the recipe id).
