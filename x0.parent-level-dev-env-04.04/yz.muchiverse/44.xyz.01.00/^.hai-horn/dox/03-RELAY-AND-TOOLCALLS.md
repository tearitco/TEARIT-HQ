# Relay injection, tool calls, and testing for HORN_CHAT

## Relay injection

HORN_CHAT writes its state to `.horn-sessions/` so other house components can read it without coupling to HORN's process. This follows the house standard: real file-based state, never in-memory.

Files produced:
- `relay.txt` — append-only event log. Each reply appends `HORN_REPLY: <text>`. Other windows/managers can poll this file to react to HORN's output.
- `state.txt` — latest state snapshot. Written on each turn: `last_reply=<text>`.
- `chat_history.txt` — full conversation log, same format as existing house chat ops.

## Running a test / generating a report

```bash
bash horn_chat_test.sh
```

This sends a fixed prompt to OpenRouter, verifies the reply contains the expected token, and writes a pass/fail report to `.horn-sessions/test_report.txt`.

To inspect the report:
```bash
cat .horn-sessions/test_report.txt
```

## Tool calls

**Current status:** HORN_CHAT does basic text completion only. OpenRouter models support tool/function calling, but `horn_chat_openrouter.c` does not yet use that API.

**Planned approach:**
1. Add a `tools` array to the JSON payload sent to OpenRouter.
2. When the model returns a `tool_calls` response, parse it and route to local shell ops or PAL scripts.
3. Tool results are sent back to the model for a final reply.

For v0.1, tool calls are out of scope. The relay + test harness above proves the house-driving layer works before adding that complexity.

## K9 relay driving HORN itself

The house standard prefers driving windows via relay file injection rather than synthetic input. If HORN_CHAT is wrapped in a `khtpm` window in the future, the input loop should poll a per-PID relay file for `KEY_PRESSED` events instead of reading stdin directly. For now, the shell main loop uses stdin; relay injection is used for **output** so other house components can see HORN's state.
