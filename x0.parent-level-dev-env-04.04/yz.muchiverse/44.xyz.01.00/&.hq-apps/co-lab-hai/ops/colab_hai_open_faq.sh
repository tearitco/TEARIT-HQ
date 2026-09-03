#!/bin/bash
# colab_hai_open_faq.sh — "FAQ" dropdown item: open USER-FAQ.md in the
# default viewer. Same argv shape as colab_hai_open_dir.sh (argc=3,
# house_root is $3, unused here since the FAQ lives next to this
# script's own package dir, not under house_root).
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
xdg-open "$HERE/USER-FAQ.md" >/dev/null 2>&1 &
disown 2>/dev/null || true
