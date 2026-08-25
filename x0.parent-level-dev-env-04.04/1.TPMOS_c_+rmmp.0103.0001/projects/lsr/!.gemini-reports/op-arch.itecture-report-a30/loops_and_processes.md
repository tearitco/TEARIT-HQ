# Simulation Loops & Financial Processes

## The Temporal Hierarchy
The simulation progresses through nested loops, each with a specific responsibility.

### 1. The Clockwise Loop (`clockwise_loop.c`)
The top-level scheduler. It monitors the `wsr_clock.txt` and executes specific binaries when time thresholds are crossed.

### 2. The Hourly Loop (`hour_loop.c`)
- **Focus:** High-frequency volatility.
- **Actions:** Triggers "Breaking News" and minor stock price adjustments.
- *Status:* Mostly functional, needs more news variety.

### 3. The Daily Loop (`day_loop.c`)
- **Focus:** General economic progression.
- **Actions:** 
  - Triggers `analysis_loop.c` for price updates.
  - Updates `data/day.txt`.
  - Generates the daily newspaper (`news_loop.c`).
- *Status:* Core logic present, but needs integration with the news engine.

### 4. The News Loop (`news_loop.c`)
- **Focus:** Narrative generation.
- **Actions:** Selects templates from `data/news.txt` and fills them with entity data to create an immersive environment.
- *Status:* Basic templating works; needs AI-driven variety.

### 5. Financial Sub-Loops (The "Placeholders")
Currently, several critical loops are copies of the main game logic and require implementation:
- **Payroll Loop (`payroll_loop.c`):** Needs to calculate and deduct wages from entity accounts.
- **Dividend Loop (`dividend_loop.c`):** Needs to distribute profits to shareholders recorded in the ledger.
- **Tax Loop (`tax_loop.c`):** Needs to calculate government revenue based on corporate earnings.

## Market Analysis (`analysis_loop.c`)
This is the core "AI" of the game. It uses a series of weights and random walks modified by news events to simulate a realistic market.
- **Inputs:** Current stock price, news sentiment, industry trends.
- **Outputs:** New stock price, volume, and updated entity health metrics.
