# PROGRESS

Dated log of verified work. Every claim here was measured, not assumed; the
evidence is in the "Evidence" column. If something is listed as not done, it
was checked and found absent.

---

## 2026-09-29 — Shareholder registry and the wall clock

### Shareholder registry — DONE and verified

| # | Change | File | Evidence |
|---|---|---|---|
| 1 | Registry op, sole writer of `shareholders.txt` and sole dividend money path | `ops/shareholder_registry.c` | registered in `default_op.txt`; `corp_apply_finances.c` rewired to call it |
| 2 | Cent-exact allocation, residual cents to largest positions, shorts pay in | `ops/shareholder_registry.c` | see the money table below |
| 3 | Accepts bare ticker and `corp_<TICKER>` keys; nets duplicate rows; drops zero shares | `ops/shareholder_registry.c` | scans every piece dir, derives holder kind from piece ID |
| 4 | `owned_by` excluded from payouts (metadata, no share count) | `ops/shareholder_registry.c` | 100 long vs −30 short conserves $1,000 exactly |
| 5 | Unheld corporation retains cash, `dividend WITHHELD` | `ops/shareholder_registry.c` | leak regression passes |
| 6 | Unaffordable payout reports `dividend REFUSED` | `ops/shareholder_registry.c` | verified |
| 7 | Per-corp audit written to `pieces/<corp>/dividends.txt` | `ops/shareholder_registry.c` | live UI run for `AFL` |
| 8 | Nav 26 `STUB` → `Shareholder List` | `pieces/wsr_main_menu/piece.pdl` | `RUN:` must be 3rd field on the PDL line |
| 9 | `summary` mode added (line-based menu state can't hold a table) | `ops/shareholder_registry.c` | verified live |

Money, measured:

| case | result |
|---|---|
| $100 across 100/25/10 shares | $74.07 / $18.52 / $7.41 |
| $1 across three 1-share holders | $0.34 / $0.33 / $0.33 |
| 100 long vs −30 short, $1,000 | conserved exactly |
| end-to-end: $1,000 − $50 R&D, 10% dividend | $95 paid, corp left $855, split $571.25 / $223.75 |

All test-state mutations were reverted; the generated `corp_AFL/dividends.txt`
was deleted.

### Wall clock — DONE and verified

`ops/econ_calendar.c` is a port of `MSR-DEPRACATED/wsr_clock.c` +
`presets/schedule.txt`, **not** the turn/quarter calendar the earlier docs
described. That design was abandoned mid-flight because it is not the original.
See `PORT-FIDELITY.md` §3 for the full rationale and the gap list.

| # | Property | Evidence |
|---|---|---|
| 1 | Epoch `2025-01-01 00:00:00.00`, 30-day months, original cascade | `wsr_clock.c:36-44`, `:120`; transcribed nesting-for-nesting |
| 2 | Colon-delimited persistence, `data/wsr_clock.txt` | matches the original's clock file, not `state.txt`'s `key=value` |
| 3 | Real elapsed ms × `data/setting.txt:ticker_speed` | original's rate constants carried over as-is |
| 4 | `data/schedule.txt` ported verbatim, all six rows | `1_hour`/`1_day`/`3_months`×3/`1_year` |
| 5 | SIGINT/SIGTERM save + `wsr_clock.pid`, via `GetCurrentProcessId()` | PID file `10848` matched the live process |
| 6 | Modes `run`/`show`/`schedule`/`due`/`step <ms>`/`set-speed` | all exercised |

Measured behaviour:

- Daemon advanced 1 game second per real second at `hour` speed (2.75s at t=3s,
  6.76s at t=7s).
- `step 86400000` → exactly +1 day: `2025-01-01` → `2025-01-02 00:05:18.60`.
- `step 7776000000` (90 real days) → `2025-04-03`, correct under 30-day months,
  and the three `3_months` rows fired while `1_year` correctly stayed silent.
- `set-speed min` then `hour`; `set-speed bogus` rejected with the valid set.
- Unbuilt ops reported `SKIPPED <op>: not built` — the honest signal, since
  every consumer in the schedule is a stub in the original too.

**A real bug found and fixed during this verification:** `read_last_run()`
fell back to *now* for a row with no stamp file, so `elapsed` was always 0 and
no row could ever fire — and because the stamp was only written *after* a row
ran, no stamp file was ever created. The schedule would have silently never
fired, forever. Fixed by arming the stamp on first observation
(`ARMED <cmd>: first observation, not run yet`), which keeps the intended
"don't fire everything on tick one" behaviour without the deadlock. Re-verified:
6 rows armed, then silence, then correct firing on the interval.

**Launch wiring.** The clock is a daemon started by `button.ps1` / `button.sh`
alongside the orchestrator, and `tick_all.sh` / `tick_all.ps1` explicitly do
*not* advance it. Two drivers previously called
`econ_calendar.+x advance 1`, an interface that no longer exists — that call
was removed from both rather than left broken. It is launched via
`cmd /c`, not `Start-Process`, because this MinGW builds ops to `<name>.+x`
without appending `.exe`.

### Documentation

Added `docs/PORT-FIDELITY.md` — a fidelity audit of the port against the
original, verified by reading the original's source rather than inferred from
names. It records what is faithful (the stock-price formula is an exact
constant-for-constant match), the 10 known gaps with citations, and which gaps
are missing in the *original* rather than regressions (no taxes, no working
dividend/payroll loops, no `salary_loop.c` at all).

---

## 2026-09-29 — Windows bring-up and the relay

### Done and verified

| # | Change | File | Evidence |
|---|---|---|---|
| 1 | Restored a working `system/prisc+x.c` for Windows op spawning | `system/prisc+x.c` | **CORRECTION:** this was never "missing entirely". The canonical file is tracked at `44.xyz.01.00/&.widgits/_shared-lib/system/prisc+x.c` and the house `.gitignore` (lines 113-116) deliberately excludes per-project copies. What was actually true: an untracked local copy carried a 28-line additive Windows quoting fix that canonical lacks. See `KNOWN-ISSUES.md` |
| 2 | Op-spawn quoting | `system/prisc+x.c` | standalone loop: `history_cursor` 0 -> 1, **empty stderr** (was 12x `The filename, directory name, or volume label syntax is incorrect.`) |
| 3 | `sim-key` stdout/stderr redirect | `button.ps1:196` | PowerShell was refusing the command outright; now runs, exit 0 |
| 4 | Op path forward slashes | `ops/wsr_menu_input.c` | `ops/+x/...` -> `'ops' is not recognized`; `ops\+x\...` -> op runs |
| 5 | POSIX `$(bash ...)` substitution | `ops/wsr_menu_input.c` | active corp now resolved in C on Windows; Linux path untouched |
| 6 | Equity trade path end to end | verified via relay | see the round trip below |

**The two-relay discovery.** `orchestrator.c:420-436` launches
`chtpm_parser_pal` and never `prisc+x`. The default-mode relay is
`pieces/keyboard/history.txt` with `KEY_PRESSED: N`; the pal-mode relay is
`pieces/apps/player_app/interact_relay.txt` with bare ints. Two misdiagnoses
came from writing the wrong one. Full detail in `DRIVING.md`.

### The trade round trip that proves the chain

Driven through the real chtpm UI via the relay, `AFL` at `$115.04`:

| action | cash | holdings | tx lines | frame said |
|---|---|---|---|---|
| start | 1038.30 | `AFL\|55` | 2 | |
| Sell 10 | 2188.70 | `AFL\|45` | 3 | `Sold 10 shares of AFL at $115.04 (proceeds $1150.40)` |
| Buy 10 | 1038.30 | `AFL\|55` | 4 | `Bought 10 shares of AFL at $115.04 (cost $1150.40)` |

Cash ties exactly (1038.30 + 1150.40 = 2188.70; - 1150.40 = 1038.30), holdings
decrement and restore, and `transactions.txt` grows by one row per trade.

**Best single proof the op genuinely runs:** attempting an unaffordable buy
produced the op's own rejection in the frame —

    |  > Insufficient cash: need $1150.40, have $1038.30.  |

Before the fix the identical click printed `Ran: Buy 10 shares` whether or not
anything happened, because the frame reports the command it issued and never
checked the op's exit status.

### Misdiagnoses worth recording

Four of them, because each cost real time and each is now a documented trap:

1. **"The relay doesn't dispatch."** It did. I was writing bare ints to the
   pal-mode file during a default-mode run.
2. **"history_cursor never advances."** It does — I was reading `cursor=` from
   `wsr_main_menu/state.txt`, which is the *menu selection* cursor, not the
   relay cursor. The relay cursor is `history_cursor` in
   `player_app/state.txt`, a different file and a different key.
3. **"Buy/Sell has no METHOD row."** It does — in
   `projects/wsr-pal/pieces/wsr_trade_menu/piece.pdl`. I had read the stale
   copy at `pieces/wsr_trade_menu/piece.pdl`, which lists only derivatives and
   loans.
4. **"prisc+x is an old version."** No. All three house copies had filesystem
   mtimes from OneDrive checkout artifacts, which made them look current.
   Git history is the only reliable ordering here: the canonical is 2026-09-09.

### Corrections to earlier claims

- A "434-byte committed wsr `prisc+x.c`" was **git's error message**, not file
  content. The path does not exist in that commit. There is no wsr-specific
  prisc source; the canonical one is the only real one.
- An earlier claim that driving "worked" (cursor 0 -> 1) was **not proven** — it
  had read the menu cursor, not the relay cursor.

---

## Still not done

Checked and confirmed absent, not assumed:

- **No payroll in the ops.** Zero matches for payroll/wage/employee in
  `corp_apply_finances.c`. It exists in `SOCIETY-ECONOMY-ARCHITECTURE.txt` only.
  (The shareholder registry is no longer on this list — see the section above.)
- **The clock has no consumers.** `econ_calendar` runs, but every op its
  schedule points at is a stub, so the macro layer still has no periodic
  effect. `ROADMAP.md` 2.2-2.5.
- **The finance pass is not on the clock yet.** The clock is wall-clock
  driven while `corp_apply_finances`/`corp_update_price` still run per End
  Turn. Those are different cadences; re-sequencing is `ROADMAP.md` 2.6,
  tracked explicitly rather than folded into the clock port.
- **Prices update on End Turn, not daily** — the single largest unfaithful
  divergence. `PORT-FIDELITY.md` gap 1.
- **22 of 31 main-menu methods are `STUB`.**
- **Player trades move no price.** `player_trade.c` deliberately leaves
  `stock_price` alone; only `corp_update_price.c` moves it, once per End Turn.
  No order book, no price impact. Flagged in the source as a v1 simplification.

---

## Uncommitted

Changes to this tree are **not yet committed**. The main repo checkout has
300+ live runtime-state modifications belonging to other agents, so committing a
sweep here is unsafe without scoping carefully to only these paths.

Two commits live on a separate branch and are NOT part of this tree:
`kilo-mars-win32` (`f66691d6`, `dce0715e`, `b6c59c5ed`) — legacy k32 Windows
port work, superseded by this migration.
