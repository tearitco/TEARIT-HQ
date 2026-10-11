# FuzzPet LLM Pilot Report
**Date:** 2026-10-10
**Author:** opencode agent

---

## 1. Executive Summary

The FuzzPet digital pet (Fuzzball) lives in the **1.TPMOS_c_+rmmp.0103.0001** framework.
We applied the **relay-driven LLM pilot pattern** (proven on WSR) to make Fuzzball a
self-directed agent. Created `fuzzpet_brain.c` — a solo-shippable C op that reads pet
state, calls Ollama, and writes PET_ACTION to the house relay event bus.

---

## 2. Framework: Does FuzzPet use 1.TPMOS?

**Yes.** FuzzPet is a project within the 1.TPMOS framework.

### Project Structure
```
1.TPMOS_c_+rmmp.0103.0001/
+-- pieces/chtpm/           # Core TPM engine (parser, renderer, keyboard)
+-- pieces/keyboard/       # Input muscle
+-- pieces/display/        # Render muscle
+-- pieces/apps/
|   +-- player_app/        # Unified input buffer
|   +-- playrm/            # Project loader
|   +-- fuzzpet_app/       # The pet app
+-- projects/
|   +-- fuzz-op/           # Pet simulation
|   +-- fuzz-op-gl/        # OpenGL renderer
+-- xo/                    # Dev notes
```

### Key Files
- State: pieces/apps/fuzzpet_app/fuzzpet/state.txt - hunger, happiness, energy, position
- Actions: playrm/ops/src/fuzzpet_action.c - feed/play/sleep/toggle_emoji
- AI Trait: traits/zombie_ai.c - pathfinding toward targets
- Layout: pieces/chtpm/layouts/fuzzpet_v2.chtpm - UI with pet_state vars
- Relay: pieces/apps/player_app/interact_relay.txt - event bus

### Launch Flow
1. button.sh r -> orchestrator.+x -> chtpm_parser.+x
2. Parser renders menu -> user selects fuzzpet
3. fuzz-op_manager.c takes over -> reads history.txt, calls ops
4. Manager updates state.txt -> writes frame_changed.txt pulse
5. Renderer reads state -> writes current_frame.txt

---

## 3. Is It Learning?

### Before: No Learning
- zombie_ai.c: Hardcoded A->B pathfinding, no adaptation
- fuzzpet_action.c: Fixed effect values (feed = hunger-20)
- No memory analysis of pet behavior patterns
- No evolution of trait thresholds

### After: Limited Learning
fuzzpet_brain.c adds:
- LLM observation: Reads full state + history + relay events
- Context-aware decisions: Goal-conditioned (survive/explore/evolve)
- Need-based guardrails: Hunger>60 must feed, happiness<20 must play
- Relay feedback: All actions recorded to interact_relay.txt

### Missing for True Learning
1. wsr_evolve.c integration to mutate trait thresholds
2. tom_layer.+x to track pet's beliefs about the player
3. A fitness function scoring pet wellness over time

---

## 4. What Was Built

### fuzzpet_brain.c

**Location:** pieces/apps/fuzzpet_app/traits/fuzzpet_brain.c

Flow:
  state.txt + history.txt + last_response
    -> Build system + user prompts
    -> curl to Ollama (gemma3:1b)
    -> Parse JSON -> action / reason / confidence
    -> Need-based override (hunger>60 -> feed)
    -> PET_ACTION|action|conf|reason -> relay
    -> Update last_response.txt + history.txt

**Key design:**
- Standalone (no model_api dependency -- solo-shippable)
- Need-based guardrails for small model reliability
- Same relay event bus pattern as WSR
- Writes PET_ACTION|action|conf|reason|pet=name|hunger=N|happiness=N|energy=N

**Compile:**
    CFLAGS="-Wall -Wextra -O2 -w"
    gcc $CFLAGS traits/fuzzpet_brain.c -o traits/+x/fuzzpet_brain.+x -lm

**Run:**
    export PRISC_PROJECT_ROOT="$PWD"
    ops/+x/fuzzpet_brain.+x . survive gemma3:1b
    # Output: {"action":"feed","reason":"Hunger critical (70/100)","confidence":0.90}

---

## 5. Push vs Pull Claude

The house has two LLM bridges:
- projects/groq-ollama/ops/src/groq-ollama_bridge.c
- #.Z.HUMAN_LLM/3.stage.llm.tomom (Claude Sonnet system)

**Push** (Claude generates pet actions):
    claude -p "You are Fuzzball. hunger=70, happiness=55, energy=80. What do you do?"
      | jq '.action' -> PET_ACTION to relay

**Pull** (Claude analyzes pet state):
    claude -p "Analyze Fuzzball's state. Urgent need?"
      < pieces/apps/fuzzpet_app/fuzzpet/state.txt
      < pieces/apps/player_app/interact_relay.txt

---

## 6. Next Steps

| Task | Priority | Description |
|------|----------|-------------|
| fuzzpet_fsm.c | HIGH | Map PET_ACTION to action handler calls |
| run_fuzzpet_agent.sh | HIGH | Automation loop for self-directed pet |
| fuzzpet_evolve.sh | MED | Integrate wsr_evolve for trait mutation |
| fuzzpet_fitness.c | MED | Score pet wellness over time |
| pet_tom_layer.c | LOW | Track pet-player relationship beliefs |

### Pipeline Vision
    fuzzpet_brain.+x -> PET_ACTION to relay
    fuzzpet_fsm.+x -> Maps action -> fuzzpet_action.+x
    zombie_ai.+x -> Pathfinding (reads relay)
    wsr_evolve.+x -> Evolve trait thresholds
    tom_layer.+x -> Track pet-player beliefs
    wsr_fitness.+x -> Score wellness

---

## 7. References

- Codebase map: xo/!.gem-xo=00.03/!.CODEBASE_MAP.txt
- TPMOS onboarding: #.docs/01-ONBOARDING/AGENT_ONBOARDING.txt
- Relay pattern (WSR): WSR_PAL-PREFERRED/LLM-HOUSE-PILOT.md
- Pitfalls: #.docs/05-PITFALLS/ directory
