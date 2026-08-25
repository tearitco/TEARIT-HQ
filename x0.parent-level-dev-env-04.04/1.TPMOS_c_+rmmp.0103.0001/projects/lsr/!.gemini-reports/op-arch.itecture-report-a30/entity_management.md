# Entity Management & Lifecycle

## Overview
WSR manages a massive ecosystem of Governments and Corporations. These entities are dynamically generated and evolve over time based on the simulation's rules.

## Governments
- **Source:** Defined in `governments/gov-list_earth.txt` and `governments/mars_geography.txt`.
- **Setup:** `setup_governments.c` initializes the state for these entities.
- **Attributes:** Each government has a financial profile (defined in `presets/financial_profile_template]gov.txt`) including budgets, tax rates, and demographics.

## Corporations
- **Spawning:**
  - **Pre-defined:** `setup_corporations.c` loads initial companies from `corporations/starting_corporations.txt`.
  - **Dynamic:** `setup_corporations_stage2.c` and `corporations/36_industries_wsr.txt` are used to populate the world with hundreds of diverse businesses.
- **Founding:** Players can use the `financing.c` module to "Startup" new corporations. This creates a dedicated file in `corporations/generated/` and records the initial capital in the ledger.
- **Lifecycle:**
  - **Growth:** Driven by `analysis_loop.c` and `day_loop.c`.
  - **Management:** Players can interact with their owned corporations through `management.c`.

## The Master Ledger (`master_ledger.txt`)
All major financial transactions are recorded here.
- **Format:** Chronological entries of capital movements, stock purchases, and tax payments.
- **Processing:** `master_reader.c` provides the logic to aggregate these entries into balance sheets and income statements.

## Identification
Entities are identified by 3 or 4-letter tickers (e.g., AFL, AZN). All data associated with a ticker is stored in a structured directory format within `corporations/generated/[TICKER]/`.
