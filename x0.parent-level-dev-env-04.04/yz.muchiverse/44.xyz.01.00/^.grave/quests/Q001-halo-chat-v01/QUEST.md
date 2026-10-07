# Q001 — HALO_CHAT v0.1 (HORN plus Concept Bank validation)

| field | value |
|---|---|
| status | open — blocked on 3 owner decisions (below) |
| tier | outside-agent (Kilo's handoff) or worker ghost with HORN |
| size | L |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | (1) HALO's validated edits: review file or direct to the bank? (2) grading: self-judge or separate judge? (3) persistent chat history across sessions? |

## Mission (one sentence)

Add HALO_CHAT: the HORN chat harness with a Concept Bank validation step in front of anything it writes to the bank.

## Why it matters

HORN (done) gives OpenRouter chat with tool calling. HALO is Sprint 2 of the attrition pipeline: the chat → DESCRIBE → validate → bank chain
that lets models propose edits the house can trust. It is the first piece the ghosts will lean on for "feeding" and "cleaning".

## Read first

1. `^.hai-horn/README.md` and `^.hai-horn/HORN_CHAT-HANDOFF.md` (the mission, what is real vs planned, the decisions already answered)
2. `^.hai-horn/dox/00-QUICK-START.md`, `01-CODE-REFERENCES.md`, `02-DECISIONS-AND-TESTING.md`, `03-HORN-CHAT-BUILD.md`
3. `&.widgits/concept-bank/ops/concept_edit_validate.c` (the validator HALO invokes to gate bank writes)
4. `&.widgits/entity-cli/ops/ai_chat_openrouter.c` and `&.widgits/entity-cli/ops/ai_describe.c` (HALO's DESCRIBE step wraps it as `ops/halo_chat_describe.c`)
5. `#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/PRISC-OPS-ARCHITECTURE.md` (pal + C ops pattern; no shell in ops)
6. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/robot-chat/ROBOT-CHAT-BLUEPRINT.md` §3.1 (personality = instance-scoped Concept Bank)

## Do

1. Get the owner's answers to the three decisions from the manager before writing code.
2. Build HALO_CHAT v0.1 beside HORN in `^.hai-horn/` as the handoff describes (new ops, a layout, a script; HORN files are not rewritten).
3. Every bank write goes through `concept_edit_validate`; a rejected edit is shown to the user with the reason and never written.
4. Add e2e coverage next to HORN's (`^.hai-horn/scripts/e2e.sh`), driven through the real UI by the relay and real nav numbers.

## Acceptance

- [ ] A fresh build from `^.hai-horn/scripts/build.sh` succeeds.
- [ ] A scripted HALO session proposes one valid edit (accepted, written) and one invalid edit (rejected with a reason, nothing written).
- [ ] The e2e passes from a clean state, and the output is pasted in `## Result`.
- [ ] No key, token or raw provider response is committed.

## Rules

Everything in `^.grave/README.md`. Provider keys come from ignored `state/*_api_key.txt` files.

## Log

## Result
