#!/bin/sh
# open_mon.sh - HQ menu "mon" row entry point. Thin wrapper (kept here,
# under a glob-safe path, because the HQ menu dispatch runs the cmd via
# `sh -c` and the real target lives at &.hq-apps/proc-mon/ - a leading
# '&' is shell job-control, so it can't be named directly in the .pdl
# row). Mirrors open_cli.sh: resolve this house root from $0, exec the
# app.
#
# REAL FIX 2026-09-11 (direct live report: "i clicked 'mon' and used
# nav. it didn't open. was it renamed or what?") - correct: mon-hq was
# renamed to proc-mon earlier this same session (5bfbe8c6), but this
# file's own path never got updated in that pass, and - checked via
# `git log --all -- open_mon.sh`, zero commits ever touch this file -
# an edit that fixed it in a prior turn was apparently never actually
# committed and got lost in one of today's several branch resets.
# Fixed for real this time and committed.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# ops -> *.livedesk-taskbar -> *.monads -> house_root
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/&.hq-apps/proc-mon/button.sh" "$HOUSE_ROOT"
