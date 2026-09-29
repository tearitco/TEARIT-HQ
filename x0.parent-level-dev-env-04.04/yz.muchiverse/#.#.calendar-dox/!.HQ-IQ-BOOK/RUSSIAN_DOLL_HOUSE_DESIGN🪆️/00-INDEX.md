# RUSSIAN_DOLL_HOUSE_DESIGN 🪆️

**What this is:** a single, shared, auditable place for a specific
recurring conversation thread — the user's wholistic ai-house
vision (boss/high-agents/OpenRouter-workers/student-agents/customers),
the concerns it raises about the current dev roadmap, and successive
agents' answers/edits to those concerns. It exists so the user can
hand ONE folder to any outside agent (Grok, a browser-LLM, a future
agent) and have it contain the full back-and-forth in one place,
rather than scattered across chat history.

**This folder holds pointers, not the primary content.** The actual
brainstorming and answers live in
`/home/no/Desktop/github/work/XO/$.Brainstorm/sept-29-my-concerns/`
(outside this repo, not under git) — that's where the user drafts
concerns and where agents write full answers. This index exists so
the house's own documentation tree (which agents already know to read
first, per `08-roadmap/00-INDEX.md`) has a real pointer into that
external thread, instead of the thread being invisible from inside
the house.

**Naming note (read this before reusing "Russian doll" elsewhere):**
"Russian doll" already means something else in this house — a
filesystem-nesting metaphor for in-game data (drag-drop-as-`mv`,
file-as-container hierarchy), documented in
`08-roadmap/design-docs/EVENT-MODULARITY-AND-BUILD-SPEED.md` §"three-tier
Russian doll hierarchy" and referenced in
`10-user-docs/2026-09-18/WHY-DSR-WSR-PAUSED.md`. This folder's name
was chosen by the user for this specific brainstorming thread and is
scoped to that — it is NOT a general "nested documentation index"
convention. Don't extend "Russian doll" to mean "doc index" elsewhere;
that collides with the existing, different, real meaning. If a
general recursive-doc-index convention is wanted later, give it its
own name.

## Threads

### 2026-09-29 — AI-house architecture concerns

- **User's concerns:** `XO/$.Brainstorm/sept-29-my-concerns/0.my-concerns.md`
- **Claude's answer (first pass):** `XO/$.Brainstorm/sept-29-my-concerns/AGENT_ROADMAP_ANSWERS.md`
  — covers: the TEARIT pipeline name, real vs aspirational learning
  status, civ-test/asa survival mechanics (none built), the
  "phymoji"/clacker-sweep naming mixup, the open-hai session-pal idea
  and BOOK:SYSTEM proposal, DSR-test/WSR_PAL-PREFERED synergy,
  the terumon-babysitter idea (new, undesigned), piececraft-hq's
  Minecraft-clone ambition (already designed as PALCRAFT), OpenRouter
  delegation's real scope/ceiling, and the boss/manager/worker/student
  agent hierarchy (pieces exist, no tier distinction yet).
- **Next in this thread:** Grok's edit pass on the Claude answer doc,
  then a HARNECIENT.SMOL "night class" script covering this material,
  then an outside agent's own weigh-in document.

Update this index with one line per new document as the thread grows,
newest-relevant-context-first is not required — just don't let a doc
get added to that external folder without a pointer landing here.
