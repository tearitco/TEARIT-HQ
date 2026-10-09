# LLM House Pilot — Design Doc

*How gemma3:1b drives the entire WSR house through the relay event bus*

---

## 1. Vision

Use a small local LLM (gemma3:1b) running on the agent's machine to pilot the
house — the full WSR ecosystem (game, economy, population, news, weather,
corporations) — from its own account, coordinating all subsystems through a
single file-mediated event bus (`interact_relay.txt`).

No external APIs, no shims, no Python. C ops + PAL loops only.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     RELAY EVENT BUS                          │
│                                                             │
│  pieces/apps/player_app/interact_relay.txt                  │
│  (append-only, line-delimited events)                       │
│                                                             │
│  LLM_DECISION|action|confidence|reason                      │
│  KEY_INJECTED|row|action                                      │
│  FSM_STATE|executing|action=...|row=...                     │
│  BEHAVIOR_START|behavior                                     │
│  BEHAVIOR_COMPLETE|behavior|ok                              │
│  CORP_EVENT|corp|ticker|price|volume                        │
│  POP_TICK|population|growth_rate                            │
│  ECON_EVENT|sector|impact|duration                          │
│  NEWS_EVENT|headline|category|impact                        │
│  ...                                                         │
└─────────────────────────────────────────────────────────────┘
   ↑           ↑           ↑           ↑           ↑
   |           |           |           |           |
+------+   +------+   +------+   +------+   +------+
|LLM   |   |FSM   |   |TOM   |   |Ticker|   |Fitness|
|Brain |   |Ctrl  |   |Layer |   |Ops   |   |Scorer |
+------+   +------+   +------+   +------+   +------+
   |           |           |           |           |
   v           v           v           v           v
   wsr_menu   keyboard    beliefs   corp/gov/  portfolio
   input      injection   tracking  econ ticks value
```

### Core Flow

1. **LLM Decision** — `llm_brain.+x` reads frame/layout/state, calls Ollama,
   writes `LLM_DECISION|action|conf|reason` to relay
2. **FSM Dispatch** — `fsm_controller.+x` reads last decision, maps to menu row,
   writes `KEY_INJECTED|row|action` + `FSM_STATE|executing|...` to relay
3. **Execution** — `wsr_menu_input.+x` reads `KEY_INJECTED` from relay, injects
   keys into `keyboard/history.txt`
4. **Macro Ticks** — `day_loop.+x` emits `CORP_EVENT`, `POP_TICK`, `ECON_EVENT`
   to relay each turn
5. **TOM Layer** — `tom_layer.+x` observes relay, updates multi-agent beliefs
6. **Fitness** — `wsr_fitness.+x` reads player portfolio, writes fitness

---

## 3. LLM Decision Pipeline

### 3.1 Goal Conditioning

The LLM receives a goal that shapes its decision strategy:

| Goal | Strategy | Preferred Actions |
|------|----------|-------------------|
| `survive` | Preserve cash, avoid risk | `end_turn`, `wait`, `check_market` |
| `accumulate` | Maximize holdings | `buy_stock`, `sell_stock` |
| `grow` | Expand & diversify | `cycle_corp`, `new_game`, `buy_sell` |

### 3.2 Fallback Policy

Small models (gemma3:1b) exhibit predictable biases. A C-level override layer
in `llm_brain.c` corrects when the model returns a goal-inappropriate action:

```c
// In llm_brain.c, after model_extract_action():
if (goal == "survive" && action != "end_turn" && action != "wait" && action != "check_market") {
    // Override: force end_turn
}
if (goal == "accumulate" && action != "buy_stock" && action != "sell_stock") {
    // Override: force buy_stock
}
if (goal == "grow" && (action == "buy_stock" || action == "end_turn")) {
    // Override: force cycle_corp
}
```

### 3.3 Anti-Loop Detection

Static variables track the last (goal, action) pair. If the model repeats the
same decision, the override layer diversifies:

```c
static const char *last_goal = NULL, *last_action = NULL;
if (last_goal && strcmp(last_goal, goal) == 0 &&
    last_action && strcmp(last_action, action) == 0) {
    override = 1;
}
```

---

## 4. Relay Event Vocabulary

All events are line-delimited, pipe-separated:

| Event | Format | Source |
|-------|--------|--------|
| `LLM_DECISION` | `LLM_DECISION\|action\|conf\|reason` | `llm_brain.+x` |
| `KEY_INJECTED` | `KEY_INJECTED\|row\|action` | `fsm_controller.+x` |
| `FSM_STATE` | `FSM_STATE\|executing\|action=...\|row=...\|conf=...\|reason=...` | `fsm_controller.+x` |
| `BEHAVIOR_START` | `BEHAVIOR_START\|behavior` | `wsr_fsm_driver.+x` |
| `BEHAVIOR_COMPLETE` | `BEHAVIOR_COMPLETE\|behavior\|ok` | `wsr_fsm_driver.+x` |
| `GOAL_ADOPTED` | `GOAL_ADOPTED\|goal_id\|desc` | `wsr_goap_planner.+x` |
| `PLAN_STEP` | `PLAN_STEP\|index\|behavior` | `wsr_goap_planner.+x` |
| `TOM_BELIEF` | `TOM_BELIEF\|event_key\|action=...\|goal=...\|conf=...\|age=...` | `tom_layer.+x` |
| `TOM_SUMMARY` | `TOM_SUMMARY\|agents=N` | `tom_layer.+x` |
| `EVOLVE` | `EVOLVE\|gen=N\|best=score` | `wsr_evolve.+x` |
| `FITNESS` | `FITNESS\|score=...\|portfolio=...` | `wsr_fitness.+x` |
| `ATTRITION` | `ATTRITION\|cash=...\|attention=...\|morale=...` | `wsr_attrition.+x` |
| `CORP_TICK` | `CORP_TICK\|corp=...\|price=...\|volume=...` | `corp_tick_idle.+x` |
| `POP_TICK` | `POP_TICK\|pop=...\|growth=...` | `pop_tick_idle.+x` |

---

## 5. Multi-Op Orchestration

### 5.1 Single-Op Chain (LLM → FSM → Input)

```
run_xod_agent.sh:
  1. ops/+x/llm_brain.+x . $GOAL gemma3:1b  →  writes LLM_DECISION to relay
  2. ops/+x/fsm_controller.+x               →  reads relay, writes KEY_INJECTED + FSM_STATE
  3. ops/+x/wsr_menu_input.+x <row>         →  reads KEY_INJECTED, injects to keyboard/history.txt
  4. ops/+x/wsr_compose_frame.+x            →  re-renders frame from state
  5. ops/+x/tom_layer.+x                    →  update TOM beliefs from relay
