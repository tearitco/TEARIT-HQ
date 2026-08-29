> **SUPERSEDED (flagged 2026-08-29 doc-audit pass):** `!.tpmos-vs-khtpm_pal.md`
> and `archive/temp/!.mass-refactor.md` both state this doc's invocation
> order is inverted vs. what any of the 7 real projects actually run.
> Despite the "(CORRECT)" in the title, treat `!.tpmos-vs-khtpm_pal.md`
> as the current reference, not this file.

# TPMOS Orchestrator Pattern (CORRECT)

## Architecture

```
User runs: orchestrator.c (meta-launcher)
    ↓
orchestrator.c calls: button.sh (starts all services)
    ↓
button.sh starts (in background):
  1. renderer
  2. chtpm_parser_pal (with layout)
  3. game_manager (polls input, calls ops)
  4. keyboard_input
    ↓
button.sh EXITS (does not wait)
    ↓
orchestrator.c stays alive, monitors child processes
    ↓
User presses Ctrl+C
    ↓
orchestrator.c catches signal, kills all children, exits gracefully
```

## Key Points

1. **orchestrator.c** is the meta-launcher (NOT the game manager)
   - Forks button.sh as background process
   - Stays alive monitoring
   - Catches Ctrl+C and kills all children
   - Then exits

2. **button.sh** starts ALL services and EXITS
   - Compiles binaries
   - Initializes game state
   - Launches renderer, chtpm, manager, keyboard_input in background
   - IMMEDIATELY EXITS (does not wait for input)

3. **game_manager.c** (C program, pthread) is the GAME LOOP orchestrator
   - Polls pieces/apps/player_app/history.txt (where keyboard_input writes)
   - Routes input to game_turn_input ops
   - Syncs state
   - Pulses frame_changed.txt marker
   - Runs in background thread

4. **chtpm_parser_pal** is the DISPLAY renderer
   - Reads the layout file
   - Displays ${game_map} variable from state
   - No module spawning needed (no PAL script)
   - Just renders

5. **keyboard_input** captures raw keys
   - Writes to pieces/apps/player_app/history.txt
   - Runs in background

## No PAL Script

- The old approach of running prisc+x with a PAL script is WRONG
- Manager IS the game loop - it reads input and calls ops directly
- Don't use modules or PAL scripts for this architecture

## State Flow

```
keyboard_input writes keycode
    ↓
pieces/apps/player_app/history.txt grows
    ↓
game_manager polling sees new entry
    ↓
game_manager converts keycode to action
    ↓
game_manager calls ./ops/game_turn_input <action>
    ↓
game_turn_input updates master_ledger.txt
    ↓
game_manager calls ./ops/game_compose_frame
    ↓
game_compose_frame reads master_ledger, renders frame
    ↓
game_compose_frame writes pieces/display/current_frame.txt
    ↓
game_manager pulses pieces/display/frame_changed.txt
    ↓
chtpm_parser_pal detects pulse (via stat)
    ↓
chtpm_parser_pal re-renders terminal
```

## Files

- **orchestrator.c** — meta-launcher (from TPMOS, adapts for each project)
- **button.sh** — startup script (starts services and exits)
- **game_manager.c** — game loop (polls input, calls ops)
- **chtpm_parser_pal.c** — display renderer (renders layout)
- **keyboard_input.c** — input capture (writes to history)
- **ops/game_turn_input** — game logic (updates state)
- **ops/game_compose_frame** — renderer (reads state, writes frame)

## Layout File

Simple. Just display the game map. No modules, no PAL scripts.

```xml
<panel>
    <text label="${game_map}" /><br/>
</panel>
```

That's it.
