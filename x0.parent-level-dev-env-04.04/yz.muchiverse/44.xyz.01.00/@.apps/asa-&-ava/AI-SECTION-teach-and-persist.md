# 🤖 asa-&-ava — AI section: teaching, persistence, sensing, oversight

**Status:** design only, nothing built yet. Companion to `asa&ava-design-plan.md` — read that first.
**Date:** 2026-08-04.
**Why a separate file:** the parent doc is the project pitch; this one is the deeper "how the smart bits actually work" doc, kept separate for human readability per direct instruction.

---

## 1. 🎓 "Bigger chat teaches smaller chat to farm" — knowledge distillation into the BT/FSM

Direct instruction: gemma-lan (the "bigger" model) should be able to teach 047.scm (the "smaller" one) how to farm — meaning: **turn gemma's own successful behavior into new, programmable/learnable events inside 047.scm's own behavior tree/FSM**, not just chat text. This is a real, already-half-built pattern in this house — here's where.

### 1.1 The real, already-working precedent: `045.muchi-pal-agent`'s tool-dispatch scaffold

`045.muchi-pal-agent🤖️+1++/!.GRAND-PLAN-TOOL-SCAFFOLD.txt` documents a real, **verified working** three-layer architecture for exactly this class of problem ("small model can't reliably plan/act, so don't make it"):

```
Layer 1 (Intent Detection)   — keyword → tool lookup + strategy selection
Layer 2 (Pre-execution)      — tool dispatcher runs deterministically, captures output
Layer 3 (LLM Wrapping)       — template-fills the result into conversation, LLM just narrates
```

Confirmed real (its own doc, Phase 1, marked DONE and verified 2026-07-21): typing "list dir" → `gemma_strategy.c` detects intent + picks a strategy (weighted, e.g. 90% pre-execute / 10% let-gemma-handle-it) → `strategy_execute_a.c` actually runs the tool and logs the result → the result gets fed back into the conversation. **This is the real shape a farming "tool" (PLANT, HARVEST, EAT) should take** — a deterministic op that runs for real, with the LLM only ever narrating/deciding *when*, never simulating the actual mechanics itself.

### 1.2 The real, already-designed distillation convention: 047.scm's own corpus-authoring rule

047.scm's own design doc (`047.scm🎓️+1/!.SCM-DESIGN.md`, locked decision S3) already specifies exactly this pattern for building its corpus: **"harvest + auto-author — pull [a bigger model's] teacher lines + [smaller model]-authored phrases."** Teaching 047.scm to farm is a real instance of this same, already-decided mechanism — not a new one:

1. **Bigger chat (gemma) acts first**, its real tool-dispatch calls (per §1.1) produce real, successful farming action sequences — a real transcript of "gemma decided to PLANT, then WAIT, then HARVEST," each step a real, deterministically-executed tool call, not invented text.
2. **Those real, successful sequences become new curriculum entries** in 047.scm's own corpus — the exact same "harvest teacher lines" pattern 047.scm's own design already committed to for conversation, just applied to action sequences instead of chat phrases.
3. **047.scm's own selector then picks from this newly-expanded action-corpus** the same way it already picks conversation phrases — same reward loop (gemma describes/judges, deterministic code scores — the same describe-don't-classify rule this whole house already enforces), just judging "was that a good farming decision" instead of "was that a good reply."

**This means a farming "event" a smaller chat can learn is structurally identical to a conversation phrase it can learn** — both are entries in a curriculum, both get selected (not generated), both get reward-tuned by the same real judge mechanism. The behavior tree/FSM's own "leaves" (§3 of the parent doc — PLAN/PLANT/GROW/HARVEST/EAT) are exactly the tool-dispatch layer's own real ops; what 047.scm learns is *which leaf to pick when*, using the same selector technology it already has for phrases.

### 1.3 Where this guidance should live

**Here, in asa-&-ava's own dir** — not duplicated into `045.muchi-pal-agent` or `047.scm🎓️+1`'s own docs. Both of those are real, general-purpose systems (tool-dispatch scaffold, curriculum-based text selection) that predate and don't know about asa-&-ava; this doc is the real, concrete **application** of both to one specific project. If either underlying system's own design changes, this doc should be updated to match — but the general mechanisms themselves stay documented at their own real home (`045.muchi-pal-agent🤖️+1++/!.GRAND-PLAN-TOOL-SCAFFOLD.txt`, `047.scm🎓️+1/!.SCM-DESIGN.md`), read and cited from here, not copied.

