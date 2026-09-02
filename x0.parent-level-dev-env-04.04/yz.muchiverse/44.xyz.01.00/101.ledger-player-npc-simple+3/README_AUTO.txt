LPNS+2 - AUTO-LOAD WORD GAME
Ledger-Driven Word Game with Pre-Configured Setup

================================================================================
QUICK START
================================================================================

cd 0.ledger-player-npc-simple+2/
bash button.sh run

No setup screen needed! Game loads from config.txt and starts immediately.

Default Setup (from config.txt):
  - Player 1: alice (HUMAN)
  - Player 2: bot1 (NPC auto-play)
  - Player 3: bot2 (NPC auto-play)
  - Player 4: bot3 (NPC auto-play)
  - 5 epochs

================================================================================
CUSTOMIZING CONFIG
================================================================================

Edit config.txt to change:
  - Player names (player_1_name, player_2_name, etc.)
  - Number of epochs (epoch_length=5 means 5 rounds)
  - Which players are human vs computer (player_X_type=human or computer)

Example config with 2 humans + 2 NPCs:
  num_players=4
  num_human_players=2
  num_npcs=2
  player_1_type=human
  player_1_name=alice
  player_2_type=human
  player_2_name=bob
  player_3_type=computer
  player_3_name=bot1
  player_4_type=computer
  player_4_name=bot2

================================================================================
HOW IT WORKS
================================================================================

1. On startup, button.sh checks if config.txt exists
2. If YES: Loads config, skips setup screen, starts game immediately
3. If NO: Runs interactive setup (chooses players, NPCs, epochs)

This means:
  - Keep config.txt for automatic/scripted play
  - Delete config.txt for interactive setup (just run and answer questions)

================================================================================
FEATURES
================================================================================

✓ Ledger-driven architecture (append-only transaction log)
✓ Ring topology (fair turn-based multiplayer)
✓ 1 human player + 0-3 NPC auto-play
✓ NPC random selection (33% word, 33% word, 33% skip)
✓ Master ledger shows all actions
✓ Player-filtered ledgers (each player sees only their own entries)

================================================================================
FILES
================================================================================

config.txt          - Pre-configured game setup (auto-loads on startup)
button.sh           - Main launcher
data/config.txt     - Active game config (runtime)
data/master_ledger.txt  - All game actions
players/alice/ledger.txt, etc. - Per-player filtered logs

================================================================================
