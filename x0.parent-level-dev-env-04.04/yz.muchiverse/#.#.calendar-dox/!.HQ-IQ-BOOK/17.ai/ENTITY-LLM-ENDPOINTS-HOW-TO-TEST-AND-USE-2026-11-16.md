# Entity LLM endpoints: where they are, how to test one safely, how a feature uses one (2026-11-16)

Written by Claude Sonnet 5.5 after the owner said: "some entities actually have LLM API endpoints, maybe u can test one ... docs on it going forward". Label key: FOUND = read in the repo or the machine, RAN = I ran it and saw the result, DOC = my reading or proposal. Companion docs (read these too, this one does not repeat them): `AI-BACKENDS-DISAMBIGUATION.md` (who is who: gemma3 LAN, the API chat backends, tomom), `AI-PUSH-ROADMAP-AND-NUANCES.md`.

## 1. What exists (FOUND)
| Piece | Where | Notes |
|---|---|---|
| The shared LAN endpoint | `#.desktop/ai_backend.pdl`: `gemma_lan_url=http://10.0.0.144:11434` (the owner's Mac running Ollama), `gemma_lan_model=gemma3:270m` | one place; every op reads it at start and falls back to the same defaults if the file is missing. Edit it there, never in a C file. |
| The chat backends (entity chat) | `&.widgits/entity-cli/ops/`: `ai_chat.c` (LAN Gemma, the "Chat-gemma" button), `ai_chat_openrouter.c` ("Chat-api"), `ai_chat_groq.c` (Groq, key file `raw_groq.txt`) | same contract for all three: `ai_chat*.+x <entity_dir> <house_root> <message text>` |
| What a chat call writes | the entity's own `<entity_dir>/chat_history.txt`, two lines per turn: `[<date time>] USER: <text>` then `[<date time>] <entity label>: <reply>`; the entity label comes from `<entity_dir>/label.txt` | the history is also the prompt context of the next turn |
| Entities that hold a chat history today | `robot_chat_001`, `eden_robot`, `page_ai_chat_demo` (under the owner's `pals/`) | read by name only; their contents were not opened |
| Other AI callers of the same endpoint | `ai_describe.c` (constrained DESCRIBE), `my-lawyer` and `my-biotech` workers | see the disambiguation doc |
| Key files for the quota-limited APIs | **none found in any entity folder on 2026-11-16** (`raw_*.txt`, `*api_key*`) | so the Groq / OpenRouter backends are NOT configured here; they were NOT called |
| The models on the Mac (RAN `GET /api/tags` 2026-11-16) | `gemma3:270m`, `gemma3:1b`, `llama3:latest`, `llama3-groq-tool-use` (8b and latest), `qwen2.5-coder` (7b, 3b, 1.5b, 0.5b), `codeqwen:7b-code`, `llama2`, `llama2-uncensored`, `stable-code`, `nomic-embed-text` | a list that changes: ask the endpoint, do not trust this table |

## 2. Rules before you touch one (house standing rules, from the owner)
1. **Never burn free API quota** (a probe once ate a day's 50 free OpenRouter calls): the Groq and OpenRouter backends are never called in a test, a loop or a probe; read headers from ONE call if you must, never loop to a 429. (memory: feedback-never-burn-free-api-quota)
2. **All Ollama calls go to the Mac** (`ai_backend.pdl`), never to a local Ollama on the dev machine. (memory: feedback-ollama-via-mac)
3. **Free models only**, no paid credits.
4. **Never print or log a key** and never put one in a doc, a commit or a harness; a key lives in a file inside the entity or the user folder and is read by the op.
5. **The prompt leaves the machine** (to the LAN Mac, or to an API provider): put nothing private in it (no user name, email, wallet, real entity data). Test with a generic sentence.
6. A harness case **must not touch the network** (harness rule): an LLM call is checked by a LIVE recipe like the one below, or against a stub, never inside a normal case.

## 3. A safe live test (RAN 2026-11-16, 0.2 s, output below)
A scratch folder, a scratch entity, the three binaries, a copy of the endpoint config. No real entity, key or quota is involved.
```
S=/tmp/llm_scratch; O='&.widgits/entity-cli/ops'          # run from the house root (44.xyz.01.00)
mkdir -p "$S/h/#.desktop" "$S/h/$O" "$S/h/ent_test"
cp '#.desktop/ai_backend.pdl' "$S/h/#.desktop/"
for b in ai_chat connect_op json_parser; do cp "$O/$b.+x" "$S/h/$O/$b.+x"; done     # ALL THREE, directly in ops/ (not ops/+x/)
printf 'test_bot\n' > "$S/h/ent_test/label.txt"
timeout 120 "$S/h/$O/ai_chat.+x" "$S/h/ent_test" "$S/h" "Say hello in five words."
tail -2 "$S/h/ent_test/chat_history.txt"
```
Seen: `ent_test: Hello.` and in `chat_history.txt`: `[2026-11-16 21:36:18] USER: Say hello in five words.` / `[2026-11-16 21:36:18] ent_test: Hello.` A 270M-parameter model does not follow "five words": a reply that is short and on topic is the success criterion for the plumbing, not the wording. To judge quality use `gemma3:1b` or `llama3` by changing `gemma_lan_model` in the SCRATCH copy of the config only.
**The traps I hit (each cost a run):**
- the op starts its helpers by a path: `.../ops/connect_op.+x` and `.../ops/json_parser.+x` must sit **directly in `ops/`** of the house you pass as `<house_root>`; copying them into `ops/+x/` gives `sh: 1: .../connect_op.+x: not found` then `ai_chat: gemma call failed`.
- the failure message is the same for a missing helper and for an unreachable endpoint: check the endpoint first (`curl -s --max-time 4 http://10.0.0.144:11434/api/tags | head -c 200`), then the helpers.
- the call is a plain LAN HTTP request with no key; if the Mac is asleep it fails fast: that is not a bug in the op.

## 4. How a feature should USE an entity's model (DOC; the Texting auto-reply is the first user)
- **Call the existing op, do not write a new client:** `ai_chat.+x <entity_dir> <house_root> "<incoming text>"`, then read the LAST line of `<entity_dir>/chat_history.txt` (`[date] <label>: <reply>`), strip the prefix, and send that as the message. The entity's accumulated `chat_history.txt` is its memory: one entity, one history, whoever the sender.
- **Put it behind an event command, off by default** (Texting design 3.5e: registry command `phone_reply_ai`): it only runs when the entity's page uses that command, it is rate-limited (`reply_max_per_pair_per_min`, a per-entity daily cap), the reply starts with `[auto]` so no two bots chain, and a failed call sends the fixed default text instead of nothing.
- **Which backend** is a property of the entity's page (the node names the op): LAN Gemma by default; the API backends only for an entity that has its own key file and only with a daily cap well under the provider's free quota.
- **Privacy:** the incoming message text goes to the model. Another user's text sent to your entity's model leaves your machine: say so in the entity's page and keep `phone_reply_ai` opt-in per entity.
- **Test it** with this recipe in a scratch house plus a harness case that stubs the model (a tiny local op that echoes) so the case stays offline; the live recipe above is the only network step.

## 5. Not done / not known
Groq and OpenRouter were not called (no keys present, quota rule). Whether `robot_chat_001`, `eden_robot` and `page_ai_chat_demo` use the LAN backend or an API one was not read (their folders hold chat data). Concurrency (two calls at once on a 270M model) and the Mac's idle cost were not measured. The `phone_reply_ai` command is a design, not built.
