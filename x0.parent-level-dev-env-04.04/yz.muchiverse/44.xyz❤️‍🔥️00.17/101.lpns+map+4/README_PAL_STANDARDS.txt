================================================================================
00.LPNS+MAP+3 - PROPER PAL STANDARDS IMPLEMENTATION
================================================================================

ARCHITECTURE OVERVIEW
================================================================================

This +3 version follows the muchiverse xyzos-standards.txt exactly:

✓ PERSISTENT MODULE PROCESS
  - prisc+x running pal/main_loop_chtpm.pal (not one-shot script)
  - Independent event loop (~30ms ticks) 
  - Matches real chtpm_manager.c precedent

✓ KEY INJECTION VIA RELAY FILE
  - Keys injected to pieces/apps/player_app/interact_relay.txt
  - Fire-and-forget, non-blocking (matching real chtpm_parser.c)
  - Module reads on each loop tick

✓ FRAME-BASED STATE TRACKING
  - pieces/display/game_state_changed.txt marks when state changed
  - Only re-renders (compose_frame) when marker changed
  - Prevents flicker from constant polling

✓ SELF-CONTAINED OPS
  - game_compose_frame.c - reads config, rebuilds map, outputs frame.txt
  - game_hit_frame.c - displays frame to output
  - game_turn_input.c - processes single key, updates config/ledger

✓ LEDGER-FIRST DESIGN (unchanged from +2)
  - config.txt is source of truth for game state
  - data/master_ledger.txt stores all actions
  - Position reconstructed from ledger on each render

================================================================================
KEY DIFFERENCES FROM +2 (SHELL SCRIPT)
================================================================================

+2 (Shell):
  └─ button.sh runs ONE-SHOT game loop (old style)
  └─ All inputs consumed upfront (no real interactivity)
  └─ Game logic mixed in shell + C ops

+3 (PAL Standards):
  ├─ prisc+x module runs persistent main_loop_chtpm.pal
  ├─ Key injection happens during play (truly interactive)
  ├─ Game logic in pure ops, pal loop is just I/O orchestration
  ├─ Matches real chtpm architecture (wsr-pal, pal-chain)
  └─ Frame history enables proper testing, debugging, replay

================================================================================
RUNNING THE GAME
================================================================================

cd 00.lpns+map+3/
bash button.sh run

Prompts for single-key input (w/m/e/q):
  w - current player says a word
  m - current player moves
  e - end turn
  q - quit game

The persistent module handles NPC auto-play between human inputs.

================================================================================
FILE STRUCTURE
================================================================================

pal/
  └─ main_loop_chtpm.pal
       Main event loop, runs in prisc+x process
       ├─ Reads from interact_relay.txt (key injection)
       ├─ Calls ops on each game tick
       ├─ Checks game_state_changed marker to gate re-renders
       └─ ~30ms sleep between ticks

ops/
  ├─ game_compose_frame.c
  │  ├─ Reads config.txt (current game state)
  │  ├─ Replays ledger to reconstruct map
  │  └─ Writes pieces/display/frame.txt
  ├─ game_turn_input.c
  │  ├─ Processes single key input
  │  ├─ Calls NPC auto-play if needed
  │  ├─ Updates config.txt (turn, epoch)
  │  └─ Marks game_state_changed
  └─ game_hit_frame.c
     └─ Displays frame.txt to output

pieces/
  ├─ apps/player_app/
  │  └─ interact_relay.txt (key injection file)
  └─ display/
     ├─ frame.txt (current render)
     └─ game_state_changed.txt (marker)

config.txt
  ├─ num_players, num_npcs
  ├─ npc_move_weight, npc_word_weight, npc_end_weight
  ├─ player names and types
  ├─ current_epoch, current_turn
  └─ game_state

registry/word_bank.txt - 20 words for NPC selection

data/master_ledger.txt - append-only action log
  Format: timestamp|epoch|player|turn|action_data|action_type

================================================================================
PAL STANDARDS COMPLIANCE CHECKLIST
================================================================================

[✓] Persistent module process (prisc+x running .pal loop)
[✓] Key injection via relay file (interact_relay.txt)
[✓] Frame-based rendering (only when game_state_changed)
[✓] Sleep 30000 (~30ms) ticks matching real chtpm
[✓] No quit path in module (quit is chtpm's job)
[✓] State tracked in config.txt, not in module memory
[✓] Self-contained ops (each is standalone binary)
[✓] Two-layer async architecture (chtpm <-> module via relay)
[✓] Ledger-first (position = derived from history, not authoritative)

================================================================================
NEXT PHASES
================================================================================

PHASE 1 (CURRENT): Architecture correct, basic gameplay working
  ✓ Persistent module with event loop
  ✓ Frame-based rendering
  ✓ Key injection via relay file
  ✓ NPC auto-play with weights
  ✓ Ledger-driven state

PHASE 2: Full chtpm GUI integration
  - Replace interact_relay manual testing with real chtpm onclick handlers
  - Replace frame.txt with proper HTML/SVG layout
  - Integrate pieces/chtpm for visual rendering
  - Add pieces_template for code generation

PHASE 3: Multi-player P2P sync
  - Ledger export/import for P2P state transfer
  - Actor-specific ledger filtering (players/alice/ledger.txt)
  - Crash recovery via ledger replay

PHASE 4: Polish
  - Touch detection for map clicks
  - Animation/smooth movement
  - Score tracking
  - Item/obstacle system

================================================================================
TESTING
================================================================================

Test Frame History:
  bash button.sh run
  cat pieces/display/frame.txt

Test Ledger:
  cat data/master_ledger.txt

Test Interact Relay:
  grep -E "." pieces/apps/player_app/interact_relay.txt

Key Injection (manual testing):
  echo "w" >> pieces/apps/player_app/interact_relay.txt
  (module will pick it up on next loop tick)

================================================================================
COMPARISON WITH REFERENCE IMPLEMENTATIONS
================================================================================

wsr-pal (financial game menu):
  ├─ pal/main_loop_chtpm.pal - same structure, different ops
  ├─ ops/wsr_compose_frame.c - renders stock market UI
  ├─ ops/wsr_menu_input.c - processes menu keys
  └─ Uses wsr_screen_changed.txt marker

pal-chain (blockchain explorer menu):
  ├─ pal/main_loop_chtpm.pal - identical to wsr-pal
  ├─ ops/chain_compose_frame.c - renders blockchain
  ├─ ops/chain_menu_input.c - processes explorer keys
  └─ Uses chain_screen_changed.txt marker

This game (+3):
  ├─ pal/main_loop_chtpm.pal - same architectural pattern
  ├─ ops/game_compose_frame.c - renders 8x8 map
  ├─ ops/game_turn_input.c - processes game keys
  └─ Uses game_state_changed.txt marker

All three follow the SAME pattern - only the op details differ.

================================================================================
END README_PAL_STANDARDS.txt
