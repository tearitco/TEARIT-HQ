# Quest q001: clamp_op (pilot quest for the delegation flywheel)
Goal: one tiny compiled op `clamp_op <value> <min> <max>`: print value clamped into [min,max] as a decimal integer and a newline on stdout, exit 0.
Usage errors (exit 2, nothing on stdout, a message on stderr): wrong argument count; an argument that is not a whole base-10 integer (empty, trailing characters, hex, decimal point); out of 64-bit range; min > max.
Accepts a leading minus or plus sign. 64-bit signed (long long).
Allowed file: `worker/clamp_op.c` only. Self-contained C (no headers of ours), compiles clean with `gcc -std=gnu11 -Wall -Wextra -O2`.
LOCKED (worker must not touch): `_shared-lib/harness/cases/quest_q001_clamp.pdl`, `_shared-lib/harness/quest_q001_clamp.pal`.
Acceptance: `VERDICT|PASS` from the `quest_q001_clamp` harness. Worker: HORN via OpenRouter (free models), run by hand by claude (quest_runner not built yet).
