# 🔧 Chapter 16: Common Pitfalls & Debugging Guide
This chapter catalogs every known TPMOS pitfall and provides a comprehensive debugging workflow. Keep this as your reference when things go wrong. 🛠️🔍

---

## 🚨 Quick Diagnostic Flowchart

```
Problem → Is it a compile error?
  YES → Check CH4 (Muscles & Brains) for compile patterns
  NO → Is it a runtime crash?
    YES → Check signal handling (CH4), file paths (CH2)
    NO → Is it a logic bug (wrong behavior)?
      YES → Check state files, PAL scripts, pipeline flow
      NO → Is it a performance issue?
        YES → Check CPU-safe patterns (CH4), stat() guard (CH2)
```

---

## 📋 Pitfall Index by Chapter

### CH2: Filesystem
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Hardcoded paths | Works on your machine, fails after moving | Use `resolve_paths()` |
| 2 | Forgetting `location_kvp` update | Entire project "disappears" | Update `project_root=` line |
| 3 | `stat()` race conditions | Input missed or processed twice | Use `fseek()` + `ftell()` pattern |
| 4 | Reading state without checking | Crash or garbage values | Check `fopen()` return value |
| 5 | **Scattered hardcoded system paths** | Changing a path requires editing 7+ .c files | Define path constant + helper in each .c file locally (no shared headers). See `TPM_PIECE_MANAGER_CMD`, `TPM_OPS_ROOT` pattern. |
| 6 | **Hardcoded piece type → icon mappings** | New piece types show `?` until you edit 3+ files | External data: `pieces/os/type_registry.pdl`. Each .c has local lookup. |
| 7 | **Hardcoded project defaults (e.g., "fuzzpet_v2")** | Ops fail on unregistered projects | Default to `"template"`, resolve from `location_kvp` → manager state → fallback. |

### CH3: Pipeline
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Using `system()` | Orphaned processes after Ctrl+C | Use `fork()/exec()/waitpid()` |
| 2 | Forgetting `waitpid()` | Zombie processes accumulate | Always call `waitpid()` |
| 3 | Not hitting `frame_changed.txt` | Screen doesn't refresh | Call `hit_frame_marker()` |
| 4 | Blocking pipeline with slow ops | System stutters | Keep ops under 16ms |

### CH4: Modules & Ops
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Orphaned processes on Ctrl+C | Game keeps running after quit | Register `handle_sigint`, use `setpgid(0,0)` |
| 2 | Tight loop without sleep | 100% CPU when idle | Always `usleep(16667)` or `usleep(100000)` |
| 3 | Scattering ops | Fondu can't find ops | Centralize in `ops/` directory |
| 4 | Forgetting `ops_manifest.txt` | Fondu skips project | Create manifest with correct format |
| 5 | Thick brain anti-pattern | 5000+ line manager | Move logic into separate ops |
| 6 | **Using shared `.h` headers for paths/types** | Breaks TPMOS convention, circular deps | Put macros and helpers directly in each `.c` file. Use external `.pdl` for data. |
| 7 | **Hardcoded dispatch by type name** (`strcmp(type, "npc")`) | Adding new types requires C changes | Check behavior/traits from state.txt instead (`strcmp(behavior, "aggressive")`) |
| 8 | **Response key conjugation in C** ("feed"→"fed") | Fragile verb handling | Use action name directly as response key |

### CH6: PAL
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Infinite loops | System hangs, CPU spikes | Ensure loops have terminating condition |
| 2 | Forgetting `sleep` between ops | Ops fail silently | Add `sleep 50-200` between heavy ops |
| 3 | Cross-project call failures | `OP other::op` does nothing | Install target project via Fondu first |
| 4 | Not ending with `halt` | Interpreter crashes | Every path must end with `halt` |

### CH7: fuzz-op & op-ed
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Zombie AI not moving | Zombie stays in place | Ensure `zombie_ai.+x` exists and uses `resolve_paths()` |
| 2 | Manager running from wrong directory | Nothing works | Kill, recompile from correct dir, restart |
| 3 | Selector state corruption | Selecting entity breaks movement | Update selector's `map_id` to match entity |
| 4 | op-ed changes not appearing | Map doesn't update | Ensure `hit_frame_marker()` after tile placement |
| 5 | Z-level desync | Map doesn't change on Z move | Update `current_z_val`, write correct map name |

### CH8: GL-OS
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Font file not found | GL-OS crashes on startup | Use dynamic font path resolution |
| 2 | ASCII/GL desync | 3D doesn't match terminal | Ensure both read same frame file |
| 3 | Model path resolution | Models appear as blank boxes | Use absolute paths from `project_root` |
| 4 | OpenGL context not created | Black/blank window | Check `glfwCreateWindow()` return value |

### CH9: Testing
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Bot moving too fast | Keys not processed | Add `sleep` between injections |
| 2 | Assertion timing | Test fails intermittently | Poll until state stabilizes |
| 3 | Bot state corruption | Wrong state between runs | Reset bot state before each test |

### CH10: P2P-NET
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Ghost port listeners | "Address already in use" | Kill processes, verify port free |
| 2 | Blockchain file corruption | Chain validation fails | Use file locking for writes |
| 3 | Wallet state mismatch | Shows connected but no activity | Update state on connect AND disconnect |
| 4 | Cross-project PAL failures | `OP p2p::op` does nothing | `./fondu --install p2p-net` first |

