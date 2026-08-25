# 🌱👫️ asa-&-ava — design plan

**Status:** design only, nothing built yet.
**Date:** 2026-08-04.
**Working title:** "Adam & Eve" in conversation → real project name **asa-&-ava**.
**Priority:** a real fork off today's tile-picker/hikikomorai/event-editor work, not a separate track — read those docs first, this one leans on them directly.
**Companion doc:** `AI-SECTION-teach-and-persist.md` (same dir) — knowledge distillation (bigger chat teaching smaller chat to farm), session persistence/autosave, the line-of-sight sensing gap, and the later Dwarf-Fortress-style controller-widget. Read it alongside this one.

---

## 0. 🎯 The one-paragraph pitch

Two entities live on your desktop (later: in a game view too). They talk to each other — one generating replies via **gemma-lan** (the house's LAN-hosted judge/teacher model), the other via **047.scm** (this house's real "select, don't generate" text system, which learns/adapts by having its choices judged by the other side, gemma). But conversation is not their life — most of the time they're running a real **behavior tree**, authored in event-editor as real `.pal`, that has them plant, grow, harvest, and eat crops, right there on the desktop, building up their own inventories. They have a relationship — shown through behavior and occasional real conversation — not a chatbot stuck in an endless dialogue loop.

---

## 1. 🧬 Why this is a fork of today's work, not a new track

Everything asa-&-ava needs already has a real, working or designed precedent from this exact session:

