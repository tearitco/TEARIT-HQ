# Exo-Bot Migration & "Missing Heart" Integration Strategy

## Overview
The "Missing Heart" (Taxes, Payroll, Dividends) is not just a missing feature; it is the first step toward a fully autonomous **Exo-Bot** managed ecosystem. We will refactor the existing WSR infrastructure to align with the **LSR (Lunar Streetrace Raider)** "Op-Based" architecture.

## 1. The Exo-Bot FSM (Finite State Machine)
The manager for the "Missing Heart" will be implemented as an FSM within the Exo-Bot framework.

### FSM States:
- **`STATE_SCAN`**: The bot traverses the `corporations/` and `governments/` containers to identify active entities.
- **`STATE_CALCULATE`**: For each entity, the bot calculates the "Op-Requirement" (How much tax is due? Who needs payroll?).
- **`STATE_EXECUTE_OP`**: The bot calls the corresponding Op (e.g., `payroll_op`, `tax_op`).
- **`STATE_LEARN`**: The bot records the transaction success and updates its **Imitation Weights** to optimize future timing (e.g., learning the best time to tax without causing a market crash).

## 2. Integrating with `@1.tpmos_c/xo/`
We will leverage the existing `xo/` infrastructure to host the WSR manager.

### Implementation Steps:
1.  **Create `xo/wsr_manager/`**: A new bot instance dedicated to the Martian economy.
2.  **Share the `xo/pieces/`**: Use shared RL weights from `bot5` for market sentiment analysis.
3.  **Portable Memory**: Ensure the bot's cognition is stored in its sovereign directory, allowing it to be moved between WSR and LSR instances.

## 3. "Missing Heart" as Exo-Ops
Each financial process will be converted into a standalone **TPM-compliant Op**.

### The Ops Specification:
- **`payroll_op`**:
  - *Input*: Entity Ticker.
  - *Process*: Read `state.txt` -> Deduct `salary_cost` -> Append to `master_ledger.txt`.
- **`tax_op`**:
  - *Input*: Entity Ticker + Government ID.
  - *Process*: Read `revenue` -> Calculate `tax_rate` -> Transfer funds in `master_ledger.txt`.
- **`dividend_op`**:
  - *Input*: Entity Ticker.
  - *Process*: Check `profit_margin` -> Distribute to `shareholder_list`.

## 4. Road to LSR (Lunar Streetrace Raider)
The final migration to LSR will involve:
- **Containerization**: Moving all entity data into nested TPM containers.
- **AI-Labs R&D**: Enabling the `research_op` where bots use LLMs to generate new corporate technologies.
- **P2P Net**: Allowing different Exo-Bots to "trade" their RL weights and entity data across different simulation instances.

---
**KPI for Success:** A fully autonomous "Heart" that pulses the economy every 30 game-days without human intervention, managed entirely by an Exo-Bot FSM.
