# Events-Driven ASCII Game Engine: Technical Design
## Can This Interface Build Meaningful Games + Self-Build Through Gemma API?

**Date:** 2026-10-10
**Author:** opencode agent
**Tags:** tpmos, events-hq, op-ed, concept-bank, promotion-ledger, gemma-api, ascii-game, self-building

---

## TL;DR

**Yes on both counts.** The events-hq + op-ed subsystem in 1.TPMOS provides
a complete pipeline for ASCII game development through declarative `.pdl`
page files + `.asm` event scripts. The Concept Bank + Promotion Ledger
system provides a real, deterministic framework for LLM-assisted
self-building — Gemma proposes edits through DESCRIBE, validator gates
them, and Laplace-smoothed scoring determines promotion. All of this
already exists in code; it just needs integration with the fuzzpet game
loop.

---

## 1. The Events Architecture (Can It Drive Games?)

### 1.1 Core Components

The events-hq subsystem (`44.xyz.01.00/&.widgits/events-hq/`) implements
RPG Maker-style event scripting with a modern file-backed architecture:

```
layout.chtpm (UI) --> events-hq manager --> event_pkg/ --> event.ir.pdl --> event.pal --> cmd_N.sh
                    (reads state)        (page 1)    (IR graph)    (compiled)  (shell)
```

**Key ops:**
- `khtpm_events_hq_manager.c` — Compiles `event.ir.pdl` → `event.pal` → `cmd_N.sh`
- `evhq_projector.c` — Projects event outcomes to state files
- `ai_relay_key.sh` — Relays keyboard input (W/A/S/D/Enter) to event triggers

### 1.2 Event Script Format (`.asm`)

PAL-style assembly with real primitives used in production:

```asm
; move.asm — WASD movement handler
start:
    read_history r1
    beq r1, x0, finish
    addi r2, r1, -119       ; 'w' key
    beq r2, x0, move_up
    ...
move_up:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x selector w project
    j start
finish:
    hit_frame
    halt
```

**Available instructions:**
| Instruction | Purpose |
|---|---|
| `console_print "..."` | Print to terminal output |
| `read_history r1` | Read last key from history.txt |
| `read_relay_row TAG r1 r2` | Read last row from relay event bus |
| `relay_write "..."` | Write to relay event bus |
| `exec ./path/to/op.+x args` | Execute a compiled house op |
| `call_method name` | Call a METHOD declared in piece.pdl |
| `SET_RESPONSE "..."` | Write response string |
| `hit_frame` | Trigger renderer update |
| `read_pdl VAR "SECTION" "key" DEST` | Read from .pdl file |
| `HALT` | Exit script |

### 1.3 Page File Format (`.pdl`)

Declarative state + method definitions:

```
SECTION      | KEY          | VALUE
-------------+--------------+------------------
META         | project_id   | fuzz-op-mirror
STATE        | starting_map | map_0001
STATE        | player_piece | hero
METHOD       | interact     | scripts/pet_interact.asm
METHOD       | feed         | pieces/ops/fuzzpet_action.+x pet_01 feed
```

### 1.4 Integration with Existing Games

The `projects/op-ed/games/` directory contains working prototypes:
- `fuzz-op-mirror/` — Mirror maze with pet interaction
- `test-game-01/` — NPC dialogue test

Both use the same pattern: `project.pdl` + `maps/` + `pieces/` (.pdl) +
`scripts/` (.asm). This is the scaffolding for a Cataclysm-style roguelike.

---

## 2. Cataclysm-Style Roguelike: What's Needed

### Existing Building Blocks

| System | Status | Files |
|---|---|---|
| Tile map rendering | EXISTS | `move_entity.+x`, `render_map.+x` |
| WASD movement | EXISTS | `move.asm`, `player_loop.asm` |
| Entity interaction | EXISTS | `pet_interact.asm`, `talk.asm` |
| State management | EXISTS | `piece.pdl`, `state.txt` |
| Combat system | MISSING | Would use `exec` pattern |
| Procedural generation | MISSING | Would need new ops |
| Inventory | MISSING | Would use `.pdl` state |
| Turn-based flow | MISSING | Would use event triggers |
| FOV/LOS | MISSING | Would need new ops |

