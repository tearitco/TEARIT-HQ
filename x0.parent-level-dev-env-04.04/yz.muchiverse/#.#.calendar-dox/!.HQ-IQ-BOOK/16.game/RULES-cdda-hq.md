# cdda-hq rules (sprint)

Hook: `GE_RULE_HOOK` in `ops/cdda_hq.c`. No recipe file exists yet. Do not import CDDA json for this slice.

| verb | who | cost | result text | file |
| --- | --- | --- | --- | --- |
| give_item | the player actor | none | `ok` | `game_events.c` |
| take | the player actor | none | `ok` or `none` | `game_events.c` |
| walk | the player, one cell | one turn | `todo:walk` until it calls `do_step` | `ops/cdda_hq.c` |
| dig | **later** | | | |
| build | **later** | | | |
| craft | the player, when the table says the inputs are in inventory | `minutes` on the clock, same event repeated | `ok` and one `give_item` of `output`, or `missing` | table below |

Recipe row. New file `@.apps/cdda-hq/recipes.pdl`. One example is enough to wire the hook. Empty file means every `craft` returns `missing`.

```
RECIPE | id | output | count | tool | skill | minutes | inputs
RECIPE | bandage | bandage | 1 | none | 0 | 1 | rag:1
```

`inputs` is `item:count` repeated, separated by commas. No spaces.

Wound, for the C1 slice only. Not a second system. New file `state/wounds.pdl`, rewritten from events when you add the hook. Parts: `head torso arm_l arm_r leg_l leg_r`. Each starts at the actor's `mhp` divided by 6, integer. One `strike` picks one part and subtracts 1. Result text `wound:<part>`. A leg at 0 means `walk` returns `slow` and still moves one cell. Other parts do not change walking. Fighting is **later**.

Monster turn: after the player verb resolves, one hostile actor in the same room acts. If adjacent, `strike` the player. Else one `walk` toward the player (`bfs_first`). No other actor acts. No spawn table yet. Hostile means a party id you mark in `party.pdl` with a later column. Until that column exists, C1 may use a single id named `zed` if you add that one row. Do not invent a second monster.

Basement: the stair is a `DOOR` row in `rooms.pdl` to a room id `cellar`. z is **later**. Do not add a z field. S2 stays unread until a board file already has `current_z`.
