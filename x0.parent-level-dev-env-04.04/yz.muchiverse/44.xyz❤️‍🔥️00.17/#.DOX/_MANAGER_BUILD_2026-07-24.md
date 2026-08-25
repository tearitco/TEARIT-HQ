# HANDOFF: Manager Loop Implementation (In Progress)

**Date**: 2026-07-24  
**Task**: Build Manager polling loop for xo project  
**Status**: BUILDING

## Problem Statement

Arrow keys were being captured and converted to keycodes in `interact_relay.txt`, but **no Manager was polling for input**. The game loop was broken because:

- ✗ No polling thread reading history.txt
- ✗ No route_input() handler for arrow keys
- ✗ No move_entity operations being called
- ✗ No frame rendering or marker signaling
- ✗ No state syncing

Reference findings: `./FINDINGS_2026-07-24.md`

## Architecture Required

```
Input Flow:
  keyboard_input.c → pieces/apps/player_app/history.txt
                  ↓
          [Manager polling loop] (16ms cycle)
                  ↓
          Parse KEY_PRESSED lines
                  ↓
          route_input(keycode)
                  ↓
  Arrow keys → move_entity.+x xlector <direction>
                  ↓
          Update pieces/xlector/state.txt
                  ↓
          Call render_map.+x
                  ↓
          Update current_frame.txt
                  ↓
          Pulse pieces/display/frame_changed.txt
                  ↓
          UI detects marker, redraws
```

## What Needs to Be Built

### 1. Manager Loop (C or Prisc)
**Location**: TBD (likely `projects/gem-xo/manager/xo_manager.c`)

**Responsibilities**:
- Poll history.txt every ~16ms
- Track file position to avoid re-reading
- Parse lines with KEY_PRESSED format
- Call route_input() for each keycode
- Sync state after moves
- Trigger renders

**Key Functions Needed**:
- `void route_input(int keycode)` — route arrow keys to operations
- `void poll_history()` — read new lines from history
- `void move_entity(const char* piece, const char* direction)` — exec move op
- `void sync_state()` — update mirror files
- `void pulse_frame()` — touch frame_changed.txt marker

### 2. move_entity.+x Operation
**Check if exists**: Look in `pieces/apps/player_app/ops/` or similar

**If missing, create**:
- Takes arguments: `piece_id direction`
- Reads current pos_x, pos_y from state.txt
- Increments based on direction (left/right/up/down)
- Writes back to state.txt
- Returns status

### 3. render_map.+x Operation
**Check if exists**: Should already exist in `pieces/apps/player_app/ops/`

**Needed for**: Reads all pieces at current location, writes to current_frame.txt

### 4. Configuration Files
- `pieces/apps/player_app/manager/state.txt` — project state
- `projects/gem-xo/manager/state.txt` — game state (if needed)

## Code References

**Existing Manager pattern** (use as template):
- File: `projects/fuzz-op/manager/fuzz-op_manager.c`
- Polling loop: line 886
- route_input(): line 539
- State sync: sync_focus(), perform_mirror_sync()

**Key code snippets to port**:
```c
// Polling loop (approximate)
while (1) {
    FILE *hf = fopen(history_path, "r");
    if (hf) {
        fseek(hf, last_pos, SEEK_SET);
        int key;
        while (fscanf(hf, "%d", &key) == 1) {
            route_input(key);
        }
        last_pos = ftell(hf);
        fclose(hf);
    }
    usleep(16000);  // 16ms = ~60 FPS
}

// route_input conversion
void route_input(int key) {
    if (key == 1000) move_entity("xlector", "left");
    else if (key == 1001) move_entity("xlector", "right");
    else if (key == 1002) move_entity("xlector", "up");
    else if (key == 1003) move_entity("xlector", "down");
    else if (key == 27) exit_map_control();  // ESC
}

// Marker pulse
void pulse_frame() {
    FILE *f = fopen("pieces/display/frame_changed.txt", "a");
    if (f) { fputc('G', f); fclose(f); }
}
```

## Files to Check/Create

- [ ] `projects/gem-xo/manager/xo_manager.c` — Create or verify
- [ ] `pieces/apps/player_app/ops/move_entity.+x` — Check if exists
- [ ] `pieces/apps/player_app/ops/render_map.+x` — Check if exists
- [ ] `pieces/apps/player_app/manager/state.txt` — Initialize if missing
- [ ] `pieces/apps/player_app/history.txt` — Should exist (input sink)
- [ ] `pieces/display/current_frame.txt` — Should exist (render output)
- [ ] `pieces/display/frame_changed.txt` — Should exist (marker file)

## Testing Plan

Once Manager is built:
1. Start the Manager daemon in background
2. Press arrow key in UI
3. Check that:
   - `history.txt` gets new `KEY_PRESSED` line
   - `xlector/state.txt` pos_x or pos_y changes
   - `current_frame.txt` updates with new position
   - `frame_changed.txt` marker grows (touched)
   - UI shows piece moved

## If Context Runs Out

Next person should:
1. Read FINDINGS_2026-07-24.md for architecture overview
2. Check if Manager was created in `projects/gem-xo/manager/`
3. If not: port from fuzz-op_manager.c and adapt for this project
4. If yes: verify it's polling and route_input is wired to movement ops
5. Test the cycle above
