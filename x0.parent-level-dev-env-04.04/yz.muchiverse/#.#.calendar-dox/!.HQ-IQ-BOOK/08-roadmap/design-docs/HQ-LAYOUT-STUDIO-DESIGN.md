# HQ-LAYOUT-STUDIO — Visual Widget-Layout Editor (seed doc)

**Status: 🔴 PROPOSED 2026-09-29. Not designed in detail, not built.**
This is a seed doc capturing the idea as the owner stated it, so the
real design work has a fixed starting point rather than being
re-derived from chat history later. Expand this doc before writing any
code against it.

**Origin:** raised while correcting a miscommunication about the
teru-test "babysitter" entity (see
`RUSSIAN_DOLL_HOUSE_DESIGN/AGENT_ROADMAP_ANSWERS.md` §10) — the
babysitter needs its own custom GUI, and that GUI should be
user-customizable. The owner generalized that immediately: a visual
layout editor is a bigger, reusable idea, not something built once for
one entity.

---

## What it is

A toy/widget that lets a user **visually create and edit layouts** for
their own custom widgets. The babysitter's dashboard (feeding/cleaning/
fighting status for managed terumon, per
`08-roadmap/design-docs/robot-chat/` sibling docs and the teru-test
concerns) is the **first real use case**, not the only one — any
future entity that wants a custom GUI should be able to build it here
instead of a bespoke `.chtpm` hand-authored from scratch every time.

**Working name only.** "HQ-LAYOUT-STUDIO" was the owner's own
first-pass name, explicitly flagged as "or something short?" — pick a
shorter name if one comes up naturally during real design; don't treat
the current name as locked.

## Where it lives

**As a sub-entity inside the ☁️ (cloud) entity** already proposed in
the DSR-test section of `0.my-concerns.md`:

> "there should be some place they can ultimately be drag dropped
> from... in the abstract... maybe 1 ☁️, or 📦️ thing holding things
> like that for game can sit in upper right corner of screen"

This is a real, existing proposal (not yet built) for a single
always-visible entity that holds draggable abstract things. HQ-LAYOUT-STUDIO
should NOT be a new top-level toy/window — it should be one sub-entity
living inside that one ☁️ entity, alongside whatever else ends up
stored there. Building the ☁️ entity itself is a prerequisite; it does
not currently exist.

## How authoring works

Confirmed directly by the owner, two integration points, both reusing
existing house conventions rather than inventing new ones:

1. **A layout can read an existing event page.** The visual editor
   should be able to point at a real, already-compiled event page (the
   same `event.ir.pdl` → `event.pal` → `cmd_N.sh` shape every other
   event uses, per `ROBOT-CHAT-BLUEPRINT.md` §2.2/§2.3 for a real
   worked example) and reflect what it does.
2. **A layout can have an event drag-and-dropped into it, as usual.**
   Same drag-and-drop convention every other event placement in this
   house already uses — no new interaction model. The novelty here is
   the **visual layout editor itself** (arranging widgets, sizing
   panels, choosing what shows where), not how an event gets attached
   to a layout once it's built.

## Real precedent to build from, not re-derive

- **Generic `.chtpm`/CSS rendering** (`CENTROID_GOLD_STD.md`,
  `khtpm-house-standards` skill) — any layout this studio produces
  must still be a real, parseable `.chtpm`+CSS document rendered by the
  shared `khtpm_core_render.c`, same as every other HQ app. A visual
  editor's job is to **generate** that file correctly, not to invent a
  second rendering path.
- **The event COMMAND registry**
  (`#.ref/menu/event_commands.registry.pdl`) — this is what a
  drag-dropped-in event actually resolves to at runtime; the visual
  editor's drop target should write into this same real, zero-recompile
  registry shape, not a bespoke format.
- **The host-context bridge** (`kh_inventory_host_dir()` /
  `MUCHI_TARGET_ENT` in `khtpm_events_hq_manager.c`) — relevant if a
  layout built for one entity (the babysitter) needs to resolve which
  entity it's actually showing data for when placed/viewed from
  another entity's context.

## Open questions (real, not yet answered)

- Does the ☁️ entity get built first as its own small, scoped feature,
  or alongside HQ-LAYOUT-STUDIO's first real use case (the babysitter
  dashboard)? Recommend building the ☁️ entity first, minimally (just
  enough to hold one sub-entity), so HQ-LAYOUT-STUDIO has a real place
  to live from day one rather than being retrofitted into it later.
- What's the actual editing interaction — drag-resize panels, a
  palette of widget types, live preview against real data? Not
  specified yet; needs its own design pass once the ☁️ entity and the
  babysitter's own needs are concrete enough to design against.
- Should HQ-LAYOUT-STUDIO's output be per-instance (each babysitter
  copy gets its own saved layout) or per-template (one shared layout,
  same template/delta split `ROBOT-CHAT-BLUEPRINT.md` §3.2 already
  proposes for personality banks)? Worth deciding together with that
  doc rather than separately, since they're the same shape of problem.

---

## Update 2026-10-06: where this goes next

The owner wants layouts made here **rendered inside the user's game** (the pc-hq board window) and
on the livedesk, saved by name and called from the pc-hq **Events** menu, working with pal, and
editable in an x11-hq editor that an **agent** can use as well as a human. The plan, the placement
and chrome mechanism that already exists (the canvas overlay strip the pc-hq hotbar uses), the phases
and a command-first studio design (`layout_op` CLI, thin editor window over it) are in
`18.pc-hq/IN-GAME-LAYOUTS-PLAN.md`. This doc stays the seed; that plan is the current design.

---

*Seed doc written 2026-09-29 from a direct owner correction/clarification
during the attrition-model roadmap thread. Expand or supersede this
doc once real design work starts — do not treat this as a finished
spec.*
