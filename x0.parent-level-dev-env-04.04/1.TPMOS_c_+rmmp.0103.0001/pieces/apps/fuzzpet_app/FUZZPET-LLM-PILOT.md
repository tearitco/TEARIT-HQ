# FuzzPet LLM Pilot — Design Doc

*Applying the relay-driven LLM pilot pattern to the digi pet ecosystem*

---

## 1. Vision

Use gemma3:1b (or Claude via `groq-ollama_bridge.c`) to pilot **Fuzzball** — the
digital pet — as a self-directed agent. The pet observes its own state, the
house state, and makes decisions through the relay event bus. Two directions
of LLM use:

- **Push**: LLM generates pet actions, narrative responses, and strategic
  decisions
- **Pull**: LLM reads pet state, history, and relay events to inform
  future decisions

---

## 2. Architecture

```
                    RELAY EVENT BUS
              interact_relay.txt
              (line-delimited, append-only)

  LLM Brain (Push)        TOM Layer          Macro Ticks
     reads pet state ──►  observes relay  ◄── tick clock
     calls Ollama         tracks beliefs     updates needs
     writes PET_ACTION ──►                    ┌──────────────┐
                                            │ pet state.txt│
     ┌─────────────────────────────────────► │ needs decay  │
     │                                       └──────────────┘
     │
     ▼
  fuzzpet_action.+x
     ┌─ feed:     hunger -= 20
     ├─ play:     happiness += 10
     ├─ sleep:    energy += 30
     ├─ explore:  move to new location
     └─ evolve:   level up + traits mutate

  Zombie AI Trait (zombie_ai.c)
     read relay ──► pathfind toward target ──► update pos_x/y ──► pulse frame
```

### Push/Pull LLM Integration

| Direction | Source | Mechanism |
|-----------|--------|----------|
| **Push** | gemma3:1b / Claude (via bridge) | Generates `PET_ACTION\|action\|conf\|reason` to relay |
| **Pull** | Claude / gemma3:1b | Reads `state.txt` + `history.txt` + relay, produces observations for next decision |

---

## 3. Pet State Schema

File: `pieces/apps/fuzzpet_app/fuzzpet/state.txt`

```
name=Fuzzball
hunger=50
happiness=55
energy=100
level=1
pos_x=5
pos_y=2
pos_z=0
status=active
emoji_mode=0
```

File: `pieces/apps/fuzzpet_app/manager/state.txt`

```
active_target_id=selector
last_key=None
```

---

## 4. Available Actions

| Action | Effect | Precondition |
|--------|--------|-------------|
| `feed` | hunger -= 20 | hunger > 20 |
| `play` | happiness += 10 | happiness < 100 |
| `sleep` | energy += 30 | energy < 70 |
| `explore` | pos_x/y ±1, find items | at_main (not in combat) |
| `heal` | energy += 15 + happiness +5 | hunger < 50 && energy < 50 |
| `evolve` | level += 1, traits mutate | level_up >= 100 |
| `toggle_emoji` | flip emoji_mode 0↔1 | any time |

---

## 5. LLM Prompt Strategy

### System Prompt

```
You are {name}, a digital pet in a text-based simulation.
You have hunger, happiness, and energy needs.

GOAL: {goal}

Your actions:
- feed: reduces hunger
- play: increases happiness
- sleep: increases energy
- explore: move to new location
- heal: recover when injured
- evolve: level up

RULES:
- If hunger > 60, you MUST feed.
- If happiness < 20, you MUST play.
- If energy < 20, you MUST sleep.

Output format (JSON only):
{"action":"<action>","reason":"<reason>","confidence":<float>}
```

### Goal Strategies

| Goal | Strategy |
|------|----------|
| `survive` | Prioritize needs (feed when hungry, sleep when tired) |
| `explore` | Move to new locations, discover items |
| `evolve` | Maximize level through activity and interaction |
| `socialize` | Interact with other pet entities (zombies, targets) |

### Need-Based Override

Small models need explicit guardrails:

```c
/* In fuzzpet_brain.c, after model_extract_action */
if (hunger > 60 && strcmp(action, "feed") != 0) {
    snprintf(action, sizeof(action), "feed");
    conf = 0.9;
}
```

---

## 6. Pipeline Integration

### Single Cycle

