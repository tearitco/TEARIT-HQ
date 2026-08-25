# Moke-Pet: Headless Biological Simulation (v1.0)

## 1. Overview
Moke-Pet is a directory-based reinforcement learning (RL) experiment within the TPMOS ecosystem. It simulates a "Lizard Tank" (Enclosure) where sovereign autonomous entities interact with dynamic objects (food, rocks, and other entities) via standalone C binaries (Ops).

The project prioritizes **headless auditing**, **architectural modularity**, and **biological evolution**.

---

## 2. Core Features

- **Dynamic Resource Discovery**: Lizards use a `scan` Op to perceive the enclosure. They do not have hardcoded food paths; they identify "edibles" by reading `type | food` from sibling `state.txt` files.
- **Sovereign Ops**: Every action (`eat`, `rest`, `scan`, `train`, `breathe`, `check_death`, `attack`, `mate`) is a standalone C binary compiled in the entity's `ops/+x/` directory.
- **Self-Resolving Paths**: Ops dynamically determine their location and the enclosure root via `argv[0]`, allowing for LEGO-like modularity.
- **Reinforcement Learning Loop**: A `train` Op audits epoch logs to award points (`EAT`: +10, `REST`: -1) and updates sovereign `weights.txt`.
- **Epoch Management**: A C-based world manager orchestrates turn rotation, perceptual phases, and training triggers from `pieces/world_tank_01/`.
- **Metabolism**: Entities `breathe` each turn; `hunger` increases, leading to HP decay if starvation occurs.
- **Predation & Mortality**: `attack` Op to reduce enemy HP. Dead lizards convert to `type | food` (Lizard Corpse), making them part of the food chain.
- **Reproduction**: `mate` Op generates new sovereign entity directories and expands the turn rotation.

---

## 3. Usage

### A. Environment Initialization
Resets the tank, populates food/rocks, and clears stomachs.
```bash
./xo-pet-init.sh
```

### B. Compile Binaries
Builds all entity Ops and the world manager.
```bash
./xo-pet-build-ops.sh
gcc -o pieces/world_tank_01/+x/manager.+x pieces/world_tank_01/manager.c
```

### C. Run Simulation
Executes one epoch (Turn -> Train -> Increment).
```bash
./button.sh auto
```

---

## 4. Architectural Proofs (Test Status)

| Test Component | Status | Verification Proof |
| :--- | :--- | :--- |
| **Directory Scaffolding** | [PASS] | `world_tank_01` hierarchy verified with isolated pet containers. |
| **Dynamic Scan** | [PASS] | `observations.txt` correctly lists sibling piece types. |
| **Ingestion (Eat)** | [PASS] | `mv` command correctly transfers food piece to `stomach/`. |
| **Manager Orchestration** | [PASS] | Sequential turn dispatch for `liz_bulb` and `liz_char` verified via logs. |
| **RL Training** | [PASS] | `weights.txt` successfully increments based on log audit. |
| **Path Sovereignty** | [PASS] | Binaries correctly resolve enclosure root regardless of manager CWD. |
| **Metabolism (Breathe)** | [PASS] | HP decay triggered after hunger threshold (>20) confirmed via stats audit. |
| **Mortality & Predation**| [PASS] | Emergent Behavior: Dead entities convert to `type | food` and are consumed by survivors. |

---

## 5. Directory Mapping (TPMOS Canonical)
```
moke-pet-project-1.0/
├── pieces/
│   └── world_tank_01/          # World Container
│       ├── manager.c           # Turn Orchestrator source
│       ├── +x/
│       │   └── manager.+x      # Turn Orchestrator binary
│       ├── logs/               # Audit Records (epoch_N.txt)
│       └── map_enclosure/      # Map Container
│           ├── liz_bulb/       # Sovereign Entity
│           │   ├── ops/        # C source & +x binaries
│           │   ├── memory/     # weights.txt & observations.txt
│           │   └── stomach/    # Consumed pieces
│           └── pellet_1/       # Dynamic Resource Piece
```

*"If it's not in a file, it's a lie. The lizard breathes because the directory exists."*
