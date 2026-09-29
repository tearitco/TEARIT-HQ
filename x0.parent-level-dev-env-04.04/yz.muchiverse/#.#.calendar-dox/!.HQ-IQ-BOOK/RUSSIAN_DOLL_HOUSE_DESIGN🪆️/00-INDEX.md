# RUSSIAN_DOLL_HOUSE_DESIGN 🪆️

**What this is:** the real, self-contained, in-house home for a
specific recurring conversation thread — the user's wholistic
ai-house vision (boss/high-agents/OpenRouter-workers/student-agents/
customers), the concerns it raises about the current dev roadmap, and
successive agents' answers/edits to those concerns. Everything an
outside agent (Grok, a browser-LLM, a future agent) needs to pick up
this thread cold lives in this one folder — no jumping outside the
repo required.

**This folder holds real content, not just pointers.** The user's
personal brainstorm folder (`XO/$.Brainstorm/`, outside this repo, not
under git) is where drafts start and stay — it's the user's own
scratchpad and should be left alone by agents. Whenever a document from
that thread reaches a state worth keeping, a real copy lands here.
Don't treat `XO/` as the source of truth for this thread going
forward; this folder is.

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

## Contents (2026-09-29 — AI-house architecture concerns)

- **`0.my-concerns.md`** — the user's original concerns doc: the
  attrition-model naming question, civ-test/asa survival mechanics,
  the phymoji/embodiment question, DSR-TOY/WSR_PAL-PREFERED synergy,
  teru-test's meta-agent babysitter idea, piececraft-hq's Minecraft-clone
  ambition, in-house chat/OpenRouter delegation scope, and the
  boss/manager/worker/student agent hierarchy vision.
- **`AGENT_ROADMAP_ANSWERS.md`** — Claude's answer (§0–8), grounded in
  four parallel read-only research passes over the real codebase, plus
  Grok's edit-pass addendum (§9, corrected one claim, proposed "the
  attrition-model" name) and the owner's direct confirmation of that
  name (§9a). Includes a summary status table (REAL / DESIGNED /
  ASPIRATIONAL) across every topic raised.
- **`ATTRITION_DIAGRAM.md`** — emoji-heavy visual roadmap (same format
  as `08-roadmap/design-docs/PIPELINE-EMOJI-DIAGRAM-REVISED.md`) laying
  out the whole attrition-model tree: A TEARIT, Concept Bank, tomom,
  FSM/GOAP, OpenRouter workers, the agent hierarchy, and all four game
  testbeds, each tagged with real status and sourced.
- **`ATTRITION-INTERACTIVE-DIAGRAM.html`** — the same roadmap as a
  clickable, dark-theme interactive page (open directly in a browser) —
  7 boxes with detail panels, plus FAQ-style toggles on the promotion
  bottleneck, OpenRouter's delegation ceiling, and the DSR/WSR_PAL-PREFERED
  relationship.

## Status of open items from this thread

- **Settled:** "the attrition-model" is the confirmed umbrella term
  (§9a of `AGENT_ROADMAP_ANSWERS.md`). `ROBOT-CHAT-BLUEPRINT.md` +
  `groks-thots.txt` moved into house docs 2026-09-29, from the user's
  personal folder to `08-roadmap/design-docs/robot-chat/` — same
  directory tier as `TERUMON-SPEC.md`/`PALCRAFT-DESIGN.md`.
- **Still open / not yet done:** a HARNECIENT.SMOL "night class"
  script covering this whole thread; an outside agent's own weigh-in
  document (not yet requested/received).
- **Genuinely new ideas raised in this thread, not yet built or
  formally ratified anywhere else in the house:** the "real entity" vs
  "magic entity/event" distinction; the `BOOK:SYSTEM` proposal
  (a house-wide, non-user-owned book, for open-hai session pals and
  eventually Claude/Grok's own persistent self-storage); the
  `AGENT-HIERARCHY-AND-PERMISSIONS.md` idea (a real tier distinction
  between manager and student agents in colab-hai/branch rules).

Update this index (contents list + status section) every time a new
document in this thread reaches a keep-worthy state — copy it in here
for real, don't just point at wherever it was drafted.
