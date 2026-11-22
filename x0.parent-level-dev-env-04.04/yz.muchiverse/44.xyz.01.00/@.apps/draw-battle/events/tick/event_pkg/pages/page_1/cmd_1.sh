#!/bin/sh
D="$(cd "$(dirname "$0")/../../../../../../.." && pwd)"
exec "$D/&.widgits/_shared-lib/ops/+x/db_battle_op.+x" tick "$@"
