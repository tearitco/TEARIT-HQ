# df-hq rules (sprint)

Hook: `GE_RULE_HOOK` in `ops/df_hq.c`. Ledger: `state/events.txt`. Placement ledger stays `state/house.txt`.

| verb | who | cost | result text | file |
| --- | --- | --- | --- | --- |
| give_item | the digger or a menu | none | `ok` (count +1). Already wired from `dig_now` as target `stone` | `game_events.c` |
| take | the hauler, later | none | `ok` or `none` | `game_events.c` |
| walk | the actor INT controls, or a pet | one step | `todo:walk` until the hook calls `do_step`. Do not double-step | `ops/df_hq.c` `do_step` |
| dig | any dwarf for this sprint. Labor prefs are **later** | the existing dig job, not a second turn | yield 1 `stone` via `give_item`. Already done in `dig_now` | `ops/df_hq.c` |
| build | **later** | | | |
| craft | **later** | | | |

Stockpile row, when haul is built. Not in `rooms.pdl`. New file `state/stockpiles.pdl`:

```
STOCK | id | room | x | y | w | h | accepts
STOCK | food | castle | 2 | 4 | 3 | 2 | stone,meat
```

Haul is **later**. A haul is `take` at the dig cell then `give_item` is wrong — the stone is already on the actor. Haul is a walk to a `STOCK` whose `accepts` contains the item, then the item leaves the actor. No row for that until the stockpile file exists.

Test fortress: marks only is enough. Do not dig it out.
