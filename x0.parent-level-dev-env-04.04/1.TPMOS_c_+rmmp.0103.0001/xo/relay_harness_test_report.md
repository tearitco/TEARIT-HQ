# Relay Harness Test Report: tomom Chatbot

**Date:** 2026-10-22  
**Project:** tomom (3-stage LLM chatbot)  
**Location:** `44.xyz.01.00/#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️`

## Summary

Tested the tomom chatbot (C-based neural network with Mixture of Experts) through four relay interface approaches: direct binary, iqa-fork `ask.sh` relay (LAN Mac ollama), HTTP relay (`http_server.c`), and local ollama. The gem-dev harness and bot5 are available but are concurrent agent frameworks, not LLM chat relay harnesses. No "middle-house" solution exists in the codebase.

## Relay Approaches Tested

### 1. Direct Binary (baseline)

```
.//+x/chatbot_moe_v1.+x curriculum_bank.txt "What is math" 5 0.8
```

**Result:** Works. Loads 2610 vocab words across 15 curricula. Response is weak (e.g., "their their the language the") — the model is untrained/synthetic, producing word-order artifacts. Expected behavior for a prototype without trained weights.

### 2. iqa-fork ask.sh Relay (LAN Mac gemma3:270m)

```
bash 44xyz.01.00/iqa-fork/ask.sh gemma3:270m "What is math?" 15
```

**Result:** Works well. This is a curl-based relay to `10.0.0.144:11434` (LAN Mac ollama). Returns coherent, well-formed responses:
> "Mathematics is a branch of science and mathematics that deals with the study of numbers, shapes, and concepts."

The `ask.sh` script uses `json_parser.+x` from `045.muchi-pal-agent` to extract the response content from the JSON API output. **This is a valid LLM relay harness.**

### 3. HTTP Relay (tomom http_server.c)

```
./+x/http_server.+x  (serves on port 8080)
curl http://127.0.0.1:8080/api/chat
```

**Result:** **BROKEN.** The HTTP server compiles and serves the HTML web interface (GET `/` returns a Bootstrap page with title "3-Stage LLM Chatbot Interface"). However, the API endpoint `/api/chat` (POST) has a request-parsing bug: it scans the HTTP buffer for `\r\n\r\n` to find the JSON body, but the buffer parsing logic corrupts the JSON payload, causing the chatbot to attempt loading files named "number", "start-token", "end-token", etc. — treating each line of the JSON/body as a vocab file path.

This results in:
- All "Error opening vocab file: No such file or directory" messages
- "Merged vocabulary size: 0"
- **Segmentation fault** (null pointer dereference on empty vocab)

The server does return a default JSON envelope: `{"prompt":"Hello","response":"","curriculum":"vocab_model.txt","length":10,"temperature":1.000000}`

**Root cause:** The `handle_api_request()` function at line 198 of `http_server.c` passes `body` to `handle_chat_request()`, but the body extraction is broken. The request body is being consumed by the request-line tokenizer (`strtok(buffer, " ")`) and the content is lost/corrupted.

### 4. Local Ollama (llama3:latest)

```
curl http://127.0.0.1:11434/api/generate -d '{"model":"llama3:latest","prompt":"Hello","stream":false}'
```

**Result:** Slow startup (~10s) but works. Local `llama3:latest` model is available. Larger models (e.g., llama3) have significant startup latency. Local ollama has only `llama2:latest` and `llama3:latest`.

## Relay Harness Options Survey

### gem-dev Harness

- **Location:** `1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev/`
- **Type:** Concurrent TPMOS application (not a chat relay)
- **Purpose:** CLI path-completion dev environment (gem-dev = "general editing medium")
- **Relevance:** NOT an LLM relay harness. Has a web UI but for code editing/dev, not LLM chat relaying.
- **Status:** Available but not applicable for LLM chat relay testing.

### iqa-fork ask.sh

- **Location:** `44.xyz.01.00/iqa-fork/ask.sh`
- **Type:** Shell script relay to Ollama API
- **Purpose:** Query a Mac LAN ollama model and return clean text output
- **Relevance:** **Valid LLM relay harness.** Uses `curl` to POST to `/api/generate` on `10.0.0.144:11434`, pipes response through `json_parser.+x` to extract content.
- **Status:** Tested and working. This is the most practical relay for the tomom chatbot.

### pal-chat-irc Test Harness (relay via key injection)

- **Location:** `44.xyz.01.00/044.pal-chat-irc👥️+2/test-harn-same/`
- **Type:** Key-injection testing framework for P2P chat
- **Purpose:** Drive a chat UI by injecting keyboard events (KEY_PRESSED lines) into session state files
- **Ops available:** `tk_inject_key`, `tk_type_text`, `tk_focus_item`, `tk_assert_contains`
- **Relevance:** Could be used to drive the tomom HTTP web interface by injecting keystrokes into a TTY session, but this is a UX testing harness, not an LLM relay.
- **Status:** Ops compile-ready but not set up for this project.

### bot5 (in xo/)

- **Location:** `1.TPMOS_c_+rmmp.0103.0001/xo/bot5/`
- **Type:** Concurrent Exo-Bot v5 muscle/agent framework
- **Purpose:** Orchestrator + keyboard muscle concurrent process launcher
- **Relevance:** Not an LLM relay. It's a TPMOS concurrent agent framework.
- **Status:** Available but not applicable.

### Middle-House Solution

**No middle-house solution exists in the codebase.** Searched for "middle.house", "middlehouse", "middle_house" — zero results. The term was not found in any file.

## Recommendations

1. **Use iqa-fork ask.sh as the relay:** It's the most practical working relay. For the tomom chatbot specifically, a simple wrapper can pipe the LLM response through the tomom vocab comparison (calculate similarity against vocab embeddings).

2. **Fix http_server.c:** The `/api/chat` endpoint has a parsing bug. The body extraction at line 363 (`strstr(buffer, "\r\n\r\n")`) appears correct, but the issue is that `strtok()` mutates the buffer before body extraction happens. The tokenizer at lines 351-353 corrupts the buffer content.

3. **Local ollama for offline use:** Local `llama3:latest` works but has ~10s startup latency. For quick tests, use the LAN Mac `gemma3:270m` which responds in <2s.

## Available LLM Endpoints

| Endpoint | Model | Status | Notes |
|----------|-------|--------|-------|
| Local | llama3:latest | Working (slow ~10s) | Available at 127.0.0.1:11434 |
| Local | llama2:latest | Available (untested) | Available at 127.0.0.1:11434 |
| LAN Mac | gemma3:270m | Working (fast ~2s) | Available at 10.0.0.144:11434 |
| LAN Mac | gemma3:latest | Unknown | Mac may have larger gemma3 |
| Local | llama3 (chat) | Timed out | /api/chat endpoint failed; try /api/generate |

## Conclusion

The tomom chatbot can be relayed through the iqa-fork `ask.sh` harness (LAN Mac gemma3) for high-quality LLM responses. The HTTP server relay is broken due to a buffer-parsing bug but could be fixed. The gem-dev and bot5 harnesses are concurrent agent frameworks, not LLM chat relays. No "middle-house" solution exists.