### Scaffolding Plan

```
projects/cataclysm-hq/
+-- project.pdl          # Project definition
+-- layouts/main.chtpm   # Terminal UI (status bars + map viewport)
+-- maps/
|   +-- dng_0001.txt     # Dungeon level (ASCII map)
|   +-- dng_template.pdl # Procedural dungeon template
+-- pieces/
|   +-- hero/
|   |   +-- piece.pdl    # HP/SP/STR/DEX/inventory slots
|   |   +-- state.txt    # Current stats
|   +-- orc_01/
|   |   +-- piece.pdl    # AI state, combat methods
|   |   +-- ai.asm       # Orc AI loop
+-- scripts/
|   +-- game_loop.asm     # Turn processing
|   +-- combat.asm        # Attack/defense
|   +-- generate_dungeon.asm # Procedural content
|   +-- pickup_item.asm   # Inventory management
+-- ops/
    +-- rng_op.c          # Random number generator (seeded)
    +-- fov_op.c          # Field of view (shadow casting)
    +-- dungeon_gen.c     # Procedural map generation
```

### Event Script Example: Combat

```asm
; combat.asm — Player attacks monster
start:
    ; Read attacker and target from state
    read_pdl r1 "STATE" "player_atk" r2
    read_pdl r3 "STATE" "monster_hp" r4

    ; Calculate damage (simple: atk - 1d6)
    exec ./ops/+x/rng_op.+x 6
    sub r2, r1, r4        ; mon_dmg = atk - roll

    ; Update monster HP
    read_pdl r5 "STATE" "monster_hp" r6
    sub r6, r5, r2
    write_pdl "pieces/monster_01/state.txt" "STATE" "hp" r6

    ; Check if monster dead
    ble r6, 0 monster_dead

    ; Monster counterattacks
    exec ./ops/+x/rng_op.+x 4
    sub r7, r3, r8
    read_pdl r9 "STATE" "player_hp" r10
    sub r10, r9, r7
    write_pdl "state.txt" "STATE" "player_hp" r10
    j end

monster_dead:
    console_print "You killed the orc!"
    relay_write MONSTER_DEATH|orc_01|player=hero
    ; Spawn loot
    exec ./ops/+x/spawn_loot.+x orc_01

end:
    hit_frame
    halt
```

---

## 3. Self-Building Through Gemma API

### 3.1 The Concept Bank

The Concept Bank (`&.widgits/concept-bank/`) is a hub-and-spoke knowledge
store where concepts are named, file-backed, and weight-bounded:

**Master concept** (`force.pdl`):
```
NODE | force
KIND | master
N_SLOTS | 8
SLOT | 0 | POINTS_TO=motion | WEIGHT=0.70
SLOT | 1 | POINTS_TO=energy | WEIGHT=0.50
MIRROR_FROM | gravity_constant | WEIGHT=0.60
MIRROR_FROM | mass | WEIGHT=0.75
```

**Spoke concept** (`gravity_constant.pdl`):
```
NODE | gravity_constant
KIND | spoke
N_SLOTS | 8
SLOT | 0 | POINTS_TO=force | WEIGHT=0.60
```

**Candidate edit** (`edit_0001_pass.txt`):
```
EDIT | id=e0001-real-smoketest | type=spoke_weight_delta
    | target=gravity_constant
    | slot=force
    | delta=+0.05
    | reason="Watch Layer: stunt jump height increased..."
    | proposer=gemma
    | status=candidate
```

### 3.2 The Self-Building Loop

Per `A-TEARIT-IS-ALL-YOU-NEED.md` section 2, the loop is:

