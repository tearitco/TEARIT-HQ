# Groq-Ollama Local Interface Design Document
**Project ID:** groq-ollama
**Version:** 1.0 (PoC)
**Status:** DRAFT

---

## 1. Project Overview
The `groq-ollama` project provides a local, ASCII-based interface to the Groq-Ollama Large Language Model. It allows users to interact with the model via a standard TPMOS CLI interface and enables autonomous pieces like `iqabel` to utilize the model for decision-making, script generation, and command execution.

---

## 2. Architecture: Piece-Module-OS
Following the TPMOS Canonical Bible:

### 2.1 PIECE: `iqabel`
- **Path:** `projects/groq-ollama/pieces/world_01/map_01/iqabel/`
- **DNA (`piece.pdl`):** Defines traits, available methods, and AI-related state.
- **State (`state.txt`):** Tracks current FSM state, last query, and RL weights.

### 2.2 MODULE: `groq-ollama_manager`
- **Path:** `projects/groq-ollama/manager/groq-ollama_manager.c`
- **Role:** 
    - Polls `pieces/keyboard/history.txt` for user input.
    - Manages the UI variables for `groq-ollama.chtpm`.
    - Forks/Execs the `groq-ollama_bridge` for AI queries.
    - Handles JSON parsing if interacting with Ollama.

### 2.3 OS / THEATER: `groq-ollama.chtpm`
- **Path:** `projects/groq-ollama/layouts/groq-ollama.chtpm`
- **UI:** ASCII layout with a large response area and a `cli_io` input field.

---

## 3. Implementation Steps

### Step 1: Scaffolding & Metadata
- Create `projects/groq-ollama/project.pdl`.
- Create `projects/groq-ollama/pieces/world_01/map_01/iqabel/piece.pdl`.
- **Registration:**
    - Append `groq-ollama` to `pieces/os/compiled_projects.txt`.
    - Create `pieces/os/ops_registry/groq-ollama.txt` listing `groq-ollama_bridge`.
    - Append `groq-ollama_bridge|Bridge to local Groq-Ollama LLM.|prompt` to `pieces/os/ops_catalog.txt`.
    - Add `groq-ollama=projects/groq-ollama/layouts/groq-ollama.chtpm` to `pieces/os/project_routes.kvp`.
    - These steps ensure the project is recognized by `fondu`, the global `LOAD_PROJECT` menu, and correctly routes to the `groq-ollama.chtpm` layout.

### Step 2: AI Bridge (`groq-ollama_bridge.c`)
- Implement a C module that wraps `/usr/local/bin/groq-ollama`.
- Capability to send prompts and capture stdout/stderr.
- Capability to parse JSON response blocks for structured command execution.

### Step 3: ASCII Layout (`groq-ollama.chtpm`)
- Header: `[ QWEN LOCAL INTERFACE ]`
- Body: `${groq-ollama_response}` (last 20 lines).
- Input: `<cli_io id="groq-ollama_input" label="Ask Groq-Ollama: " />`.
- Buttons for common tasks (e.g., "Clear History", "Export Script").

### Step 4: Manager Logic (`groq-ollama_manager.c`)
- Use the **Fuzzpet Pattern** for CPU-safe execution.
- Monitor `pieces/apps/player_app/gui_state.txt` for `groq-ollama_input` updates.
- On `ENTER`:
    1. Capture input buffer.
    2. Write to `pieces/debug/groq-ollama_queries.txt`.
    3. Call `groq-ollama_bridge` with the prompt.
    4. Update `${groq-ollama_response}` and trigger render via `frame_changed.txt`.

### Step 5: `iqabel` FSM & RL Integration
- **FSM States:** `IDLE`, `THINKING`, `ACTING`, `LEARNING`.
- **Connect to Groq-Ollama:** `iqabel` writes its current state to a prompt template.
- **RL Loop:** 
    - Bot executes an AI-suggested command.
    - Success/Failure is labeled in `ledger.txt`.
    - Labeled memories are used to refine the "When to use script X" policy.

---

## 4. KPIs (Key Performance Indicators)
- **K1: Response Latency:** Time from `ENTER` to first character of response < 2s (local).
- **K2: Parse Accuracy:** > 95% success rate in extracting JSON commands from Ollama/Groq-Ollama output.
- **K3: FSM Reliability:** `iqabel` correctly transitions between states based on AI feedback 10/10 times in smoke tests.
- **K4: CPU Safety:** Manager stays under 2% CPU usage while idling (Pulse Throttling).

---

## 5. Integration with iqabel & Imitation Learning
`iqabel` will serve as the proof-of-concept for "Agentic AI in TPMOS". It will:
1.  Monitor the global `master_ledger.txt`.
2.  When it detects a system error or a complex user request, it will "Wake Up" (FSM transition).
3.  Query `groq-ollama` for a PAL script to resolve the situation.
4.  **Imitation Learning:** 
    - All user prompts and Groq-Ollama responses in the `groq-ollama` project are recorded to `projects/groq-ollama/pieces/world_01/map_01/iqabel/memories/`.
    - `iqabel` uses these (Prompt, Response) pairs to learn the "Ubiquitous Language" of the user and the system.
    - This allows the bot to imitate human troubleshooting patterns over time.
5.  Execute scripts via `prisc`.
6.  Wait for feedback and store the session in `pieces/memories/` for RL training.

---
*"The machine speaks, the piece acts, the ledger remembers."*
