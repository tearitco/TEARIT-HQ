# ZOO-SAVE-LOAD: Game State Save/Load Window

**Created**: 2026-07-26  
**Status**: Planning Phase  
**Goal**: A GL window that allows saving/loading game states into running mutaclsym instances

---

## Vision

**"Drag the mutaclsym window onto this to save/load its state"**

A dedicated window that:
1. Shows list of saved game states
2. Allows saving current mutaclsym instance
3. Allows loading a saved state into running mutaclsym
4. Displays PID of connected mutaclsym instance
5. Can be dragged onto mutaclsym window to transfer state

---

## Architecture

### How It Works

**Save Flow**:
1. User opens zoo-save-load window
2. Window scans for running mutaclsym instances (by PID file or process name)
3. User selects "Save" → window reads mutaclsym's `pieces/world_01/` directory
4. Window copies entire world state to `saves/save_N/` with metadata
5. Window displays save confirmation

**Load Flow**:
1. User opens zoo-save-load window
2. Window shows list of saves with metadata (turn count, timestamp)
3. User selects a save → window reads `saves/save_N/world_01/`
4. Window copies state into running mutaclsym's `pieces/world_01/`
5. Mutaclsym reloads state on next tick

**Drag-Drop Flow** (Future):
1. User drags mutaclsym window onto zoo-save-load window
2. zoo-save-load reads mutaclsym's PID from window property
3. zoo-save-load connects to mutaclsym's state directory
4. User selects save/load action

---

## Reference: op-ed Save/Load

From `1.TPMOS_c_+rmmp.0103.0001/projects/op-ed/`:

### op-ed's Architecture

**Sovereign Context Model**:
- Each game is a self-contained directory in `projects/op-ed/games/`
- Loading a game copies entire directory to working RAM (Disk Buffer)
- Maps, pieces, scripts all contained in game's folder
- Save = copy working state back to game folder

**Save Mechanism** (from `save_game.c`):
```c
// op-ed's save pattern
mkdir("games/<game_name>/saves/<save_name>");
system("cp -r pieces/world_01/ games/<game_name>/saves/<save_name>/world_01/");
// Write metadata: turn count, timestamp, etc.
```

**Load Mechanism**:
```c
// op-ed's load pattern
system("cp -r games/<game_name>/saves/<save_name>/world_01/ pieces/world_01/");
// Reload state from files
```

### How mutaclsym Already Does It

From `101.mutaclsym.../ops/save_game.c`:

```c
// mutaclsym's current save
void save_game() {
    // Auto-number saves
    int save_num = get_next_save_number();
    
    // Create save directory
    mkdir("pieces/saves/save_N/world_01/", 0755);
    
    // Copy entire world
    system("cp -r pieces/world_01/ pieces/saves/save_N/world_01/");
    
    // Write metadata
    FILE *meta = fopen("pieces/saves/save_N/save_meta.txt", "w");
    fprintf(meta, "turn=%d\n", current_turn);
    fprintf(meta, "timestamp=%ld\n", time(NULL));
    fclose(meta);
}
```

### What's Missing for Cross-Instance Save/Load

1. **PID Discovery** - How to find running mutaclsym instance
2. **State Transfer** - How to inject state into running instance
3. **Reload Trigger** - How to tell mutaclsym to reload state
4. **Conflict Resolution** - What if save conflicts with live state

---

## Implementation Plan

### Phase 1: Basic Save/Load Window (2-3 hours)

**File Structure**:
```
zoo-save-load/
├── button.sh              # Launcher
├── system/
│   ├── orchestrator.c     # Process manager
│   ├── renderer.c         # Frame display
│   ├── keyboard_input.c   # Input handler
│   └── save_load_window.c # GL window (based on egg_window.c)
├── ops/
│   ├── scan_instances.c   # Find running mutaclsym instances
│   ├── save_instance.c    # Save state from running instance
│   ├── load_instance.c    # Load state into running instance
│   └── list_saves.c       # List available saves
├── pieces/
│   ├── os/
│   │   ├── kill_all.sh
│   │   └── proc_list.txt
│   └── saves/             # Saved game states
└── pal/
    └── main_loop.pal      # Main loop
```

### Phase 2: Instance Discovery (1-2 hours)

**How to Find Running mutaclsym**:

1. **PID File Method** (Recommended)
   - mutaclsym writes `pieces/system/mutaclsym.pid` on startup
   - zoo-save-load reads this file to find instance
   - Simple, reliable, no process scanning

2. **Process Name Method** (Fallback)
   - Scan `/proc/` for `system/orchestrator` processes
   - Check `PRISC_PROJECT_ID` env var in `/proc/<pid>/environ`
   - More complex, but works without PID file

3. **Window Property Method** (Future)
   - Set X11 window property with PID
   - Drag-drop reads property from dropped window
   - Most elegant, but requires Xdnd implementation

### Phase 3: State Transfer (2-3 hours)

**Save Transfer**:
```c
// 1. Find mutaclsym instance
int pid = find_mutaclsym_instance();
char *root = get_instance_root(pid);

// 2. Copy state
char cmd[1024];
snprintf(cmd, sizeof(cmd), "cp -r %s/pieces/world_01/ %s/pieces/saves/save_N/world_01/", root, project_root);
system(cmd);

// 3. Write metadata
write_save_metadata(save_num, pid);
```

