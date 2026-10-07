#!/bin/sh
# build_entity_cli_ops.sh - build the entity-cli C ops that Act:Move and the
# inventory use. Run from anywhere; picked up by $.crypts/compile-runner.sh
# (any ops/build_*.sh).
#
#   move_entity_init / move_entity_tick  - the desk's Move (shared range/path/
#                                          queue lib, khtpm_move_range.c)
#   inventory_op                         - Take / Place / slot / projection
#                                          (khtpm_inventory.c, khtpm_page_rows.c)
#   json_parser                          - dot-notation JSON reader; ^.hai-horn's OpenRouter ops call
#                                          '&.widgits/entity-cli/ops/json_parser.+x' to read the reply
#                                          (choices[0].message.content). Added 2026-10-07: it had no
#                                          build line, so a fresh build left HORN unable to parse replies.
#
# Added 2026-10-05: these were compiled by hand and had no build script, so when
# the compiled binaries were lost with the working tree nothing rebuilt them and
# Act:Move silently lost its placer. Each binary is installed in ops/ and in
# ops/+x/ (both locations are read by the callers).
set -e
OPS="$(cd "$(dirname "$0")" && pwd)"
SHARED="$(cd "$OPS/../../_shared-lib" && pwd)"
cd "$OPS"
mkdir -p +x
for n in move_entity_init move_entity_tick inventory_op json_parser; do
    gcc -O2 -I"$SHARED" -o "$n.+x" "$n.c"
    cp "$n.+x" "+x/$n.+x"
    echo "-- built $n.+x"
done
echo "build ok"
