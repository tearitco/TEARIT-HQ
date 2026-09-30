#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
exec ../../ops/+x/mr_change_gold.+x "$PWD" '10'
