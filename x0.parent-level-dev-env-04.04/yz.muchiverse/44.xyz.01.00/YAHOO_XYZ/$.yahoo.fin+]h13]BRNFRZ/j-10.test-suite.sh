#!/bin/bash
set -e

TEST_HASH="TEST123"
USER_FILE="usr_acc.$TEST_HASH.txt"
echo "Starting automated tests for stock trading system..."

# Setup test user
echo "balance,1000.00" > $USER_FILE
echo "watchlist,AAPL" >> $USER_FILE

# Test 1: Lookup stock price
echo "Test 1: Lookup stock price for NVDA"
OUTPUT=$(./+x/orchestrator.+x -d <<< "1\nNVDA\nquit")
if echo "$OUTPUT" | grep -q "NVDA Current Price:"; then
    echo "Test 1 PASSED"
else
    echo "Test 1 FAILED"
    exit 1
fi

# Test 2: Add credit
echo "Test 2: Add $500 credit"
./+x/add_credit.+x $TEST_HASH 500
BALANCE=$(grep "^balance," $USER_FILE | cut -d',' -f2)
if [ "$BALANCE" = "1500.00" ]; then
    echo "Test 2 PASSED"
else
    echo "Test 2 FAILED: Expected 1500.00, got $BALANCE"
    exit 1
fi

# Test 3: Buy stock
echo "Test 3: Buy 10 NVDA"
./+x/buy_stock.+x $TEST_HASH NVDA 10
if grep -q "portfolio,NVDA,10," $USER_FILE; then
    echo "Test 3 PASSED"
else
    echo "Test 3 FAILED"
    exit 1
fi

# Test 4: Sell stock
echo "Test 4: Sell 5 NVDA"
./+x/sell_stock.+x $TEST_HASH NVDA 5
if grep -q "portfolio,NVDA,5," $USER_FILE; then
    echo "Test 4 PASSED"
else
    echo "Test 4 FAILED"
    exit 1
fi

# Test 5: Options pricing
echo "Test 5: Options pricing for NVDA"
./+x/options_pricing.+x -s NVDA -p 120.52 -k 120.00 -r 0.05 -v 0.2 -d 0.01 -t 2025-06-10T21:21:04
if [ -f "option_prices.NVDA.csv" ]; then
    echo "Test 5 PASSED"
else
    echo "Test 5 FAILED"
    exit 1
fi

echo "All tests PASSED"
