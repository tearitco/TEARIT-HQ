# Q002 — open-hai provider config (no more hardcoded Ollama host)

| field | value |
|---|---|
| status | open |
| tier | worker (HORN) or outside-agent |
| size | M |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | which providers open-hai may offer in its UI first |

## Mission (one sentence)

Make open-hai pick its backend from configuration (reusing HORN's provider table) instead of the Ollama host and model hardcoded in its C source.

## Why it matters

Today open-hai only talks to one LAN Ollama (`10.0.0.144:11434`, model `stable-code:latest`, in `g_model_name`). Student ghosts (local models)
and worker ghosts (HORN providers) both need an app that can switch backends; this is the smallest real step toward desktop robots
and open-hai reaching the HORN provider layer.

## Read first

1. `&.widgits/open-hai/README.md` and `&.widgits/open-hai/ONBOARDING.md` (how the window and backend work; build with `ops/build_open_hai.sh`)
2. `&.widgits/open-hai/ops/` (find where the host and `g_model_name` are set)
3. `^.hai-horn/ops/horn_chat_backend.c` (the multi-provider backend and its provider table, with where each key comes from)
4. `#.desktop/ai_backend.pdl` (the house-wide Gemma LAN url/model config the blueprint mentions)
5. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/robot-chat/ROBOT-CHAT-BLUEPRINT.md` §2.1 and §5

## Do

1. Move the backend choice (host, model, provider) into a small config file read at start; keep today's Ollama as the default so nothing changes for the owner.
2. Reuse HORN's provider table instead of a second copy (text-include or call `horn_chat_backend`; follow the house sharing rule in `AGENTS.md`/memory: inline for one consumer, text-include for pure code shared by 2+, never header+link).
3. Keys only from ignored `state/*_api_key.txt` files.

## Acceptance

- [ ] Default behavior unchanged: open-hai answers from the LAN Ollama as before (show a real transcript).
- [ ] Changing the config file to a HORN provider makes the next message go there (show it with a stub or a real key, no key committed).
- [ ] Build clean with `ops/build_open_hai.sh`; the window still opens and scrolls.

## Rules

Everything in `^.grave/README.md`. Do not touch HORN's files except to share the table the approved way.

## Log

## Result
