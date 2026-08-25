#!/bin/bash

# Test script for knowledge distillation framework

echo "Testing Knowledge Distillation Framework"

# Create directories for teacher and student models if they don't exist
mkdir -p teacher_models
mkdir -p student_models

# Copy current model files to teacher_models as an example
echo "Setting up teacher model directory..."
cp attention_model.txt teacher_models/ 2>/dev/null || echo "No attention_model.txt found"
cp mlp_model.txt teacher_models/ 2>/dev/null || echo "No mlp_model.txt found"
cp output_layer.txt teacher_models/ 2>/dev/null || echo "No output_layer.txt found"

echo "Running knowledge distillation..."
./distill teacher_models student_models vocab_model.txt 0.7

echo "Test completed."