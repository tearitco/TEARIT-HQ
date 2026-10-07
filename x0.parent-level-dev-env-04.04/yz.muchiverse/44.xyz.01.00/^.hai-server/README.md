# ^.hai-server — the entity that manages all entities through their phones (🖥️)

Routes messages between phones, is the single writer of every inbox, keeps the global ledger, wakes/sleeps robots and ghosts,
assigns quests and escalates `ask-human` messages to the human's phone. Design:
`#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §4 (read first). Working name.

- Config: `_TEMPLATE/server.pdl` (permissions, rate limits, spawn limits: max depth, max children, max total, spend caps).
- Ledger: `ledger.txt` (append-only, every routed message; the audit trail and later training data). Created when the server first runs.
- Window: Entities | Phones | Traffic | Escalations | Ledger (layout-driven, nav-numbered). Many servers allowed (one per grave/project).

Nothing here runs yet (design phase 0).