| asa-&-ava needs | Already real, from today | Where |
|---|---|---|
| A "thing" that lives on the desktop as a real GL window | `tp_desktop_window.c` — live, draggable, grid-snapped, 30fps-capped, real emoji/sprite texture rendering | `&.widgits/tile-picker/` |
| A real right-click context menu with data-driven methods | Just built — `METHOD` rows in a package's own `meta.pdl`, read by `tp_desktop_window.c`, dispatched by action string | `&.widgits/tile-picker/ops/tp_desktop_window.c`, `TILE_PICKER_DESIGN.md` §4.5 |
| "Open Event Editor" as a context-menu method, behavior authored as real `.pal` | Designed, not built yet — but the plan and the real reference (`201.rpg-maker-clone`'s command IR) are already written | `&.widgits/event-editor/EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md` |
| "One button opens a whole group of related desktop entities" | This IS hikikomorai's own core premise — a desk-session task-bar showing/managing all living desktop procs | `@.apps/hikikomorai/hikikomorai-design.md` |
| A real audit trail of what these two entities did/said | The master-ledger convention (append-only event log, materialized live state stays the fast-read source of truth) | `aomorai-editor-blueprint.md` §2, §2.5 |
| Hunger/inventory as a real mechanic, not invented from scratch | Mutaclysm's own hero already has a real `hunger` field and a real (if currently empty) `inventory/` directory | `101.mutaclsym…/pieces/world_01/map_start/hero/state.txt` + `.../hero/inventory/` |
| Two different text-generation approaches, already real | `gemma-lan` = the LAN-hosted gemma judge/teacher (`10.0.0.144:11434`, per `045.muchi-pal-agent🤖️+1/⛔️.compute-constraint.READ-FIRST.txt`); `047.scm` = a real, partially-built "Student-Curriculum Model" (selects phrases from a human-written corpus, reward-tuned by gemma's own describe-then-score judging) | `047.scm🎓️+1/!.SCM-DESIGN.md`, `047.scm🎓️+1/prog-rep-au2.txt` |

**asa-&-ava's real job is wiring these existing pieces together around a farming behavior-tree loop** — not inventing a new architecture.

---

## 2. 💬 The conversation half

### 2.1 Who generates what

- **One entity's replies** come from **gemma-lan** — a real HTTP call to the LAN Ollama-style host already used elsewhere in this house as the judge/teacher model.
- **The other entity's replies** come from **047.scm** — NOT free-generation. Per its own design doc, it *selects* a phrase from a small, human-written corpus (a "curriculum"), biased by plain-text weights, with an FSM/bandit skeleton for coherence. `047.scm` is real and partially built already: loaders, router, feature extraction, selector, slot-fill, and a demo CLI (`scm_cli`) are done and verified (per `047.scm🎓️+1/prog-rep-au2.txt`, "SCM steps 1+2 DONE and verified").

### 2.2 "The scm will learn from the other" — what that really means

This is 047.scm's own designed reward loop, not a new mechanism: SCM's phrase-choice weights get tuned based on gemma's judgment of its replies, using the house's own established, hard-learned rule — **gemma describes, deterministic code scores** (never ask a small model to directly rate/classify its own output; see house memory `reference_describe_dont_classify_small_llm.md` and `047.scm🎓️+1/!.SCM-DESIGN.md` §1's own citation of that exact rule). In asa-&-ava's case: gemma's own in-conversation replies act as the live "teacher" signal 047.scm's selector reward-tunes against, turn by turn, inside this specific relationship — a real, live instance of the reward loop 047.scm's own design already specifies, not a new one invented for this project.

### 2.3 Where conversations get saved, and the "every desk-created event gets its own button.sh" convention

Direct instruction: **every event created on the desktop should be independently openable via its own `button.sh`** — this is a real, house-wide convention being stated explicitly now, not just an asa-&-ava-specific rule, and belongs documented in event-editor's own doc too (see §5 below — cross-referenced there, not duplicated).

Concretely, for asa-&-ava: each real conversation is one **event package**, saved the same shape as any other desktop-tray package this session already builds on (`#.desktop/events/<conversation_id>/`, following `#.desktop/README.txt`'s own existing `events/` folder convention — already provisioned, just unused until now). Each conversation package needs:

```
#.desktop/events/asa_ava_convo_<timestamp>/
    transcript.pal          # the real event script - what was said, as real .pal (not just log text)
    transcript_readable.txt # human-readable mirror, same "ASCII + ledger" duality this house always keeps
    meta.pdl                 # METHOD rows - "Open in Event Editor", "Replay", "Continue" etc.
    button.sh                 # <- the real ask: opens THIS conversation independently
```

That per-package `button.sh` is small and mechanical (matches every other widget's own `button.sh` shape this session already built): it resolves event-editor's own real binary/session path and launches it pre-focused on this specific package, the same "cross-link via a small state file, never shared process infra" rule every widget in this house already follows (see `TILE_PICKER_DESIGN.md` §2.1 and `hikikomorai-design.md` §1). Building this `button.sh` template is real work for whoever builds event-editor's own package-export step (§4 of `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md`) — asa-&-ava is simply the first real consumer that needs it to exist.

### 2.4 They should NOT spend their whole time talking

Direct instruction, important design constraint: conversation is an **occasional, triggered event** in these entities' lives, not a permanent state. Concretely: the behavior tree (§3) is what they spend most simulated time running (planting/growing/harvesting/eating); a conversation is one possible **behavior-tree leaf/branch** — triggered by proximity, by a shared event (e.g. both idle, or a harvest just happened), or on a real cooldown — not a background chat loop running every tick. Their **relationship** should instead show up as state that *persists and slowly changes* between conversations (e.g. a simple real affinity/familiarity value, or literally: what they say next time is colored by what happened last time, via 047.scm's own real memory-ring mechanism already built — "memory gives turn variety," per its own prog-rep) — the relationship is real accumulated state, not just repeated dialogue.

---

## 3. 🌾 The farming half (the real bulk of their simulated life)

### 3.1 Behavior tree, authored in event-editor

Direct instruction: their plant/grow/harvest/eat loop should be a real **behavior tree**, built in event-editor, compiling to real `.pal` — same "buttons AND script, both real" philosophy `aomorai-editor-blueprint.md` §3 already commits to for aomorai-editor's own event editor, and the same real command-IR shape `201.rpg-maker-clone` already proves out (`CMD_SHOW_TEXT`, `CMD_SET_SWITCH`, `CMD_IF_SWITCH`, etc. — a behavior tree's branch/condition nodes map naturally onto that same IR, particularly `CMD_IF_SWITCH`-style conditionals). This is real, additional motivation for building event-editor's real CHTPM widget next (per `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md` §4) — asa-&-ava is a second, concrete real consumer of it, alongside aomorai-editor's own in-game events.

### 3.2 The loop itself (v1 scope, desktop only)

Direct instruction: **desktop only for now**, board-view/game-view placement is explicitly deferred (ties into `TILE_PICKER_DESIGN.md` §5's own "^ mode" click-into-board-view work, already partially built this session — asa-&-ava's own crops/entities can reuse that exact same placement mechanism once it's further along, not a separate one).

A real, minimal v1 behavior-tree loop:

```
PLAN   → pick an open desktop grid cell (same GRID_CELL_PX=80 convention
         tp_desktop_window.c/egg_window.c already share) to plant in
PLANT  → spawn a real "seed" desktop entity there (same live-GL-window
         mechanism tile-picker's own tp_place_desktop.c already proves out)
GROW   → real, ticked state change over time (reuse the same real
         game-clock file convention just documented in
         aomorai-editor-blueprint.md §2.5 - growth stage keyed off real
         elapsed game time, not a hardcoded timer)
HARVEST→ remove the grown-crop entity, add a real item to the harvesting
         entity's own inventory/ (same real, already-existing
         pieces/.../hero/inventory/ directory convention mutaclysm
         already has, reused here per-entity)
EAT    → consume an inventory item, restore a real hunger field (same
         hunger= field mutaclysm's own hero/state.txt already has -
         reuse the field name/semantics, don't invent a new one)
```

### 3.3 Items on desk are part of "the group," not separate

Direct instruction: asa-&-ava's own `button.sh` should open **both entities AND any of their items currently on the desk** — not just the two "characters." This is real, direct hikikomorai territory: a desk session's task-bar (hikikomorai's own core widget, per its design doc §2.2) is precisely "show all living procs/entities on a desktop session, allow switching between them." asa-&-ava doesn't need to reinvent that — it's a concrete, motivating **use case** for hikikomorai's own task-bar, and building asa-&-ava's own "open everything in this group" button is real, load-bearing pressure toward actually building hikikomorai's task-bar sooner rather than later (currently still "suggested build order, not started" per hikikomorai's own §6). **This can inform hikikomorai's own design back**, too — asa-&-ava is a real, concrete test case for what a "desk session group" needs to feel like, ahead of a fully general implementation.

---

## 4. 🗂️ Where this lives on disk

```
@.apps/asa-&-ava/
    asa&ava-design-plan.md   # this doc
    (future) button.sh        # opens asa + ava + their on-desk items together
    (future) pieces/asa/, pieces/ava/   # per-entity state, same shape as mutaclysm's hero/
    (future) pal/                       # behavior-tree .pal scripts, compiled by event-editor
```

Conversation event packages themselves live under the house-wide `#.desktop/events/` tray (§2.3), not inside this app's own dir — they're desk-session artifacts, portable and independently openable, same as any other desktop-placed thing this session already built.

---

## 5. 🚧 What actually needs to get built first (real order, not a wishlist)

1. **Event-editor's real CHTPM widget** (per `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md` §4) — asa-&-ava's behavior tree AND its conversation packages both need this to exist before anything else here is real, not stubbed.
2. **The "every desk-created event gets its own `button.sh`" convention**, built once as part of event-editor's own package-export step (§2.3 above) — reused by asa-&-ava, not built specially for it.
3. **047.scm's own eval harness + Phase B `.pal`-writing work** (per its own `prog-rep-au2.txt` "NEXT STEPS") — asa-&-ava's own conversational entity depends on 047.scm actually being able to hold a real, judged multi-turn exchange, which isn't fully wired yet.
4. **A real gemma-lan conversational op** (a small, new op that does the actual HTTP call and returns a reply) — not confirmed to exist yet as a reusable op; needs a real check before assuming it's ready.
5. **hikikomorai's task-bar**, at least a minimal read-only version (per its own §6 build order, step 3) — asa-&-ava's own "open the whole group" button needs *something* to show/manage multiple live desktop entities as one group, even before hikikomorai is fully built.
6. Only then: the actual behavior tree (§3.2) and the actual conversation trigger logic (§2.4).

---

## 6. ❓ Open questions (not decided)

1. Exact affinity/relationship state shape — a single scalar, or something richer? Not designed yet.
2. Whether asa/ava's own farming state should live under mutaclysm's own `pieces/world_01/` (reusing its real hero/inventory conventions directly) or under this app's own `pieces/`, cross-linked the way every other widget pairing in this house already is. Leaning toward the latter (own state, referencing mutaclysm's conventions rather than living inside its tree) but not decided.
3. How "both entities AND their items" gets resolved technically before hikikomorai's real task-bar exists — a temporary, asa-&-ava-specific version of the same idea, or wait for hikikomorai's own minimal read-only version (§5 item 5)? Leaning toward the latter, to avoid duplicating logic that'll need to move into hikikomorai anyway.
