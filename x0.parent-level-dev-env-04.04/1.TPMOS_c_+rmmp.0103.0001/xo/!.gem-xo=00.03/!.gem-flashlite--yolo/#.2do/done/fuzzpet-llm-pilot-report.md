# FuzzPet LLM Pilot Report
**Date:** 2026-10-10  
**Author:** opencode agent  

---

## 1. Executive Summary

The FuzzPet digital pet (Fuzzball) lives in the **1.TPMOS_c_+rmmp.0103.0001** framework.
We applied the **relay-driven LLM pilot pattern** (proven on WSR) to make Fuzzball a
self-directed agent, using a **hybrid relay + book:page events architecture**.

---

## 2. Framework: Yes, 1.TPMOS + book:page

FuzzPet uses the full TPM/OS stack PLUS the `.pdl` page-file events architecture:

```
1.TPMOS_c_+rmmp.0103.0001/
+-- pieces/chtpm/            # TPM engine (parser, renderer, keyboard)
+-- pieces/apps/fuzzpet_app/ # The pet application
|   +-- fuzzpet/
|   |   +-- state.txt        # Flat key=value state (relay pattern input)
|   |   +-- behavior.pdl      # Declarative behavior model (.pdl METHOD table)
|   |   +-- scripts/          # .asm event scripts (book:page events layer)
|   |   +-- history.txt       # Decision history
|   |   +-- last_response.txt # LLM raw output
|   +-- traits/
|   |   +-- fuzzpet_brain.c  # LLM brain (reads state, calls Ollama, writes relay)
|   |   +-- fuzzpet_fsm.c    # FSM (reads PET_ACTION from relay, calls ops)
|   +-- manager/state.txt    # Active target, last_key
+-- projects/op-ed/          # Editor project (uses same book:page .pdl pattern)
+-- projects/slop-ed-dev/    # Editor dev project (games + .asm scripts)
```

### Book:Page pattern in this house

- `.pdl` files define state declaratively (`SECTION | KEY | VALUE`)
- `project.pdl` declares the project entry layout and state
- `piece.pdl` declares entity methods → `.asm` script mappings
- `.asm` scripts use PAL-style instructions: `exec`, `call_method`, `read_history`,
  `read_relay_row`, `relay_write`, `hit_frame`, `console_print`, `halt`

---

## 3. Is It Learning?

### Before: No
- `zombie_ai.c`: Hardcoded pathfinding, no learning
- `fuzzpet_action.c`: Fixed effect values
- No pattern analysis or evolution

### Now: Hybrid Learning
The pipeline creates multiple learning surfaces:

```
state.txt ──┐                      ┌──→ behavior.pdl (synced RUNSTATE)
history.txt ├──→ fuzzpet_brain.c   │
LLM API     └─→ PET_ACTION relay ──┼──→ fuzzpet_fsm.c ──→ fuzzpet_action.+x
                │                   │
                └──→ brain_dispatch.asm ──→ calls .asm scripts
                                          └──→ exec fuzzpet_action.+x
                │
                └──→ wsr_evolve (future) ──→ mutates behavior.pdl thresholds
                │
                └──→ wsr_fitness (future) ──→ scores pet wellness
                │
                └──→ tom_layer (future) ──→ tracks pet-player beliefs
```

**Current learning surface:**
1. History analysis — `history.txt` logs every decision with state context
2. Behavior sync — `behavior.pdl` RUNTIME section mirrors state values
3. Relay trace — `interact_relay.txt` captures all actions + guardrail overrides

**Missing for full evolution:**
- `wsr_evolve.c` integration to mutate `.pdl` GUARDRAIL thresholds
- `tom_layer.+x` for belief tracking
- `wsr_fitness.c` for wellness scoring

---

## 4. What Was Built (Hybrid)

### fuzzpet_brain.c — Relay Brain + PDL Sync

Standalone C op that:
1. Reads `state.txt`, `history.txt`, `last_response.txt`
2. Builds context prompt (goal + state + history)
3. Calls Ollama API via curl
4. Parses JSON response (action/reason/confidence)
5. Applies need-based guardrails (hunger>60 → feed, happiness<20 → play, energy<20 → sleep)
6. Writes `PET_ACTION` to relay event bus
7. Syncs state values to `behavior.pdl` RUNTIME section
8. Falls back to need-based policy if Ollama is down

Compile + run:
```bash
export PRISC_PROJECT_ROOT="$PWD"
gcc -O2 traits/fuzzpet_brain.c -o traits/+x/fuzzpet_brain.+x -lm
traits/+x/fuzzpet_brain.+x . survive gemma3:1b
# {"action":"feed","reason":"hunger critical","confidence":0.90,"guardrail":true}
```

### fuzzpet_fsm.c — Relay-to-FSM Controller

Reads `PET_ACTION` from relay, maps action names to `fuzzpet_action.+x` calls,
handles composite actions (heal = sleep+play, evolve = wsr_evolve).

### behavior.pdl — Declarative Behavior Model

```
METHOD | feed           | scripts/pet_feed.asm
METHOD | play           | scripts/pet_play.asm
METHOD | sleep          | scripts/pet_sleep.asm
METHOD | explore        | scripts/pet_explore.asm
METHOD | heal           | scripts/pet_heal.asm
METHOD | evolve         | scripts/pet_evolve.asm
METHOD | toggle_emoji   | scripts/pet_toggle_emoji.asm
METHOD | dispatch       | scripts/brain_dispatch.asm
GUARDRAIL | hunger_critical | 60
GUARDRAIL | happiness_low   | 20
GUARDRAIL | energy_low      | 20
```

### .asm Event Scripts (book:page events layer)

Each script uses house PAL instructions (`console_print`, `exec`, `hit_frame`, `halt`):

```
pet_feed.asm:
    console_print "Fuzzball: *nom nom*"
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball feed
    hit_frame
    halt
```

`brain_dispatch.asm` is the bridge — reads `PET_ACTION` rows from relay via
`read_relay_row`, then `call_method` dispatches to the appropriate handler script.

---

## 5. Push vs Pull Claude (Hybrid)

**Push:** `fuzzpet_brain.c` writes to relay → `brain_dispatch.asm` reads → `.asm` scripts execute  
**Pull:** `brain_dispatch.asm` reads `state.txt` history → calls relay → `fuzzpet_fsm.c` polls → actions execute  

```
# Claude generates pet action (push)
claude -p "Fuzzball: hunger=70, happiness=55. What do you do?" \
  | jq '.action' > pieces/apps/player_app/interact_relay.txt

# Claude analyzes pet state (pull)
claude -p "Analyze Fuzzball history and relay. Next trait to evolve?" \
  < pieces/apps/fuzzpet_app/fuzzpet/history.txt \
  < pieces/apps/player_app/interact_relay.txt
```

---

## 6. Next Steps

| Task | Priority | Description |
|------|----------|-------------|
| `run_fuzzpet_agent.sh` | HIGH | Loop: brain → dispatch → state update |
| Integrate wsr_evolve → behavior.pdl | HIGH | Mutate guardrail thresholds based on fitness |
| `fuzzpet_fitness.c` | MED | Score: avg_happiness * survival_time * levels |
| `pet_tom_layer.c` | MED | Track pet's beliefs about player |
| op-ed integration | LOW | Bridge editor `.pdl` game scripts to fuzzpet traits |
