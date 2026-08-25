#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

# Resolve shared-lib canonical source (symlink-free)
_sr="$PWD"; while [ ! -d "$_sr/&.widgits/_shared-lib" ] && [ "$_sr" != "/" ]; do _sr="$(dirname "$_sr")"; done
_SS="$_sr/&.widgits/_shared-lib"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"
LDFLAGS="-lm"

echo "--- Building system processes ---"
gcc $CFLAGS "$_SS/system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "$_SS/system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building chtpm_rgb_render ---"
gcc $CFLAGS "$_SS/ops/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

echo "--- Building orchestrator ---"
gcc $CFLAGS -o "system/orchestrator" "system/orchestrator.c"

echo "--- Building gl_mirror ---"
if gcc $CFLAGS -o "system/gl_mirror" "system/gl_mirror.c" -lglut -lGL -lGLU -lX11 2>/tmp/yahoo_broker_gl_build.log; then
    echo "    gl_mirror: built ok"
else
    echo "    gl_mirror: skipped (GLUT/GL not available)"
    rm -f /tmp/yahoo_broker_gl_build.log
fi

echo "--- Building yahoo-broker ops ---"
gcc $CFLAGS -o "ops/+x/broker_menu_input.+x" "ops/broker_menu_input.c"
gcc $CFLAGS -o "ops/+x/broker_compose_frame.+x" "ops/broker_compose_frame.c"
gcc $CFLAGS -o "ops/+x/deposit_withdraw.+x" "ops/deposit_withdraw.c"
gcc $CFLAGS -o "ops/+x/lookup_stock.+x" "ops/lookup_stock.c"
gcc $CFLAGS -o "ops/+x/portfolio_new.+x" "ops/portfolio_new.c" $LDFLAGS
gcc $CFLAGS -o "ops/+x/profit_loss.+x" "ops/profit_loss.c"
gcc $CFLAGS -o "ops/+x/buy_stock.+x" "ops/buy_stock.c"
gcc $CFLAGS -o "ops/+x/sell_stock.+x" "ops/sell_stock.c"
gcc $CFLAGS -o "ops/+x/buy_option.+x" "ops/buy_option.c"
gcc $CFLAGS -o "ops/+x/sell_option.+x" "ops/sell_option.c"
gcc $CFLAGS -o "ops/+x/sell_option_inventory.+x" "ops/sell_option_inventory.c"
gcc $CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"
gcc $CFLAGS -o "ops/+x/options_pricing.+x" "ops/options_pricing.c" $LDFLAGS
gcc $CFLAGS -o "ops/+x/predictions.+x" "ops/predictions.c"
gcc $CFLAGS -o "ops/+x/add_credit.+x" "ops/add_credit.c"
gcc $CFLAGS -o "ops/+x/read_price.+x" "ops/read_price.c"
gcc $CFLAGS -o "ops/+x/research_refresh.+x" "ops/research_refresh.c"
gcc $CFLAGS -o "ops/+x/fetch_stock.+x" "ops/fetch_stock.c"
gcc $CFLAGS -o "ops/+x/yahoo_compose_rgb_frame.+x" "ops/yahoo_compose_rgb_frame.c"

echo "build ok"
