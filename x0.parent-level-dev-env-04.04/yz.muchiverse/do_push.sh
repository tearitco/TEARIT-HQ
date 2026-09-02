#!/bin/bash
set -e
# REAL FIX 2026-09-01 (S1_HOUSE_PATH_MIGRATION.md) - was a hardcoded
# absolute path, broke the moment the house moved. Real house standard:
# resolve dynamically. This script always lives a fixed 2 levels under
# the real git root (yz.muchiverse's own parent's parent) - climb from
# \$0 instead of hand-editing a literal path every time the house moves.
cd "$(cd "$(dirname "$0")/../.." && pwd)"
gh repo create tearitco/TEARIT-HQ --public --source=. --remote=origin --push
echo "--- done ---"
gh repo view tearitco/TEARIT-HQ --web=false --json url -q .url
