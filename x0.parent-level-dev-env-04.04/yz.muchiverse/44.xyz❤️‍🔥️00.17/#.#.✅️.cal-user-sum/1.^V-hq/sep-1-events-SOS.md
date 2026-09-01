# sep-1-events-SOS.md — the EASIEST db-hq events, step by step, for a weak agent

Read this if you were handed "make some events work in db-hq" and you
don't have deep context on this house's codebase. Every command below
is copy-pasteable. Every claim is backed by a real file this doc
points you at — go look at it if something doesn't make sense.

**Do not invent new file formats or command types.** Everything you
need already exists. Your job is to use the existing "block palette"
(5 ready-made commands, listed below) through the existing UI, or —
if the UI misbehaves — by hand-editing the exact same plain text files
the UI itself writes. Nothing here requires touching C code.

---

## Step 0 — orient yourself: what is a "common event"?

A common event = one folder under `common_events/<name>/`. Look at a
real, working example right now:

```
cat "common_events/greet_player/event_pkg/pages/page_1/event.ir.pdl"
cat "common_events/greet_player/event_pkg/pages/page_1/event.pal"
```

`event.ir.pdl` is the human-readable "what this event does" list.
`event.pal` is what actually runs (a tiny shell script under the
hood — each line either `exec cmd_N.sh` or `halt`). You will never
hand-write `event.pal` — the app generates it for you when you save.

## Step 1 — launch db-hq

```
bash "*.monads/*.muchi-pet/ops/open_db_hq.sh" "<house_root>"
```

(`<house_root>` = the absolute path to this whole repo's top folder —
the one that contains `#.desktop/`, `&.hq-apps/`, `*.monads/`, etc.)

This launches TWO processes (a renderer window + a manager) and prints
their PIDs. If it prints an error instead, `cat /tmp/db-hq.log` and
read the actual error — don't guess.

## Step 2 — go to the "Common Events" tab

db-hq opens with several tabs across the top (Actors, Classes, Skills,
...). Click (or nav to) the one labeled **"Common Events"**. This is
literally an RPG Maker-style event list — you'll see existing entries:
`greet_player`, `nested_inner`, `shop_open`, `test_target`.

Reference: the full real tab list is in
`*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` around line 1097
(`DB_HQ_TAB_LABELS[]`) if you want to double check you're on the right
tab by name.

## Step 3 — the 5 EASIEST commands that already exist (use these first)

There is a ready-made "block palette" — 5 commands, already wired up,
already tested, already the simplest thing you can drop into any
event page. Do not build anything new until you've tried these. Real
source: `khtpm_core_render.c` line ~3882 (`g_evhq_palette[]`):

| Palette label | internal type   | example params                  | what it actually does |
|---------------|------------------|----------------------------------|------------------------|
| Change Gold   | `change_gold`    | `amount=10`                      | runs `mr_change_gold.+x`, adds gold |
| Take Gold     | `take_gold`      | `amount=10`                      | same op, subtracts |
| Switch ON     | `control_switch` | `switch_name=flag_0\|switch_value=1` | sets a named flag to 1 |
| Show Text     | `show_text`      | `text=Hello!`                    | runs `mr_show_text.+x`, displays a message |
| Wait          | `wait`           | `ms=100`                         | pauses N milliseconds |

**"Show Text" with a custom message is the single easiest thing to
build and test end to end.** Do that one first.

## Step 4 — create a brand-new common event (or reuse an existing one)

In the Common Events sidebar there's a button labeled **"+ Add Common
Event"**. Click it, type a name (letters/numbers/underscore only, e.g.
`test_hello`), press Enter. This creates a real folder:
`common_events/test_hello/event_pkg/` — confirm it exists:

```
ls "common_events/test_hello/event_pkg/"
```

If it's empty, that's expected — it gets real content once you add a
page/command in the next step.

## Step 5 — add a command using the block palette

1. Open the event you just made (click its name in the sidebar).
2. Click **"+ Add Command"** (or, if it's a fresh event with no page
   yet, **"+ New Page"** first, then "+ Add Command" inside it).
