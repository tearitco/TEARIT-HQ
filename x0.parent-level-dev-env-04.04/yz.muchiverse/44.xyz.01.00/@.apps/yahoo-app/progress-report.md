# Yahoo-App Broker-Sim — Progress Report

Date: 2026-08-05
Goal: in-app Yahoo Finance broker-sim trading screen (bank -> broker_select ->
broker.chtpm -> trade commands), verified headless via the k3 flow test.

## Done this session

### 1. Root cause of the action blocker — found and fixed
`yahoo_compose_frame.c`'s `read_kv_str_local` did **not** strip the trailing
`\r\n` (unlike every other fixed op). `user_hash=` in config.txt therefore read
back as `"\n"` — non-empty — so the hash-generation branch never fired and no
account file was ever created. Every money/data op aborted with
"No user account yet".

- Fixed `ops/yahoo_compose_frame.c` read-KV to `v[strcspn(v, "\r\n")] = '\0'`.
- Same fix applied to `ops/deposit_withdraw.c` (app + widget copies).
- Seeded `broker_balance=0.00` in the broker idle-sync (frame previously showed
  an empty broker balance).

### 2. House auditability standard — master ledger (implemented)
Answer to "is this project using a master ledger?": **No** — transactions were
only recorded per-user inside `usr_acc.<hash>.txt` (a CSV `history` section).
Per `!.HOUSE_STDS.md` + `#.DOX/LEDGER_ADVANCED_STUDY.txt`, implemented the
append-only audit trail using the reference in `101.ledger-player-npc-simple+3`:

- Copied the generic `ops/ledger_append.c` (auto-increments `turn` from line
  count; format `timestamp|epoch|player|turn|word|action_type`).
- Created `projects/yahoo-app/data/master_ledger.txt` with the house header.
- Registered `ledger_append` in `scripts/build.sh` + `default_op.txt`.
- Wired appends into every money-moving op (run from project-root CWD):
  - `add_credit`          -> `add_credit:<amt>:<new_bal>` / `add_credit`
  - `buy_stock`           -> `buy:<SYM>:<shares>:<price>:<bal>` / `buy`
  - `sell_stock`          -> `sell:<SYM>:<shares>:<price>:<bal>` / `sell`
  - `sell_option_inventory`-> `sell_option:<type>:<SYM>:<contracts>:<price>:<bal>` / `sell_option`
  - `deposit_withdraw`    -> `deposit:<amt>:broker_balance:<bal>` / `deposit`
                           and `withdraw:<amt>:bank_balance:<bal>` / `withdraw`
- Verified `ledger_append` end-to-end: two appends produced
  `...|0|abc123|0|buy:NVDA:10:123.45:5000.00|buy` and `...|abc123|1|...` (turn
  auto-increments).

Deliberately deferred (hard parts of the study): state-reconstruction-by-replay,
checkpoint/rollback, ring turns, P2P sync. Today's scope = append-only audit
trail, matching the house standard format.

### 3. `--debug--` menu item (METHOD 14)
- Added `METHOD | --debug-- | DEBUG_LEDGER` to `pieces/broker/piece.pdl`
  (auto-generates as item 14, between Predict=13 and Back).
- `broker_menu_input.c` `DEBUG_LEDGER` handler: opens
  `data/master_ledger.txt` in gedit when `$DISPLAY` is set; otherwise reports
  the ledger path in the status line (headless-safe).

### 4. LOOKUP price — offline simulated-quote fallback
Real cause of "No price cached": `fetch_stock.c` does raw-socket HTTP to Yahoo,
unreachable in the sandbox, so the `<SYM>.txt` price cache is never written.
Added a deliberate, scoped fallback in `lookup_stock.c`: when the network fetch
fails, generate a deterministic per-symbol **daily** quote (hash-seeded),
write a `read_price`-parseable `<SYM>.txt`, publish it to
`yfin_master_list.txt`, and label it `(simulated, <ts>)`. Real-network path is
untouched.

