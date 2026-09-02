# sep-1-events-SOS.md — the EASIEST db-hq events, step by step, for a weak agent

Read this if you were handed "make some events work in db-hq" and you
don't have deep context on this house's codebase. Every command below
is copy-pasteable. Every claim is backed by a real file this doc
points you at — go look at it if something doesn't make sense.

**CORRECTION (2026-09-01):** everything in "Step 3" below (the 5-item
block palette) is ALREADY BUILT and working — fine to use for
pre-testing the flow (launching db-hq, adding an event, verifying
files), but it is NOT new coding work. If the task is "code something
new," skip to **"WHAT'S ACTUALLY LEFT TO CODE"** near the bottom of
this doc instead — that section lists real, confirmed-missing RPG
Maker commands, ranked easiest first, with copy-paste starting points.

**Do not invent new file formats or command types.** Almost every
"simple" RPG Maker command (anything that's really just "set one
value" or "wrap one op") goes through ONE data-driven registry file,
`#.ref/menu/event_commands.registry.pdl` — read that file's own header
comment first, it explains the exact block format. Adding a real new
command is usually a registry-only edit, ZERO C/recompile, unless it
truly needs a brand-new op binary (see the ranked list below for which
kind each missing command is).

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
- Don't invent a 6th palette command type before checking the real
  registry first — see "WHAT'S ACTUALLY LEFT TO CODE" below. Most
  "new" commands are a registry-only edit (zero C, zero recompile),
  NOT a new op binary — check there before writing any C.
- Don't touch `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
  unless you've confirmed (via the steps above) that the bug is
  actually in the shared renderer and not just in how you used it.

---

## WHAT'S ACTUALLY LEFT TO CODE (ranked easiest first)

Everything in `#.ref/menu/event_commands.registry.pdl` is ALREADY
DONE — I read the whole file plus the manager's `compile_page()`
(`&.widgits/events-hq/ops/khtpm_events_hq_manager.c:429`) and every
single command type listed there — including all of Flow Control
(if/else/end/loop/break_loop/repeat_above/comment/exit_event/label/
jump_to_label) — is genuinely implemented and wired up, not a stub.
This covers the vast majority of RPG Maker MV/MZ's real command list:
all Message, Game Progression, Party, Actor, and Character commands.

What's below is a real gap: RPG Maker command categories with **zero**
registry entry at all. Confirmed by grepping the registry file and
`grep -rn "shop\|Shop" event_commands.registry.pdl` (zero hits) plus
checking `common_events/shop_open/` on disk — it's an empty stub
folder, name only, no real page/command content. Ranked by how much
real work each needs:

### Tier 1 — registry-only, copy an existing block, ~5 min each, ZERO new C
These are single on/off flags, the exact same shape as `control_switch`
(already in the registry — copy that block, rename). No new op binary
needed; they just need a new `COMMAND` block appended to
`#.ref/menu/event_commands.registry.pdl`, then they show up in the
"Add Command" picker on the manager's next poll tick (proven
zero-recompile — see that file's own "REAL LIVE PROOF" comment about
`take_gold` being added while the manager was running).

- **Change Menu Access** (enable/disable the in-game menu) — one flag,
  e.g. `{STATE_DIR}/system_flags.txt` key `menu_enabled`.
- **Change Save Access** (enable/disable saving) — same shape, key
  `save_enabled`.
- **Change Encounter** (enable/disable random encounters) — same
  shape, key `encounter_enabled`.
- **Change Formation Access** (enable/disable party reordering) — same
  shape, key `formation_enabled`.

Template to copy (this is literally `control_switch` from the
registry, renamed — verify against the real file, don't retype from
memory):
```
COMMAND change_menu_access
  LABEL Change Menu Access
  FIELD1 Enabled (ON/OFF):
  FIELD2 -
  PARAMS enabled
  PAL li x15, 7
  PAL li x12, {enabled}
  PAL ecall "{STATE_DIR}/system_flags.txt" "menu_enabled"
END
```
(Check whether ON/OFF needs translating to 1/0 first — the real
`control_switch` block does this translation in
`evhq_submit_picker()`, C-side, specifically for that one command; a
new command either needs the same C hook added for it, or should ask
the user to type `1`/`0` directly like `change_party_member` does with
its `SELECT2 1:0` — the SELECT2 route is genuinely zero-C, prefer it.)

