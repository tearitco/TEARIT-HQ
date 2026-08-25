#!/bin/bash
# train_corpus.sh <name> [epochs] - wraps IQABOD's existing vocab_only +
# train CLI modes, matching this project's own compile_lab.sh/
# lab_autotrain.sh shell-wrapper convention. See ROADMAP-models.txt §11.0
# and TODO.txt Phase A3. IQABOD_PROJECT_ROOT must be set (same convention
# as generate_corpus.c).
set -e

if [ -z "$IQABOD_PROJECT_ROOT" ]; then
    echo "IQABOD_PROJECT_ROOT is not set" >&2
    exit 1
fi
if [ -z "$1" ]; then
    echo "Usage: train_corpus.sh <name> [epochs]" >&2
    exit 1
fi

NAME="$1"
EPOCHS="${2:-5}"

cd "$IQABOD_PROJECT_ROOT"
echo "--- vocab_only ---"
./+x/main_orchestrator.+x vocab_only "corpuses/$NAME.txt"
echo "--- train ($EPOCHS epochs) ---"
./+x/main_orchestrator.+x train "corpuses/$NAME.txt" "$EPOCHS"
echo "--- done ---"
ls -la "curriculum/$NAME/"