## State of the k3 flow test
The flow is now **fully green** — all 12 phases pass:

```
SUMMARY: 22 PASS, 0 FAIL
VERDICT: PASS - safe to proceed
```

All 12 phases pass: bank boot, broker_select, broker frame + broker_state seed,
ADD_CREDIT ($100 + account file), WATCHLIST_ADD, CHECK_BALANCE, marker
discipline (broker_screen_changed stable, 4 bytes), LOOKUP (simulated NVDA
price), DEBUG_LEDGER (ledger path reported), master-ledger rows (header +
add_credit), **BUY_STOCK** (top-up $1000, buy 1 NVDA -> ledger `|buy` row +
`stocks,NVDA,1.00` account holding), and **SELL_STOCK** (sell the share ->
ledger `|sell` row + holding removed).

### 5. Last failing phase debugged — `KEY:13` = Enter collision (fixed)
Phase 9 (DEBUG_LEDGER) was the last failure. Root cause chain:

- The broker piece.pdl has **12 METHODs** — more than any mature house app
  (they top out at 9), so this path had never been exercised.
- `render_piece_methods()` assigns buttons `onClick="KEY:<idx>"` where the
  shared counter starts at 2 (`sym_field=0`, `amt_field=1`), so the 12th
  method renders `KEY:13`.
- `send_command("KEY:n")` in `system/chtpm_parser_pal.c` only converted
  single-digit keys to printable:
  `if (k >= 0 && k <= 9) inject_raw_key('0' + k); else inject_raw_key(k);`
  so `KEY:13` relayed **raw 13 = Enter**, which `broker_menu_input.c`
  correctly ignores. Methods 10-12 only worked by luck (raw 10/11/12 ->
  the `keycode > 9 -> keycode-1` fallback); method 12 (KEY:13) collided with
  Enter and never dispatched.
- Fix: extended the printable range to `k <= 13`, so methods 10-13 relay the
  chars `':' ';' '<' '='` — matching the `':'->9 .. '='->12` decode that
  `broker_menu_input.c` already had. Safe: the cli_io form-Enter path calls
  `inject_raw_key(13)` directly (bypasses `send_command`), so form submit is
  untouched; only the broker screen has >= 10 methods.

Debug aid discovered along the way: `active_gui_index.txt` is **1-based**
(indices 1..15 for broker: 1=sym, 2=amt, 3..14=METHOD1..12, 15=Back; wraps
15->1).

### 6. BUY/SELL phases debugged — cli_io buffer resurrection (fixed)
Phases 11-12 (BUY_STOCK/SELL_STOCK) were the last failures: the "bought/sold"
status landed but the ledger/account never changed. Root cause chain:

- `compose_frame()` calls `sync_cli_input_from_gui_state()`
  (chtpm_parser_pal.c) on every render, which re-seeds a cli_io element's
  `input_buffer` from its gui_state value (`sym_input`/`amt_input`).
- The cli_io Enter handler clears the buffer and relies on the one-shot
  `suppress_next_gui_sync` flag, assuming the external consumer clears
  gui_state by the next render. The broker form doesn't: the METHOD op
  (`BUY_STOCK`) only clears `sym_input`/`amt_input` when the method button is
  pressed, many renders after the field's Enter. So the stale value kept
  re-seeding the buffer; re-typing **appended**: 2nd `NVDA` -> `NVDANVDA`,
  1 share -> `10001` -> buy of 10001 shares -> insufficient balance -> no
  ledger/account rows. Confirmed live from a kept session's `cli_buffers.txt`
  (`sNVDANVDA`, `a100011`).
