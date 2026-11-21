#!/bin/sh
# The page's one command: the shared op. Arguments come from the caller.
D="$(cd "$(dirname "$0")/../../../../../../.." && pwd)"
exec "$D/&.widgits/_shared-lib/ops/+x/db_battle_op.+x" stroke "$@"
