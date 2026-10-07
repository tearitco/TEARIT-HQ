# ^.ghost — the babysitter (👻)

The ghosts the gravestone (`^.grave`) sends out. Design:
`#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md`. Working name; may be renamed.

A **ghost** is one appointed bot: a pal (entity) with a glyph, a **tier**, a **brain**, a current quest and an append-only history.

- Tiers: `worker` (HORN via OpenRouter and the other providers: `^.hai-horn`) and `student` (a local Ollama model, in training).
- Appoint a ghost: copy `roster/_TEMPLATE/` to `roster/<ghost-id>/` and fill in `ghost.pdl`.
- Every event for a ghost is appended to `roster/<ghost-id>/history.log` (never edited, never deleted). The gravestone's History
  tab is just this file read in order.
- The manager (Claude) appoints, retires, assigns quests and may schedule **training events** (deterministic FSM plans through
  `%.harnesses/harnecient-fsm/`): feeding = quests and context, cleaning = review/verify/revert, fighting = benchmark between ghosts.

Nothing here runs yet (design phase 0). First runnable piece: a worker ghost that takes one quest through `^.hai-horn`'s `horn_turn`
with HORN's approval gate on.
