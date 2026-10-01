# Quick start for HORN_CHAT handoff

**For kilo:** Read this first (5 min), then open `../HORN_CHAT-HANDOFF.md` (15 min).

---

## What am I building?

A terminal chat program (`HORN_CHAT`) that talks to OpenRouter API models, using the same rendering engine as an existing tool called `gem-dev`.

## The three pieces

1. **Renderer** — displays a chat window in the terminal (code exists, copy it: `chtpm_parser.c` from gem-dev)
2. **OpenRouter round-trip** — sends a prompt to an OpenRouter model, gets a reply (code exists, copy it: `ai_chat_openrouter.c`)
3. **Main loop** — reads keyboard input, sends to OpenRouter, displays reply (you write this, small `.pal` file ~50 lines)

## Code that already exists and proves this works

- `gem-dev` — a terminal harness using the same renderer you'll use. This is existence proof that the terminal rendering works.
- `ai_chat_openrouter.c` — OpenRouter integration. Already tested, proven round-trip.
- `robot-chat` — a different UI (graphical), same OpenRouter backend. Proves the backend is solid.

## What you're NOT doing

- Building a new rendering engine (use existing chtpm parser)
- Designing a new OpenRouter integration (copy existing ai_chat_openrouter.c)
- Inventing a new architecture (follow gem-dev's exact pattern)

## Timeline

- **Sprint 1 (HORN_CHAT):** Basic chat, ~3-4 days
- **Sprint 2 (HALO_CHAT):** Add Concept Bank validation, ~2-3 days
- **Sprint 3 (IRL):** Design only, no code yet (flagged for owner decision)

## One critical question before you start

Should HALO_CHAT's validated edits go to a **review file** (human reads before promotion) or **direct to the bank** (faster feedback, higher error risk)?

This decision isn't made yet. Raise it with the owner before Sprint 2.

---

**Next:** Open `../HORN_CHAT-HANDOFF.md` for the full context.
