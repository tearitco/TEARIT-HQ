# 🧑‍🌾 khtpm FAQ — context menus, configurable guards & text popups

A plain-English guide to the right-click context menus and "Show Text"
popups on the muchiverse desktop (the tile windows run by
`&.widgits/tile-picker/ops/+x/tp_desktop_window.+x` — book-stack, asa,
ava, the muchis, and friends). Written for humans. 💬

## 1. What are these menus? 🪄

- **Context menu** — right-click any entity tile. It lists real actions
  read from that entity's own `meta.pdl` (Read, Play, Events, Dir,
  Close, Cancel, …). Every entity ships a **Close** row (`Close | CLOSE`)
  and a **Cancel** row (`Cancel | void`).
- **Sub-menu** — the little menu that opens next to the main one when
  you click the `User` row (Move / Inventory / Skill / Cancel).
- **Show Text popup** 📖 — a small floating window that shows text, e.g.
  a bible verse or page of an event. This one is deliberately
  dismiss-on-any-click (RPG-Maker "confirm press" style) — no Cancel row.

## 2. Context menus stay open until you close them 🆕

That's the whole point — like a normal menu you leave sitting there
while you read it, click around, or screenshot. The default is
`menu_stay_open = 1`.

| You do this…                          | What happens |
| ------------------------------------- | ------------ |
| Right-click a tile                    | ✅ menu opens |
| Click a menu row                      | ✅ runs that action |
| Click the **Close** row               | ✅ closes the tile window |
| Click the **Cancel** row              | ✅ menu closes, nothing happens |
| Press **Escape** / **Enter**          | ✅ menu closes |
| Click the same tile again             | 🚫 menu stays open |
| Click anywhere *else* (desktop, toolbar) | 🚫 menu stays open AND that click reaches its real target |
| Press **Print Screen** 🖥️             | 🚫 menu stays open, screenshot works |
| Open another entity's menu            | 🚫 this one stays open too — they coexist 😎 |

Menu rows are still clickable because a stay-open menu simply doesn't
take a pointer grab — clicks land where they're meant to, and the menu
keeps working for the clicks that do land on it.

## 3. The Show Text popup is a "confirm press", not a menu 📖

Different from context menus on purpose (KISS — restored 2026-08-07):

- Click it **anywhere** → closes. 👆
- Press **any key** → closes. ⌨️
- No Cancel row, no `[>]` navigation, no staying open. It's a quick
  "you saw this text" acknowledgement.

## 4. The menu guards are configurable — no rebuild, no restart ⚙️

Each entity's `meta.pdl` can set three `STATE` rows to tune how its
context menus behave. Missing rows keep the compile-time defaults. Edit
the file and the **next right-click** re-reads it.

```
SECTION      | KEY                | VALUE
STATE        | menu_stay_open     | 1    outside/repeat clicks keep menu open
STATE        | grab_pointer       | 1    modal pointer grab while a menu is open
STATE        | grab_keyboard      | 1    modal keyboard grab while a menu is open
```

- `menu_stay_open = 1` (default) — menus stay open; **no pointer grab**,
  so the desktop/toolbar keep responding and the menu coexists with
  other menus.
- `menu_stay_open = 0` — old dismiss-on-any-click behavior; the menu
  takes a pointer grab again and closes on the first outside click.
- `grab_pointer = 0` — never pointer-grab a menu, even when
  `menu_stay_open = 0`.
- `grab_keyboard = 0` — turn off the keyboard grab. (Default keeps it
  ON so arrow keys always move the `[>]` cursor in a menu.)

Example for book-stack (now living at `*.monads/*.book-stack/`):

```
# *.monads/*.book-stack/entities/book-stack/meta.pdl
STATE        | menu_stay_open     | 1
STATE        | grab_pointer       | 1
STATE        | grab_keyboard      | 1
```

## 5. Adding a Cancel/Close row is a one-line edit 🛠️

Context menus are data-driven — rows come from the entity's `meta.pdl`:

```
METHOD       | Cancel               | void      # dismiss, do nothing
METHOD       | Close                | CLOSE     # close the tile window
```

`void` = "do nothing, just close the menu"; `CLOSE` = end the tile's own
event loop. Every desktop entity already ships both.

## 6. Relaunching after a binary change 🔄

Behavior changes that live in the compiled binary need a relaunch of the
open windows:

```bash
./scrypts.sh openall        # reopen everything (idempotent)
./scrypts.sh book run       # book-stack alone (window + reader)
./scrypts.sh book kill      # close book-stack's window
```

(`scrypts.sh` is the dispatcher at the house root.)

## 7. Anything else I should know? 🧠

- The menu row numbers `[N]` you see are **global live addresses** — an
  AI harness / test-agent can drive menus remotely by writing
  `ACTIVATE_NAV:<N>` into the entity's `interact_relay.txt`, which is
  why menus shouldn't just vanish while another process is working. 👾
- Clicking the header row (the entity id at the top of a menu) is a
  harmless no-op — it just refocuses the menu. Not a button.
- book-stack now lives at `*.monads/*.book-stack/` (entity package in
  `entities/book-stack/`) — the old `@.apps/book-stack` and
  `#.desktop/entities/book-stack` paths are gone.
- These notes describe Linux/X11 behavior. The Windows build
  (`tp_desktop_window.exe`) keeps working as before — nothing here
  touches it.
