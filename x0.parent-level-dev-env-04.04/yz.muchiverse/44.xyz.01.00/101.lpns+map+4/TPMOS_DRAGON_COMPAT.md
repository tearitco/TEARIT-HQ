# TPMOS Dragon Compatibility — Portability Shim
## Pulling projects out of tpmos/ and pushing them back in

---

## The Gap

TPMOS projects live inside a host `x0.*` project under `projects/`:
```
x0.moke-pet-project-04.04/x0.5-liz.fiter4-mew-00.03/projects/fuzz-op/
```

Standalone projects live alongside:
```
01.lpns+map+4/
0.ledger-player-npc-simple+3/
```

**The question:** Can a project move between these two worlds without code changes?

---

## Pulling a TPMOS project out (standalone)

### What already works
If a project under `projects/` uses:
- `system/orchestrator.c` as entry point (fork/exec, PID tracking)
- `pieces/` for state files
- `ops/` for binary ops
- `system/` for persistent processes
- Relative paths everywhere (no hardcoded TPMOS paths)

Then you can copy it out and it runs with `button.sh run`.

### What breaks

| Issue | Cause | Fix |
|-------|-------|-----|
| `projects/wraith-alpha/...` hardcoded paths | chtpm_parser_pal.c resolves some paths relative to TPMOS root | Use `PRISC_PROJECT_ROOT` env var (already set by orchestrator) |
| `pieces/os/proc_list.txt` path | x0.moke orchestrator uses TPMOS-relative paths | Self-contained: each project owns its own `pieces/os/` |
| `<module>system/prisc+x pal/main_loop_chtpm.pal</module>` | References TPMOS's prisc+x | Replace with local binary: `<module>system/game_manager</module>` |
| `pieces/chtpm/plugins/` vs `system/` | x0.moke stores binaries in plugins/+x/ | Standalone uses `system/` directly |

### The shim: `pull-project.sh`

```bash
#!/bin/bash
# pull-project.sh — Extract a project from tpmos projects/ to standalone
# Usage: ./pull-project.sh <project_name>
#
# This script:
# 1. Copies the project out of tpmos projects/ dir
# 2. Rewrites <module> tags to point to local binaries
# 3. Ensures button.sh and orchestrator.c exist
# 4. Does NOT modify the original tpmos project

set -e
PROJECT="$1"
if [ -z "$PROJECT" ]; then echo "Usage: $0 <project_name>"; exit 1; fi

TPMOS_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$TPMOS_ROOT/projects/$PROJECT"
DST="$(dirname "$TPMOS_ROOT")/$PROJECT"

if [ ! -d "$SRC" ]; then echo "Source not found: $SRC"; exit 1; fi
if [ -d "$DST" ]; then echo "Destination exists: $DST"; exit 1; fi

echo "Pulling $PROJECT from tpmos..."
cp -r "$SRC" "$DST"
cd "$DST"

# Rewrite <module> tags: prisc+x → game_manager
find pieces/chtpm/layouts/ -name "*.chtpm" -exec \
    sed -i 's|<module>system/prisc+x.*</module>|<module>system/game_manager</module>|g' {} +

# Ensure button.sh exists
if [ ! -f button.sh ]; then
    cat > button.sh << 'EOF'
#!/bin/bash
case "$1" in
    run)
        echo "Compiling and starting..."
        gcc -o system/orchestrator system/orchestrator.c 2>/dev/null
        ./system/orchestrator
        ;;
    kill)
        kill -9 $(pgrep -f "$(pwd)/system") 2>/dev/null
        echo "Stopped."
        ;;
    *) echo "Usage: sh button.sh {run|kill}" ;;
esac
EOF
    chmod +x button.sh
fi

echo "Done. Project pulled to: $DST"
echo "Run with: cd $DST && sh button.sh run"
```

### The shim: `push-project.sh`

