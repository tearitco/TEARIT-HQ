#!/bin/bash
# build.sh for Bot5
# PURE COMPILATION - NO LINKING BETWEEN MODULES

BOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$BOT_DIR"

echo "--- Building Bot5 Orchestrator ---"
# Orchestrator is built as a standalone executable
gcc -Wall -Wextra -O2 orchestrator.c -o bot5_orchestrator

echo "--- Building Bot5 Keyboard Hand (Muscle) ---"
# Keyboard muscle is also a standalone executable
gcc -Wall -Wextra -O2 keyboard_muscle.c -o bot5_keyboard_muscle

echo "--- Build Complete ---"
ls -l bot5_orchestrator bot5_keyboard_muscle



