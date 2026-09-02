#!/bin/bash
# nav_intent_to_index.sh — deterministic app-side resolver for the
# "delegate navigation" pattern tested live 2026-08-13 (see
# au11-hq/HARNESS-DELEGATION-PIPELINE.md §6): a Harnecient model
# reliably names the CORRECT real menu item in plain text (3/3 live
# trials on gemma3:1b) when given the actual current labels and asked
# to name one, NOT asked to pick a raw index number (that was 2/4
# WRONG in the same live test - matches PITFALL 69's documented
# finding that small models can't reliably self-classify against a
# raw enumerated list).
#
# This script is the deterministic half of that pattern: it takes the
# model's free-text reply and the REAL current nav labels (must come
# from the live app, never hardcoded - labels/order both drift, see
# _.0.aigent-testing-k9.txt Rule 4/Rule 7) and resolves to a nav index
# via case-insensitive substring match. The model never picks an
# index - it only ever describes intent in words, same DESCRIBE-not-
# CLASSIFY discipline HARNECIENT-HACK.md already established for tool
# dispatch, applied here to navigation instead.
#
# NOT YET WIRED INTO ANY LIVE APP: open-hai's receipt only exposes nav
# COUNTS (nav=<focus> n_nav=<total>), not each item's real label - so
# there is no live source for the labels array this script needs yet.
# Real next step (not done): add a labels dump to open-hai's receipt
# (or a dedicated --dump-nav-labels mode) before this can drive a real
# session end-to-end.
#
# Usage: nav_intent_to_index.sh "<model's plain-text reply>" "<label1>" "<label2>" ...
# Prints the 1-based index of the first label whose text appears
# (case-insensitive) in the reply, or nothing + exit 1 if no match.
set -u
REPLY="${1:-}"
shift || true
if [ -z "$REPLY" ] || [ "$#" -eq 0 ]; then
    echo "usage: nav_intent_to_index.sh \"<reply>\" \"<label1>\" [\"<label2>\" ...]" >&2
    exit 2
fi
reply_lc="$(echo "$REPLY" | tr '[:upper:]' '[:lower:]')"
idx=0
for label in "$@"; do
    idx=$((idx + 1))
    label_lc="$(echo "$label" | tr '[:upper:]' '[:lower:]')"
    case "$reply_lc" in
        *"$label_lc"*)
            echo "$idx"
            exit 0
            ;;
    esac
done
exit 1