---

## 2. 💾 Multiple experiment sessions — do they autosave, or do we need file-menu?

Direct question: as we experiment with multiple asa-&-ava sessions, do they get autosaved, or do we need the file-menu (fm) widget?

**Real, confirmed answer from this house's own existing convention: sessions do NOT autosave, and yes, you need file-menu (or an equivalent explicit save step).**

Every widget's own `button.sh` this session (`tile-picker`, `board-viewer`, `file-menu` itself) uses the identical real pattern:

```bash
trap '... rm -rf "$SESSION_DIR"' EXIT INT TERM
```

A session directory (`pieces/sessions/<timestamp>-<pid>/`) is **deliberately ephemeral** — it exists only for that one run, and is deleted the moment the process exits, on purpose (this is *why* multiple simultaneous instances of the same widget don't collide with each other, see `TILE_PICKER_DESIGN.md` §2.1). Nothing written only inside a session dir survives past that one run.

**What this means for asa-&-ava specifically:** whatever real, persistent state asa and ava accumulate (inventory contents, hunger, relationship/affinity state, which crops are growing where) must live in the **project's own real root** (`@.apps/asa-&-ava/pieces/...`), not inside any one run's ephemeral session dir — the same real distinction mutaclysm's own `pieces/world_01/state.txt` (persistent, real save data) vs. a widget's own `pieces/sessions/<id>/` (throwaway) already draws. **file-menu is the real, existing house mechanism for this** — its own real `NEW`/`SAVE`/`SAVE AS`/`LOAD` commands (see `&.widgits/file-menu/`) are exactly the tool for managing multiple named, persistent asa-&-ava save slots while you experiment, the same way it already does for any other project. No new save mechanism needs inventing — reuse file-menu, same as everything else.

---

## 3. 👀 Farming sensing loop — "plant seeds, see it grow via line of sight, wait, then pick it"

Direct instruction: asa/ava plant seeds from their own inventory; the plant grows; **they see it via line of sight** and wait for it; then they go pick it.

**Honest gap check, done before writing this:** a direct search for any existing "line of sight" / field-of-view / sight-range mechanic anywhere in this house (`101.mutaclsym…`, `01.muchi-pals…`) turned up **nothing** — this is genuinely new territory, not an existing mechanic being reused. Said plainly so nobody assumes a real LOS system already exists somewhere and goes looking for it.

### 3.1 What this needs, concretely (not built yet)

A minimal, real v1 doesn't need a full FOV/raycasting system — the desktop-only scope (per parent doc §3.2) makes this much simpler than an in-game LOS system would be:

1. **"Seeing" a planted crop** = a real, deterministic distance/visibility check between asa/ava's own desktop grid cell (same `GRID_CELL_PX=80` grid every desktop entity already shares) and the crop's own cell — e.g. "within N grid cells, and nothing else occupies a cell directly between them" (a real but simple check, not true raycasting, given the desktop is a flat 2D grid of independent OS windows, not a occlusion-aware scene).
2. **"Waiting for it"** = a real behavior-tree leaf (§1.2) that polls the crop entity's own real growth-stage field (itself read from the real game-clock file convention already documented in `aomorai-editor-blueprint.md` §2.5) until it reports "ready," rather than a hardcoded timer.
3. **"Going to pick it"** = real movement on the same shared desktop grid every entity already uses, terminating in the existing, real HARVEST tool-op (§1.1).

### 3.2 Where this belongs

This is real, asa-&-ava-specific game logic — it does not belong in tile-picker, egg_window, or hikikomorai's own general infrastructure (those stay generic "a thing lives on the desktop" mechanisms). It's a new, small op (or a few) living under this project's own future `pal/`/`ops/` dirs, consuming the shared grid/position conventions those generic systems already expose, same "reuse the primitive, don't reuse the whole system" relationship the rest of this doc already establishes for tool-dispatch and 047.scm.

---

## 4. 🎛️ Later: a Dwarf-Fortress-style "controller-widget"

Direct instruction: later, build a controller-widget — like Dwarf Fortress's own UI — that sees all of asa-&-ava's entities and lets a human assign tasks to them.

This is real, distinct future scope, related to but **not the same as** hikikomorai's own task-bar:

| | hikikomorai's task-bar (already designed, `hikikomorai-design.md` §2.2) | The DF-style controller-widget (new, this section) |
|---|---|---|
| Shows | All living desk-session procs/entities, generically | Specifically asa-&-ava's own entities (a filtered/specialized view) |
| Lets you | Switch focus between entities, reassign which session lives under what | **Assign tasks** — e.g. "go plant here," "prioritize harvesting over eating" — a real command/priority-queue layer on top of viewing |
| Scope | House-wide, generic | Project-specific, asa-&-ava's own (though other future projects could reuse the same pattern later) |

**Real implication:** the controller-widget should be built **as a consumer of hikikomorai's own task-bar infrastructure** (once that exists — still "not started" per hikikomorai's own build order) rather than a wholly separate window-discovery mechanism — it needs the same "find and show live desktop entities" foundation hikikomorai's task-bar already designs, plus a new, asa-&-ava-specific task-assignment layer on top (which would work by writing real commands into each entity's own behavior-tree/FSM input, the same tool-dispatch mechanism §1.1 already establishes — a human assigning a task and gemma deciding to act are the same real underlying mechanism, just two different sources of the same command).

