#!/bin/bash
# build.sh for iqamoji-xo5
# PURE COMPILATION - NO LINKING BETWEEN MODULES

BOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$BOT_DIR"

echo "--- Building iqamoji-xo5 Orchestrator ---"
# Orchestrator is built as a standalone executable
gcc -Wall -Wextra -O2 orchestrator.c -o bot5_orchestrator

echo "--- Building iqamoji-xo5 Keyboard Hand (Muscle) ---"
# Keyboard muscle is also a standalone executable
gcc -Wall -Wextra -O2 keyboard_muscle.c -o bot5_keyboard_muscle

echo "--- Building iqamoji-xo5 Ops ---"
mkdir -p ops/+x
for src in ops/*.c; do
    if [ -f "$src" ]; then
        op_name=$(basename "$src" .c)
        gcc -Wall -Wextra -O2 "$src" -o "ops/+x/${op_name}.+x"
    fi
done

echo "--- Build Complete ---"
ls -l bot5_orchestrator bot5_keyboard_muscle ops/+x


