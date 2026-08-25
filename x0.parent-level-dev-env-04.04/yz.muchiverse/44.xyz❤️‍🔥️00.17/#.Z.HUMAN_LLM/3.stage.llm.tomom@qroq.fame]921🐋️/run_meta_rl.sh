#!/bin/bash

# Script to run the meta RL orchestrator and update its weights

# Check if the meta_rl executable exists
if [ ! -f "./meta_rl/meta_rl" ]; then
    echo "Error: meta_rl executable not found in ./meta_rl/"
    echo "Please compile it first with: gcc -o meta_rl/meta_rl meta_rl/meta_rl.c -lm"
    exit 1
fi

# Check if arguments are provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 \"<prompt>\" [length] [temperature]"
    echo "Example: $0 \"hello world\" 10 1.0"
    exit 1
fi

# Set default values
PROMPT="$1"
LENGTH=${2:-10}
TEMPERATURE=${3:-1.0}

# Run the meta RL orchestrator
echo "Running meta RL orchestrator with prompt: '$PROMPT'"
echo "Length: $LENGTH, Temperature: $TEMPERATURE"
echo ""

./meta_rl/meta_rl "$PROMPT" $LENGTH $TEMPERATURE

# Check if the weights file was created/updated
if [ -f "meta_rl_weights.txt" ]; then
    echo ""
    echo "Meta RL weights have been updated and saved to meta_rl_weights.txt"
    echo "You can view the current weights with: cat meta_rl_weights.txt"
else
    echo ""
    echo "Warning: meta_rl_weights.txt not found"
fi