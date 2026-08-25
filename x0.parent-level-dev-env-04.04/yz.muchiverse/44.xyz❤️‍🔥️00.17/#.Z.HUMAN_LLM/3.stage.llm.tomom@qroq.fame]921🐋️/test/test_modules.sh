#!/bin/bash

# Test script for modularized neural network components
# This script tests individual components separately.
# For end-to-end training, use the trainer.c orchestrator instead.

echo "Testing individual modularized neural network components..."

# Check if corpus files exist
if [ ! -f "corpuses/corpus_yang.txt" ] && [ ! -f "corpuses/corpus_ying.txt" ]; then
    echo "Warning: No corpus files found in corpuses/ directory."
    echo "Please add some text files to corpuses/ to fully test the pipeline."
    exit 1
fi

# Create vocabulary model from corpus
echo "Creating vocabulary model from corpus..."
if [ -f "corpuses/corpus_yang.txt" ]; then
    VOCAB_FILE=$(./+x/vocab_model.+x corpuses/corpus_yang.txt)
elif [ -f "corpuses/corpus_ying.txt" ]; then
    VOCAB_FILE=$(./+x/vocab_model.+x corpuses/corpus_ying.txt)
fi

# Check if vocab file was created
if [ ! -f "$VOCAB_FILE" ]; then
    echo "Error: $VOCAB_FILE not created"
    exit 1
fi

echo "Vocabulary model created at: $VOCAB_FILE"
CURRICULUM_DIR=$(dirname "$VOCAB_FILE")

# Initialize attention module
echo "Initializing attention module..."
./+x/attention.+x init "$CURRICULUM_DIR/attention_model.txt"

# Initialize MLP layer
echo "Initializing MLP layer..."
./+x/mlp_layer.+x init "$CURRICULUM_DIR/mlp_model.txt"

# Initialize optimizer with learning rate 0.001, beta1 0.9, and beta2 0.999 for Adam
echo "Initializing Adam optimizer..."
./+x/optimizer.+x adam-init "$CURRICULUM_DIR/optimizer_state.txt" 0.001 0.9 0.999

# Test forward propagation with the first word in vocabulary
echo "Testing forward propagation with the first word in vocabulary..."
./+x/forward_prop.+x "$VOCAB_FILE" 0

echo "All modules tested successfully!"

echo ""
echo "Results have been saved to the $CURRICULUM_DIR directory:"
echo "- Attention scores: $CURRICULUM_DIR/attn_scores.txt"
echo "- Hidden state: $CURRICULUM_DIR/hidden_state.txt"

echo ""
echo "To train the model using the orchestrator, run: ./+x/trainer.+x \"$VOCAB_FILE\""
echo "To chat with the bot, run: ./+x/chatbot.+x \"$VOCAB_FILE\" \"your prompt here\""