```
WATCH LAYER --> GEMMA --> SCORER (C) --> CANDIDATE EDIT
                    |                  |
                    v                  v
              (DESCRIBE only)    (deterministic pattern match)

CANDIDATE EDIT --> VALIDATOR (C) --> REPLAY/SIMULATE --> PROMOTION LEDGER
                 (bounds+hub check)                   (Laplace: (r+1)/(r+p+2))

PROMOTION LEDGER --> LIVE STATE
                     (Concept Bank, FSM, GOAP, Bank Layer)
```

### 3.3 Gemma Integration

The `concept_bank_ctx.c` op reads concept bank spokes + weights and
formats them as prompt context. Gemma's role (per house law: DESCRIBE
only, never CLASSIFY, never route):

1. **Observation record:**
   ```
   OBS | id=<uuid> | target=<action/fsm-transition> | outcome=<text>
   FEEDBACK | valence=<+1/-1/0> | concept=<z-node name> | intensity=<0.0-1.0>
   ```

2. **Gemma response (natural language description):**
   > "The player's stunt jump increased height when the physics
   > primitive's gravity constant was lower; this seems related to
   > the force and motion concepts."

3. **Scorer converts to candidate edit:**
   ```
   EDIT | type=spoke_weight_delta | target=gravity_constant
        | slot=force | delta=+0.05 | reason="..."
        | proposer=gemma | status=candidate
   ```

4. **Validator blocks hallucination:**
   - Rejects target/slot referencing non-existent masters
   - Rejects delta outside [-0.2, +0.2]
   - Rejects malformed records

5. **Promotion ledger scores** via Laplace: `(reward+1)/(reward+punish+2)`

### 3.4 Bootstrap Tiers (Gemma's Trust Level)

Per A-TEARIT section 5, Gemma earns autonomy through measured tiers:

| Tier | What Gemma Can Propose | Auto-Promote Threshold |
|---|---|---|
| Preschool | `spoke_weight_delta` on existing spokes, delta in [-0.1, +0.1] | Never — always human review |
| Elementary+HS | Same as preschool, slightly larger delta [-0.15, +0.15] | Never — always human review |
| Associate+Bachelor | `spoke_weight_delta` only, delta up to [-0.2, +0.2] | Score >= 0.90 + N >= 20 replay obs |
| Master+PhD | All four edit types including FSM/GOAP descriptions | Score >= 0.90 + N >= 20 replay obs |

No auto-promotion for `new_concept_node` at any tier below Master+PhD.

### 3.5 FSM Self-Authoring (The Key Link)

Per A-TEARIT section 2.6, Gemma never writes FSM/GOAP tables directly.
When a Gemma-proposed `fsm_transition_describe` or `goap_action_describe`
candidate clears the promotion ledger, its description text is fed
through the **same events-hq compiler** (`khtpm_events_hq_manager.c`:
`event.ir.pdl` → `event.pal` → `cmd_N.sh`) that human authors use.

Zero new code-generation risk surface. An LLM-authored FSM is compiled
by literally the same pipeline, with the same validation, as a human-typed one.

---

## 4. What We Built (fuzzpet-brain Branch)

### Files Created

| File | Purpose |
|---|---|
| `traits/fuzzpet_brain.c` | LLM brain: reads state, calls Ollama, writes PET_ACTION + syncs behavior.pdl |
| `traits/fuzzpet_fsm.c` | FSM: reads PET_ACTION from relay, dispatches to fuzzpet_action.+x |
| `fuzzpet/behavior.pdl` | Declarative behavior model (METHOD table for 7 actions) |
| `fuzzpet/scripts/brain_dispatch.asm` | Relay-to-events dispatcher |
| `fuzzpet/scripts/pet_feed.asm` | Feed action script |
| `fuzzpet/scripts/pet_play.asm` | Play action script |
| `fuzzpet/scripts/pet_sleep.asm` | Sleep action script |
| `fuzzpet/scripts/pet_explore.asm` | Explore action script |
| `fuzzpet/scripts/pet_heal.asm` | Heal action script (sleep+play) |
| `fuzzpet/scripts/pet_evolve.asm` | Evolve action script |
| `fuzzpet/scripts/pet_toggle_emoji.asm` | Toggle emoji display |

