LPNS+MAP - LEDGER-DRIVEN SPATIAL GAME (PHASE 1 COMPLETE)
Working Demonstration of Map-Based Gameplay with Ledger Architecture

================================================================================
WHAT WAS BUILT
================================================================================

✅ COMPLETE PHASE 1: Map ASCII Rendering + Movement Input

  ✓ 8x8 spatial game map
  ✓ Player positions tracked in ledger (MOVE action format)
  ✓ Human player input: (w)ord, (m)ove, (e)nd turn
  ✓ NPC auto-play: randomly choose words or move
  ✓ Map reconstruction from ledger (replay all moves)
  ✓ Movement validation (bounds check, occupancy check)
  ✓ Full game loop: setup → play epochs → show final state

================================================================================
QUICK START
================================================================================

cd 00.lpns+map/
bash button.sh run

Setup:
  - Players: 2 (alice human, bob computer)
  - NPCs: 0
  - Epochs: 1

Gameplay:
  - Map displays 8x8 grid with players (1,2,3,4...) and NPCs (A,B,C,D...)
  - Players start at:
    * Player 1: (0,0)
    * Player 2: (7,0)
    * Player 3: (0,7)
    * Player 4: (7,7)
  - Human turn: choose (w)ord, (m)ove, (e)nd turn
  - NPC turn: auto-play (33% word, 33% move, 33% end turn)

Output:
  - Final map showing positions after all epochs
  - Master ledger with all actions (words + moves)
  - Each player's filtered ledger

================================================================================
FILES CREATED
================================================================================

OPERATIONS:

  ops/game_init.c
    - Setup with map support
    - Collects player names, NPC names, epochs
    - Initializes starting positions (default: corners + opposite corners)
    - Creates config.txt with all settings

  ops/reconstruct_map.c
    - Rebuilds 8x8 map from master ledger
    - Replays all MOVE actions to get current positions
    - Renders as ASCII (8x8 grid with actors as digits 1-8)
    - Shows current position list below map

  ops/handle_movement_input.c
    - Validates movement (bounds check 0-7, occupancy check)
    - Appends MOVE action to ledger
    - Format: x:N,y:M (e.g., "x:3,y:5")
    - Returns error if invalid

LAUNCHER:

  button.sh
    - Compiles all operations
    - Runs interactive game loop
    - Handles player input and NPC auto-play
    - Commands: run, map, clean

DATA:

  registry/word_bank.txt
    - 20 words for computer players

  data/master_ledger.txt (runtime)
    - Extended format: timestamp|epoch|player|turn|action_data|action_type
    - Actions: word, move, end_turn

  data/config.txt (runtime)
    - Game settings (players, NPCs, epochs)
    - Starting positions

================================================================================
LEDGER FORMAT (EXTENDED)
================================================================================

Old LPNS format (still supported):
  timestamp|epoch|player|turn|word|action_type
  Example: 2026-07-23T10:00:00|1|alice|0|hello|word

New MAP format:
  timestamp|epoch|player|turn|action_data|action_type
  Examples:
    - 2026-07-23T10:00:00|1|alice|0|hello|word
    - 2026-07-23T10:01:00|1|alice|1|x:3,y:5|move
    - 2026-07-23T10:02:00|1|bob|2|N/A|end_turn

Backward compatible: old entries still readable

================================================================================
ARCHITECTURE ALIGNMENT
================================================================================

✓ LEDGER-FIRST DESIGN
  - Map state derived from ledger (not authoritative)
  - All position changes → MOVE entries in ledger
  - Replay ledger from turn 0 to reconstruct any past game state

✓ RING TOPOLOGY
  - Turn management unchanged from LPNS
  - All actors (human + NPC) cycle fairly
  - current_player = turn % num_actors

✓ APPEND-ONLY LOG
  - Master ledger never modified (only appended)
  - Enables crash recovery, P2P sync, time-travel replay

✓ ACTOR-FILTERED LEDGERS
  - players/<actor>/ledger.txt contains only that actor's entries
  - Synced after each turn
  - Enables privacy + P2P efficiency

================================================================================
EXAMPLE GAME SESSION
================================================================================

Setup:
  2 players (alice, bob), 0 NPCs, 1 epoch

Turn 0 (alice):
  Chooses: (w)ord "hello"
  Ledger: 2026-07-23T10:00:00|1|alice|0|hello|word

Turn 1 (bob):
  Chooses: (m)ove to (5,3)
  Ledger: 2026-07-23T10:01:00|1|bob|1|x:5,y:3|move

Turn 2 (alice):
  Chooses: (m)ove to (2,1)
  Ledger: 2026-07-23T10:02:00|1|alice|2|x:2,y:1|move

Turn 3 (bob):
  Chooses: (e)nd turn
  Ledger: 2026-07-23T10:03:00|1|bob|3|N/A|end_turn

Final Map:
   01234567
 0 ........
 1 ..1.....  (alice at 2,1)
 2 ........
 3 .....2..  (bob at 5,3)
 4 ........
 5 ........
 6 ........
 7 ........

================================================================================
TESTING CHECKLIST
================================================================================

[✓] Game compiles and runs
[✓] Setup collects player names and epochs
[✓] Map displays correctly (8x8 grid)
[✓] Starting positions initialized
[✓] Human input: (w)ord works
[✓] Human input: (m)ove works with bounds validation
[✓] Human input: (e)nd turn works
[✓] NPC auto-play: word selection works
[✓] NPC auto-play: random movement works
[✓] Ledger format extended for moves
[✓] Map reconstruction from ledger works
[✓] Multiple epochs work
[ ] RGB window rendering (PHASE 2)
[ ] Position overlay in RGB (PHASE 3)
[ ] Collision detection edge cases
[ ] Crash recovery

================================================================================
PHASE 2 & 3 ROADMAP (NOT YET BUILT)
================================================================================

PHASE 2: Movement Validation Enhancements
  - Occupied cell detection
  - Prevent multiple actors same position
  - Optional: distance-based movement costs

PHASE 3: RGB Window Rendering
  - Separate display process (chtpm_rgb_render based)
  - 256x256 pixel window (32x32 per cell)
  - Color-coded actors (player 1=red, 2=blue, etc.)
  - Grid lines showing cell boundaries
  - Player name overlay on cells
  - Live sync with CLI (both read same ledger)

PHASE 4: Polish
  - Collision detection edge cases
  - Game-ending conditions (victory, timeout)
  - Score tracking
  - Optional: items on map, obstacles

================================================================================
KNOWN ISSUES
================================================================================

1. Name truncation on setup (cosmetic, same as LPNS)
   - Affects display only, not functionality

2. Random movement may hit occupied cells
   - Currently shows "NPC move failed" and skips
   - PHASE 2 will improve this

3. No persistence across runs
   - Each "bash button.sh run" starts fresh
   - By design (testing convenience)
   - Could add "resume" feature for Phase 4

================================================================================
CONCLUSION
================================================================================

LPNS+MAP demonstrates:

  1. Extended ledger format (supports multiple action types)
  2. Map reconstruction from ledger (derive spatial state)
  3. Movement validation (bounds, occupancy)
  4. Human + NPC gameplay on shared map
  5. Ledger-first architecture (position = derived, not authoritative)

Ready for Phase 2/3 (movement refinement, RGB rendering).

All code:
  - POSIX-compliant shell (sh, not bash)
  - Self-contained C operations
  - No external dependencies
  - Follows muchiverse patterns

================================================================================
END README_MAP_GAME.txt
