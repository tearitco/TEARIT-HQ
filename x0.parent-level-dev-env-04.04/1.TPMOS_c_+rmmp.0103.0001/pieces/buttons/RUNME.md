# CHTPM Button System

Centralized launcher system for CHTPM+OS compile and run scripts.

## Quick Start

### Windows (PowerShell)

```powershell
.\button.ps1 compile    # Compile all binaries
.\button.ps1 run        # Run CHTPM orchestrator
.\button.ps1 kill       # Kill all processes
.\button.ps1 debug      # Launch GL-OS desktop
.\button.ps1 watchdog   # Start PAL watchdog
.\button.ps1 help       # Show help
```

### Linux/macOS (Bash)

```bash
./button.sh compile     # Compile all binaries
./button.sh run         # Run CHTPM orchestrator
./button.sh kill        # Kill all processes
./button.sh debug       # Launch GL-OS desktop
./button.sh watchdog    # Start PAL watchdog
./button.sh help        # Show help
```

## Directory Structure

```
#.buttons/
├── README.md                  # This file
├── windows/
│   ├── compile.ps1            # Compile wrapper
│   ├── run.ps1                # Run wrapper (via MSYS2)
│   ├── kill.ps1               # Kill wrapper
│   ├── debug_gl.ps1           # GL-OS debug wrapper
│   ├── watchdog.ps1           # Watchdog wrapper
│   └── legacy/                # Original Windows scripts
├── linux/
│   ├── compile.sh             # Compile wrapper
│   ├── run.sh                 # Run wrapper
│   ├── kill.sh                # Kill wrapper
│   ├── debug_gl.sh            # GL-OS debug wrapper
│   ├── watchdog.sh            # Watchdog wrapper
│   └── legacy/                # Original Linux scripts
└── shared/
    └── run_orchestrator.sh    # Cross-platform orchestrator launcher
```

## Action Aliases

| Action   | Aliases           | Description                        |
|----------|-------------------|------------------------------------|
| compile  | c, build          | Compile all CHTPM binaries         |
| run      | r, start          | Run CHTPM orchestrator             |
| kill     | k, stop           | Kill all CHTPM processes           |
| debug    | d, gl             | Launch GL-OS desktop in new window |
| watchdog | w                 | Start PAL watchdog (background)    |

## Legacy Scripts

Original scripts are preserved in `#.buttons/<os>/legacy/` for reference:

**Windows:**
- `compile_all.ps1`
- `run_chtpm.ps1`
- `kill_all.ps1`
- `pal_watchdog.ps1`
- `install_deps.ps1`
- `add_to_path.ps1`
- `*.bat` files

**Linux:**
- `compile_all.sh`
- `run_chtpm.sh`
- `kill_all.sh`
- `pal_watchdog.sh`
- `run_gl_desktop.sh`

## Windows Notes

- The `run` action uses MSYS2 bash (`run_orchestrator.sh`) for reliable path handling
- Requires MSYS2 installed at `C:\msys64`
- The `debug` action launches GL-OS in a separate mintty window

## Troubleshooting

### "Command not found" on Linux
Make sure scripts are executable:
```bash
chmod +x button.sh
chmod +x #.buttons/linux/*.sh
```

### "MSYS2 not found" on Windows
Install MSYS2 from https://www.msys2.org/ or run:
```powershell
.\button.ps1 deps   # If deps.ps1 exists in legacy/
```

### Processes not dying
Run kill explicitly:
```bash
./button.sh kill    # Linux
.\button.ps1 kill   # Windows
```

## Migration Note

Scripts were moved from root directory on 2026-03-31 to reduce clutter.
All functionality is preserved in `#.buttons/<os>/legacy/`.
