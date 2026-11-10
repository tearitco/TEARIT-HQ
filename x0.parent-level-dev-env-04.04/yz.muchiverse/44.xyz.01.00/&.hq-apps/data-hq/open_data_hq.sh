#!/bin/sh
# open_data_hq.sh - launch the Data Studio (data-hq) window: ONE process, the shared renderer on data-hq.xhtpm. Its <module> (ops/+x/data_hq_manager.+x) is forked, registered and SIGTERM'd by the
# renderer itself. Shape = eden-viewer-hq/open_eden_viewer_hq.sh (the house launcher shape for an HQ window), started through the registry funnel kh_spawn.sh. Single instance: an older one is ended first.
#   sh open_data_hq.sh <house_root>        (the db menu passes it; the Toys menu passes "run" or nothing, then the house is found by walking up)
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then _d="$(cd "$(dirname "$0")" && pwd)"; while [ "$_d" != "/" ] && { [ ! -d "$_d/#.desktop" ] || [ ! -d "$_d/&.widgits" ]; }; do _d="$(dirname "$_d")"; done; HOUSE_ROOT="$_d"; fi
[ -d "$HOUSE_ROOT/#.desktop" ] || { echo "open_data_hq.sh: could not resolve the house root" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"; HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"; XHTPM="$HERE/data-hq.xhtpm"
for t in data_op chart_op data_hq_manager; do [ -x "$HERE/ops/+x/$t.+x" ] || { echo "open_data_hq.sh: $t.+x is not built (cd &.widgits/_shared-lib/harness/ops; sh build_harness_ops.sh)" >&2; exit 1; }; done
[ -x "$BIN" ] || { echo "open_data_hq.sh: missing $BIN (sh \$.crypts/button.sh build)" >&2; exit 1; }
old="$(ps -eo pid,args | awk -v h="$HOUSE_ROOT" 'NR > 1 { p = $1; a0 = $2; if (index($0, h) && a0 ~ /khtpm_core_render\.\+x$/ && index($0, "data-hq.xhtpm")) print p }')"
for p in $old; do kill "$p" 2>/dev/null || true; done; [ -n "$old" ] && sleep 0.5
KH_ROLE=window KH_APP=data-hq KH_LIFETIME=house sh "$HOUSE_ROOT/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE_ROOT" 0 data-hq -- "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null
echo "data-hq launched"
