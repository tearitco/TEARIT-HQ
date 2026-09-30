# Entity Animation Pattern — "Play Sequence" via Position Ledger

**Design principle (2026-09-27):** Entity animations (movement, NPC pathfinding, etc.) should not use a custom animation system. Instead, leverage the entity's own game engine loop by writing sequential position updates to the same ledger/file the entity reads from during normal play.

## Pattern

1. **Pathfinding op** generates a list of waypoints from current position to target.
2. **Sequential writes** to the entity's position ledger (e.g., `desktop_pos.txt` or an animation-queue file).
3. **Entity's own game loop** reads each position update and renders it in-frame ("play sequence").
4. Entity stays **within the game engine's own constraints** — no external animation system, no custom frame timing.

## Why This Works

- **No custom animation code:** the entity's own renderer already handles position→visual smoothly.
- **Scalable:** works for NPCs, enemies, interactive elements — anything the entity can already do.
- **Ledger-driven:** the same position source as Move/placement events; no new state channels.
- **Game-engine-native:** animations respect physics, collision, game state — they're not external overlays.

## Implementation Pattern

```
move_entity_with_animation.sh
├── reads: current position (desktop_pos.txt)
├── calls: pathfinding_op.sh (takes old_pos, new_pos, returns waypoints)
├── writes: each waypoint to entity's position ledger
└── entity's game loop: reads and renders each step
```

The **pathfinding op is not hardcoded** — it's pluggable:
- `PATHFIND_OP` env var, or
- `.pdl` config key (e.g., `ENTITY | pathfind_op | /path/to/op.sh`)
- Simple default: straight line + step size (linear interpolation)
- Complex: A*, tile-constrained, avoidance, etc. — same interface

## Example: Linear Interpolation (Default)

Pseudocode for a dead-simple pathfinding op:
```sh
# pathfind_linear.sh <old_x> <old_y> <new_x> <new_y> <step_size> <output_ledger>
# Writes waypoints to output_ledger, one per line: x=N y=N
```

The entity's game loop reads and renders each in sequence; the motion appears smooth.

## When To Use

- Any visible entity movement (players, NPCs, objects).
- Any time a single "move" command should show motion, not teleport.
- Anything that benefits from the entity's own physics/constraints.

## NOT In This Pattern

- Camera pans / pan operations (out-of-engine effects).
- UI element animations (those stay outside entities).
- Non-game-loop timing (this assumes the entity updates every frame).