### Tier 2 — needs ONE new small op binary, but copy an existing one almost verbatim, ~20-30 min
Real new C, but trivial: `mr_character.c` (in `&.widgits/events-hq/ops/`)
is a ~48-line file — read it, it's the exact template. One binary,
dispatches on `argv[2]` (a mode string), writes one `key=value` line
to a state file. This shape covers:

- **Screen effects** (Fadeout/Fadein/Tint/Flash/Shake Screen) — new
  `mr_screen.c`, copy `mr_character.c`'s structure exactly, modes
  `fadeout|fadein|tint|flash|shake`, writes to
  `<entity_dir>/screen_state.pdl`. Registry TEMPLATE lines mirror the
  existing `change_transparency`/`followers`/`show_animation` blocks
  (just swap the binary name and mode).
- **Change Actor Graphics** (face/character/battler image path) — new
  mode(s) on `mr_actor_string.c` (already exists, already takes a
  string value — this is adding 1-3 more `if` branches to its existing
  dispatch, not a new file) or a small `mr_actor_image.c` twin of it.

Real, honest limitation to flag if you build these: like the existing
Character commands, this writes real, verifiable STATE — it does NOT
make anything visually happen on screen (no sprite/rendering layer
consumes these files yet). That's consistent with what's already
shipped (`mr_character.c`'s own header comment says the same about
transparency/followers/animation) — not a new gap you're introducing.

### Tier 3 — needs a new op binary AND a real external dependency, verify the dependency first
- **Play SE / BGM / BGS / ME** (sound effects/music) — needs an op
  binary that shells out to a real audio player (`aplay`/`paplay`/
  `ffplay`). Before writing this: run `which aplay paplay ffplay` on
  the actual target machine and confirm ONE of them is genuinely
  available — don't assume, don't hardcode one that might not exist
  and silently fail. If none is available, say so rather than shipping
  a command that silently no-ops.

### Tier 4 — NOT easy, genuinely bigger systems, don't start these without checking in first
- **Shop Processing** — the `shop_open` common event already exists as
  an empty NAME-only stub (`common_events/shop_open/` has no
  `event_pkg/pages/` content at all) — it needs real buy/sell UI,
  price/inventory math, and gold deduction wired together, not a
  single command. Real design work, not a quick registry add.
- **Set Movement Route / Transfer Player / Scroll Map** — needs a real
  concept of "where is the player/event on a map" that doesn't exist
  in any state file checked this session. Don't build the command
  before confirming whether ANY map/position system exists elsewhere
  in the house (not found in this pass — search before assuming).
- **Battle Processing** — needs a real battle system; out of scope for
  a "left to code, easiest first" pass.
- **Script / Plugin Command** (arbitrary code execution) — this house
  already has a real op-binary-per-command pattern specifically so
  events never need to eval arbitrary text; adding a raw "run this
  code" command would be a deliberate architecture change, not a
  small addition — check in before building this one specifically.

## Full reference paths (all real, all checked to exist as of 2026-09-01)

- Launch db-hq: `*.monads/*.muchi-pet/ops/open_db_hq.sh <house_root>`
- Event palette (5 easy commands): `khtpm_core_render.c:3882`
  (`g_evhq_palette[]`)
- "+ Add Common Event" button wiring: `khtpm_core_render.c:1790-1811`
- Common events state file (sidebar list): `#.desktop/db_hq_common_events.state.txt`
- Real worked example to copy: `common_events/greet_player/`
- Op binaries behind each command: `&.widgits/events-hq/ops/mr_*.c`
  (`mr_change_gold.c`, `mr_show_text.c`, `mr_show_choices.c`, etc.)
- **The full command registry (the real source of truth for "what's
  already coded")**: `#.ref/menu/event_commands.registry.pdl` — read
  this before assuming anything is missing.
- Where the registry is compiled/consumed: `compile_page()` in
  `&.widgits/events-hq/ops/khtpm_events_hq_manager.c:429`.
- Fuller architecture explanation (read this if the above isn't
  enough): `sep-1-grok.md` (same folder as this file).
