# Using an API model through co-lab-hai (the approval gate)

Written 2026-10-09 (claude). Why: the owner wants to SEE and APPROVE every API call and reply. co-lab-hai already has a human approval queue; `ops/colab_api_gate.sh` puts the API behind it.

## What it is
`ops/colab_api_gate.sh "<prompt>"` is a drop-in **backend** with the same contract as `^.hai-horn/ops/+x/horn_chat_backend.+x` (prompt = argv[1], reply on stdout, provider lines on stderr, exit code). So anything that takes a backend (`ghost_run --backend`) can use it unchanged.

Per call:
1. Posts the prompt into the room as `claude -> @groq-worker  [gate-<id>] ...` (pending: the owner sees it in the approval area).
2. Waits until that line appears in the room's `conversation.txt` (= the owner clicked Approve). Rejected or no approval within `COLAB_GATE_TIMEOUT` seconds = **no API call**, exit 3.
3. Calls the real backend (Groq first, then Poolside, then OpenRouter; free models only).
4. Posts the reply as `groq-worker -> @claude  [gate-<id> reply] ...` (also pending, so the owner can read it) and prints it on stdout.
Full text of every prompt/reply: `#.desktop/colab_hai/gate/<id>.prompt.txt` / `.reply.txt` (the room line is cut to ~1800 chars). Keys are never printed or logged.

## How to use it
1. Open the room: `bash '&.hq-apps/co-lab-hai/button.sh' <house_root>`.
2. Optional fresh session: `bash '&.hq-apps/co-lab-hai/ops/colab_hai_action.sh' newsession '&.hq-apps/co-lab-hai' <house_root>`.
3. One call by hand: `bash '&.hq-apps/co-lab-hai/ops/colab_api_gate.sh' "your prompt"`, then approve in the window.
4. Through the delegation runner (a quest, see `&.widgits/quest-pilot/ops/GHOST_RUN.md`):
   `ghost_run <quest_dir> claude <house_root> --ghost <id> --backend <house>/'&.hq-apps/co-lab-hai/ops/colab_api_gate.sh'`
5. Reading the room as an agent: `#.desktop/colab_hai/sessions/<current_session>/feed_claude.txt`.

## Environment knobs
| var | meaning |
|---|---|
| `COLAB_GATE_TIMEOUT` | seconds to wait for approval (default 900) |
| `COLAB_GATE_TEST=1` | the gate approves its OWN prompt. **Owner allowed this for test pings only (2026-10-09); never for real quest prompts.** Only fires when our line is the oldest pending line. |
| `COLAB_GATE_AUTO=1` | post but do not wait (owner away); the room still shows everything |
| `COLAB_GATE_AGENT` | the name shown for the API side (default `groq-worker`) |
| `COLAB_GATE_BACKEND` | the real backend (default `^.hai-horn/ops/+x/horn_chat_backend.+x`; tests use a fake) |
| `COLAB_HOUSE` | house root (default: the house this script lives in) |

## Gotchas (each one was hit)
- The backend must run with **cwd and `PRISC_PROJECT_ROOT` = the house root** and `HORN_TOOLS=off` (the gate does this). It writes its request payload to `<house>/pieces/horn/`; if that folder is missing every provider fails with `payload-write-failed` (the gate creates it; `pieces/` is runtime output, do not commit it).
- Keys live in `&.widgits/open-hai/state/` (`raw_groq.txt`, `raw_poolside.txt` ...) or env (`GROQ_API_KEY`, `HORN_POOLSIDE_KEY`). Missing key: ask the owner, never invent one.
- `ghost_run`'s `max_seconds` budget **includes the wait for approval**: give quests `max_seconds` of 1800+ so a slow click does not kill the attempt.
- The approval area approves the OLDEST pending line first, so with several gated calls waiting they are approved in order. The reply post is a second approval.
- Tool calls: the gate sends prompts with tools off (same as `ghost_run`). API tool-calling through the room is a separate, not yet built step.
- Messages in the room strip `|` and newlines; use the `.prompt.txt` / `.reply.txt` files for exact text.
- Free quota: Groq measured 1000 requests/day and 8000 tokens/min per model (quest Q014). Spend is counted per attempt in the quest ledger.

## Proof
Live 2026-10-09: ping through the gate answered by `groq (openai/gpt-oss-120b)`; a scratch-room run with a fake backend covers post, wait, approve, call, reply.
