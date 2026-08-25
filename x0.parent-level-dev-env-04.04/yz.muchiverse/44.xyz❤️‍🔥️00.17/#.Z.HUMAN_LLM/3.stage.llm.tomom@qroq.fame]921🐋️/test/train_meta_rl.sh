#!/bin/bash

# Training script for the meta RL orchestrator
# Runs multiple iterations to help the system learn better curriculum selection

# Check if the meta_rl executable exists
if [ ! -f "./meta_rl/meta_rl" ]; then
    echo "Error: meta_rl executable not found in ./meta_rl/"
    echo "Please compile it first with: gcc -o meta_rl/meta_rl meta_rl/meta_rl.c -lm"
    exit 1
fi

# Default values
NUM_ITERATIONS=5
DEFAULT_PROMPTS=(
    "hello world"
    "how are you today"
    "tell me a story"
    "what is your purpose"
    "describe your capabilities"
    "explain machine learning"
    "what are neural networks"
    "how does attention work"
    "what is reinforcement learning"
    "describe a chatbot"
)

echo "Meta RL Training Script"
echo "======================"
echo "This script will run $NUM_ITERATIONS iterations of the meta RL orchestrator"
echo "with various prompts to help it learn better curriculum selection."
echo ""

# Run iterations
for i in $(seq 1 $NUM_ITERATIONS); do
    echo "Iteration $i/$NUM_ITERATIONS"
    echo "------------------------"
    
    # Select a random prompt from the default prompts
    RANDOM_INDEX=$((RANDOM % ${#DEFAULT_PROMPTS[@]}))
    PROMPT="${DEFAULT_PROMPTS[$RANDOM_INDEX]}"
    
    echo "Using prompt: '$PROMPT'"
    
    # Run the meta RL orchestrator
    ./meta_rl/meta_rl "$PROMPT" 10 1.0
    
    echo ""
    echo "Waiting 2 seconds before next iteration..."
    sleep 2
    echo ""
done

echo "Training complete!"
echo "The meta RL orchestrator should now have updated weights in meta_rl_weights.txt"

# Show the updated weights if they exist
if [ -f "meta_rl_weights.txt" ]; then
    echo ""
    echo "Current weights:"
    echo "==============="
    head -20 meta_rl_weights.txt
    echo "..."
    echo "(showing first 20 lines)"
fi