```bash
# 1. LLM Brain observes pet state, calls Ollama, writes to relay
PRISC_PROJECT_ROOT="$ROOT" ops/+x/fuzzpet_brain.+x . survive gemma3:1b

# 2. FSM maps action to execution
PRISC_PROJECT_ROOT="$ROOT" ops/+x/fuzzpet_fsm.+x

# 3. Action handler modifies pet state
PRISC_PROJECT_ROOT="$ROOT" ops/+x/fuzzpet_action.+x fuzzpet <action>

# 4. TOM layer updates beliefs
PRISC_PROJECT_ROOT="$ROOT" ops/+x/tom_layer.+x

# 5. Frame re-render
PRISC_PROJECT_ROOT="$ROOT" ops/+x/wsr_compose_frame.+x  # or fuzzpet renderer
```

### Full Automation Loop

```bash
# run_fuzzpet_agent.sh — 10 cycles of self-directed pet behavior
for i in $(seq 1 10); do
    fuzzpet_brain.+x . survive gemma3:1b
    fuzzpet_fsm.+x
    fuzzpet_action.+x fuzzpet ${PET_ACTION}
    tom_layer.+x
done
```

---

## 7. Push vs Pull Claude

The house has two LLM bridge patterns:

### Push: Claude as Pet Brain
```bash
# Claude generates pet decisions
claude -p "You are Fuzzball, a digital pet. Your hunger=50, happiness=55, energy=100.
What do you want to do? Output JSON with action/reason/confidence." \
  | jq -r '.action,.reason,.confidence' > /tmp/pet_decision.txt

# Pet brain reads decision, writes to relay
echo "PET_ACTION|$(cat /tmp/pet_decision.txt | head -1)|..." >> interact_relay.txt
```

### Pull: Claude as Pet Observer
```bash
# Claude reads pet state, provides analysis
export PRISC_PROJECT_ROOT="$ROOT"
claude -p "Analyze this pet's needs from state.txt and relay events.
What is the most urgent need? What action should follow?" \
  < pieces/apps/fuzzpet_app/fuzzpet/state.txt \
  < pieces/apps/player_app/interact_relay.txt
```

### Hybrid: groq-ollama_bridge.c
The existing bridge can be used as a bidirectional LLM relay:

```bash
# Push: pet generates response via bridge
ops/+x/groq-ollama_bridge.+x --prompt "$(build_pet_prompt)" --model claude-3-5-sonnet

# Pull: bridge reads pet state, returns analysis
ops/+x/groq-ollama_bridge.+x --pull --input state.txt --query "urgent need?"
```

---

## 8. Evolution Integration

The `wsr_evolve.c` op can evolve pet behavior chains:

```bash
# After each session, evolve traits
ops/+x/wsr_evolve.+x \
    pieces/display/fitness.txt \
    pieces/apps/fuzzpet_app/traits/ \
    pieces/display/pet_population.txt

# Each trait is a .c file; evolution mutates thresholds
# Example: hunger_threshold 60 → 55 (more cautious)
```

---

## 9. Status

- `fuzzpet_brain.c` — created, compiles clean ✓
- `fuzzpet_action.c` — exists, handles feed/play/sleep ✓
- `zombie_ai.c` — exists, pathfinding trait ✓
- `fuzzpet_fsm.c` — needs creation (maps PET_ACTION → action handler)
- `run_fuzzpet_agent.sh` — needs creation (automation loop)

---

## 10. How to Run

```bash
cd 1.TPMOS_c_+rmmp.0103.0001
export PRISC_PROJECT_ROOT="$PWD"

# Single decision cycle
ops/+x/fuzzpet_brain.+x . survive gemma3:1b
echo "Check relay:"; tail -3 pieces/apps/player_app/interact_relay.txt

# Compile the brain
CFLAGS="-Wall -Wextra -O2 -w"
gcc $CFLAGS traits/fuzzpet_brain.c -o traits/+x/fuzzpet_brain.+x -lm

# Action handler (existing)
ops/+x/fuzzpet_action.+x fuzzpet feed
```

---

## 11. References

- FuzzPet State: `pieces/apps/fuzzpet_app/fuzzpet/state.txt`
- Manager: `pieces/apps/fuzzpet_app/manager/state.txt`
- Actions: `pieces/apps/playrm/ops/src/fuzzpet_action.c`
- Zombie AI: `traits/zombie_ai.c`
- LLM Bridge: `projects/groq-ollama/ops/src/groq-ollama_bridge.c`
- Relay Pattern: `LLM-HOUSE-PILOT.md` (WSR implementation)
