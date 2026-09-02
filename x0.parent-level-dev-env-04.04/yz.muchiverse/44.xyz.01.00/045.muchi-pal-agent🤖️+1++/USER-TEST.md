# MUCHI-PAL-AGENT — HOW TO RUN IT

## Start it
```bash
cd 1.muchi-pal-agent🤖️
bash button.sh run
```

You get a chat interface. Type messages, press Enter. The AI responds with random words (default model).

## What to look for

**Basic chat works:**
- Type a message → it shows as "You: [message]"
- AI responds with random words below it
- Keep typing, builds a conversation history above

**Model switching:**
- Type `/model gemma-lan` and press Enter
- Header at top changes from `model: random-words` to `model: gemma-lan`
- Try it again with `/model gemini-flash` (if you have a Gemini API key)

**It remembers:**
- If you quit (Ctrl+C) and run it again, your chat history is still there
- Same conversation continues
- (Chat is stored in `pieces/world_01/session_01/chat/context_log.txt`)

## What might look weird

**The response takes a few seconds** — The app is polling for the LLM's answer in the background. Wait for the "Response received" message at the bottom before the next prompt.

**Random-words model is just random** — It picks words from a wordbank file, not actually an LLM. That's intentional (fast, local, no API). Switch to `gemma-lan` if you want something smarter.

**Scrollback** — If the chat gets long, scroll up in your terminal to see older messages.

## Key commands

- `/model <id>` — Switch to a different model (see list below)
- Ctrl+C — Quit and save chat

## Available models

- `random-words` — picks random words from a list (LOCAL, FAST)
- `gemma-lan` — small Gemma3 on LAN Mac (LOCAL, FAST, ~1 sec)
- `groq-tool-use-mac` — bigger Llama3 on LAN Mac (LOCAL, SLOWER, ~3 sec, can use tools)
- `gemini-flash` — Google's Gemini (needs API key, SLOW, ~2-5 sec)

## That's it

Just chat. Type. Press Enter. See the AI respond. Switch models with `/model`. Quit with Ctrl+C.

---

**For testing under the hood:** See the ops/compose_frame.c code if you want to know how frames are rendered, or context_log.txt if you want to see the raw conversation data. But for just using it — type and chat.