### Hybrid Architecture

```
                     fuzzpet_brain.c (Ollama/gemma3:1b)
                             |
                             | PET_ACTION row -> relay
                             v
                    interact_relay.txt
                    /               \
        fuzzpet_fsm.c           brain_dispatch.asm
        (poll relay)            (read_relay_row)
                     \               /
                      v             v
                  call_method / exec
                      \   /
                       v
            pet_feed/play/sleep/explore/heal/evolve/toggle_emoji.asm
                    |
                    v
         fuzzpet_action.+x (existing C op)
```

brain.c also syncs to `behavior.pdl` RUNTIME section — dual write for
both relay pattern consumers and book:page events consumers.

---

## 5. Integration Plan: Cataclysm + Self-Build

### Phase 1: Game Skeleton (Week 1)
- Scaffold `projects/cataclysm-hq/` with book:page `.pdl` files
- Port fuzzpet brain -> cataclysm brain (goal: survive/floor/level_up)
- Create dungeon generation `.asm` script using `move_entity` op
- Wire keyboard input via `ai_relay_key.sh` -> event triggers

### Phase 2: Concept Bank Integration (Week 2)
- Create `cataclysm-concept-bank/` with masters:
  `combat`, `exploration`, `magic`, `survival`
- Create spokes:
  `sword_combat`, `stealth`, `fire_magic`, `hunger`, `thirst`
- Wire `concept_bank_ctx.c` to output context for brain prompts
- Gemma proposes weight deltas for combat effectiveness based on outcomes

### Phase 3: Self-Building Loop (Week 3)
- `brain_dispatch.asm` checks relay, dispatches to combat/move/dungeon scripts
- After each game session: `OBS` records written to relay
- Gemma DESCRIBE -> candidate edit -> validator -> promotion ledger
- Passed edits auto-applied to `behavior.pdl` METHOD table
- `khtpm_events_hq_manager.c` recompiles affected `.asm` scripts on pdl change

### Phase 4: Terminal Display (Week 4)
- Use Wraith terminal for display
- Layout renders status bars + map viewport
- `console_print` in `.asm` scripts -> terminal output
- `hit_frame` triggers renderer update

---

## 6. Risks & Mitigations

| Risk | Mitigation |
|---|---|
| gemma3:1b hallucinates invalid edits | Validator blocks non-existent masters/slots, delta bounds [-0.2, +0.2] |
| Small model drifts from prompt | Need-based guardrails in brain.c override critical decisions |
| Event scripts become spaghetti | book:page `.pdl` keeps METHOD table clean |
| Self-modifying games unstable | Promotion ledger only promotes passed candidates; git history preserves originals |
| No real-time performance | Turn-based by design; Wraith terminal 8s timeout sufficient |

---

## 7. References

- `A-TEARIT-IS-ALL-YOU-NEED.md` (claude branch: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/`)
- `khtpm_events_hq_manager.c` (`44.xyz.01.00/\&.widgits/events-hq/ops/`)
- `concept_edit_validate.c` (`44.xyz.01.00/\&.widgits/concept-bank/ops/`)
- `promotion_ledger.c` (`44.xyz.01.00/^.hai-horn/ops/`)
- `concept_bank_ctx.c` (`44.xyz.01.00/^.hai-horn/ops/`)
- `AUTO-PROMOTION-RULE.md` (`44.xyz.01.00/\&.widgits/concept-bank/`)
- `OP-ED README.md` and `2do-m24.txt` (`1.TPMOS_c_+rmmp.0103.0001/projects/op-ed/`)
- `player_loop.asm`, `move.asm` (op-ed game scripts)
- WSR pipeline work (`LLM-HOUSE-PILOT.md`, `wsr_goap_planner.c`, `llm_brain.c`)
- FuzzPet hybrid (`fuzzpet_brain.c`, `fuzzpet_fsm.c`, `behavior.pdl`, `.asm` scripts)
