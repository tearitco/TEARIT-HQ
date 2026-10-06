# XOD Roadmap — Multi-Agent WSR Driving
*October 5, 2026 — infra roadmap and implementation status*

---

## 🎯 Vision
FSM drives WSR through its native relay — no APIs, no shims, just keyboard events.
Behaviors are composable, chainable, trainable, and scored by GOAP against an attrition model.
Multiple agents run isolated sessions. Harnesses chain. Everything records.

---

## ✅ What Was Built

### Phase 1 — Session Isolation
- `WSR_PAL-PREFERED/button.sh` — session isolation (xyzos-standards §23)
- Each `./button.sh run` creates `pieces/sessions/<timestamp>-<pid>/`
- Isolated state: keyboard, display, game state, event bus per agent
- `--no-session` flag for backward compatibility

### Phase 2 — Behavior Bank
- `ops/xod/wsr_fsm_driver.c` — unified C op for all behaviors
- Maps real piece.pdl rows to key injections
- Reads `current_frame.txt` + `current_layout.txt` to decide actions
- Emits `KEY_PRESSED` codes to `keyboard/history.txt`
- `behaviors/real.behaviors` — 13 behavior configs (YAML)

### Phase 3 — GOAP + Attrition
- `ops/xod/wsr_goap_planner.c` — A* planner over behavior graph
- `ops/xod/wsr_attrition.c` — resource degradation (cash, attention, morale, time)
- Scoring: `score = goal_value - attrition_cost - behavior_cost_sum`

### Phase 4 — Event Bus
- `events/xod/schema.json` — event schema (8 event types)
- All components write to `pieces/apps/player_app/interact_relay.txt`
- Events: KEY_INJECTED, BEHAVIOR_START, BEHAVIOR_COMPLETE, ATTRITION_TICK, GOAL_ADOPTED, PLAN_STEP, HARNESS_DONE, EVOLVE, FITNESS

### Phase 5 — Training Loop + Evolution
- `ops/xod/wsr_evolve.c` — selection, mutation, crossover
- `train_xod.sh` — evolutionary training loop (generations × scenarios)
- `tournament_xod.sh` — multi-agent tournament (isolated sessions, fitness tracking, winner declared)

### Phase 6 — Multi-Agent Tournament
- `tournament_xod.sh` — 3 agents, 2 generations, 2 scenarios each
- Winner declared based on fitness score
- All sessions isolated, no conflicts

### Phase 7 — Real Fitness + Dashboard (NEW)
- `ops/xod/wsr_fitness.c` — reads real `corp_ORB/state.txt`, computes portfolio fitness
- `dashboard_xod.sh` — real-time CLI dashboard (ANSI colors, agent standings, event stream)
- `x11_dashboard_xod.sh` — X11 window wrapper (xterm)
- `harnesses/xod/presentation.sh` — proof video capture harness
- `live_tournament_xod.sh` — tournament against live WSR instances

---

## 📁 File Structure

```
WSR_PAL-PREFERED/
├── ops/xod/
│   ├── wsr_fsm_driver.c      # Unified FSM driver
│   ├── wsr_goap_planner.c    # GOAP A* planner
│   ├── wsr_attrition.c       # Attrition model
│   ├── wsr_evolve.c          # Behavior evolution
│   └── wsr_fitness.c         # Real fitness scoring
├── ops/+x/                   # Compiled binaries
│   ├── wsr_fsm_driver.+x
│   ├── wsr_goap_planner.+x
│   ├── wsr_attrition.+x
│   ├── wsr_evolve.+x
│   └── wsr_fitness.+x
├── pal/xod/
│   ├── wsr-fsm_driver.pal    # FSM dispatcher
│   ├── goap_plan.pal         # GOAP executor
│   ├── attrition_tick.pal    # Attrition ticker
│   └── training_loop.pal     # Training loop
├── behaviors/
│   └── real.behaviors        # 13 behavior configs
├── events/xod/
│   └── schema.json           # Event bus schema
├── harnesses/xod/
│   ├── setup_new_game.sh     # Harness A
│   ├── drive_fsm.sh          # Harness B
│   ├── verify_state.sh       # Harness C
│   ├── presentation.sh       # Proof video capture
│   └── run_chain.sh          # Chain runner
├── train_xod.sh              # Training loop runner
├── tournament_xod.sh         # Multi-agent tournament
├── live_tournament_xod.sh    # Live WSR tournament
├── dashboard_xod.sh          # CLI dashboard
├── x11_dashboard_xod.sh      # X11 dashboard
└── button.sh                 # Session isolation (updated Oct 5)
```

---

## 🚀 How to Run

### Quick Test (mock data)
```bash
cd WSR_PAL-PREFERED
bash test_xod_pipeline.sh
```

### Training Loop
```bash
cd WSR_PAL-PREFERED
bash train_xod.sh 10 5    # 10 generations, 5 scenarios each
```

### Tournament (mock)
```bash
cd WSR_PAL-PREFERED
bash tournament_xod.sh 4 5    # 4 agents, 5 generations
```

### Dashboard
```bash
cd WSR_PAL-PREFERED
bash dashboard_xod.sh          # CLI dashboard
bash x11_dashboard_xod.sh      # X11 window
```

### Live Tournament (real WSR)
```bash
cd WSR_PAL-PREFERED
bash live_tournament_xod.sh 2 60    # 2 agents, 60 seconds
```

---

## ➡️ Next Steps

1. **Live WSR integration** — point FSM driver at running WSR session (real `current_frame.txt` updates)
2. **Expand Behavior Bank** — add financing/management/derivatives behaviors that map to real piece.pdl rows
3. **Real fitness scoring** — replace mock fitness with `wsr_fitness.+x` reading live `corp_ORB/state.txt`
4. **Presentation Harness** — wire `make_presentation_video.py` into harness chain for auto proof videos
5. **PAL dispatcher refinement** — replace stub `pal/xod/*.pal` with real prisc+x loops calling compiled `+x` ops
6. **Multi-agent live tournament** — run multiple `./button.sh run` sessions simultaneously, pipe each agent's relay into its own XOD harness

---

## 📚 References

- Session Isolation: `044.pal-chat-irc/button.sh` lines 37-76
- WSR Relay Injection: `WSR_PAL-PREFERED/ops/wsr_menu_input.c`
- WSR PAL Loop: `WSR_PAL-PREFERED/pal/main_loop_chtpm.pal`
- FSM Driver Proof: `dsr_driver.py` (Python proof, ported to C)
- House Pattern: `xyzos-standards.txt §23` (session isolation)

---

*Built on house standards: session isolation, file-mediated P2P, proof-driven development. C ops + PAL only. No Python.*
