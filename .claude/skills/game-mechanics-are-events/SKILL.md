---
name: game-mechanics-are-events
description: Use BEFORE adding or changing any game mechanic, rule, enemy, pickup, power-up, win condition or "what happens when" in any game. EVERY rule is an op (owner: no native rules, no exceptions), called by event pages / FSM / tool calls, numbers in RULE rows, never hard-coded in game_core.c.
---

# Game mechanics are event pages (written 2026-11-18 after decision log D92)

**The mistake this skill exists to stop:** Teario's enemies, stomp, fireball, power-up and patrol were written as C constants inside `game_core.c` MODE platform. The Teario plan (`16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md` section 2) says every mechanic is an event page; Teario has no `events/` folder and blueprintz shows nothing. The owner found it, not a test.

**Before you write a line:**
1. Read plan section 2 (and the section for what you touch), `02-architecture/EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`, `02-architecture/PAL-VS-EVENTS.md`, decision log D62, D63, D90 and **D92**.
2. Answer the five design-choice questions in `DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (one authority per fact, modular, scalable, long term, house shapes) and add the log row BEFORE building. "I will do the quick C version" needs a removal condition in writing.
3. Where each thing lives: a NUMBER = a RULE row (joins the rules hash, replay stays exact); a RULE = an event page under `@.apps/<app>/events/<name>/event_pkg/pages/page_N/` (trigger, condition, nodes; fast loop nodes are the planned `tp_*` family, compiled to a rule table, a PROJECTION declared in `state.pdl`); BEHAVIOUR = one reusable C op that the pages call (an interpreter, not a per-game branch); a PICK / window choice = a PREFERENCE file; game state = the ledger only (no ghost data).
4. A harness case that can fail sits beside it (sabotage-prove it), and the result must be visible in the real window (open the PNG). Blueprintz must be able to show the page.

**State of play (2026-11-18):** the `tp_*` nodes and the tick / touch / tilehit / input triggers are NOT built, so today a fast mechanic has no home but C: that is exactly the gap to design first (D92 fix order), not a licence to add more C. If you must add one meanwhile, put every number in a RULE row, say so in the log, and tell the owner.

## OWNER RULING 2026-11-18 (decision log D92): no native rules, all ops, forever, no exceptions
Every rule of every game is an op: a small reusable program a harness, an FSM node, an event page or an AI tool call can invoke by name (data in, rows out), so reuse can be SCORED. "Simple games may keep a native mode" (D63) is withdrawn: snake, chutes, walk and platform are all to be unpacked. `game_core` may only be the loop that calls ops and writes the ledger. Speed is not a reason to go native: build the ops, measure, and only after the behaviour is proven and approved consider the wraith-alpha in-memory DB. If you catch yourself writing a rule inside an engine file, stop: it is a new op with its own harness case.
