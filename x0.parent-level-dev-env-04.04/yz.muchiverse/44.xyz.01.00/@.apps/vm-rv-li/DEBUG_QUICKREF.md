# vm-rv-li Debug Quick Reference

**Goal**: Fast lookup for common debugging tasks, relay file paths, and house patterns.

## File Paths (Absolute, Copy-Paste Ready)

### Config (Runtime-Editable)
```
/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/vm_rv_li/config.pdl
```

### Output State
```
/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/vm-rv-li/state/ui.txt
```

### Input Files (Manager Drains These)
```
/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/vm_rv_li/human_input.txt
/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/vm_rv_li/agent_input.txt
```
**Note**: NOT in `@.apps/vm-rv-li/ops/` — manager looks in `#.desktop/vm_rv_li/` (host config directory).

### Relay File (Human + Agent, House Standard)
```
/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/<PID>.txt
```
where `<PID>` is the `khtpm_core_render` process ID for the vm-rv-li window.

## Common Tasks

### Find Window PID
```bash
ps aux | grep khtpm_core_render | grep vm-rv-li
```
Copy the PID (second column if ps aux format).

### Watch ui.txt Live
```bash
tail -f /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/vm-rv-li/state/ui.txt
```

### Send Keystroke via Relay (House Standard, Not xdotool)
```bash
PID=<paste-pid-from-ps-above>
RELAY=/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/$PID.txt

# Send Enter key (13)
echo "KEY_PRESSED: 13" >> $RELAY

# Send 'a' character (97)
echo "KEY_PRESSED: 97" >> $RELAY

# Send Ctrl-C (3)
echo "KEY_PRESSED: 3" >> $RELAY

# Send Backspace (8)
echo "KEY_PRESSED: 8" >> $RELAY

# Send Tab (9)
echo "KEY_PRESSED: 9" >> $RELAY

# Send arrow keys (200=Up, 201=Down, 202=Left, 203=Right)
echo "KEY_PRESSED: 200" >> $RELAY
```

### Read Relay File (What Happened?)
```bash
PID=<paste-pid>
RELAY=/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/$PID.txt
cat $RELAY
```

### Send Text via Agent File (Alternative)
```bash
# Append "type:ls" line to agent input (CORRECT path)
echo "type:ls" >> /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/vm_rv_li/agent_input.txt

# Or send bare line (no "type:" prefix)
echo "pwd" >> /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/vm_rv_li/human_input.txt
```

**Input format** (manager-understood):
- `type:COMMAND` — send text + newline (line 234-240 of vm_rv_li_manager.c)
- `paste:TEXT` — send text without newline (line 241-243)
- `key:DECIMAL` — send single byte (0-255) as decimal (line 244-246)
- Plain line — treated as typed text + newline (line 247-251)

### Check Emulator Process
```bash
# Is it running?
ps aux | grep "RVLV_EMUL.*build/main" | grep -v grep

# Emulator stdout goes to manager's stdout, not displayed directly
# Read ui.txt to see what the emulator output
```

### Rebuild Manager
```bash
cd /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/vm-rv-li
sh build.sh
# Output: +x/vm_rv_li_manager.+x (gitignored binary)
```

### Kill Window (Clean Reset)
```bash
# Find the window PID
PID=$(ps aux | grep khtpm_core_render | grep vm-rv-li | awk '{print $2}')

# Kill it
kill $PID

# Wait for cleanup, then relaunch from taskbar or:
sh /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/vm-rv-li/button.sh
```

## State File Format (ui.txt)

Example:
```
app=vm-rv-li
cols=80
rows=24
rowcount=24
row_0_text=                                                                                
row_1_text=xv6 kernel is booting
row_2_text=                                                                                
...
row_23_text=                                                                                
cursor_x=0
cursor_y=3
status=running
interact_armed=1
input_action='...' 'type'
```

**Key fields**:
- `status=running` — emulator is live
- `rowcount=24` — grid height
- `row_N_text` — terminal line N (0-23)
- `cursor_x/y` — cursor position
- `interact_armed=1` — ready for input

## Manager Code Paths (If Debugging C)

**File**: `vm_rv_li_manager.c`

Key functions:
- `main()` — spawn emulator, setup pipes, enter tick loop
- `reparse_config()` — read config.pdl, apply new emu_args
- `drain_input_files()` — read human_input.txt + agent_input.txt, write bytes to emulator stdin
- `read_emu_output()` — non-blocking read from emulator stdout (UART)
- `strip_ansi_into_grid()` — parse ANSI sequences, update grid
- `publish_ui_state()` — write ui.txt atomically

## House Standards Checklist

Before committing changes:
- [ ] Manager compiles clean: `gcc -std=c11 -Wall -O2` (no `-Werror`)
- [ ] Fresh build: `sh build.sh`
- [ ] Fresh run: launch from taskbar or button.sh
- [ ] Real evidence: ui.txt shows running state, xv6 boot message present
- [ ] Input test: keystroke via relay file arrives at guest (visible in ui.txt)
- [ ] Process cleanup: `SIGTERM` to window, no stray emulator processes
- [ ] Staging: `git add vm_rv_li_manager.c vm-rv-li.xhtpm vm-rv-li.css` (NOT state/ files)
- [ ] Commit message: "fix: ...", "docs: ...", or "scoped feature: ..." (full thought)
- [ ] Commit to own branch (not main, not another agent's branch)

## Related Documentation

- **README.md** — full architecture, config, verification tests
- **ASSET_STATUS.md** — asset inventory, Linux build checklist
- **../../vm_rv_li_manager.c** — manager source (single-file, ~450 lines)
- **../../../docs/CENTROID_GOLD_STD.md** — house rendering standard
- **../../../../../../AGENTS.md** — commit discipline, house rules
- **../../../../../../TEARIT-HQ/.claude/skills/khtpm-house-standards/** — khtpm testing patterns