```bash
#!/bin/bash
# push-project.sh — Push a standalone project into tpmos projects/
# Usage: ./push-project.sh <project_name>
#
# This script:
# 1. Copies the project INTO tpmos projects/ dir
# 2. Rewrites <module> tags to reference TPMOS's prisc+x if needed
# 3. Does NOT modify the original standalone project

set -e
PROJECT="$1"
if [ -z "$PROJECT" ]; then echo "Usage: $0 <project_name>"; exit 1; fi

TPMOS_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$(dirname "$TPMOS_ROOT")/$PROJECT"
DST="$TPMOS_ROOT/projects/$PROJECT"

if [ ! -d "$SRC" ]; then echo "Source not found: $SRC"; exit 1; fi
if [ -d "$DST" ]; then echo "Destination exists: $DST"; exit 1; fi

echo "Pushing $PROJECT into tpmos..."
cp -r "$SRC" "$DST"
cd "$DST"

# Optionally rewrite <module> tags back to prisc+x
# (only if the project uses prisc+x-specific features)
# find pieces/chtpm/layouts/ -name "*.chtpm" -exec \
#     sed -i 's|<module>system/game_manager</module>|<module>system/prisc+x pal/main_loop_chtpm.pal</module>|g' {} +

echo "Done. Project pushed to: $DST"
echo "Launch from tpmos: projects/$PROJECT/button.sh run"
```

---

## What Would Make It "Just Work"

### Minimal Requirements for a tpmos-compatible standalone project

1. **`button.sh`** with `run` and `kill` commands
2. **`system/orchestrator.c`** using fork/exec (Bible §3)
3. **All paths relative** to project root (use `PRISC_PROJECT_ROOT` env)
4. **Layout file** with `<module>` tag pointing to local binary
5. **`pieces/` directory** for all state files
6. **`ops/` directory** for all one-shot operations

### What tpmos provides that standalone doesn't

| tpmos Feature | Standalone Equivalent | Gap |
|---------------|----------------------|-----|
| `prisc+x pal/main_loop_chtpm.pal` | `<module>system/game_manager</module>` | Must write game_manager.c |
| `pieces/chtpm/plugins/+x/` binaries | `system/` binaries | Must compile locally |
| `pieces/os/proc_list.txt` (global) | Per-project `pieces/os/proc_list.txt` | Self-contained |
| `projects/*/PDL` project descriptions | N/A | Not needed standalone |
| `pieces/apps/playrm/ops/` shared ops | `ops/` local ops | Must write ops |

### The Hard Part: chtpm_parser_pal.c

The parser is shared across all projects. When running standalone, you need a local copy:
- `01.lpns+map+4/system/chtpm_parser_pal.c` (PAL fork, 3382 lines)
- `0.ledger-player-npc-simple+3/system/chtpm_parser.c` (original x0.pet, 3028 lines)

**Rule from user:** Use the ORIGINAL chtpm_parser.c from x0.pet for new projects, NOT the PAL fork, unless you specifically need PAL features.

---

## Architecture Decision: Self-Contained vs Shared

```
SELF-CONTAINED (current approach, recommended):
  project/
    system/           ← all persistent processes
    ops/              ← all one-shot operations
    pieces/           ← all state files
    button.sh         ← entry point
    config.txt        ← runtime config
    
  Pros: No dependencies, works anywhere, easy to copy
  Cons: Code duplication across projects

SHARED (tpmos approach):
  x0.moke/
    projects/fuzz-op/
      pieces/chtpm/layouts/  ← layout only
      pieces/xlector/        ← project-specific state
    pieces/chtpm/plugins/    ← SHARED parser
    pieces/keyboard/plugins/ ← SHARED keyboard_input
    pieces/display/plugins/  ← SHARED renderer
    
  Pros: No code duplication, updates propagate
  Cons: Fragile, cross-project dependencies, harder to extract
```

**Recommendation:** Keep projects self-contained. Code duplication is acceptable for portability.

---

## Testing the Shim

```bash
# Pull fuzz-op out of tpmos
./pull-project.sh fuzz-op
cd ../fuzz-op
sh button.sh run

# Push it back
cd ..
./push-project.sh fuzz-op
# Verify it still works from tpmos
cd x0.moke-pet-project-04.04/x0.5-liz.fiter4-mew-00.03/projects/fuzz-op
sh button.sh run
```
