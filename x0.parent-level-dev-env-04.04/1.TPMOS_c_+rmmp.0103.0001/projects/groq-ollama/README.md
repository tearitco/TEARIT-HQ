# Groq-Ollama TPMOS Agent

> **Tool-calling verified working end-to-end 2026-07-15.** Before assuming
> it's broken again, read `!!.EMERGENCY-READ-ME-IT-WORKS.txt` in this
> directory - the model is picky about phrasing and this hardware is slow,
> both of which look like bugs but aren't.

**Status:** Canonical TPMOS Integration Active
**Model:** Dynamic Resolution (Ollama Tags)
**Date:** May 18, 2026

## 1. Overview
High-fidelity integration of the **Aida** agent logic into TPMOS. This project provides a multi-turn, tool-calling agent capable of local system interaction via Groq-supported models in Ollama.

## 2. Architecture & CPU Safety
The manager (`groq-ollama_manager.c`) is designed for maximum responsiveness and system health:
- **Non-Blocking queries:** AI calls (`curl`) run in background processes. The UI remains smooth even during 5-minute timeouts.
- **CPU Health (PID Tracking):** Every process (Manager, Curl, Tools) is logged to `pieces/os/proc_list.txt` and uses `setpgid(0,0)` for clean termination by the TPMOS orchestrator.
- **Compile Clean:** Adheres to PIECEMARK-IT standards (dynamic allocation via `asprintf`, no format truncation warnings).

## 3. Usage & Controls
- **Input:** Type into "Ask Agent" and hit **Enter**.
- **[1] Clear Context:** Wipes conversation history. Automatically re-injects Aida's persona on the next query.
- **[2] Switch API:** Toggles between endpoints in `config/apis.txt`. **Automatically re-resolves the best model for the new host** (e.g., switches to `groq-tool-use` when connecting to the Mac).
- **[3] Summarize:** Condenses the conversation into a JSON summary.
- **[ESC] Exit:** Returns to Project Loader.

## 4. Network Setup
- **Master API (Mac):** 10.0.0.144:11434. 
- **Remote Access:** Ensure the Mac runs Ollama with `OLLAMA_HOST=0.0.0.0`. Use the provided `mac_remote_fix.sh` script to automate this.
- **Timeouts:** Set to 300s to allow for cold-starting large models on remote hardware.

## 5. Troubleshooting
- **Empty JSON ({}) Responses:** Usually caused by a missing system prompt. Fixed by automatic persona injection.
- **Curl Code 7:** Connection refused. Check if `OLLAMA_HOST` is set correctly on the target machine.
- **Curl Code 28:** Operation timeout. Check `state/curl_debug.log` for verbose handshake details.

## 6. Engineering Standards
- **Binary Paths:** Resolved dynamically via `config/paths.txt`.
- **Sandbox Mode:** All tool executions (read/write/exec) are confined to `projects/groq-ollama/sandbox`.
- **Logs:** Full interaction history stored in `iqabel/memories/history.txt`.