- Fix (two paired changes in `system/chtpm_parser_pal.c`, applied to app +
  widget copies, rebuilt):
  1. cli_io **activation** (Enter on a focused field) now clears the buffer —
     a fresh activation is a fresh typing session. The auto-fill path (main
     loop's `cli_input` check) sets `active_index` directly, NOT through this
     Enter branch, so it is unaffected.
  2. `sync_cli_input_from_gui_state()` now **skips the currently-active
     element** — the buffer is authoritative while typing (each keystroke
     already mirrors it to gui_state live), so a stale value can never
     resurrect a cleared buffer. After ESC the sync re-seeds from gui_state,
     so an inactive field still displays its last committed value.
- Also made the test ledger-hygienic: `k3_flow.sh` now uses a per-run
  **scratch** `data/master_ledger.txt` instead of symlinking the real app
  `data/` — before, the append-only master ledger accumulated test rows and
  phase 11's `|buy$` check passed spuriously off a leftover manual row.

## Next steps
1. **Widget parity (`&.widgits/yahoo-broker`)** — DONE (see below).
2. Publish the frame report and confirm the widget build stays green.
3. (Deferred) widget-side k3 coverage of BUY/SELL + `--debug--` survival; widget
   ops remain on the legacy 5-method dispatch while app uses the 12-method key
   relay.

## Widget parity — done
- Copied `ops/ledger_append.c` into `&.widgits/yahoo-broker/ops/`; registered in
  its `scripts/build.sh` (gcc line) + `default_op.txt`.
- Copied the app's ledger-wired money ops over the widget copies (they were
  byte-identical except the ledger blocks): `add_credit`, `buy_stock`,
  `sell_stock`, `sell_option_inventory`, `deposit_withdraw`.
- Added `DEBUG_LEDGER` branch to the widget's `broker_menu_input.c` (gedit or
  print path, same as app) + `METHOD | --debug-- | DEBUG_LEDGER` row in
  `pieces/broker_widget/piece.pdl`.
- Symlinked `data/` into widget sessions (`button.sh`) so CWD-relative
  `./+x/ledger_append.+x data/master_ledger.txt` resolves; created
  `data/master_ledger.txt` house header.
- Synced the widget's `system/chtpm_parser_pal.c` `send_command` fix
  (`k <= 13`, same bug class as the app).
- Verified: `bash scripts/build.sh` -> `build ok`; `ledger_append.+x` runs
  end-to-end from a simulated session CWD (turn auto-increments, header kept).

## Files touched this session
- `@.apps/yahoo-app/ops/yahoo_compose_frame.c` — read-KV newline strip (the fix)
- `@.apps/yahoo-app/ops/deposit_withdraw.c` + widget copy — read-KV strip + ledger append
- `@.apps/yahoo-app/ops/ledger_append.c` — new, copied from house reference
- `@.apps/yahoo-app/projects/yahoo-app/data/master_ledger.txt` — new, house header
- `@.apps/yahoo-app/scripts/build.sh` — register ledger_append
- `@.apps/yahoo-app/default_op.txt` — register ledger_append
- `@.apps/yahoo-app/ops/add_credit.c` — ledger append
- `@.apps/yahoo-app/ops/buy_stock.c` — ledger append
- `@.apps/yahoo-app/ops/sell_stock.c` — ledger append
- `@.apps/yahoo-app/ops/sell_option_inventory.c` — ledger append
- `@.apps/yahoo-app/ops/broker_menu_input.c` — `DEBUG_LEDGER` dispatch, broker_balance seed
- `@.apps/yahoo-app/projects/yahoo-app/pieces/broker/piece.pdl` — METHOD 14 `--debug--`
- `@.apps/yahoo-app/ops/lookup_stock.c` — offline simulated-quote fallback
- `@.apps/yahoo-app/system/chtpm_parser_pal.c` — `send_command` KEY:10-13 ->
  printable `':'..'='` (fixes METHOD12 = Enter collision); cli_io buffer
  resurrection fix (clear on activation + sync skips active element)
