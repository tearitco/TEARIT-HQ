# Piececraft In-Scene Desks Implementation Plan (2026-08-30)

## Current State (Investigation Results)

### Confirmed Architecture
- **Blocking setup screen**: new_game piece → CONFIRM_START/CONFIRM_START_DEBUG → game starts
- **World storage**: Always fresh world_01/state.txt + world_01/animals.txt (no persistence)
- **No desk/save system**: Each launch generates a brand new world, no load mechanism
- **Screen flow**: new_game.chtpm (setup) → main.chtpm (game view)
  - Rendered by pc_compose_frame.c (lines 278-309 for new_game, 309-375 for main)
  - Menu items loaded from piece.pdl files by pc_menu_input.c (lines 223-248)

### File Locations (relative to @.apps/piececraft-xyz/)
- **Setup screen rendering**: ops/pc_compose_frame.c lines 278-309
- **Setup screen menu items**: projects/piececraft-xyz/pieces/new_game/piece.pdl
- **Menu action dispatch**: ops/pc_menu_input.c lines 665-863
- **Game state rendering**: ops/pc_compose_frame.c lines 309-375
- **Game config**: pieces/system/config.txt
- **World state**: pieces/world_01/state.txt (real root after CONFIRM_START)
- **Hero state**: pieces/hero_01/state.txt

## Design: Piececraft Desks (NOT Desktop Desks)

