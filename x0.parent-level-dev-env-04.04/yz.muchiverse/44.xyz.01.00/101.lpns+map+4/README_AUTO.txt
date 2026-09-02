LPNS+MAP+2 - AUTO-LOAD SPATIAL GAME
Ledger-Driven Map Game with Pre-Configured Setup

================================================================================
QUICK START
================================================================================

cd 00.lpns+map+2/
bash button.sh run

No setup screen needed! Game loads from config.txt and starts immediately.

Default Setup (from config.txt):
  - Player 1: alice (HUMAN) at (0,0)
  - Player 2: bot1 (NPC auto-play) at (7,0)
  - Player 3: bot2 (NPC auto-play) at (0,7)
  - Player 4: bot3 (NPC auto-play) at (7,7)
  - 8x8 map
  - 5 epochs

================================================================================
CUSTOMIZING CONFIG
================================================================================

Edit config.txt to change:
  - Player names and types (player_X_name, player_X_type)
  - Starting positions (player_X_start_x, player_X_start_y)
  - Number of epochs (epoch_length=5 means 5 rounds)
  - Which players are human vs computer

Example config with 2 humans + 2 NPCs:
  num_players=4
  num_human_players=2
  num_npcs=2
  player_1_type=human
  player_1_name=alice
  player_1_start_x=0
  player_1_start_y=0
  player_2_type=human
  player_2_name=bob
  player_2_start_x=7
  player_2_start_y=7
  player_3_type=computer
  player_3_name=bot1
  player_3_start_x=0
  player_3_start_y=7
  player_4_type=computer
  player_4_name=bot2
  player_4_start_x=7
  player_4_start_y=0

================================================================================
HOW IT WORKS
================================================================================

1. On startup, button.sh checks if config.txt exists
2. If YES: Loads config, skips setup screen, starts game immediately
3. If NO: Runs interactive setup (chooses players, NPCs, map size, epochs)

Gameplay:
  - Human player chooses: (w)ord, (m)ove to X,Y, or (e)nd turn
  - NPC auto-play: random choice of word, move, or end turn
  - 8x8 map shows all player positions (1-8)
  - Moves stored in ledger, positions reconstructed from history

================================================================================
FEATURES
================================================================================

✓ Ledger-driven architecture (append-only transaction log)
✓ Ring topology (fair turn-based multiplayer)
✓ 8x8 spatial grid with player movement
✓ 1 human player + 0-3 NPC auto-play
✓ NPC random selection (33% word, 33% move, 33% end turn)
✓ Position validation (bounds check, occupancy check)
✓ Master ledger shows all actions (word, move, end_turn)
✓ Player-filtered ledgers (each player sees only their own entries)
✓ ASCII map rendering with live position updates

================================================================================
FILES
================================================================================

config.txt          - Pre-configured game setup (auto-loads on startup)
registry/word_bank.txt  - 20 words for NPC word selection
button.sh           - Main launcher
data/config.txt     - Active game config (runtime)
data/master_ledger.txt  - All game actions (with movement data)
players/alice/ledger.txt, etc. - Per-player filtered logs
ops/game_init.c     - Setup with map support
ops/reconstruct_map.c   - 8x8 grid renderer
ops/handle_movement_input.c - Movement validation

================================================================================
LEDGER FORMAT
================================================================================

timestamp|epoch|player|turn|action_data|action_type

Examples:
  2026-07-24T05:32:14|3|alice|10|hello|word
  2026-07-23T22:32:15|3|alice|11|x:5,y:3|move
  2026-07-24T05:32:15|4|alice|12|N/A|end_turn

================================================================================