- `&.widgits/yahoo-broker/system/chtpm_parser_pal.c` — synced both fixes
- `@.apps/yahoo-app/scripts/k3_flow.sh` — printf fixes, "Broker Balance" grep,
  phases 4-12 (nav, DEBUG_LEDGER, ledger rows, BUY/SELL), scratch ledger (no
  longer symlinks the real app data)

---

# Part A — house-login player identity (done, verified)

- `resolve_player_id()` added to `ops/yahoo_compose_frame.c` (app + widget
  identical): reads `pieces/system/house_root.txt` -> `0.user-pal👤️/00.login-signup/current_login.txt`
  `current_user_id`; deterministic FNV-1a 24-bit `user_hash=%06X`.
- Config writes `user_hash` + `player_id`; bank view shows `Player: <id>`.
- Ledger `player` column = human login id (e.g. `jb`) via per-op
  `resolve_player()` in add_credit / buy_stock / sell_stock /
  sell_option_inventory / deposit_withdraw / buy_option.

# Part B — options coverage + k3 to 31/31 (done, verified)

- Broker piece.pdl now **13 METHODS**; `broker_menu_input.c` decodes
  `':'..'>'` (METHODS 9..13); `chtpm_parser_pal.c` maps `KEY:n` (`k<=14`) to
  printable `'0'+k`; `buy_option.c` writes `buy_option:<type>:<sym>:<contracts>:<strike>:<price>`.
- **BUY_OPTIONS handler fixed**: old code called 3-arg `buy_option`
  (hash,sym,index) but the op needs 4 (hash,sym,index,contracts). Now builds
  direct cmd `ops/+x/buy_option.+x <hash> <sym> <index> <contracts>` via
  `run_popen_cwd` (amt `"index[,contracts]"`, contracts default 1) after
  `read_current_price`.
- **Account-file history format unified to 7 fields** with `-` sentinel
  expiration (strtok-safe): `buy_stock`/`sell_stock` writers+readers now emit
  `...,time,-,0.00`.
- **`add_credit.c` full canonical account parser/writer** (balance, watchlist,
  stocks, options, history, last_lookup) — was corrupting files on top-up.
- **`buy_option.c` history loop** now stops at `last_lookup`.
- **SELL_OPTIONS fixed (last failure)**: (1) `sell_option_inventory.c` treated
  `option_index` as 0-based while UI passes 1-based — now 1-based (matches
  buy_option + pricing CSV index), array access via `idx = option_index - 1`;
  (2) it re-priced off the **strike** (`-p <strike>`, giving $0.17) — now reads
  the held option's price from the existing `option_prices.<SYM>.csv` sheet
  (type+strike+expiry match), regenerating only if absent using the current
  underlying from `yfin_master_list.txt`; (3) its binary was silently missing
  from `ops/+x/` (gcc aborted on implicit `strptime`/`toupper` decl warnings
  under `-Werror`-style flags) — builds cleanly now, app + widget synced.
- **k3 now 31 PASS / 0 FAIL** including OPTIONS_PRICING (csv), BUY_OPTIONS
  (ledger + holding), SELL_OPTIONS (ledger + option removed). Offline seed:
  NVDA Yahoo-chart JSON + `yfin_master_list.txt` row (age 0).
- **k3 exit-hang fixed**: orchestrator now runs under `setsid` and cleanup
  kills its whole process group (`kill -9 -- -$ORCH_PID`) instead of pattern
  `pkill`s that interrupted bash's EXIT trap. Full run: 31 PASS in ~9s, no
  leftover processes/sessions.

# Part C — research terminal + chart widget (next)

- Universe fetched/merged: S&P 500 + NASDAQ + NYSE/Arca -> `/tmp/research_universe/universe.txt` (6830 symbols).
- C-1 research data generator (batch-cycle daily refresh, rate-limit/backoff).
- C-2 `&.widgits/yahoo-chart` widget (ranges 10y/5y/2.5y/1y/1mo/1wk).
- C-3 app research-terminal screen (before broker choice).
- C-4 k3 phases for research terminal + chart widget.
