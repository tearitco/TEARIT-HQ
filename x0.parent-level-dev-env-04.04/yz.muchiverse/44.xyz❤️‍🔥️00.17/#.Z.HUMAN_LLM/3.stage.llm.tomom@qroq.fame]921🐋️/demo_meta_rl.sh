#!/bin/bash

# Demo script for the meta RL orchestrator
# This script shows how to use the meta RL system with example parameters

echo "Meta RL Orchestrator Demo"
echo "========================"
echo ""
echo "This demo shows how to use the meta RL orchestrator to:"
echo "1. Analyze a prompt and select appropriate curricula"
echo "2. Run the MOE chatbot with selected curricula"
echo "3. Collect user feedback to improve curriculum selection"
echo ""

# Example usage with dummy prompt and arguments
echo "Example usage:"
echo "  ./run_meta_rl.sh \"hello world\" 10 1.0"
echo ""
echo "Parameters:"
echo "  \"hello world\"     - The input prompt"
echo "  10              - Desired response length (number of tokens)"
echo "  1.0             - Temperature (controls randomness, 1.0 is default)"
echo ""
echo "Running demo..."
echo ""

# Run with example parameters
./run_meta_rl.sh "hello world" 10 1.0

echo ""
echo "Demo completed!"
echo ""
echo "To use the meta RL orchestrator with your own prompts:"
echo "  ./run_meta_rl.sh \"<your prompt>\" [length] [temperature]"
echo ""
echo "For training the meta RL system with multiple iterations:"
echo "  ./train_meta_rl.sh"