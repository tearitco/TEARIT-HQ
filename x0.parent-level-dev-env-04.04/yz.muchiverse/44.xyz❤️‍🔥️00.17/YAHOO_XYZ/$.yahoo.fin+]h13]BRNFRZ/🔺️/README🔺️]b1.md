Stock Trading Simulator Documentation
Overview
The Stock Trading Simulator is a command-line application written in C that allows users to manage a virtual stock and options portfolio. Users can look up stock prices, buy/sell stocks and options, view their portfolio and profit/loss, and manage a watchlist. The program uses a file-based user account system (usr_acc.<hash>.txt) and interfaces with external binaries (e.g., lookup_stock.+x, options_pricing.+x) for data retrieval and calculations.
Features

User Management: Generates unique 6-character hexadecimal user hashes (e.g., 2D528E, 3FC6A4) to track accounts.
Menu Options:
Lookup Stock Price: Fetches current stock price (e.g., H at $134.73) and caches results.
Check Balance: Displays current cash balance.
Add Credit: Adds funds to the account (e.g., $39,399.00).
Buy Stock: Purchases shares (e.g., 2 shares of H at $134.73).
Sell Stock: Sells shares with negative share recording for history.
View Portfolio: Shows owned stocks and options.
View Profit/Loss: Displays transaction history and net profit/loss (e.g., buy 2 H, sell 1 H).
Add to Watchlist: Tracks stocks of interest.
Options: Views and trades call/put options for a stock (e.g., H with strike $134.73).
Predict: Not implemented.
Quit: Exits the program.


Data Storage: User data (balance, stocks, options, history, last lookup) stored in usr_acc.<hash>.txt.
External Integration: Uses lookup_stock.+x for price data and options_pricing.+x for options pricing.

Setup
Prerequisites

Compiler: GCC or compatible.
OS: POSIX-compliant (e.g., Linux, macOS).
Dependencies: Standard C libraries, external binaries (+x/*.+x).

Compilation
Compile all source files using the provided script:
sh xsh.compile-all.+x🥅️🌐️📺️.sh

Or compile orchestrator.c individually:
gcc -o ./+x/0.yfin💴️._orch]-10]f10.+x orchestrator.c -D_POSIX_C_SOURCE=200809L

Ensure other binaries (lookup_stock.+x, buy_stock.+x, etc.) are compiled and in the ./+x/ directory.
Usage

Run the Program:
Start with an existing user hash:./+x/0.yfin💴️._orch]-10]f10.+x 2D528E


Or generate a new user:./+x/0.yfin💴️._orch]-10]f10.+x

Example output:Generated new user hash: 2D528E
[INIT] Created usr_acc.2D528E.txt




Interact with the Menu:
Example: Add credit, buy stock, view portfolio, check options.Enter option: 3
Enter amount to add: 39399
Added $39399.00. New balance: $39399.00

Enter option: 4
Enter symbol and shares: H 2
Bought 2.00 shares of H at $134.73. New balance: $39129.54

Enter option: 6
Portfolio:
Stocks:
H: 2.00 shares
Options:
(none)

Enter option: 9
Using latest lookup: H (Price: 134.73)
Options for H (Strike: 134.73):
Index | Type | Expiry     | Strike | Price
------+------+------------+--------+-------
(options data)




Exit: Select Option 11 or press Ctrl+C.

Example User File
usr_acc.2D528E.txt after the above actions:
balance,39129.54,watchlist,stocks,H,2.00,options,history,Buy,H,2.00,134.73,2025-06-11T00:38:49,last_lookup,H,134.73,2025-06-11T00:38:49,

Recent Changes

Option 9 Fix (June 2025): Corrected snprintf in orchestrator.c to fix option_prices.<symbol>.csv filename (removed extra space). Restored options table display.
Profit/Loss (Option 7): Added profit_loss.+x to show transaction history and net profit/loss.
Portfolio Bug: Partial fix in portfolio.c to remove garbage entries (OPTIONS, LAST_LOOKUP), but not yet applied in test output.
Sell Stock: Fixed sell_stock.c to record negative shares in history.
Last Lookup: Preserved in buy_stock.c and sell_stock.c to prevent overwrites.
Compilation: Fixed syntax errors in read_user_account (e.g., extra parentheses, invalid break statements).

Known Issues

Portfolio Display: Shows garbage entries (OPTIONS: 0.00 shares, LAST_LOOKUP: 0.00 shares) if unpatched portfolio.c is used. Apply portfolio.c (artifact ID c71999fe-c6c7-4495-83df-b42a53a9d3bb).
Test Suite: Fails due to check_user_state misparsing stocks section and timestamp mismatches.
Options Trading: buy_option.+x and sell_option.+x may overwrite last_lookup or fail to update history correctly.
Profit/Loss: Does not include unrealized gains (e.g., current value of held shares).
Options Pricing: options_pricing.+x may fail with certain parameters (e.g., -d 0.00); needs validation.

Development Notes

Source Files:
orchestrator.c: Main program (0.yfin💴._orch]-10]f10.+x).
portfolio.c, profit_loss.c, buy_stock.c, sell_stock.c: Feature modules.
options_pricing.+x, lookup_stock.+x: External binaries for data.


User File Format:
CSV-like: balance,<float>,watchlist,<symbols>,stocks,<symbol>,<shares>,...,options,<type>,<symbol,>,...,history,<type>,<symbol>,<shares>,<...,last_lookup,<symbol>,<price>,<time>


Future Work:
Fix test suite parsing.
Implement Option 10 (Predict).
Enhance profit/loss for unrealized gains.
Validate options_pricing.+x inputs.



Contact
For issues or contributions, reach out to the project maintainer (TBD).

