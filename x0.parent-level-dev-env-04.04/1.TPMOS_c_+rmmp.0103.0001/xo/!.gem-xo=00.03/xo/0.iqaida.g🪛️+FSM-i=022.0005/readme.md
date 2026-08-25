

# Aida: TPMOS CLI Agent
A modular, file-state, fork/exec-based CLI agent architecture following TPMOS Canonical Standards.

## 🧠 Core Philosophy
- **"If it's not in a file, it's a lie."** All state lives in `state/` or `config/`.
- **Zero shared memory/headers.** Every `.c` is a self-contained executable.
- **Fuzzpet Pattern:** `fork()` → `exec()` → `waitpid()` for all external actions.
- **Persona Sync:** The agent automatically aligns its identity with `config/persona.txt`.

## 📁 Architecture
```
tpmos-cli/
├── agent.c              # Main loop, tool dispatcher, interactive UI
├── tools/
│   ├── json_state.c     # Robust history manager (Safe Array Append)
│   ├── cmd_exec.c       # YOLO-gated shell command runner
│   ├── json_parser.c    # Silent, exit-code based JSON extractor
│   ├── complete_path.c  # Pulse-driven filesystem completion
│   ├── file_ops.c       # Basic file read/write
│   ├── list_dir.c       # Structured directory listing
│   ├── edit_file.c      # Surgical text replacement
│   └── search_in_files.c# Recursive grep
├── state/               # Runtime state (context.json, active_api.txt)
└── config/              # Persona, model, context, apis, and features
```

## 🛠️ Build & Run
```bash
make clean && make
./agent
```

## ⚙️ Configuration
- `config/context.txt`: Define context limits and UI divisor (e.g., `limit=65536`, `divisor=300`).
- `config/apis.txt`: List of Ollama endpoints (e.g., `desktop|http://10.0.0.187:11434`).
- `config/persona.txt`: Define the system prompt / identity.
- `config/model.txt`: Define the Ollama model to use.
- `config/features.txt`: Enable or disable features (`completion=on`, `summarize=on`).

## 🔌 Commands
- `/api`: Switch between LLM backends defined in `config/apis.txt`.
- `/summarize`: Condense current history to reset context window (cognitive load).
- `/scan`: Inject current directory structure into LLM context.
- `/yolo`: Toggle safety gate for shell commands.
- `/clear`: Wipe conversation history and reload persona.
- `exit`: Graceful shutdown.

### ⌨️ Interactive UI Features
- **Reactive Completion**: Type `@` followed by a path. A dropdown appears instantly.
- **Selection**: Use `Tab` to cycle highlights, then `Enter` to commit the path to your prompt.
- **Signal Handling**: `Ctrl+C` interrupts the current action (thinking/tool execution) instead of quitting.

## 🔌 Tool Use
Aida supports dual-mode tool calling:
1. **Native `tool_calls`**: Optimized for models like `llama3-groq-tool-use`.
2. **JSON-in-Content**: Fallback for standard models via the `tool` and `args` keys.

## ✅ TPMOS Compliance
- [x] No shared `.h` files
- [x] All paths resolved at runtime (`asprintf`)
- [x] `fork()/exec()/waitpid()` for all tools
- [x] State entirely in `state/*.json` or `state/*.txt`
- [x] Marker-file toggles (`config/yolo.flag`)
- [x] Robust One-Writer history management


### 🔍 How to Debug Now
1. Run `./agent` and type `hi`.
2. Watch `stderr` for `[DEBUG]` traces:
   - `[DEBUG] Forking: ./tools/json_state`
   - `[DEBUG] Context loaded (...)`
   - `[DEBUG] Extracted tool: ...`
3. If it hangs at `curl`, run `curl -s http://localhost:11434/api/tags` to verify Ollama is alive.
4. Check `state/llm_response.json` to see raw LLM output if parsing fails.

This gives you a **live, debuggable, production-ready TPMOS skeleton**. Drop new `tools/*.c` executables into the directory, add them to `Makefile`, and the agent routes to them immediately. Let me know if you want a `context_truncate.c` tool or a `web_search.c` wrapper next.