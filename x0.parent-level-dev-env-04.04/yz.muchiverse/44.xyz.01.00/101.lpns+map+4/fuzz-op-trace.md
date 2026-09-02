================================================================================
FUZZ-OP TRACE: x0.moke WORKING vs 01.lpns+map+4 BROKEN
================================================================================
Document: Comparing working fuzz-op (x0.moke-pet-project-04.04) interact,
xlector, possession, and dynamic menus against broken 01.lpns+map+4.
================================================================================

## 1. ARCHITECTURE COMPARISON

### x0.moke fuzz-op (WORKING)
```
keyboard_input → history.txt ──┬── parser (circular nav + cli_io + INTERACT relay)
                               └── fuzz-op_manager.c (reads history.txt, routes to move_entity/scan/collect/place)
                                      └── writes to interact_relay.txt when INTERACT active? NO!
                                      └── moves xlector/entity position directly via move_entity.c
```

Key: fuzz-op_manager.c does NOT use interact_relay.txt for movement.
It reads history.txt directly and calls move_entity.c via fork/exec.
INTERACT mode is handled by chtpm_parser.c writing keys to interact_relay.txt,
but fuzz-op_manager.c ALSO reads history.txt and handles arrow keys itself.

### 01.lpns+map+4 (BROKEN)
```
keyboard_input → history.txt ──┬── chtpm_parser_pal.c (reads history.txt, handles nav/INTERACT)
                               └── game_manager.c (reads history.txt, maps keys to game actions)
```

Key: TWO consumers reading the SAME file = race condition. Parser reads a key,
game_manager reads the SAME key. Both consume it. Double processing.

## 2. INTERACT MODE

### x0.moke fuzz-op (WORKING)
Layout: `fuzz-op.chtpm` line 3:
```xml
<interact src="pieces/apps/player_app/history.txt" />
```
This sets `interact_history_path` in chtpm_parser.c.
When INTERACT button is clicked:
  1. Parser sets `active_index = focus_index` (line 3025)
  2. Subsequent keys hit the INTERACT branch (line 3039-3054)
  3. Arrow keys → inject_raw_key(1000-1003) → writes to interact_history_path
  4. ESC → exits INTERACT mode (line 3040-3045)

BUT ALSO: fuzz-op_manager.c reads history.txt directly (line 886) and handles
arrow keys itself (line 790-853) by calling move_entity.c. So movement works
EVEN WITHOUT the parser's INTERACT relay.

### 01.lpns+map+4 (BROKEN)
Layout: `lpns_main_menu.chtpm` - only 3 lines:
```xml
<panel>
    <text label="${game_map}" /><br/>
</panel>
```
MISSING: No `<interact>` tag. No `<module>` tag. No buttons.
  - `interact_history_path` = "" (empty)
  - When INTERACT button is clicked (if it existed), inject_raw_key() writes to
    player_app/history.txt (fallback), NOT to interact_relay.txt
  - game_manager.c reads history.txt and maps arrow keys to move_right/left/up/down
  - But game_manager has NO awareness of INTERACT mode vs NAV mode
  - game_manager ALWAYS processes arrow keys as movement, even during nav mode

## 3. XLECTOR AND POSSESSION

### x0.moke fuzz-op (WORKING)
xlector = a piece with `type=xlector` in its state.txt:
```
pieces/xlector/state.txt:
name=Xlector
type=xlector
on_map=1
map_id=map_01_z0.txt
pos_x=12
pos_y=2
pos_z=0
```

Possession flow (fuzz-op_manager.c lines 559-634):
1. User presses Enter/Space while `active_target_id == "xlector"`
2. Manager scans pieces/ directory for any entity at xlector's position
3. Excludes zombies (AI controlled)
4. If found: sets `active_target_id = entity_name` (e.g., "pet_01")
5. Xlector inherits entity's map_id
6. Subsequent WASD/arrows move the POSSESSED entity (not xlector)
7. Press 9/ESC → returns to xlector mode

Shadow xlector sync (line 814-837):
When entity moves, xlector's pos_x/pos_y is updated to match entity position.
This keeps xlector "on top of" the entity for visual rendering.