**Load Transfer**:
```c
// 1. Find mutaclsym instance
int pid = find_mutaclsym_instance();
char *root = get_instance_root(pid);

// 2. Backup current state (safety)
snprintf(cmd, sizeof(cmd), "cp -r %s/pieces/world_01/ %s/pieces/saves/backup_%d/world_01/", root, root, time(NULL));
system(cmd);

// 3. Load saved state
snprintf(cmd, sizeof(cmd), "cp -r %s/pieces/saves/save_N/world_01/ %s/pieces/world_01/", project_root, root);
system(cmd);

// 4. Signal reload (write to marker file)
snprintf(cmd, sizeof(cmd), "echo 'reload' > %s/pieces/system/reload_request.txt", root);
system(cmd);
```

### Phase 4: GL Window (1-2 hours)

**Based on egg_window.c**:
- Shaped GL window (X11 Shape Extension + GLX)
- Shows save list or instance list
- Click to select, Enter to confirm
- Displays PID of connected instance

**UI Layout**:
```
+============================================================+
|                    ZOO SAVE/LOAD                           |
+============================================================+
| Connected: mutaclsym (PID 12345)                           |
|                                                            |
| [>] 1. Save Current State                                  |
| [ ] 2. Load Saved State                                    |
| [ ] 3. List Saves                                          |
| [ ] 4. Disconnect                                          |
|                                                            |
| Saves:                                                     |
|   save_1: Turn 42, 2026-07-26 18:30                       |
|   save_2: Turn 38, 2026-07-26 17:15                       |
|                                                            |
+============================================================+
 [ctrl+c] Quit
```

---

## PID Display for mutaclsym and Pets

### Current State

- **muchi-pals pets**: Show `window.pid` file in pet directory
- **mutaclsym**: No PID display currently

### Proposed Solution

**1. mutaclsym PID Display**:

In `system/orchestrator.c`:
```c
// Write PID file on startup
void write_pid_file() {
    FILE *f = fopen("pieces/system/mutaclsym.pid", "w");
    fprintf(f, "%d\n", getpid());
    fclose(f);
}
```

In `ops/compose_frame.c`:
```c
// Show PID in frame header
int pid = read_pid_file();
if (pid > 0) {
    fprintf(frame, "muchaclsym PID: %d\n", pid);
}
```

**2. Pet PID Display**:

In `system/egg_window.c`:
```c
// Already writes window.pid
// Just need to display it in window title or overlay

// Option A: Window title
char title[64];
snprintf(title, sizeof(title), "Pet: %s (PID %d)", pet_id, getpid());
XStoreName(display, window, title);

// Option B: Overlay text
draw_text(10, 10, "PID: %d", getpid());
```

**3. Drag-Drop PID Transfer** (Future):

When dragging mutaclsym window onto zoo-save-load:
1. Set X11 window property `MUTACLSYM_PID` = current PID
2. zoo-save-load reads property on drop
3. No need for PID file scanning

---

## Integration with Export/Import

### How This Enables Pet Export/Import

**Export Flow** (muchi-pals → mutaclsym):
1. User opens zoo-save-load window
2. User drags pet window onto zoo-save-load
3. zoo-save-load reads pet's PID from window property
4. zoo-save-load calls `pet_export` with pet_id
5. Pet directory moves to exchange/
6. User drags zoo-save-load onto mutaclsym window
7. zoo-save-load calls `pet_import` in mutaclsym context
8. Pet appears in mutaclsym

**Import Flow** (muchaclsym → muchi-pals):
1. User selects pet in mutaclsym
2. User selects "Export" method from dynamic menu
3. mutaclsym calls `pet_export` with pet_id
4. Pet directory moves to exchange/
5. User opens muchi-pals, selects "Import"
6. muchi-pals calls `pet_import`
7. Pet appears in muchi-pals, egg_window spawns

---

## KPIs

| Metric | Target | Current |
|--------|--------|---------|
| Save/load success rate | 100% | 0% |
| Instance discovery time | <1 second | N/A |
| State transfer integrity | 100% | N/A |
| GL window render FPS | 30+ | N/A |
| PID display accuracy | 100% | 0% |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| State corruption during transfer | High | Backup before load, atomic copy |
| PID file stale after crash | Medium | Validate PID exists before use |
| Instance not found | Medium | Fallback to process scanning |
| Concurrent save/load conflicts | Medium | File locking during transfer |

---

## Next Steps

1. **Create orchestrator.c** - Fork from mutaclsym
2. **Create save_load_window.c** - Based on egg_window.c
3. **Implement scan_instances.c** - Find running mutaclsym
4. **Implement save_instance.c** - Copy state to saves/
5. **Implement load_instance.c** - Copy state from saves/
6. **Test basic save/load** - Manual instance entry
7. **Add PID display** - Show in mutaclsym and pet windows
8. **Implement drag-drop** - X11 window property transfer

---

## See Also

- `1.TPMOS_c_+rmmp.0103.0001/projects/op-ed/` - Reference save/load implementation
- `101.mutaclsym.../ops/save_game.c` - Current save implementation
- `01.muchi-pals.../system/egg_window.c` - GL window reference
- `muta-pets.md` - Pet export/import analysis
