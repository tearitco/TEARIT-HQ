# Wall Street Race - macOS Documentation

## Overview
Wall Street Race is a multi-platform, multiplayer (and single-player) wall street simulator that simulates financial markets, trading, corporate management, and economic events in a dynamic game environment.

## Architecture

### Core Components
- **Game Engine**: The main game loop is located in `main.c` and `game.c`, managing the overall game state, time progression, and UI rendering
- **Thread Orchestrator**: `^.button.🔘️.🪄️🔮️📲️h12.c` - A sophisticated thread management system that runs multiple game subsystems concurrently
- **Event Systems**: Multiple loop functions (`clockwise_loop.c`, `day_loop.c`, `hour_loop.c`, `dividend_loop.c`, etc.) that manage different aspects of the simulation
- **AI Components**: Located in the `/ai` directory with modules for approximation and real-time decision making
- **Data Management**: Various `.c` files for managing corporations, governments, payroll, taxes, and financial data

### Threading Architecture
The game uses a sophisticated threading system where:
- Each major game subsystem runs in its own thread
- The orchestrator (`^.button.🔘️.🪄️🔮️📲️h12.c`) manages these threads and can detect when "killer" threads (critical components) exit
- Commands are read from `locations.txt` and executed by different threads
- The system supports both foreground and background execution (marked with `&`)

### File Structure
```
├── +x/                    # Compiled executables
├── ai/                    # AI-related components
├── db/                    # Database components
├── dev/                   # Development tools
├── xs/                    # junk code
├── *.c files             # Core game components
├── locations.txt         # Configuration file for thread commands
└── gl_cli_out.txt        # Global CLI output log
```

## Game Functionality

### Player Management
- Multiplayer support with player accounts and financial positions
- Corporate ownership and management
- Certificate of Deposit (CD) system for savings
- Trading and investment mechanisms

### Economic Systems
- Dynamic market simulation with hourly/dayly cycles
- Corporate dividend distribution
- Government and public sector management
- Tax systems and payroll processing
- Inflation and economic indicators

### Time Management
- Hourly, daily, and monthly game cycles
- Real-time market movements
- Scheduled events and updates
- Time-based financial computations

## Completion State: ~45%

### Completed Features
- ✅ Basic game loop and state management
- ✅ Multi-threaded architecture with thread orchestration
- ✅ Core financial systems (CDs, trading, dividends)
- ✅ Player account management
- ✅ Corporate management (setup, incorporation)
- ✅ Basic AI components
- ✅ Event loop systems (hourly, daily, etc.)
- ✅ Cross-platform compilation support (including macOS fixes)

### Missing Features
- ❌ Complete AI implementation in event loops
- ❌ Advanced player management functions
- ❌ Complete multiplayer networking
- ❌ Advanced corporate management tools
- ❌ Enhanced government interaction
- ❌ Complete debugging and error handling
- ❌ Advanced visualization (currently CLI based)
- ❌ Complete save/load systems
- ❌ Advanced trading algorithms

## Technical Requirements for Completion

### 1. AI Integration
- **Priority**: High
- **Task**: Integrate AI components into the main game loops
- **Technical Details**: Connect `ai_manager.c` and related modules to the main game loop in `game.c`

### 2. Networking Layer
- **Priority**: High
- **Task**: Implement multiplayer networking infrastructure
- **Technical Details**: Add socket-based communication for multiplayer functionality

### 3. Advanced Game State Management
- **Priority**: High
- **Task**: Implement comprehensive save/load functionality
- **Technical Details**: Create state serialization for complex data structures

### 4. Enhanced Management Functions
- **Priority**: Medium
- **Task**: Complete corporate and government management systems
- **Technical Details**: Extend `setup_corporations.c`, `setup_governments.c`, and related management files

### 5. Platform-Specific Optimizations
- **Priority**: Medium
- **Task**: Optimize for macOS and other target platforms
- **Technical Details**: Fix any remaining macOS-specific issues (note: `stdbuf` dependency added to PATH)

### 6. Error Handling & Debugging
- **Priority**: Medium
- **Task**: Implement comprehensive error handling
- **Technical Details**: Add proper error checking and recovery mechanisms

### 7. User Interface Enhancement
- **Priority**: Low-Medium
- **Task**: Improve CLI interface or implement GUI
- **Technical Details**: Potentially add OpenGL-based visualization

## Known Issues
- `stdbuf` command requirement for buffering (resolved by installing coreutils on macOS)
- Some format specifier issues in `setup_corporations.c`
- The thread manager (`^.button.🔘️.🪄️🔮️📲️h12.c`) required pthread extension fixes for macOS compatibility
- Missing `usleep` declaration in some files

## Dependencies
- Coreutils (for `stdbuf` command) - installed via Homebrew on macOS
- OpenGL framework for graphics (when enabled)
- pthread library for threading
- Various system libraries for networking and file I/O

## Build Instructions
1. Ensure coreutils is installed (`brew install coreutils`) and path is updated
2. Run `./xsh.compile-all.+x.sh` to compile all components
3. Execute with `./<main_executable>` or use the threading orchestrator

## Platform Notes
- **macOS**: Requires installation of coreutils for `stdbuf` compatibility
- **Linux**: Should work with standard coreutils installation
- **Threading**: The system has been made portable with fallbacks for pthread extensions
