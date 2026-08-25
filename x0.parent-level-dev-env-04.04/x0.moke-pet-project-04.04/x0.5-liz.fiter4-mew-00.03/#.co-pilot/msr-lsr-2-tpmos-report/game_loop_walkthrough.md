# WSR & LSR Game Loop Walkthrough

## 1. The Human Player Loop (Current WSR)
The human player interacts with a real-time terminal interface.

### Step-by-Step Execution:
1.  **Entry:** Player executes the orchestrator (`m.button...`).
2.  **Navigation:** Uses keyboard shortcuts (F1-F4) to navigate menus:
    - **F1 (File):** Save, Load, Exit.
    - **F2 (Settings):** Toggle ticker, Adjust game speed.
    - **F3 (Management):** View corporate health, Adjust strategy.
    - **F4 (Financing):** Startup a new company, Buy/Sell shares.
3.  **Observation:** The player watches the "Market Ticker" and "News Feed" to identify trends.
4.  **Decision:** Player initiates a "Startup" or a "Trade".
5.  **Persistence:** Changes are written to `master_ledger.txt` and entity files in `corporations/generated/`.

---

## 2. The Robot Player Loop (Exo-Bot v5 Framework)
The Exo-Bot does not "see" the terminal; it observes the "Pieces" (data files) and executes "Ops" (standalone binaries).

### Step-by-Step Execution:
1.  **Observation Phase:**
    - The bot reads `data/wsr_clock.txt` to sync its internal FSM.
    - It reads `data/master_ledger.txt` to assess the current wealth of all entities.
2.  **Analysis Phase (Policy Layer):**
    - The bot uses its **RL Weights** (`xo/bot5/pieces/weights/`) to calculate the probability of a "Market Entry" or "Tax Audit".
    - It compares current entity states against its "Imitation Memories".
3.  **Action Phase (Ops Injection):**
    - Instead of simulating keypresses, the bot executes specific binaries directly:
      - `exec("./tax_loop")` to perform a sweep of all corporations.
      - `exec("./analysis_loop")` to force a price update based on its internal strategy.
4.  **Meta-Management:**
    - The bot records the outcome of these ops back into its external memory for future training.

---

## 3. The "Missing Heart" Refactor Strategy
To complete the simulation, we must migrate from "static loops" to "Exo-Ops".

### The Migration Path:
1.  **Modularization:** Move the logic in `tax_loop.c`, `payroll_loop.c`, and `dividend_loop.c` into a shared `xo/ops/` directory.
2.  **TPM Container Model:**
    - Refactor `data/` to use the **LSR Container Hierarchy**: `world/mars/corporations/[TICKER]/state.txt`.
    - This allows the Exo-Bot to "traverse" the world as if it were a directory tree.
3.  **FSM Integration:**
    - Implement a `wsr_manager_bot` that uses an FSM to decide when to "pulse" the economy.
    - **States:** `IDLE` -> `CHECK_DATE` -> `PROCESS_PAYROLL` -> `COLLECT_TAXES` -> `DISTRIBUTE_DIVIDENDS`.

---

## 4. Transition to LSR (Lunar Streetrace Raider)
WSR is the "Earth-Mars" testbed. LSR will be the "Lunar" implementation.

### Key Refactor Points for LSR:
- **From C-Loops to AI-Ops:** The economy shouldn't just be a C program; it should be a series of "Contracts" executed by bots.
- **R&D via `ai-labs`:** Use the `generate_tech` op to create new corporate products dynamically.
- **Unified Contract Model:** Ensure that the `tax_loop` logic can run both in WSR and as a P2P contract in the future LSR network.

---
*This walkthrough provides the blueprint for migrating the current Martian simulation into the autonomous, multi-agent Lunar ecosystem.*
