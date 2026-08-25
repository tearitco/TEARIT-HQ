#!/bin/bash

# Test script for the orchestrator trainer

echo "Testing trainer as orchestrator..."

# Check if corpus files exist
if [ ! -f "corpuses/test_emoji.txt" ]; then
    echo "Warning: No test corpus found. Creating a simple test file..."
    mkdir -p corpuses
    echo "test emoji file" > corpuses/corpus]similarity10.txt
fi

# Create vocabulary model from corpus
echo "Creating vocabulary model from corpus..."
VOCAB_FILE=$(./+x/vocab_model.+x corpuses/corpus]similarity10.txt)

# Check if vocab file was created
if [ ! -f "$VOCAB_FILE" ]; then
    echo "Error: $VOCAB_FILE not created"
    exit 1
fi

echo "Vocabulary model created at: $VOCAB_FILE"

# Train the model using the orchestrator
echo "Training model using orchestrator..."
./+x/trainer.+x "$VOCAB_FILE"

echo "Training complete!"

# Check if model files were created
CURRICULUM_DIR=$(dirname "$VOCAB_FILE")

if [ -f "$CURRICULUM_DIR/attention_model.txt" ]; then
    echo "✓ attention_model.txt created"
else
    echo "✗ attention_model.txt not found"
fi

if [ -f "$CURRICULUM_DIR/mlp_model.txt" ]; then
    echo "✓ mlp_model.txt created"
else
    echo "✗ mlp_model.txt not found"
fi

if [ -f "$CURRICULUM_DIR/optimizer_state.txt" ]; then
    echo "✓ optimizer_state.txt created"
else
    echo "✗ optimizer_state.txt not found"
fi

if [ -f "$CURRICULUM_DIR/loss.txt" ]; then
    echo "✓ loss.txt created"
    echo "Loss values:"
    cat "$CURRICULUM_DIR/loss.txt"
else
    echo "✗ loss.txt not found"
fi

echo "Test complete!"
