# 08 — Roadmap

- `OPEN-ITEMS.md` — the current, real open-item summary (start here).
- `au-31/` — 2026-08-31's live in-progress work directory (`00-todo.md`
  real todo list, `01-manager-design.md`/`02-network-browser-...md`
  design docs). Moved verbatim from `1.^V-hq/au-31/`.
- `design-docs/` — 63 design/plan/handoff/investigation docs moved in
  bulk (`git mv`, history preserved) from `1.^V-hq/`. **Not
  individually hand-condensed** in this pass (see note below) — still
  real, readable source material, just not yet trimmed to the book's
  target prose tightness. Treat as a holding pen: the next pass
  through this chapter should fold each into `OPEN-ITEMS.md` (if still
  open) or a short dated `04-bugs`/status note (if resolved), then
  delete the original.

## Honest scope note

Given time budget, chapters 01-06 (orientation, architecture,
pitfalls, bugs, faq, testing) got the full "condense, don't just
relocate" treatment this migration's principles call for. Chapter 08
did not get the same per-file treatment — 63 files from `1.^V-hq/`
were moved in bulk into `design-docs/` rather than each read in full
and hand-condensed into prose. This is a deliberate, disclosed
tradeoff (the task instructions explicitly allow lighter/faster
treatment of 07-09), not an oversight. Nothing was deleted without
reading its INDEX.md summary first — the dead `archive/`-prefixed
pointers were the only outright deletions.

## Real, current known-open work (from `INDEX.md`'s own Tier-1 list, 2026-08-31/09-01)

- **Events/db-hq**: real, low-risk next steps identified but not
  started (see `design-docs/!.OPEN-2do-events-db-networking-2026-08-
  28.md`, `design-docs/EVENTS_AND_DB_GUIDE_🎪.md`).
- **Cross-platform (Windows/Mac)**: tracked in
  `design-docs/CROSS-PLATFORM-PENDING-2026-08-29.md`.
- **Generic khtpm dispatch table** (replacing `g_is_<mode>` flags):
  design in `02-architecture/xperiments/khtpm-generic-dispatch-
  design.md`, not yet implemented — see `CENTROID_GOLD_STD.md` §3
  rule 7 for the ordered migration plan.
- **ASCII/headless khtpm renderer** (`ascii_draw_elem()`): designed,
  not yet built — `02-architecture/xperiments/chtpmx11-refactor.md`
  §8.
- **LayDoc → Elem/CSS taskbar retarget**: not started — see
  `design-docs/LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md`.
- **`dbhq_load_actors()` and sibling inline loaders**: audit pass not
  done — see `04-bugs/BUG-LOG.md` and `TPMOS-COMPLIANCE-DEBT.md` (in
  `design-docs/`).
- **Toys-launch PID tracking gap**: see `04-bugs/BUG-LOG.md`.
- **chat-hai migration to Harnecient/khtpm standard**: per
  `design-docs/HANDOFF-2026-09-01.md`, chat-hai itself is NOT yet
  migrated onto whatever that handoff scoped — read that file directly
  for the real, current scoped plan before assuming it's done.
- **Joystick / controller support**: referenced in
  `0.browser-prompting/bugs-toys-gl/5.remaining-bugs-joystick-toys-
  delegation.md` (see appendix cross-reference) — not started as of
  this pass.
- **Future games planned**: `44.xyz.01.00/2xx.*` and `3xx.*` app
  folders (glut-craft, dwarf-fortress-clone, rpg-maker-clone,
  snes-civ, gb-pokemon, sw-battlefront, ttg-tactics, sp-irl, rpg-xyz,
  rtp-xyz) each carry their own `ARCHITECTURE.md`/`PROMPT.md`/
  `README.md` — left in place (per-app docs, out of this migration's
  scope), but worth a future roadmap pass to summarize status here.
