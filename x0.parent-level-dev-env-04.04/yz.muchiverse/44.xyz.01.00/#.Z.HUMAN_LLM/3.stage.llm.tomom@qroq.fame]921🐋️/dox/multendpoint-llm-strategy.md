# Multi-Endpoint LLM Strategy

## Overview
The TEARIT house uses multiple LLM endpoints across the LAN, each serving specific roles:

## Endpoints

### 1. Local Ollama — `127.0.0.1:11434`
- **Models**: `llama2:latest`, `llama3:latest`
- **Use**: tomom hand-tuning (`tune.py`), interactive dev
- **Latency**: <1s for 8B models, CPU-only
- **Location**: This machine (the dev environment)

### 2. Mac LAN Node — `10.0.0.144:11434`
- **Models**: `gemma3:1b`, `gemma3:270m`, `codeqwen:7b-code`, `qwen2.5-coder` family, `llama3-groq-tool-use`, `nomic-embed-text`, etc.
- **Use**: `iqa-fork/ask.sh`, `verify_cell.c` (review), `check_response.c`
- **Latency**: Variable (cross-machine)
- **Location**: Mac master node (shared)

### 3. Linux LAN Server — `10.0.0.187:11434`
- **Models**: `gemma3:270m`
- **Use**: `fill_cell.c` (generation — fast 270M model on dedicated hardware)
- **Latency**: Fast (dedicated server)
- **Location**: Linux server with gemma3:270m

### 4. Local OpenCode Serve — `127.0.0.1:4123`
- **Models**: opencode (embedded)
- **Use**: `opencode_ask.c`
- **Location**: Local opencode server

## Config Pattern

All C ops that call ollama read from the shared config:
```
#.desktop/ai_backend.pdl
  gemma_lan_url=http://10.0.0.144:11434
  gemma_lan_model=gemma3:270m
```

Each op reads this at startup, falling back to a hardcoded default if missing.
This ensures a single source of truth — edit the IP once in the .pdl, not in N C files.

## Model Selection Strategy

| Task | Endpoint | Model | Rationale |
|------|----------|-------|-----------|
| tomom vocab tuning | local | llama3:latest | Good reasoning, <1s latency |
| Content generation | LAN server | gemma3:270m | Fast, low-resource |
| Content review | Mac LAN | gemma3:1b | Better quality than 270m |
| Code tasks | LAN Mac | qwen2.5-coder:7b | Coding-specialized |
| Embeddings | LAN Mac | nomic-embed-text | Local embedding model |
| Interactive chat | local | llama2:latest | Fallback if llama3 busy |

## Updating tune.py to Use LAN Backend

Currently `tune.py` uses `http://127.0.0.1:11434` (hardcoded). To update:

```python
# Add at top of tune.py
import urllib.request
def resolve_api_url():
    pdl_path = Path(__file__).parent.parent.parent / "#.desktop" / "ai_backend.pdl"
    if pdl_path.exists():
        for line in open(pdl_path):
            if line.startswith("gemma_lan_url="):
                return line.split("=",1)[1].strip() + "/api/generate"
    return "http://127.0.0.1:11434/api/generate"
```

This follows the house pattern: shared config in `.pdl`, fallback to local.