3. Pick **"Show Text"** from the palette.
4. Edit its param to something recognizable, e.g. `text=IT WORKS`.
5. Save/confirm (however the UI's own save action is labeled — look
   for a checkmark/confirm button, don't guess a keybind).

## Step 6 — VERIFY it actually saved (don't trust the screen alone)

Real files on disk are the source of truth, not what the window shows.
Check them directly:

```
cat "common_events/test_hello/event_pkg/pages/page_1/event.ir.pdl"
cat "common_events/test_hello/event_pkg/pages/page_1/event.pal"
```

You should see a `NODE | id=1 type=show_text | text=IT WORKS` line in
the `.ir.pdl` file, and `event.pal` should now contain `exec
cmd_1.sh` followed by `halt`. If `event.pal` is empty or still just
says `halt` alone, the save didn't actually register — go back and
retry Step 5, don't move on.

## Step 7 — run it for real, outside the UI, to prove it's not fake

Every `cmd_N.sh` is a real, standalone, executable shell script. You
can run the WHOLE event by hand, no window needed:

```
bash "common_events/test_hello/event_pkg/pages/page_1/event.pal"
```

Wait — `event.pal` is NOT directly a shell script itself (it's a list
of `exec` lines meant to be interpreted one at a time), so instead run
each `cmd_N.sh` it references directly, in order:

```
bash "common_events/test_hello/event_pkg/pages/page_1/cmd_1.sh"
```

For a `show_text` command this should print/display the text (check
`mr_show_text.c` in `&.widgits/events-hq/ops/` if you want to see
exactly what it does — it's a small, readable file). For a
`change_gold` command, check whatever gold/ledger file it touches
(look at `common_events/greet_player/master_ledger.txt` for what a
real ledger file looks like after gold changes ran).

## Step 8 — if something's broken, where to actually look (in order)

1. `/tmp/db-hq.log` — the renderer's own stdout/stderr from launch.
2. `#.desktop/db_hq_common_events.state.txt` — the flat list of event
   names the sidebar reads from. If your new event's name isn't in
   here, the manager hasn't published it yet — check the manager is
   still running: `pgrep -fa khtpm_hq_manager`.
3. The event's own `event.ir.pdl` — if this is empty/missing, nothing
   was ever really added; the UI silently failed the save.
4. The event's own `event.pal` — if `.ir.pdl` looks right but `.pal`
   is stale, the compile step from IR→pal didn't run — that's a real
   manager bug worth reporting, not something to work around by
   hand-editing `.pal` (it gets overwritten on the next real save
   anyway).

## What NOT to do

- Don't hand-write `event.pal` files — they're compiled output,
  always regenerated from `event.ir.pdl` by the manager. Editing them
  directly will be silently thrown away on the next save.
- Don't invent a 6th palette command type before trying the 5 that
  exist. If you truly need a new one, it needs a real op binary (like
  `mr_show_text.c`) AND a new palette entry — that's real C work, flag
  it rather than faking a `type=` string the manager doesn't know.
- Don't touch `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
  unless you've confirmed (via the steps above) that the bug is
  actually in the shared renderer and not just in how you used it.

## Full reference paths (all real, all checked to exist as of 2026-09-01)

- Launch db-hq: `*.monads/*.muchi-pet/ops/open_db_hq.sh <house_root>`
- Event palette (5 easy commands): `khtpm_core_render.c:3882`
  (`g_evhq_palette[]`)
- "+ Add Common Event" button wiring: `khtpm_core_render.c:1790-1811`
- Common events state file (sidebar list): `#.desktop/db_hq_common_events.state.txt`
- Real worked example to copy: `common_events/greet_player/`
- Op binaries behind each command: `&.widgits/events-hq/ops/mr_*.c`
  (`mr_change_gold.c`, `mr_show_text.c`, `mr_show_choices.c`, etc.)
- Fuller architecture explanation (read this if the above isn't
  enough): `sep-1-grok.md` (same folder as this file).
