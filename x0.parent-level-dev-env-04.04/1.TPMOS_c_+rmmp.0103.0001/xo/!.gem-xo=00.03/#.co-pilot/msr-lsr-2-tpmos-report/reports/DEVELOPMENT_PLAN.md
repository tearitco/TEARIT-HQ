# WSR Development Plan: The Road to v1.0

## Phase 1: Establishing the Economic Metabolism (Weeks 1-2)
**Goal:** Make the money flow.
- [ ] **Task 1.1:** Refactor `tax_loop.c` to read `master_ledger.txt`, calculate corporate tax, and append payment entries.
- [ ] **Task 1.2:** Refactor `payroll_loop.c` to deduct monthly wages based on entity size.
- [ ] **Task 1.3:** Refactor `dividend_loop.c` to distribute a portion of profits to shareholders.
- **Verification:** Run the simulation for 1 game-year and verify that government accounts grow and corporate accounts show periodic deductions in the ledger.

## Phase 2: Reactive Market & News (Weeks 3-4)
**Goal:** Create a world that reacts to events.
- [ ] **Task 2.1:** Enhance `news_loop.c` with a template system that dynamically inserts company and government names.
- [ ] **Task 2.2:** Modify `analysis_loop.c` to check `news.txt` for recent events and apply sentiment-based modifiers to stock prices.
- **Verification:** Trigger a "Bad News" event for a specific ticker and observe a corresponding dip in its price.

## Phase 3: Advanced Player Management (Weeks 5-6)
**Goal:** Give the player real control.
- [ ] **Task 3.1:** Implement the "Set Strategy" feature in `management.c` (e.g., Growth, Dividend, R&D).
- [ ] **Task 3.2:** Enable "Secondary Offerings" in `financing.c` for companies needing a capital injection.
- **Verification:** Change a company's strategy and verify it affects its dividend payout or price volatility.

## Phase 4: Visualization & Polish (Weeks 7-8)
**Goal:** Make it look like a pro tool.
- [ ] **Task 4.1:** Implement an ASCII chart renderer in `display_portfolio.c` for stock price history.
- [ ] **Task 4.2:** Clean up the UI, ensuring consistent color-coding (Green for profit, Red for loss).
- **Verification:** Visual review of the dashboard.

---
**Total Estimated Effort:** ~2 months of focused development.
**Next Step:** User approval of this plan to begin Phase 1.
