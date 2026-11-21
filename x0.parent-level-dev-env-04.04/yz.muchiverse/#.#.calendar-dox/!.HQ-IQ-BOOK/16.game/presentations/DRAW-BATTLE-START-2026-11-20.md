# Draw Battle — start deck (2026-11-20)

Paint living soldier types onto a table. Press start. They fight by themselves.

The browser page is the reference. The house app is the project. They are not the same file.

## What exists

| Thing | Where | Status |
|---|---|---|
| Browser game, phase 5 (sand, food, growth, 2D and a 3D camera) | the owner's folder `!.🔫️🖋️/index.html` | reference only |
| House app | `44.xyz.01.00/@.apps/draw-battle` | generated 2026-11-20, editor kind |
| Type catalog | `rulesets/default/units.pdl` | numbers copied from the page; nothing reads them yet |
| Shared engine | `game_core.+x` | no Draw Battle rules were added to it |
| Checks | `game_new_op check` 11/11; harness `draw-battle` 28/28 PASS | the document machine, not the battle |

User-facing name: **Draw Battle**. Folder id: `draw-battle`. Product around it is LyfeDesk.

## What the scaffold can do today

It is the same shape as a Teario editor book:

- A document of rows (`add`, `set`, `del`).
- `publish` is refused until the document has the ids `start` and `goal` (the generated stub, still in `rules.pdl`).
- Replay, a hand-edited ledger refused, a stray file refused, deleted projections regenerated.
- One empty board desk so the rpg-edit Books list can see the book after the editor board is relaunched.

It does not paint soldiers, run sand, or fight. Those stay in the HTML until a pure op exists.

## How the HTML maps

| In the page | In the house |
|---|---|
| `UNIT_DEFS` | `units.pdl` rows |
| A brush stroke | one ledger row, not a rewrite of the whole grid |
| The 180×110 sand array | a projection, rebuilt by replaying strokes and ticks |
| `units[]`, `projectiles[]`, `nextId` | derived, not a second save file |
| 2D canvas and Three.js sprites | the board viewer. This table is top-down. Do not set `axis=side` (that flag is for Teario, where a row means up) |
| START / pause | the editor running switch. Pause writes no row |
| Visitor badge and `ipapi.co` | leave them out |
| `localStorage` theme | a preference, if anyone asks later |

## How to run the tests

From `44.xyz.01.00`:

```
&.widgits/_shared-lib/harness/ops/+x/game_new_op.+x "$PWD" check draw-battle
```

From `44.xyz.01.00/&.widgits/_shared-lib/harness`:

```
rm -f results/draw-battle.txt
./ops/+x/harness_case_op.+x cases/draw-battle.pdl
./ops/+x/harness_verdict_op.+x cases/draw-battle.pdl
```

A pass is `VERDICT|PASS|passed=28|failed=0`. The case uses a scratch folder. It does not open the live window.

## What the next agent builds, in order

1. Keep `units.pdl` as the only list of types. Add a small C op that refuses an unknown id and prints one soldier's hp. Harness case around that op. Do not edit `game_core.c` for it.
2. One paint verb: a stroke row (who, type, cell, team). Replay redraws the same units.
3. One battle step verb: given the same seed and the same strokes, one tick returns the same deaths. Hunger, xp, and friendly fire stay switches in data.
4. Only then point the board desk at that projection.

A tick must not launch a process. An event page is for authoring and for the harness.

## Do not

- Paste `index.html` into the house or wrap it in a web view and call it done.
- Add a `draw_battle` mode inside `game_core.c`.
- Generate another app on top of this one.
- Turn the sand grid into one ledger row per cell per frame.
- Invent Soul Pen names. The ids already in `units.pdl` are the names.
- Commit, push, or touch `xyzfs/users`.
- Relaunch the live Teario window while doing this.

## X11 pair for a boss (2026-11-20, later the same night)

The HTML page was not wrapped. Two event pages, `events/stroke` and `events/tick`, call `db_battle_op`. The op is not inside `game_core`.

Before: `XO/31.Nov/nov-20/3.draw-battle-before/` — the X11 board, book Draw Battle, no bodies.

After: `XO/31.Nov/nov-20/4.draw-battle-after/` — soldier dead, roman still up at cell 7,7. Harness `draw_battle_events` passed 9.

## Open questions

Send them to Grok before coding the battle. The list is in the coach note next to the HTML.
