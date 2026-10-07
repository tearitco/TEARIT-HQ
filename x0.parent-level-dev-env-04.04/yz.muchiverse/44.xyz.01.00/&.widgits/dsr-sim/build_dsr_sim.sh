#!/bin/sh
# build_dsr_sim.sh - builds the three dsr-sim ops into ops/+x/ (verify: pal harness _shared-lib/harness/dsr_sim_step1.pal).
# -ffp-contract=off keeps the WSR float price math identical on machines that would otherwise fuse multiply-add.
set -e
cd "$(dirname "$0")/ops"
mkdir -p +x
for n in dsr_scenario_gen dsr_day_tick dsr_sim_query_op; do
  ${CC:-gcc} -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -O2 -ffp-contract=off -o +x/$n.+x $n.c -lm
  echo "OK +x/$n.+x"
done