**Build-order implication:** this genuinely comes *after* hikikomorai's task-bar (§5 of the parent doc already lists that as a prerequisite for even the simple "open the whole group" button) — the controller-widget is a second, more specialized consumer of the same infrastructure, not a reason to build that infrastructure differently.

---

## 5. 🐕🐈🐔 Real pets — dog, cat, chicken (built 2026-08-04, later same day)

Direct instruction: give asa & ava a real pet (well, three), extracted from a real RPG Maker MV character sheet (`Nature.png`, confirmed via direct inspection: standard 8-slot/48px layout). Slots 0/1/2 = dog/cat/chicken respectively, per direct confirmation. Real, live desktop windows for all three now exist, built via `&.widgits/tile-picker/ops/tp_rmmv_character_extract.c` (see that project's own `TILE_PICKER_DESIGN.md` §8.3 for the real extraction mechanics and the "individual vs. tilemap" designation this uses).

### 5.1 What's real right now
- Three real sprites extracted (dog/cat/chicken, default down-facing standing frame).
- Three real, live, draggable desktop windows, spaced on the shared grid, with real transparency (X11 Shape Extension mask, see `TILE_PICKER_DESIGN.md` §8.2).
- The extraction op supports a `direction` parameter (0-3) specifically so a future tick loop can re-run it as a pet's movement direction changes — **the mechanism exists, nothing calls it yet.**

### 5.2 Designed, NOT built yet — direct instruction, real scope for next session
1. **Line-of-sight AI**: dog chases cat when in sight, cat chases chicken when in sight, all three wander otherwise. Needs a real "can A see B" check (simple grid-distance, per this doc's own §3.1 — no true FOV/occlusion system exists in this house, confirmed) and a real tick loop moving each pet's window accordingly (reusing the shared 80px grid every desktop entity already uses).
2. **"Eventable" AI**: the above behavior should be expressed as real events (not hand-rolled C logic buried in a tick loop) so it's visible/editable through event-editor once that's real — direct instruction, this is why event-editor genuinely needs to exist before this AI is "done" in the intended sense, not just functionally working.
3. **A real "current behavior" view**: direct instruction — a pet's current behavior (wandering / chasing X / etc.) should be visible in "their event sheet menu widget view." This needs event-editor's own real CHTPM widget to exist first (see `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md` — still not built, still the real blocker for this whole line of work).
4. **Wire the `Events` context-menu method for real**: currently still `void` (an honest no-op) on every entity's `meta.pdl`/`methods.pdl`, per direct instruction "clicking events in context doesn't yet open event widget window, we need to change that." The real reference to build against is `&.widgits/event-editor/gl_mock/ee_gl_mock.c` (read in full this session) — its own real command-list/pages/contents/footer zones are the UX to mirror in a real CHTPM widget, not its raw-GLUT rendering code itself (same "reference for data shape and UX flow, not for how to build a house widget" distinction `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md` §2 already makes).
5. **Later, explicitly deferred by direct instruction**: male/female chickens, egg-laying, pooping — "just the chickens for now." Not designed in detail yet, just recorded as real, upcoming scope.

### 5.3 Why this section exists separately from §1-4 above
§1-4 of this doc were written *before* the pets existed — they're about asa/ava's own conversational AI (gemma-lan/047.scm) and the future farming loop. The pets are a real, concrete, smaller-scope test case for the exact same "eventable AI" idea (§5.2 item 2) — getting a dog to chase a cat via real events is a much smaller, faster proof of the event-driven-behavior concept than the full farming loop, and should probably be built *first*, as a real rehearsal, before attempting the bigger farming behavior tree.
