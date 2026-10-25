# Handoff: tomom Dashboard & Relay Tests — 2026-10-25

## What was done this session

### Completed
- **Fixed `http_server.c` body parsing bug** — `strtok()` was corrupting the buffer before `\r\n\r\n` body extraction. Moved body pointer extraction before tokenizing.
- **Tested relay harnesses:**
  - **iqa-fork `ask.sh`** — working LLM relay to LAN Mac ollama (`10.0.0.144:11434`, gemma3:270m). Returns coherent responses.
  - **HTTP relay (`http_server.c`)** — now works after fix. `/api/chat` returns valid JSON with chatbot response. `/api/curricula` lists available files.
  - **Direct binary** — works, weak responses (untrained model).
  - **Local ollama (llama3:latest)** — works, slow startup (~10s).
- **gem-dev harness** — concurrent TPMOS agent framework, not an LLM relay.
- **bot5** — orchestrator + keyboard muscle, not an LLM relay.
- **No "middle-house" solution exists** — zero results for "middle.house" across codebase.
- **Created and committed**: `relay_harness_test_report.md` + `.html` in `xo/` directory.
- **Fixed button.sh path** — RENDERER path now correctly points to `44xyz.01.00/_.monads/...`.

### Remaining Issues
- **button.sh SCRIPT_DIR resolution** — The emoji in the tomom directory name causes bash path resolution to fail, so `khtpm_core_render.+x` is not found at runtime. Need to hardcode or use a different discovery method.

## Next Steps
1. Fix button.sh to correctly find `khtpm_core_render.+x` despite the emoji directory path
2. Test full `button.sh run` in an X11 terminal (needs raw mode for keyboard input)
3. Wire up khtpm interactive stack (keyboard_input, prisc+x) for .chtpm interactivity

## Files Changed
- `http_server.c` — buffer parsing fix
- `button.sh` — path fix for RENDERER
- `xo/relay_harness_test_report.md` — new
- `xo/relay_harness_test_report.html` — new