Piececraft will have its own saved-world system, separate from the house's desktop pals/desks:
- **Storage location**: pieces/piececraft-desks/<desk_name>/world_01_* files (chunk-based, piececraft's own format)
- **Metadata**: pieces/piececraft-desks/<desk_name>/state.txt (name, created_date, last_played, etc.)
- **Default desks**: Auto-created "world_01" (seeded), "debug_flat" (flat test world)
- **NOT used**: House xyzfs/users/*/desks system (different domain, different format)

## Implementation Steps

### Step 1: Add "select_world" Piece (New Screen)
- **New file**: projects/piececraft-xyz/pieces/select_world/piece.pdl
  - Lists available desks from pieces/piececraft-desks/
  - METHOD entries: "Load <desk_name>" → LOAD_WORLD:<desk_name>
  - Always includes "Create New Seeded World" → CREATE_WORLD_SEEDED
  - Always includes "Create New Debug Flat" → CREATE_WORLD_DEBUG
- **New layout**: pieces/chtpm/layouts/select_world.chtpm
  - Similar structure to new_game.chtpm (panel + module + interact + text)
  - Shows world list via ${world_list} placeholder
  - Shows piece methods via ${piece_methods}

### Step 2: Modify Flow (NO BLOCKING SETUP)
- **Remove**: new_game setup gate
- **New flow**: 
  1. On launch: Auto-create default desks if missing (world_01, debug_flat)
  2. Auto-load the LAST-USED desk (store in pieces/system/config.txt: last_world=<name>)
  3. Start the game immediately
  4. In-game, add menu option to "Switch World" → navigates to select_world piece

### Step 3: Modify Compose Frame (pc_compose_frame.c)
- **New section**: Handle "select_world" piece (similar to new_game/main)
  - List all desks from pieces/piececraft-desks/
  - Read ${world_list} placeholder content
  - Generate piece.pdl with LOAD_WORLD/CREATE_WORLD methods
- **Update**: main piece to add "Switch World" menu option
  - Maps to new piece.pdl METHOD → SWITCH_WORLD command

### Step 4: Modify Menu Input (pc_menu_input.c)
- **New commands**:
  - `LOAD_WORLD:<name>` - Load specified desk's world files into pieces/world_01/
  - `CREATE_WORLD_SEEDED` - Same as current CONFIRM_START
  - `CREATE_WORLD_DEBUG` - Same as current CONFIRM_START_DEBUG
  - `SWITCH_WORLD` - Navigate to select_world piece (write to current_layout.txt)
- **Update existing**: CONFIRM_START/CONFIRM_START_DEBUG to also save desk metadata

### Step 5: Add World Management Helper Op (pc_world_manager.c - NEW FILE)
- **Functions**:
  - `list_piececraft_desks()` - Scan pieces/piececraft-desks/, return names
  - `load_world_by_name(name)` - Copy desk's world files to pieces/world_01/
  - `save_current_world(name)` - Copy pieces/world_01/ to pieces/piececraft-desks/<name>/
  - `create_default_desks_if_missing()` - Ensure world_01 and debug_flat exist
- **Called from**: pc_menu_input.c for LOAD_WORLD/CREATE_WORLD commands

### Step 6: Update button.sh Build (if needed)
- Ensure new ops (pc_world_manager.c) compile
- Verify no breaking changes to existing ops

### Step 7: Update Verification Recipe (PIECECRAFT-LOCAL-VERIFY-2026-08-29.md)
- **New flow**: No more manual "Confirm & Start" + "Enter Game" steps
- **Changed**: Replace those steps with world-select demonstration
- **Add**: Screenshot showing in-game "Switch World" menu option

## Real Files to Modify

### New Files
1. `projects/piececraft-xyz/pieces/select_world/piece.pdl` - Menu definitions
2. `pieces/chtpm/layouts/select_world.chtpm` - Screen layout
3. `ops/pc_world_manager.c` - World load/save helpers

### Modified Files
1. `ops/pc_compose_frame.c` - Add select_world rendering (new section after line 309)
2. `ops/pc_menu_input.c` - Add new command handlers
3. `projects/piececraft-xyz/pieces/main/piece.pdl` - Add SWITCH_WORLD method
4. `button.sh` - Compile new pc_world_manager.+x
5. `#.#.✅️.cal-user-sum/1.^V-hq/PIECECRAFT-LOCAL-VERIFY-2026-08-29.md` - Update recipe

### Unchanged (Existing Logic Reused)
- `pieces/system/config.txt` - Just add last_world= field
- `pieces/world_01/` - Still used, just loaded from desks instead of fresh
- `pc_generate_chunk.c` - Still generates chunks, called by CREATE_WORLD_* commands

## Real Data Structures

### Desk Metadata (pieces/piececraft-desks/<name>/state.txt)
```
name=<desk_name>
created=<timestamp>
last_played=<timestamp>
world_seed=<uint>
world_type=seeded|debug_flat
```

### Desk Storage Directory
```
pieces/piececraft-desks/
  world_01/
    state.txt (metadata)
    hero_01/
      state.txt (hero position/hp/chunk)
    world_01/
      animals.txt
      state.txt (tick counter)
    (future chunk files: chunk_0_0_z0.txt, etc.)
  debug_flat/
    state.txt (metadata)
    hero_01/
      state.txt
    world_01/
      animals.txt
      state.txt
```

## Scope Notes

- **Small, focused scope**: Only adds in-scene world selection, reuses existing world generation
- **No new game mechanics**: Just file browsing/loading, same as desktop desks but for piececraft worlds
- **Backward compatible**: Existing CONFIRM_START logic still works, just refactored as CREATE_WORLD_* in new context
- **House design pattern reused**: "Browse named items, pick one, it loads" mirrors livedesk_spawn_desk() conceptually

## Risk/Testing Checklist

- [ ] Compile pc_world_manager.c cleanly
- [ ] Test default desks auto-creation on first launch
- [ ] Test LOAD_WORLD command loads correct world state
- [ ] Test SELECT_WORLD screen lists desks correctly
- [ ] Test in-game "Switch World" navigation works
- [ ] Test CREATE_WORLD_SEEDED/DEBUG still generates fresh worlds
- [ ] Verify no symlink/real_root issues (this house's known gotcha)
- [ ] Live verify with tk_inject_key test harness (per LOCAL-VERIFY doc)