### 01.lpns+map+4 (BROKEN)
No xlector piece exists. No possession system.
game_manager.c has:
  - `active_target_id` read from state.txt but NEVER set dynamically
  - Arrow keys always move the CURRENT PLAYER (determined by turn order)
  - No concept of "possessing" an entity
  - No shadow sync
  - No entity scanning at position

## 4. DYNAMIC MENUS (piece_methods)

### x0.moke fuzz-op (WORKING)
Layout `fuzz-op.chtpm` line 27:
```xml
<text label="║  " />${piece_methods}<text label=" ║" /><br/>
```

gui_state.txt includes (fuzz-op_manager.c line 347-357):
```c
asprintf(&op_path, "'%s/pieces/chtpm/ops/+x/get_piece_methods_op.+x' %s %s",
         project_root, active_target_id, current_project);
FILE *pf = popen(op_path, "r");
if (pf) {
    char methods[16384];
    size_t n = fread(methods, 1, sizeof(methods)-1, pf);
    methods[n] = '\0';
    pclose(pf);
    fprintf(f, "piece_methods=%s\n", methods);
}
```

Dynamic behavior:
- get_piece_methods_op reads the piece's PDL (Piece Definition Language)
- Returns method list based on piece type and position
- On grass: shows "2. [Scan]" "3. [Collect]" etc.
- On different tiles: methods change
- Methods are executed via pdl_reader → handler binary (fork/exec)

### 01.lpns+map+4 (BROKEN)
Layout `lpns_main_menu.chtpm`:
```xml
<panel>
    <text label="${game_map}" /><br/>
</panel>
```
No ${piece_methods} variable. No dynamic menu system.
game_manager.c has no concept of piece methods.
No PDL system. No get_piece_methods_op.

## 5. KEY FLOW COMPARISON

### x0.moke fuzz-op
```
keyboard_input → history.txt
                    │
         ┌──────────┴──────────┐
         │                     │
    parser (process_key)   fuzz-op_manager (poll_history)
         │                     │
    Nav: move focus         route_input(key)
    cli_io: type text          │
    INTERACT: inject_raw_key   ├── arrow/WASD → move_entity.c (fork/exec)
         │                     ├── Enter on xlector → scan pieces/ → possess
         │                     ├── 2-9 → pdl_reader → handler
         │                     ├── x/z → Z-level change
         │                     └── 6 → emoji toggle
         │
    compose_frame() ←── triggered by K\n marker from process_key
```

### 01.lpns+map+4
```
keyboard_input → history.txt
                    │
         ┌──────────┴──────────┐
         │                     │
    parser (process_key)   game_manager (poll_history)
         │                     │
    Nav: move focus         keycode_to_action()
    INTERACT: inject_raw_key    │
    cli_io: type text       arrow → game_turn_input (move)
         │                 w → game_turn_input (word)
    compose_frame()        e → game_turn_input (end_turn)
         │                 ESC → ignored (just logs)
    MISSING: <interact> tag
    MISSING: <module> tag
    MISSING: buttons for interact
```

## 6. ROOT CAUSES OF FAILURE

### Issue 1: Layout Missing Tags
`lpns_main_menu.chtpm` has NO:
- `<module>` tag → game_manager never launched by parser
- `<interact>` tag → interact_history_path never set
- `<button>` elements → no INTERACT button to click
- `<cli_io>` → no text input

### Issue 2: Dual Consumer Race Condition
Both parser and game_manager read `history.txt`. Parser processes keys for
navigation/INTERACT. Game_manager processes keys for game actions.
Both consume the same keys. Result: keys processed twice or skipped.

### Issue 3: game_manager Has No INTERACT Awareness
game_manager.c always treats arrow keys as movement actions.
It has no concept of:
- INTERACT mode (where keys should go to interact_relay.txt)
- NAV mode (where keys should move focus, not game pieces)
- CLI_IO mode (where keys should be typed as text)

