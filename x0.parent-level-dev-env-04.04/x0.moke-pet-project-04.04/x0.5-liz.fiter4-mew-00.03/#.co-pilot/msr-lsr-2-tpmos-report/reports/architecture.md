# WSR Project Architecture Report

## Overview
Martian Street Race (WSR) is a multi-process, terminal-based financial and political simulation. It operates on a real-time (or accelerated) clock system that triggers various economic and social events.

## Component Breakdown

### 1. The Orchestrator (`m.button.🔘️.🪄️🔮️📲️i13]mac.c`)
Located in the `$.m$rr.🔘️.®™]x2]ON!` directory, this is the main entry point. It handles:
- Initialization of shared resources.
- Spawning child processes for the Game UI, the Clock, and the Scheduler.
- Monitoring process health.

### 2. The Main Game (`game.c`)
The primary interface for the player. It provides:
- Navigation through various sub-menus (File, Settings, Management, Financing).
- Real-time display of the current game date and time.
- Interaction with entities (Founding corporations, managing portfolios).

### 3. The WSR Clock (`wsr_clock.c`)
A background process that:
- Maintains the internal game time.
- Saves the current state to `data/wsr_clock.txt`.
- Allows for time acceleration or pausing.

### 4. The Scheduler (`clockwise_loop.c`)
The "Heartbeat" of the simulation. It reads `presets/schedule.txt` and triggers:
- **Daily Loop:** Updates prices, generates daily news.
- **Hourly Loop:** Triggers rapid market fluctuations and news updates.
- **Quarterly/Yearly Loops:** Handles taxes, dividends, and government budget cycles.

### 5. Financial Engine (`analysis_loop.c`)
Analyzes all active entities and calculates:
- Stock price movements based on company performance and news.
- Market trends.
- Portfolio valuations.

## Inter-Process Communication (IPC)
The system primarily uses **File-Based IPC**.
- Modules read and write to common files in the `data/` directory (e.g., `wsr_clock.txt`, `master_ledger.txt`).
- The `master_reader.c` utility is used across various components to parse these state files efficiently.

## Directory Structure Strategy
- `corporations/`: Contains templates and generated data for all corporate entities.
- `governments/`: Contains data for Earth and Martian government entities.
- `data/`: The "Live" state of the game.
- `presets/`: Configuration and template data for starting new games.
- `ai/`: Contains experimental or approximation modules for market behavior.
