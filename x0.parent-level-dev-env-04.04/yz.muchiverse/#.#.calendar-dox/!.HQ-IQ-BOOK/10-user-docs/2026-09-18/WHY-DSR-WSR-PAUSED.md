# Why DSR and WSR-CIV are paused (2026-09-18)

This is a **user-facing** note so we do not forget the reason. It is
not a cancellation. Agent calendar copy: `12.calendar/2026-09-18/2do.md`.

## What those names are

- **WSR-CIV** — the Dustopia / world-sim civ track: events-only play,
  piececraft board, watching IRL and Cursword so the sim can “see”
  what you do.
- **DSR** — the dungeon / menu-class track (New / Load / Save / Save As
  as a real HQ menu), parallel to WSR-CIV in the kilo handoff.

Both were the morning plan in `13.agent-coms/KILO/claude-2-kilo-9.17.md`.

## Why we delayed them

Not because they are wrong. Because the **container they need was not
there yet**, and building a full civ/dungeon loop on a naive chat
hook would have been slower than a small, real bag.

The naive next step was “AI Chat (events)” on **Cursword main**. That
is the old theory. What we actually need first:

1. Cursword is a **thing that can hold other things** — a real
   `inventory/` directory, File Explorer grid, cut/copy/paste/place,
   drag in/out as `mv` on the linux filesystem (russian-doll purity).
2. Chat is a **separate entity** (a 🤖️ pal), not a second button on
   Cursword’s main menu. You drop the robot *into* the bag (or pull it
   out). Events live on that pal.
3. Gemma / synonym banks / hand-set “attention” weights come **after**
   that bag exists. No full LLM this pass.

Doing WSR-CIV/DSR in parallel *while* inventing that bag would split
focus: two agents on board/events, none on the inventory that later
chat and “watching Cursword” both assume. Pausing is cheaper than
rewriting those tracks onto a bag that did not exist yet.

## What “paused” means

- **Not cancelled.** Not handed to another agent to finish in secret.
- Resume when Inventory has been **clicked once for real** (relay or
  human) and the grid shows `cursword/inventory/`.
- On resume, still use **live** input (`hqcell` / `mgrcode` /
  `entity_menu_history/<pid>.txt`). Do not use dead `nav.sh nav`.
- Still-open board bug: piececraft “.main tab only” in `04-bugs/BUG-LOG.md`.

## What we did instead (so the pause is not empty)

File Explorer grid with names, chrome `_` `!` `X`, breadcrumbs that
wrap, Cut/Copy/Paste/Delete/Place on right-click, Cursword **Inventory**
in the entity menu. That is the bootstrap the civ/dungeon tracks will
drop into later.
