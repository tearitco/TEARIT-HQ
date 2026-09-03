# 08 — Roadmap

- `FORWARD-ROADMAP-2026-09-02.md` — **the current real plan going
  forward**: hardening a live, human-supervised Sonnet/Grok chat
  channel, then Grok's real task sequence (media-studio + network-app
  khtpm ports, settings/polish, db-hq RPG-Maker parity, image-editor+AI
  roadmap). Start here for "what's next."
- `design-docs/GROK-HANDOFF-2026-09-02.md` — **the current Grok
  onboarding doc** — replaces the old, now-archived render/input
  handoff (stale filenames, predates `CENTROID_GOLD_STD.md` and
  everything since). Read this before tasking Grok with anything.
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

## ⚠️ 2026-09-02 correction — a stray nested duplicate held newer content

A leftover pre-migration duplicate folder was found nested inside
`44.xyz.01.00/#.#.✅️.cal-user-sum/1.^V-hq/` (missed by the main sweep
since it was outside the "root-only .md files" scope) — 9 real files,
all dated 2026-08-29 through 2026-09-01, now filed into `design-docs/`.
**Two of them SUPERSEDE files already in this chapter** — the older
versions are kept alongside as `*-EARLIER-SUPERSEDED.md` rather than
deleted, in case anything in the earlier draft didn't make it into the
newer one:
- `design-docs/HANDOFF-2026-09-01.md` — the newer one describes chat-hai
  fully migrated onto the shared renderer, 4 house-wide bugs fixed, and
  a working global z-order/always-on-top toggle — all genuinely newer
  than the superseded version's status.
- `design-docs/CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md`
  — the newer copy is 867 lines vs. the superseded 225-line draft.

Two more of the 9 are real, still-relevant, standalone reference docs
worth flagging directly (not just buried in the holding pen):
- `design-docs/sep-1-events-SOS.md` — a ranked, copy-pasteable "what's
  actually left to code" list for db-hq/events-hq commands (Tier 1
  registry-only edits through Tier 4 real-system work), plus a
  step-by-step guide for a low-context agent to add a Common Event.
- `design-docs/sep-1-grok.md` — explains the real events/pal-script
  architecture as the template the other 13 db-hq tabs (Actors,
  Classes, Items, etc.) should be measured against; names the concrete
  next audit step (none of those tabs have been checked yet for how
  close they are to this standard).

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