```

### 5.2 Macroeconomic Tick (Ticker Layer)

```
day_loop.pal:
  1. ops/+x/corp_tick_idle.+x   →  CORP_TICK to relay
  2. ops/+x/pop_tick_idle.+x    →  POP_TICK to relay
  3. ops/+x/gov_decide.+x       →  ECON_EVENT to relay
  4. ops/+x/econ_calendar.+x    →  NEWS_EVENT to relay
  5. ops/+x/weather_tick_idle.+x → WEATHER_TICK to relay
  All read from / write to the shared relay.
```

### 5.3 Tournament Mode

Multiple agents run isolated sessions, each with its own relay:

```
session_001/
  ├── pieces/apps/player_app/interact_relay.txt
  ├── pieces/display/current_frame.txt
  ├── pieces/sessions/<timestamp>/
  └── agent_config.json  (goal, model, behavior bank)

session_002/
  └── (same structure, independent relay)

tournament_xod.sh:
  1. Launch N sessions in parallel
  2. Each agent drives via relay
  3. wsr_fitness.+x scores each session
  4. wsr_evolve.+x selects winners, mutates behavior bank
  5. Repeat for G generations
```

---

## 6. State Files

| File | Purpose |
|------|---------|
| `pieces/display/current_frame.txt` | Screen buffer (rendered frame) |
| `pieces/display/current_layout.txt` | Active layout (wsr_main_menu, etc.) |
| `pieces/display/current_layout.txt` | Active layout name |
| `projects/wsr-pal/pieces/corp_ORB/state.txt` | Corp financial state |
| `projects/wsr-pal/pieces/player_you/state.txt` | Player cash/shares |
| `pieces/apps/player_app/interact_relay.txt` | Event bus (shared relay) |
| `pieces/apps/player_app/history.txt` | Decision history (read by main_loop.pal) |
| `pieces/apps/player_app/tom_state.txt` | TOM multi-agent beliefs |
| `pieces/apps/player_app/plan.txt` | GOAP plan chain |
| `pieces/display/fitness.txt` | Computed fitness score |
| `behaviors/real.behaviors` | Behavior bank (YAML) |
| `behaviors/all.behaviors` | Extended behavior bank |

---

## 7. Implementation Status

### Built & Verified

- `llm_brain.c` — goal-conditioned decisions with fallback policy ✓
- `fsm_controller.c` — single-shot state machine, row emission ✓
- `tom_layer.c` — multi-agent belief tracking ✓
- `wsr_fitness.c` — portfolio-based fitness scoring ✓
- `wsr_chart.c` — HTML chart from event log ✓
- `run_xod_agent.sh` — full pipeline with layout bridge ✓

### On the Roadmap

- `wsr_goap_planner` — wire into pipeline for plan-based execution
- `wsr_evolve` — connect fitness scoring to evolution driver
- `wsr_fsm_driver` — deprecated (replaced by fsm_controller)
- `tournament_xod.sh` — multi-agent tournament harness

---

## 8. How to Run

```bash
export PRISC_PROJECT_ROOT="$PWD"

# Single agent, one decision cycle
ops/+x/llm_brain.+x . survive gemma3:1b
ops/+x/fsm_controller.+x

# Full live pipeline
bash run_xod_agent.sh --live --goal survive --model gemma3:1b --cycles 10
```

---

## 9. References

- House Pattern: `xyzos-standards.txt §23` (session isolation)
- WSR Relay Injection: `ops/wsr_menu_input.c`
- WSR PAL Loop: `pal/main_loop.pal`
- FSM Driver Proof: `ops/xod/fsm_controller.c`
- Behavior Bank: `behaviors/real.behaviors`
- Event Schema: `events/xod/schema.json`
