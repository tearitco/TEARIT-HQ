# 17 — AI

The front door for AI across the house: the desk, the game, dev, and
productivity. The game it can drive is chapter `16.game`. This
chapter does not own actors, playtest, or event commands. It owns
who is allowed to append one, and the other house jobs that are not
a game at all.

- `AI.md` — start here.
- `../08-roadmap/design-docs/LLMUD-HACK.md` — the research-side
  design. Describe, score, store. Not a license to classify in one
  token.
- `../08-roadmap/design-docs/DUSTOPIA-HACK.md` — the game-facing half
  of the same loop. Read with `16.game`.
- `../13.agent-coms/KILO/claude-2-kilo-9.17.md` — the only new C
  currently named for AI is the event commands `ai_describe`,
  `ai_fsm_transition`, and `ai_goap_plan`. None are in the registry.
- `../13.agent-coms/README.md` — Co-lab, the approval room. It is not
  the local model.
- `AUTO-AND-AUTO-STUDIO-DESIGN.md` — design (not built): per-user `auto` and `auto-studio`, relay-injection conductor, tiered brains, trained subs, safety rails, slices A0-A8. `AUTO-TOYS-AND-AGENT-LEVERAGE.md` — how it drives the 2026-10 toys and multiplies agents. Hand-off folder: `12.calendar/2026-10-14/`.
- `HARNESS-AS-GAME-BRAIN.md` — the whole picture of harness as judge and memory for in-game AI: game verb, ledger, phase table, harness verdict, RESULT ledger, behavior-bank weights, concept bank / tomom / pet decisions, with an ASCII diagram, what is built vs designed, the AI hook, and the planned decision ledger (2026-10-17).
