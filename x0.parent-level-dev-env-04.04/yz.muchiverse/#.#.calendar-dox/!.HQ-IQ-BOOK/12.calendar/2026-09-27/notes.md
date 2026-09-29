# 2026-09-27 — three refinements: placer persistence, nav wraparound, entity animation pattern

## What landed today

- **Placer highlight stays visible when pointer moves away** — Motion
  event handler now only clears `kb_active` when NOT in `has_view` mode
  (unlimited/full-screen palette-stamp tool). In Move mode, the highlight
  persists until Escape or placement, independent of pointer movement.
  Direct report fix: "when i move mouse away 'placer' disappears."
  Commit `79ecb47c`.

- **Nav-index bidirectional wraparound** — `kh_nav_step()` now always
  wraps at boundaries: going backward below 1 wraps to max index, going
  forward above max wraps to 1. Applies everywhere (taskbar, context
  menus, any nav-enabled page). Previously only active when
  `g_default_scope_confine` was true. Commit `0ffc4967`.

- **Entity animation pattern + initial implementation** (design-forward,
  not yet integrated into Move):
  - **Design doc** (`entity-animation-pattern.md`): standing pattern for
    NPC/entity animations — game-engine-native, not a custom overlay.
    Pathfinding op generates waypoints, written sequentially to entity's
    position ledger, entity's game loop reads and renders each step.
    Pluggable pathfinding ops (not hardcoded), same interface for linear,
    A*, tile-constrained, etc. Direct user vision: "entity slides... this
    will be used alot for npcs... path finder writes updates position the
    same way move from event would."
  - **`pathfind_linear.sh`** (new op): default pathfinding — linear
    interpolation, straight-line path, configurable step size. Simple
    reference implementation, can be replaced.
  - **`move_entity_with_animation.sh`** (new script): writes waypoints
    from source to target, updates entity's position file for each step
    with frame-rate delays (50ms), lets entity's game loop animate
    smoothly. Pluggable pathfinding (PATHFIND_OP env var). Ready for
    integration into Move or use standalone.
  - Commit `f5544f59`.

- Index entry added to `!.HQ-IQ-BOOK-MAP🗺️` (00-compact section).

- Everything pushed to `origin/claude`.

## Open concerns (flagged, not yet investigated)

- **Entity position persistence on book pages** — User reported: "when i
  use to move stuff on the book:page, it would remember the last
  positions of entities even after reset and stuff. did that break or
  something? do i have to save manually?" Current behavior unclear;
  whether positions are still persisted to `desktop_pos.txt` across
  restarts and resets needs verification. Likely related to whether
  entity instance files are being saved before restart/reset, or whether
  a persistence layer was inadvertently broken.

## Real, open next steps

1. **Integrate animation into Move** (optional enhancement) — if desired,
   wire `move_entity_on_desk.sh` to use `move_entity_with_animation.sh`
   as an option (env var or `.pdl` toggle). Can also be left as separate
   tools for different use cases (instant vs. animated).

2. **Test animation with real entities** — build a test entity that reads
   position ledger on every frame and verify smooth rendering of
   waypoints. May need to tweak frame-rate delays (currently 50ms per
   waypoint).

3. **Pathfinding enhancements** (future) — A* for complex paths, tile-
   constrained for grid-based games, collision avoidance, etc. — each as
   a new pluggable op, same interface as `pathfind_linear.sh`.

4. **Investigate entity persistence concern** — verify whether position
   saving still works across resets on book pages; may require looking
   at how entity instances are finalized on shutdown/reset.

5. `use` (`act_row.sh`) — still a plain "recorded" no-op.

6. `attack` — still hardcoded demo, not per-entity logic.

7. Next real focus (when ready): AI-driven agent/gameplay work — use
   `move_entity_to.sh` (no-UI direct position) for scripted/agent
   movement; animation can run separately if desired.