### CH11: Recursive Forge
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Gate evaluation order | Wrong circuit output | Topologically sort gates |
| 2 | Clock too fast | State doesn't propagate | Minimum 50ms clock period |
| 3 | Infinite PAL recursion | Stack overflow | Ensure recursive `call` has base case |

### CH12: Simulation
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Simulation too fast | State corrupted | Minimum 10ms between steps |
| 2 | State dependency cycles | Nonsensical results | Detect and break cycles |
| 3 | Memory exhaustion | System slows/crashes | Chunk simulation updates |

### CH13: Business
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Hardcoded business logic | Requires recompile for term changes | Store terms in state files |
| 2 | Onboarding script failures | Missing permissions | Add error checking after each OP |
| 3 | Div-Point exploitation | Infinite points earned | Track last claim time, enforce cooldown |

### CH14: Soul Pen
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Scope creep | Development stalls | Focus on one Piece at a time |
| 2 | Fiction/function confusion | Lore vs code blur | Document implemented vs planned |

### CH15: Cross-Platform
| # | Pitfall | Symptom | Fix |
|---|---------|---------|-----|
| 1 | Hardcoded Linux paths on Windows | "File not found" | Use `build_path_malloc()` |
| 2 | Missing MSYS2 dependencies | Compile fails | Install required packages |
| 3 | Signal handling differences | Ctrl+C doesn't cleanup | Use `SetConsoleCtrlHandler()` on Windows |
| 4 | Font path not found | GL-OS crashes | Use platform-specific font paths |

---

## 🔍 Debugging Workflow

### Step 1: Identify the Symptom
What exactly is happening?
- [ ] Compile error? → Check error message, verify file exists
- [ ] Runtime crash? → Check signal handling, file paths
- [ ] Wrong behavior? → Check state files, PAL scripts
- [ ] Performance issue? → Check CPU-safe patterns

### Step 2: Check the Debug Log
```bash
cat projects/fuzz-op/manager/debug_log.txt | tail -20
```
Look for:
- Last successful operation
- Error messages
- State changes

### Step 3: Verify State Files
```bash
# Check piece state
cat projects/fuzz-op/pieces/world_01/map_01/zombie_01/state.txt

# Check manager state
cat pieces/apps/player_app/manager/state.txt

# Check location_kvp
cat pieces/locations/location_kvp
```

### Step 4: Verify Binaries Exist
```bash
# Check if binary exists
ls -la pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x

# Check if it's executable
file pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x
```

### Step 5: Check Process List
```bash
# See what's running
ps aux | grep -E "(fuzz-op|chtpm|manager)" | grep -v grep

# Kill everything
./button.sh kill
```

### Step 6: Recompile
```bash
./button.sh compile
./button.sh check
```

### Step 7: Restart
```bash
./button.sh run
```

---

## 🛠️ Common Debug Commands

### View Recent Debug Output
```bash
tail -50 projects/fuzz-op/manager/debug_log.txt
```

### Check All Binaries
```bash
./button.sh check
```

### Kill All Processes
```bash
./button.sh kill
```

### Check Port Usage (P2P issues)
```bash
lsof -i :8000
```

### Verify File Permissions
```bash
ls -la pieces/apps/fuzzpet_app/traits/+x/
```

### Test Single Op
```bash
./pieces/apps/playrm/ops/+x/move_entity.+x selector up
```

---

## 📊 Health Check Script
Create a health check script to verify system state:

```bash
#!/bin/bash
# health_check.sh - Verify TPMOS system health

echo "=== TPMOS Health Check ==="

# Check location_kvp
if [ -f "pieces/locations/location_kvp" ]; then
    echo "✓ location_kvp exists"
else
    echo "✗ location_kvp MISSING"
fi

# Check binaries
BINARIES=(
    "pieces/chtpm/plugins/+x/chtpm_parser.+x"
    "pieces/apps/playrm/ops/+x/move_entity.+x"
    "pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x"
)

for bin in "${BINARIES[@]}"; do
    if [ -f "$bin" ]; then
        echo "✓ $bin"
    else
        echo "✗ $bin MISSING"
    fi
done

# Check state files
if [ -f "pieces/apps/player_app/manager/state.txt" ]; then
    echo "✓ Manager state exists"
else
    echo "✗ Manager state MISSING"
fi

# Check for orphaned processes
ORPHANS=$(ps aux | grep -E "(manager|parser|renderer)" | grep -v grep | wc -l)
if [ $ORPHANS -gt 0 ]; then
    echo "⚠ $ORPHANS processes running"
else
    echo "✓ No orphaned processes"
fi

echo "=== Health Check Complete ==="
```

---

## 🏛️ Scholar's Corner: The "Debug That Debugged Itself"
A developer once wrote a PAL script to automatically diagnose and fix common TPMOS issues. The script would check state files, verify binaries, restart processes, and even recompile missing ops. One day, the script encountered an error it couldn't fix: its own source file had been corrupted. Instead of failing, the script regenerated itself from the compile script and continued running. This became known as **"The Debug That Debugged Itself."** It taught us that **"The best debugger is one that can fix itself."** 🔧♾️

---

## 📝 Study Questions
1.  You press Ctrl+C but the game keeps running. What are three possible causes?
2.  The zombie isn't moving. Walk through the diagnostic steps from this chapter.
3.  **Scenario:** Your PAL script hangs after calling an OP. What's the most likely cause?
4.  **Write a health check** that verifies all P2P-NET binaries exist.
5.  **True or False:** The first step in debugging is always to recompile.
6.  **Critical Thinking:** Why is checking `debug_log.txt` more useful than just restarting the system?

---
[Return to Index](INDEX.md)
