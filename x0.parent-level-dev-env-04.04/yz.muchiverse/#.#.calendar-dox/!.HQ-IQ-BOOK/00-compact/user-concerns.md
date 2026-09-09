# User concerns — RPG Maker parity (events + db)

Date: 2026-09-08. Honest, not a sales sheet. Sanity-test every claim
yourself. Do not trust `event.commands.remaining.txt` as current.

House: `44.xyz.01.00`. Compact: `00-compact/!.HQ-IQ-COMPACT🧭v0.1.md` §16.

---

## How far we are (one line)

**Authoring is ~halfway to MV’s event editor. Play is not MV Test Play.**
The database window has 15 tabs; **only Common Events is a real editor.**
Registry lists many commands; most **write kv files**, they do not move
the camera, open a shop, or fight.

---

## Events — done vs left

**Done (you can open events-hq / CE and see this):**

- IR: `event.ir.pdl` `NODE | id=N type=<cmd> | k=v` → `event.pal` → `cmd_N.sh`
- Same editor for entity events and db-hq **Common Events** tab
- Registry: `#.ref/menu/event_commands.registry.pdl` (add a SIMPLE cmd
  with no C recompile)
- Real-ish ops (file/UI, not “full MV”): Show Text, Show Choices,
  Change Gold / Take Gold, Control Switch/Variable, Call Common Event,
  wait, some party/item/hp wrappers, if/else/end as IR structure
- Guides: `#.ref/menu/event-guides/` (authoring sheets, not auto-run)
- Samples: `mineclonia_sample` / `cdda_sample` `events.pdl` (notes only)

**Left (MV has these; we have names or kv only):**

- **Play interpreter** that runs pages with RM triggers (Action /
  Touch / Autorun / Parallel) on **desk or pchq**
- **Transfer Player** — `mr_world` → `map_state.pdl`; map/desk does
  not change
- **Shop / Battle** — same, `shop_state.pdl` / `battle_state.pdl`
- Fade/tint/shake/scroll/move_route — kv `screen_state.pdl`
- Map events on a cell (click voxel → event_pkg) **not wired**
- Pages with conditions (switch/variable/self-switch/item/actor)
- Vehicles, followers, encounter steps, troop pages
- `tp_range_grid` deleted; range overlay **not built**

**Not parity:** “command is in the Add Command picker” ≠ it plays.

---

## Database — done vs left

Ladder: `#.ref/menu/db-tabs-remaining.txt` (still the right order).

| Tab | Honest status | Play reads it? |
|---|---|---|
| Common Events | **Live** — embeds events-hq on `common_events/<name>/event_pkg` | CE pkg yes; not a ticking Autorun on the desk |
| Items, Actors, Classes, Skills, Weapons, Armors, Enemies, Troops, States, Animations, Tilesets, System, Types | Field tiles / placeholders | **No** canonical play file. Writes `#.desktop/db_hq_*.state.txt` (form buffer) |
| Terms | File is **INI** `[Basic Terms]` | A TAG\|id\|name projector shows **empty** |

**Left:** every tab except CE must **flush** a stable play pdl/json
(suggest `$HOUSE/shared/db/play/` or xyzfs `home/db/`) on save.
**System** is the silent New Game blocker (start map, xy, party, gold).
Items prices required before Shop is real.

---

## How you should sanity-test claims

Do **not** accept “it’s in the registry” or a clean compile.

**Events**

1. Open events-hq or db-hq → Common Events → `greet_player`. Confirm
   `n_cmds` / NODE rows on disk:  
   `common_events/greet_player/event_pkg/pages/page_1/event.ir.pdl`
2. Add Show Text, save, confirm a new NODE and `cmd_N.sh`.
3. **Play claim:** Interact ON, walk into an event. If nothing but a
   kv file appeared under the entity, it is **not** play.
4. Transfer: `grep transfer` on `map_state.pdl` after running the cmd
   is **not** a transfer. You must **see** the desk or chunk change.
5. Shop/Battle: gold/inventory files must change **and** a UI or
   choices must appear. Empty `shop_state.pdl` is fail.

**DB**

1. Open db-hq. Click Items. If you only get swatches / empty list,
   Items is **not** done.
2. After an edit, `ls` `#.desktop/db_hq_items.state.txt` **and** look
   for a play file. Only the first = form buffer.
3. Terms: `head #.desktop/db_hq_terms.state.txt` — must start
   `[Basic Terms]`. If the tab is blank, the UI is parsing TAG rows.
4. CE: list_open, edit a cmd, confirm IR file mtime/content.

**Maps / studio (related, you’ll be lied to)**

- Toys **Piececraft-HQ** ≠ **Piececraft**.
- Stamp: palettes arm then click desk — amber wash with no grid is
  the known placer; tiles vanishing on reset = no DESK row.
- Chrome in play must **stay**.

**Files to grep when someone says “done”**

```
event_commands.registry.pdl
common_events/*/event.ir.pdl
map_state.pdl shop_state.pdl battle_state.pdl
db_hq_*.state.txt
44.xyz.01.00/shared/*-ASSET-SOURCE-LOCATION.pdl
```

---

## What “MV parity” would actually mean (so you can say no)

A stranger: New Game from System → walk a map → door Transfer → mart
Shop (gold + items db) → troop Battle → Save slot restores switches
and self-switch chests. Until that loop is **visible**, we are an
editor shell + kv stubs.
