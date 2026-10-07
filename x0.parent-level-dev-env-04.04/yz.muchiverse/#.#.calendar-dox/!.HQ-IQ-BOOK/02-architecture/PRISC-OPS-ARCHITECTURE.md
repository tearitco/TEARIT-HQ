# Prisc + Ops Architecture — House Standard for Event Managers

**Date:** 2026-09-27  
**Status:** Living documentation  
**Applies to:** Page managers, event dispatchers, game loops, and any long-running orchestration code

## What This Is

The house uses a **prisc+ops** pattern for orchestrating game/world state:
- **`.pal` files** are RISC-V assembly that coordinates the main loop (NOT Tcl, NOT shell)
- **`ops/`** are compiled C binaries that do the real work (NOT shell scripts)
- One `.pal` file `exec`s multiple ops, sleeping and looping between ticks

**Example structure:**
```
pages/test_page_001/
├── manager/
│   ├── page_manager.pal           (prisc control: exec ops, sleep, loop)
│   └── ops/
│       ├── page_manager_init      (compiled C binary)
│       └── page_manager_tick      (compiled C binary)
```

## Prisc: The RISC-V VM

`prisc+x` is a **real RISC-V virtual machine** with:

**Instructions:**
- Arithmetic: `li`, `addi`, `add`, etc. (full RISC-V ISA)
- Control flow: `j <label>`, `beq`, etc.
- I/O: `exec <path>`, `halt`, `sleep <ms>`
- Syscalls: `ecall` for integer KV store ops (`SYS_GET_KV_INT`, `SYS_SET_KV_INT`)

**.pal File Format:**
```
# Comment

# Initial setup
exec ./ops/my_init_op
hit_frame

loop:
  exec ./ops/my_tick_op
  sleep 16
  j loop
```

**Key points:**
- `.pal` IS assembly code, not a scripting language
- `exec ./ops/<name>` calls an executable (shell script or compiled binary)
- **Arguments (changed 2026-10-07, owner: "should be able to accept many args")**: `exec <path> <a1> <a2> <a3> ...`. `a1` is a literal or a register (its value is passed), `a2` a register (as before), and **every further token is a literal passed as an extra argument, in order** (whitespace-split, so a token cannot contain a space). Before this, a third token that was not a register was silently dropped (the VM's own comment said "we don't have literal_arg3 yet"), so an op could receive at most one literal. Backward compatible: no existing `.pal` (183 scanned) has a fourth token. Source: `&.widgits/_shared-lib/system/prisc+x.c`; **per-project copies of the VM are separate files and are unchanged**, and a binary built before this change still drops the extras. Proof: `&.widgits/_shared-lib/harness/prisc_exec_args.pal` (3 checks; the old VM fails all 3).
- **Output and exit status are discarded** by `exec` (stdout/stderr go to `/dev/null`, the status is not consulted), so a pal cannot branch on whether an op succeeded: ops report through files/ledgers (this is why the harness ops write PASS/FAIL rows).
- Paths are **relative to the .pal file's directory** (CWD when prisc runs)
- `sleep` is in milliseconds (16 = ~60fps)

## Ops: Compiled C Binaries

Each op is a **standalone C program** that:
- Takes its own `argv[0]` to derive paths (using `realpath` + path math)
- Reads/writes state files independently
- Runs to completion and exits

**Example op structure:**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Derive paths from binary location
    char binary_path[2048];
    realpath(argv[0], binary_path);
    
    // Make a COPY before dirname() — it modifies the string
    char binary_copy[2048];
    strcpy(binary_copy, binary_path);
    char *ops_dir = dirname(binary_copy);
    
    // Build paths: ops_dir/../ = page_root
    char page_root[2048];
    snprintf(page_root, sizeof(page_root), "%s/..", ops_dir);
    realpath(page_root, page_root);  // Normalize
    
    // Now read/write state files relative to page_root
    char state_file[2048];
    snprintf(state_file, sizeof(state_file), "%s/state/entities.txt", page_root);
    
    // ... do work ...
    return 0;
}
```

**Compile:**
```bash
gcc -o page_manager_init page_manager_init.c
gcc -o page_manager_tick page_manager_tick.c
```

## Pattern: Append-Only Ledgers + Cursor Polling

Game managers use this idiom for state:

```
pages/test_page_001/state/
├── entities_live.txt        (append-only: entity positions)
├── animation_queue.txt      (append-only: pending animations)
├── world_events.txt         (append-only: game events)
└── page_manager.cursor      (cursor markers for each ledger)
```

Each op tick:
1. **Read cursors** from `page_manager.cursor`
2. **Poll ledgers** starting from cursor position (never re-read earlier lines)
3. **Process** new entries (check triggers, queue animations, etc.)
4. **Advance cursors** and write back to `page_manager.cursor`

This ensures **idempotent polling**: rerunning the op won't reprocess the same events.

## Why This Pattern

✅ **Decoupled:** Each op is independent; can be developed/tested separately  
✅ **Event-sourced:** Ledgers are immutable audit trails  
✅ **Testable:** Ops are standalone binaries  
✅ **Fast:** RISC-V VM is lightweight; C ops are compiled  
✅ **House-native:** Uses prisc infrastructure already deployed everywhere  

## Common Mistakes

❌ **Writing `.pal` files in Tcl or shell** — prisc is RISC-V, not a script interpreter  
❌ **Shell scripts in `ops/`** — should be compiled C binaries  
❌ **Not copying before `dirname()`** — `dirname()` modifies its argument  
❌ **Hardcoding absolute paths** — use dynamic path derivation from `argv[0]`  
❌ **Not using append-only ledgers** — leads to race conditions and lost events  

## Bootstrap vs. Production: The Shell Script Boundary

**Shell scripts are a bootstrapping convenience only.** Use them to get the system running before prisc+ops infrastructure is available:

```bash
# Bootstrap: quick setup before prisc VM is live
./page_manager_bootstrap.sh

# Production: prisc VM + compiled C ops
prisc page_manager.pal
```

Once prisc VM and compiled ops are live, **never use shell orchestration again**. The production state converges entirely to prisc+ops:

- **No shell scripts** driving the game loop or event dispatch
- **No shell ops** (only compiled C binaries)
- **Everything** driven by `.pal` assembly + C ops

This rule exists because shell is fragile at scale (no bounded memory, no type safety, easy race conditions), while prisc+C is:
- Fast (RISC-V VM + compiled C)
- Safe (bounded memory, static types, idempotent via ledgers)
- Verifiable (assembly is explicit; C is compiled)

**The long-term goal:** even the `.pal` files will become internal — ops themselves may eventually be written in and compiled from higher-level languages, but the rule remains the same: never shell orchestration at scale.

## Future Work

- [ ] Standardize op template / boilerplate (e.g., `op_template.c`)
- [ ] Document prisc syscall set (SYS_GET_KV_INT, SYS_SET_KV_INT, etc.)
- [ ] Write ops for common game commands (Change Gold, Show Text, etc.)
- [ ] Real multi-page book manager using prisc coordination
- [ ] Long-term: compile `.pal` from higher-level DSL; keep ops as compiled binaries
