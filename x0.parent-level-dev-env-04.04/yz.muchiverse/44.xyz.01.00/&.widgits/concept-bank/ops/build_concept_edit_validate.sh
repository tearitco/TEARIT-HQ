#!/bin/sh
# build_concept_edit_validate.sh - same real build-into-+x/ convention
# as ai-lab/ops/build_ai_lab_picker.sh: compiled binary lands in ./+x/,
# never git-tracked (+x/ binaries aren't committed, per house convention
# confirmed against ai-lab's own git-tracked file list).
set -eu
SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SELF_DIR/+x"
cc -O2 -Wall -o "$SELF_DIR/+x/concept_edit_validate.+x" "$SELF_DIR/concept_edit_validate.c"
echo "built: $SELF_DIR/+x/concept_edit_validate.+x"
