# Entity Position Persistence — read-once model + file-based state

**Status (2026-09-27):** Entity positions are saved persistently but only read at startup. Animation requires architectural changes to enable frame-by-frame updates.

## Current Model

### Read-Once at Startup
- `khtpm_entity.c`'s `read_initial_pos()` reads `desktop_pos.txt` **exactly once** when the entity window opens
- Converts REFERENCE px to screen px for the current display
- No continuous polling; file changes after startup are not auto-reflected

### Saves on Every Position Change
- `write_pos()` updates `desktop_pos.txt` whenever entity moves:
  - Mouse drag
  - Arrow-key nudge
  - Click-to-place via placer
  - Direct `move_entity_to.sh` calls
- Converts screen px back to REFERENCE px (display-agnostic)
- Always writes x/y/z on every update (z unchanged unless entity changes layer)

### Persistence Across Restarts
✅ **Works as designed:** positions saved to `desktop_pos.txt` persist across:
- Session restart (entity relaunched, reads new position from disk)
- Desktop reset (entity files preserved, not deleted)
- Monitor change (REFERENCE px format means position stays semantically same)

## Why Only Read-Once?

1. **Simplicity:** avoids per-frame file I/O (polling would be expensive)
2. **Event-driven:** position changes are already user-initiated (clicks, drags, keys)
3. **Atomicity:** write happens immediately after user action; no stale reads

## Animation Challenge

The "trace path" / sliding animation pattern requires **frame-by-frame position updates** that violate the read-once model. Current options:

### Option A: Keep Entity Alive, Poll Position (Ideal but Requires Code Change)
- Modify `khtpm_entity.c`'s main render loop to check if `desktop_pos.txt` changed each frame
- If changed, re-read and re-render at new position
- **Cost:** adds per-frame file I/O; needs timestamp/marker check to avoid constant reads
- **Benefit:** smooth animation, no process cycling

### Option B: Write Waypoints, Rapid Restart (Current Workaround)
- Write each waypoint to `desktop_pos.txt`
- Kill + relaunch entity for each waypoint
- Achieves animation effect but with visible frame gaps (each restart ~100ms)
- **Cost:** overhead, potential visual stuttering
- **Benefit:** no core changes; works now; `move_entity_with_animation.sh` already implements this

### Option C: Separate Animation State File
- Keep `desktop_pos.txt` as final-position record only (never mid-animation)
- Add a separate `animation_queue.txt` or `_pos_animation.txt` that entity reads every frame
- Entity reads animation file if present; otherwise reads `desktop_pos.txt` normally
- **Cost:** new state channel; slightly more complex
- **Benefit:** cleanest design; doesn't break existing position-update contract

## Current Recommended Direction

**For now:** use Option B (waypoint + rapid restart) with `move_entity_with_animation.sh` for Move animations. Low friction, works immediately, documented clearly.

**Future:** implement Option C if smooth animation becomes critical (N NPCs animating simultaneously). Option A requires deeper khtpm_entity.c refactoring and would need per-frame marker-based dirty-check to avoid I/O thrashing.

## Persistence Check (2026-09-27 Investigation)

**Verified working:** entity positions persist to `desktop_pos.txt` and reload correctly on restart. All position-change codepaths (`write_pos()`) update the file. Concern about "book:page persistence" may have been about:
1. Entities placed on pages within a book-stack session and whether they persist after the book closes/reopens
2. Or general position-save guarantee (now confirmed working)

**Status:** ✅ Persistence model is as designed and functional.
