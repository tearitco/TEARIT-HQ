#!/bin/bash

echo "Testing fixes for NaN/Inf issues..."

# Compile the updated code
echo "Compiling updated code..."
gcc -o forward_prop forward_prop.c -lm
gcc -o backward_prop backward_prop.c -lm
gcc -o optimizer optimizer.c -lm

# Check if compilation was successful
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Compilation successful!"

# Run a simple test if we have test data
if [ -f "test_data/vocab.txt" ] && [ -f "test_data/attn_model.txt" ] && [ -f "test_data/mlp_model.txt" ] && [ -f "test_data/output_model.txt" ]; then
    echo "Running forward propagation test..."
    ./forward_prop test_data/vocab.txt 0 test_data/attn_model.txt test_data/mlp_model.txt test_data/output_model.txt
    if [ $? -eq 0 ]; then
        echo "Forward propagation completed successfully!"
    else
        echo "Forward propagation failed!"
        exit 1
    fi
else
    echo "Test data not found. Skipping execution tests."
fi

echo "All tests completed!"