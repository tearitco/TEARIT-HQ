# PIECECRAFT-WRAITH PHASE 1 TEST SETUP
Date: 2026-07-02
Status: READY FOR TESTING

---

## Setup Complete ✅

All necessary files have been created for Phase 1 testing.

### Files Created:
- ✅ Manager daemon: `manager/+x/piececraft-wraith_manager.+x` (26KB)
- ✅ Render op: `ops/+x/render_map_wraith.+x` (17KB)
- ✅ Map files: `maps/map_01_z0.txt`, `map_01_z1.txt`, `map_01_z2.txt`
- ✅ Layout: `layouts/piececraft-wraith.chtpm` (wired to manager)
- ✅ Session state: `session/state.txt`, `session/xelector_state.txt`
- ✅ Session body: `session/wraith_body.txt`, `session/scene.objects.pdl`
- ✅ Session history: `session/history.txt` (reset and ready)
- ✅ Marker files: `pieces/display/frame_changed.txt`, `pieces/apps/player_app/state_changed.txt`
- ✅ Game map: `session/game_map.txt` (initial render)

---

## How to Test in Wraith

### 1. Start Wraith
```bash
cd /path/to/ZEST-01.00/x0.parent-level-dev-env-02.01/1.TPMOS_c_+rmmp.0102.0027
# Launch Wraith (through usual startup method)
```

### 2. Load Piececraft-Wraith Project
In Wraith project loader:
- Navigate to Wraith hosted projects
- Select: `piececraft-wraith`
- Project should load with manager daemon running

### 3. Expected Initial State
You should see:
```
####################
#..................#
#...>..........R...#    ← xelector at (4,2,0)
#..................#
#.........T........#
#....R.............#
#..................#
#...........T......#
#..................#
####################
[2d_topdown] [Xelector: (4,2,0)] [Tile: .]
```

With buttons:
- [X] Ascend
- [Z] Descend
- [8] Toggle 3D
- [D] Debug
- [ESC] Menu

---

## Testing Checklist

### Basic Movement (Arrow Keys)
- [ ] Press RIGHT arrow → xelector moves right (4,2,0) → (5,2,0)
- [ ] Press LEFT arrow → xelector moves left back
- [ ] Press DOWN arrow → xelector moves down (4,2,0) → (4,3,0)
- [ ] Press UP arrow → xelector moves up back

**Expected:** Xelector `>` symbol moves in rendered map within 100ms

### Z-Level Navigation
- [ ] Press X key → pos_z changes 0→1, different map displayed
  - Z1 has fewer obstacles (only rock at far right)
- [ ] Press X key again → pos_z changes 1→2, mostly empty map
- [ ] Press Z key → pos_z changes 2→1, back to intermediate level
- [ ] Press Z key again → pos_z changes 1→0, back to original map

**Expected:** 
- Map visibly changes at each Z level
- Xelector maintains same x,y position across Z changes
- No crashes or glitches

### Display Mode Toggle
- [ ] Press 8 key → status line changes `[3d_voxel]`
- [ ] Press 8 key again → status line changes `[2d_topdown]`
- [ ] In GL window: 3D view should switch (if GL rendering enabled)

**Expected:** Mode toggles instantly, visible in status line

### Debug Mode Toggle
- [ ] Press D key → should show additional debug info line
- [ ] Press D key again → debug line disappears

**Expected:** Debug display toggles without affecting map

### Input Responsiveness
- [ ] Press arrow keys rapidly (20+ keys/second)
- [ ] Mix movement with Z-level changes
- [ ] Alternate between buttons and key presses

**Expected:** No crashes, no missed inputs, consistent behavior

### State File Verification
Check `session/state.txt` during testing:
```bash
tail -f session/state.txt
```

After each input, verify:
- [ ] `xel_x`, `xel_y`, `xel_z` update correctly
- [ ] `active_z` matches current Z level
- [ ] `display_mode` matches toggle state
- [ ] `debug_mode_on` matches debug toggle
- [ ] `game_map` shows correct Z level (map_01_z0.txt, map_01_z1.txt, etc.)

### Manager Execution
Check `session/debug_log.txt` to verify manager is running:
```bash
tail -f session/debug_log.txt
```

Should show:
- [ ] Manager startup message
- [ ] Key received messages for each input
- [ ] Timestamp entries

---

## Success Criteria for Phase 1

| Test | Target | Status |
|------|--------|--------|
| Manager starts without errors | ✓ | __ |
| Render op creates valid maps | ✓ | __ |
| Arrow keys move xelector | < 100ms | __ |
| Z-level changes work | Instant | __ |
| Mode toggle works | Instant | __ |
| Debug toggle works | Instant | __ |
| No crashes on rapid input | 0 crashes | __ |
| State file updates correctly | 100% | __ |
| Map display updates match input | Real-time | __ |

---

## Troubleshooting

### Problem: No response to input
**Check:**
1. Is manager process running? `ps aux | grep piececraft-wraith_manager`
2. Is history.txt being created? `ls -l session/history.txt`
3. Are key codes being appended? `tail -f session/history.txt` while pressing keys

### Problem: Xelector doesn't move
**Check:**
1. Is render op working? `./ops/+x/render_map_wraith.+x 5 2 0 map_01`
2. Are state files updating? `tail -f session/state.txt`
3. Check manager debug log: `tail -f session/debug_log.txt`

### Problem: Map shows static or old state
**Check:**
1. Is game_map.txt being regenerated? `ls -l session/game_map.txt` (timestamp should update)
2. Is frame_changed.txt marker being touched? `ls -l pieces/display/frame_changed.txt`
3. Layout variable substitution: Check that `${game_map}` is being replaced

### Problem: Crashes or segmentation faults
**Check:**
1. Compile with debugging: `gcc -g -O0 ... piececraft-wraith_manager.c`
2. Run under gdb: `gdb ./manager/+x/piececraft-wraith_manager.+x`
3. Check for buffer overflows in manager paths (warnings noted during compile)

---

## Next Steps After Phase 1 Testing

Once Phase 1 tests pass:
1. Commit working state to git
2. Create Phase 2 (Z-levels already work, now add map file variety)
3. Implement Phase 3 (UI polish)

---

## Technical Reference

### Manager Input Flow
```
User presses key
  ↓
Wraith appends to session/history.txt
  ↓
Manager reads history.txt (input_thread)
  ↓
Manager calls route_input(key_code)
  ↓
Updates xel_x, xel_y, or xel_z
  ↓
Calls render_map() → run render op
  ↓
Calls save_state_txt() → write state.txt
  ↓
Calls trigger_render() → touch markers
  ↓
Parser notices state changed
  ↓
Parser substitutes ${game_map}, ${xel_x}, etc.
  ↓
Layout re-renders
  ↓
User sees updated map in < 100ms
```

### File Locations
- Manager source: `manager/piececraft-wraith_manager.c`
- Manager binary: `manager/+x/piececraft-wraith_manager.+x`
- Render op source: `ops/src/+x/render_map_wraith.c`
- Render op binary: `ops/+x/render_map_wraith.+x`
- Layout: `layouts/piececraft-wraith.chtpm`
- State: `session/state.txt`
- History: `session/history.txt`
- Rendered map: `session/game_map.txt`

---

**Ready to test! Launch Wraith and load piececraft-wraith project.**