### Issue 4: orchestrator Uses system() for Launch
orchestrator.c uses `system("./system/game_manager &")` which doesn't
track PIDs. On Ctrl+C, game_manager becomes orphaned.
Bible §3: NEVER use system(). ALWAYS use fork()/exec()/waitpid().

### Issue 5: game_manager Uses system() for Ops
game_manager.c line 88: `int ret = system(cmd);`
game_manager.c line 92: `system("./ops/game_compose_frame");`
Both violate Bible §3.

## 7. FIX PLAN

### Fix 1: Update lpns_main_menu.chtpm
Add `<module>`, `<interact>`, `<button>` elements:
```xml
<panel time_reactive="true">
    <module>system/prisc+x pal/main_loop_chtpm.pal</module>
    <interact src="pieces/apps/player_app/history.txt" />
    <text label="╔═══════════════════════════════════╗" /><br/>
    <text label="║  LPNS - Map Variant              ║" /><br/>
    <text label="${game_map}" /><br/>
    <text label="║  " /><button label="Move (arrows)" onClick="INTERACT" /><text label="                    ║" /><br/>
    <text label="║  [KEY]: ${last_key}               ║" /><br/>
    <text label="╚═══════════════════════════════════╝" /><br/>
</panel>
```

### Fix 2: game_manager.c — Stop Polling history.txt
game_manager should NOT read history.txt directly.
Instead, it should:
- Poll interact_relay.txt for INTERACT mode keys
- Poll gui_state.txt for CLI_IO input
- Poll state_changed.txt for game state changes
- Use fork/exec instead of system() for ops

### Fix 3: orchestrator.c — Use fork/exec
Replace `system("./system/game_manager &")` with fork/exec/waitpid pattern
matching 0.ledger-player-npc-simple+3's orchestrator.c.

### Fix 4: Remove PAL Dependencies
Remove `prisc+x` dependency. game_manager.c should be self-contained.
The main_loop_chtpm.pal should NOT be needed if game_manager handles
game state directly.

### Fix 5: Add Xlector/Possession (Optional — Phase 2)
Create xlector piece with state.txt.
Add possession logic to game_manager (scan pieces/ at xlector position).
Add shadow xlector sync.
Add dynamic menu via get_piece_methods_op or equivalent.

## 8. CRITICAL VARIABLES

| Variable | x0.moke | 01.lpns+map+4 | Status |
|----------|---------|----------------|--------|
| interact_history_path | "pieces/apps/player_app/history.txt" | "" (empty) | BROKEN |
| active_target_id | "xlector" (dynamic) | read from state.txt | BROKEN |
| piece_methods | popen(get_piece_methods_op) | not set | BROKEN |
| game_map | set by render_map | set by game_compose_frame | OK |
| last_key | set by route_input | not set | MISSING |

## 9. KEY FILE LOCATIONS

x0.moke fuzz-op (REFERENCE):
  manager:  projects/fuzz-op/manager/fuzz-op_manager.c (942 lines)
  layout:   projects/fuzz-op/layouts/fuzz-op.chtpm (31 lines)
  move:     pieces/apps/playrm/ops/src/move_entity.c (264 lines)
  scan:     pieces/apps/playrm/ops/src/scan_op.c (223 lines)
  interact: pieces/apps/playrm/ops/src/interact.c (219 lines)
  selector: pieces/apps/playrm/ops/src/move_selector.c (198 lines)
  xlector:  projects/fuzz-op/pieces/xlector/state.txt

01.lpns+map+4 (TO FIX):
  parser:   system/chtpm_parser_pal.c (3382 lines) — has INTERACT code but unused
  manager:  system/game_manager.c (163 lines) — broken dual consumer
  layout:   pieces/chtpm/layouts/lpns_main_menu.chtpm (3 lines) — missing tags
  orchestr: system/orchestrator.c (98 lines) — uses system()
  compose:  ops/game_compose_frame.c (133 lines) — writes view.txt
  turn:     ops/game_turn_input.c (156 lines) — updates ledger
  map:      ops/reconstruct_map.c (153 lines) — builds ASCII map

================================================================================
END OF TRACE
================================================================================
