# WSR-PAL Features Implemented - 2026-07-17

## What's Done

### 1. DB Search (Option 28) - COMPLETE ✅
- **Piece**: `projects/wsr-pal/pieces/wsr_search_menu/piece.pdl`
  - 15 menu options for criteria adjustment
  - Option 14: Display Results (calls display_search_results.+x)
  - Option 15: Clear Criteria (resets to defaults)
  - Back option returns to main menu

- **Operations**:
  - `ops/display_search_results.c` (21K) - Scans all corps, applies filters (market cap range, price/book %), displays matching stocks
  - `ops/wsr_search_input.c` (21K) - Manages search criteria state, supports criterion adjustment

- **Menu Wiring**: Option 28 `GOTO:wsr_search_menu` ✅

### 2. List Portfolio (Option 24) - COMPLETE ✅
- **Operation**: `ops/player_list_portfolio.c` (21K)
  - Reads transaction log (TICKER|ACTION|SHARES|PRICE|DATE format)
  - Calculates holdings, average cost basis, current value
  - Shows: Ticker | Shares | Avg Cost | Current Price | Gain/Loss $ | Gain/Loss %
  - Displays portfolio totals

- **Dependency**: Transaction logging in player_trade.c

- **Menu Wiring**: Option 24 `RUN:./ops/+x/player_list_portfolio.+x` ✅

### 3. Transaction Logging - COMPLETE ✅
- **Modified**: `ops/player_trade.c`
  - Added `#include <time.h>`
  - Added `log_transaction()` function
  - Logs every buy/sell/short/cover to `player_you/transactions.txt`
  - Format: `TICKER|ACTION|SHARES|PRICE|DATE` (e.g., `ORB|buy|100|50.00|2026-07-17`)
  - 5 calls total (1 per trade type, called after each successful trade)

### 4. Auto-Active Corp (On IPO) - COMPLETE ✅
- **Modified**: `ops/corp_ipo.c`
  - After corp creation, counts total corporations
  - Sets `active_corp_index = corp_count - 1` in `wsr_menu/state.txt`
  - Newly created corps immediately become the active selection
  - No menu navigation needed

## Architecture Notes

**Piece Data Structure**:
- Menu pieces: `projects/wsr-pal/pieces/wsr_*_menu/`
- Game entities: `projects/wsr-pal/pieces/corp_*/ and player_you/`
- Each piece has `piece.pdl` (METHOD table) and `state.txt` (persistent data)

**Menu Navigation**:
- `GOTO:piece_id` changes active_menu_piece in state.txt
- `RUN:./ops/+x/op.+x` executes operations (first line of output shown as message)
- File-based state persisted between game ticks

**Transaction Log**:
- Append-only `player_you/transactions.txt`
- Format enables cost-basis calculation for portfolio listings
- Supports long/short positions via negative share counts

## What's NOT Done (Can Wait)

- **Research Report (Option 23)**: screener.c - Shows fundamental value analysis (uses complex ported formula)
- **Screen rendering optimization**: Currently renders every tick; should wait for state changes

## Testing

Compile: `bash button.sh compile`

Check ops:
```bash
ls -lh ops/+x/{display_search_results,wsr_search_input,player_list_portfolio}.+x
```

Verify pieces:
```bash
cat projects/wsr-pal/pieces/wsr_search_menu/piece.pdl
grep -n "DB Search\|List Portfolio" projects/wsr-pal/pieces/wsr_main_menu/piece.pdl
```

## Backup

Final stable backup: `wsr-pal-features-complete-j17.zip` (660K)
