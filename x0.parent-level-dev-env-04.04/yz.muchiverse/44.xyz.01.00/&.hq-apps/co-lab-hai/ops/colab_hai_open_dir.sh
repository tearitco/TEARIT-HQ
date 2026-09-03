#!/bin/bash
# colab_hai_open_dir.sh — "Dir" toolbar button: open this app's own
# real state dir (#.desktop/colab_hai/, where incoming/pending/
# rejected/conversation/request all live) in the desktop file manager,
# so the owner can inspect real logs directly. Same argv shape as
# every other <item> action in this house (item action appends
# <package_dir> <house_root>, argc=3, house_root is $3).
set -e
if [ $# -lt 3 ]; then
    echo "colab_hai_open_dir.sh: unexpected argc ($#), expected 3" >&2
    exit 1
fi
HOUSE_ROOT="$3"
DIR="$HOUSE_ROOT/#.desktop/colab_hai"
mkdir -p "$DIR"
xdg-open "$DIR" >/dev/null 2>&1 &
disown 2>/dev/null || true
