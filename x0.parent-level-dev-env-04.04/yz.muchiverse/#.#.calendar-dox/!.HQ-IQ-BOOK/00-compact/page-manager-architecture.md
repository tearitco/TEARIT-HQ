# Page Manager Architecture — RPG Maker GameManager Pattern for TEARIT-HQ

**Design Pattern (2026-09-27):** Central manager process for each page/book, mirroring RPG Maker's global manager architecture (`$dataMap`, `$gameMap`, `$gameParty`, etc.).

## RPG Maker Reference (rpg_managers.js)

**Key managers:**
- **DataManager** — loads static game data (Actors.json, Items.json, CommonEvents.json, MapInfos.json, Map*.json)
- **SceneManager** — main loop; manages scene stack and transitions
- **GameManager** (implied, distributed across $game* globals) — manages runtime state
- **ConfigManager** — config/settings
- **ImageManager** — asset loading
- **AudioManager** — sound playback
- **TextManager** — text/messages

**Global state variables:** `$dataActors`, `$dataItems`, `$dataMap`, `$gameMap`, `$gamePlayer`, `$gameSwitches`, `$gameVariables`, `$gameParty`, etc.

**Architecture:** Static managers (no instances); data and state globally accessible; main loop calls manager updates each frame.

## TEARIT-HQ Equivalent: Page Manager via `.pal`

**One manager per page** (or one per book, managing active page at a time).

### Files & State Ledgers

```
pages/{page_id}/
├── state/
│   ├── page_state.pdl          (static config: entities, triggers, etc.)
│   ├── entities_live.txt        (append-only: entity positions, states)
│   ├── world_events.txt         (append-only: what happened this session)
│   ├── animation_queue.txt      (append-only: pending animations)
│   └── page_manager.cursor      (marker: last-read line of above files)
├── event_pkg/
│   ├── common_events.pal        (house-wide or page-wide scripts)
│   └── event_triggers.pdl       (what fires which event)
└── manager/
    └── page_manager.pal         (the central loop; manages all above)
```

### Page Manager Loop (`.pal`)

```
page_manager.pal:
  1. tick = current frame #
  2. read page_state.pdl (static entities, settings)
  3. poll entities_live.txt from cursor (new entity positions/state)
     - If entity moved, decide: entity-initiated or manager-initiated?
     - If manager-initiated: run animation_queue entry, then clear it
  4. poll world_events.txt from cursor (triggers: quest complete, NPC arrive, etc.)
  5. for each trigger: call matching common_event from common_events.pal
  6. broadcast results back to entities_live.txt (entity state changes)
  7. advance cursor markers
  8. goto 1 (next frame)
```

### Hybrid Control

**Entity moves itself:**
- Entity clicks drag, arrow-keys, direct action → writes to `entities_live.txt`
- Page manager sees movement, logs it, runs any triggers (e.g. "entered zone")

**Manager moves entity:**
- Manager writes to `animation_queue.txt` + calls `move_entity_with_animation.sh`
- Entity reads waypoints, animates
- Entity writes final position back to `entities_live.txt`
- Manager sees final position, doesn't re-animate (idempotent check: "already did this")

**Common Events:**
- Stored as `.pal` functions in `common_events.pal` (reusable scripts)
- Manager calls them by name on triggers
- Can be global (shared across pages) or per-page

### Cursor Tracking

Each manager reads three append-only ledgers, advancing a cursor through each:
- `entities_live.txt` cursor = "up to which line have we processed entity state?"
- `world_events.txt` cursor = "which triggers have we handled?"
- `animation_queue.txt` cursor = "which animations did we start?"

Prevents re-processing same events; ledger never shrinks (audit trail).

## Implementation Path

1. **Phase 1 — Single-Page Manager (Immediate):**
   - Create `pages/{page_id}/manager/page_manager.pal`
   - Read `entities_live.txt` for position changes
   - Call `move_entity_to.sh` for manager-initiated movement
   - Log world state to `world_events.txt`

2. **Phase 2 — Common Events (Early):**
   - Write `common_events.pal` with reusable event scripts
   - Page manager calls events on triggers ("enter zone", "defeat enemy", etc.)
   - Events can spawn entities, start animations, etc.

3. **Phase 3 — Multi-Page Book Manager (Later):**
   - One manager can manage multiple pages (page stack like SceneManager's scene stack)
   - Handles page transitions, shared globals across pages

4. **Phase 4 — Plugins/Extensibility (Much Later):**
   - Plugin system: third-party `.pal` scripts that hook into manager
   - Mirrors RPG Maker's plugin architecture

## Why This Works

- ✅ **Single source of truth:** manager owns world state
- ✅ **Decoupled:** entities don't need to coordinate with each other
- ✅ **Scriptable:** common events are data-driven (`.pal` files, not C code)
- ✅ **Replayable:** append-only ledgers are audit trail; can replay session
- ✅ **Hybrid:** entities + manager can both initiate actions (manager arbitrates)
- ✅ **Native to TEARIT-HQ:** uses `.pal`, marker files, ledger conventions already in place

## Comparison: RPG Maker vs. TEARIT-HQ

| Aspect | RPG Maker | TEARIT-HQ |
|--------|-----------|-----------|
| Main loop | SceneManager.update() | page_manager.pal (prisc loop) |
| Game state | $game* globals (JS objects) | entities_live.txt (append-only ledger) |
| Data loading | DataManager.loadDatabase() | page_state.pdl (human-editable config) |
| Events | EventList[] array | common_events.pal (`.pal` functions) |
| Entity logic | Event interpreter + move cmds | Entity-driven + manager coordination |
| Animation | Built-in Move command | move_entity_with_animation.sh + animation_queue.txt |
| Persistence | Save file (JSON) | Files already persistent (state/ dir) |

## Next: Sketch Out First Page Manager

Once confirmed this pattern makes sense, I'll write the first `page_manager.pal` that:
- Runs a simple loop (tick, poll ledgers, call events, advance cursors)
- Handles entity position changes + manager-initiated animations
- Calls common events on simple triggers (zone enter, entity move)
