#!/bin/bash

# Script to update optimizer state with learning rate from config.txt
# Usage: ./update_optimizer.sh <output_dir>

if [ $# -lt 1 ]; then
    echo "Usage: $0 <output_dir>"
    exit 1
fi

OUTPUT_DIR=$1
CONFIG_FILE="$OUTPUT_DIR/config.txt"
OPTIMIZER_FILE="$OUTPUT_DIR/optimizer_state.txt"

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Check if optimizer file exists
if [ ! -f "$OPTIMIZER_FILE" ]; then
    echo "Optimizer file not found: $OPTIMIZER_FILE"
    exit 1
fi

# Extract learning rate from config file
LEARNING_RATE=$(grep "^learning_rate=" "$CONFIG_FILE" | cut -d'=' -f2)
BETA1=$(grep "^beta1=" "$CONFIG_FILE" | cut -d'=' -f2)
BETA2=$(grep "^beta2=" "$CONFIG_FILE" | cut -d'=' -f2)

# If not found, use defaults
if [ -z "$LEARNING_RATE" ]; then
    LEARNING_RATE=0.001
fi

if [ -z "$BETA1" ]; then
    BETA1=0.9
fi

if [ -z "$BETA2" ]; then
    BETA2=0.999
fi

# Update the first line of optimizer_state.txt with new values
sed -i "1s/.*/$LEARNING_RATE $BETA1 $BETA2 0/" "$OPTIMIZER_FILE"

echo "Updated optimizer state with learning rate: $LEARNING_RATE, beta1: $BETA1, beta2: $BETA2"