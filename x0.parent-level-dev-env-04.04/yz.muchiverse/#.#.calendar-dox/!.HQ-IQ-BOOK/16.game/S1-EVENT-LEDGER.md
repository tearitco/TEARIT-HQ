# S1 — the event line and resolver (built 2026-10-17)

Sprint item S1 of `GAME-MECHANICS-SPRINT.md`, shared by df-hq, cdda-hq and mcl-hq. Code: `44.xyz.01.00/&.widgits/_shared-lib/game_events.c` (text-included, pure file I/O,
prefix `ge_`), three thin verbs in `ops/df_hq.c`, `ops/cdda_hq.c`, `ops/mcl_hq.c`.

## The rows (one ledger: `state/events.txt`, no second file — grok's answer 2026-10-16)

```
EVENT|ts|actor|verb|target|at|queued       written by the `event` verb (or a menu row / pet); validated first
EVRES|ts|<offset of the EVENT row>|verb|result   written once, by the resolver
REJECT|ts|<offset of the bad row>|reason         written once, for an EVENT-looking row that is malformed
```

`verb` is one of `give_item take walk dig build craft`. `at` is `-` or `<room>:<x>,<y>`. No field may contain `|`, CR or LF. The existing `STEP DOOR DAY DIG RESET` rows are not touched and
their readers ignore the new row types (checked in df-hq: a dug cell and a rock cell behave the same with and without EVENT/EVRES/REJECT rows in the ledger).

## Verbs (all three games, same syntax)

| verb | what it does |
| --- | --- |
| `event <actor> <verb> <target> <room:x,y or ->` | validates, appends one EVENT row, resolves it. A bad field refuses with the reason and writes nothing |
| `resolve` | resolves every legal EVENT row that has no EVRES yet and rejects every malformed one that has no REJECT yet. Idempotent: a second run appends nothing |
| `inventory [actor]` | replays the ledger and prints `actor:item=n`. Writes nothing |

## Deviation from the sprint line: no cursor file

The sprint asked for a cursor over a size-growth marker. Resolution status lives in the ledger instead (an EVRES row names the offset of its EVENT row). Reason: Save, Load, New game and
reset swap `events.txt` as a whole, and a separate cursor file would disagree with the restored ledger. The state (inventory) is a pure function of the file, replayed from byte 0 on each call
(a ledger is 100 KB in df-hq; the call happens on a verb, not per tick). A daemon that wants to resolve live should call `resolve` when the file's size grew (marker, not mtime).

## What is built and what is empty

Built: validation, append, replay, EVRES/REJECT, inventory effect of `give_item` (+1) and `take` (−1, result `none` when the actor has none; no state change).
Empty on purpose (result `todo:<verb>`): `walk dig build craft`. The rule bodies are grok's: labor, mood, recipes, wounds, mob weights. A game wires a rule by defining `GE_RULE_HOOK(actor, verb, target, at)`
(returns a result text or NULL for the default) before including `game_events.c`.
`TODO(grok)`: df-hq `dig` already exists as DESIGNATE/CLAIM/DIG rows (`jobs.txt` + `events.txt`) and is not routed through EVENT yet; `build`/`craft` bodies belong in
`ops/df_hq.c`, `ops/cdda_hq.c`, `ops/mcl_hq.c`.
`khtpm_inventory.c` was looked at and not reused: it models a desk entity's inventory as a directory of entity packages, which these games do not have.

## Tested (scratch copies, paths rewritten, verbs only; live games untouched)

Same script on each game (`XO/test_s1.sh`): 8 scripted events give `pip:stone=1`; a STEP row already in the ledger is untouched; 8 EVENT + 8 EVRES rows; six malformed `event` calls
are refused and the ledger size does not change; three hand-written bad rows (3 fields, 8 fields, non-numeric time) get 3 REJECT rows, a second `resolve` changes nothing (sha1 equal) and the inventory
is unchanged; a fresh game given the same ledger replays to the same inventory and resolving it appends nothing; negative control: turning the first `give_item` into `take` in the copy
changes the inventory (so the equality checks can fail). Result: 0 failures in df-hq, cdda-hq and mcl-hq.
Not tested: the verbs from a menu row or a pet (no menu row exists yet), a live game, a ledger over 32 MB (refused by design), concurrent writers.
