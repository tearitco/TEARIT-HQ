# Page Manager Sketch — First Implementation

**Goal:** Build a working page_manager.pal that manages a single page's state via ledgers and common events.

## Files to Create

```
pages/{page_id}/
├── manager/
│   ├── page_manager.pal              (main loop - NEW)
│   └── common_events.pal             (event functions - NEW)
├── state/
│   ├── page_state.pdl                (config - maybe from page template)
│   ├── entities_live.txt             (NEW - append-only)
│   ├── world_events.txt              (NEW - append-only)
│   ├── animation_queue.txt           (NEW - append-only)
│   └── page_manager.cursor           (NEW - markers)
└── event_pkg/
    └── event_triggers.pdl            (NEW - what fires what)
```

## page_manager.pal Pseudocode

```pal
# page_manager.pal - Main loop for page state management

# Init: load page config, clear/initialize ledgers
page_load

# Main loop (continuous, or triggered per frame)
loop:
  tick = tick + 1
  
  # Poll entities_live.txt
  for each new line in entities_live.txt (since last cursor):
    entity_id, x, y, state = parse(line)
    check_triggers(entity_id, x, y, state)  # zone enter, etc.
  
  # Poll world_events.txt for completed triggers
  for each new line in world_events.txt (since last cursor):
    trigger_name, entity_id, data = parse(line)
    run_common_event(trigger_name, entity_id, data)
  
  # Poll animation_queue.txt for manager-initiated animations
  for each new line in animation_queue.txt (since last cursor):
    entity_id, target_x, target_y, anim_type = parse(line)
    call move_entity_with_animation.sh(entity_id, target_x, target_y)
    write entities_live.txt: entity_id moved (log the result)
  
  # Broadcast results, advance cursors
  advance_cursors()
  
  goto loop
```

## common_events.pal

```pal
# common_events.pal - Reusable event scripts

event_zone_enter:
  zone_id, entity_id = args
  # Do something when entity enters zone
  # Can queue animations, trigger other events, etc.
  return

event_entity_moved:
  entity_id, old_x, old_y, new_x, new_y = args
  # Log movement, check if NPC should react, etc.
  return

event_quest_complete:
  quest_id, entity_id = args
  # Mark quest done, maybe trigger NPC dialogue
  return
```

## event_triggers.pdl

```
TRIGGER         | CONDITION          | EVENT
entity_moved    | any entity moves   | event_entity_moved
zone_enter      | entity enters zone | event_zone_enter (zone_id, entity_id)
quest_complete  | quest finished     | event_quest_complete (quest_id, entity_id)
```

## Data Flow Example

**Scenario: Player moves entity, NPC should react**

1. User drags entity from (100, 200) to (150, 200)
2. Entity writes: `entities_live.txt: entity_player | x=150 | y=200 | state=idle`
3. page_manager.pal reads new line, checks triggers
4. Matches `zone_enter` trigger if (150, 200) is in a zone
5. Writes to world_events.txt: `zone_enter | npc_01 | should_react=true`
6. Calls `event_zone_enter(zone_id=2, entity_id=player)`
7. That event might queue an animation: `animation_queue.txt: npc_01 | x=160 | y=200`
8. Next loop: page_manager sees animation queue, calls `move_entity_with_animation.sh npc_01 160 200`
9. NPC slides to (160, 200)
10. Entity writes back: `entities_live.txt: npc_01 | x=160 | y=200 | state=idle`
11. Cursors advance, audit trail is complete

## Test Case

**File:** `test_page_001/`
- Simple page with one player entity, one NPC, one zone
- Drag player into zone → NPC should slide toward player
- Verify ledgers log each step

## Implementation Order

1. ✅ Write page_manager.pal (main loop + ledger polling)
2. ✅ Write common_events.pal (stub functions)
3. ✅ Write event_triggers.pdl (basic triggers)
4. Create test_page_001/ with test entities
5. Run page_manager.pal on test page
6. Verify ledgers + entity animations work
7. Iterate on event logic

## Known Unknowns

- How long should page_manager sleep between ticks? (16ms for 60fps? Or event-driven?)
- Should cursor be per-file, or one cursor with line ranges?
- How to handle errors in common_event scripts? (continue, halt, log?)
- Should page_manager be killable/restartable without losing state?
