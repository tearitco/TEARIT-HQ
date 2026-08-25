# Knowledge Distillation Framework

This directory contains a basic knowledge distillation framework that implements the concepts described in the roadmap. The framework is designed to compress large "teacher" models into smaller, more efficient "student" models while preserving performance.

## Overview

Knowledge distillation is a technique where a smaller "student" model is trained to reproduce the behavior of a larger, more powerful "teacher" model. This approach allows us to:

1. Create compact models that can run on resource-constrained devices
2. Transfer knowledge from complex models to simpler ones
3. Maintain performance while reducing computational requirements

## Implementation Details

The current implementation includes:

1. **Model Loading/Saving**: Functions to load and save teacher and student model parameters
2. **Vocabulary Handling**: Functions to work with vocabulary files
3. **Softmax with Temperature**: Implementation of temperature-scaled softmax for softer probability distributions
4. **Distillation Loss**: KL divergence-based loss function for comparing teacher and student outputs
5. **Framework Structure**: Basic framework for implementing the full distillation process

## Usage

To compile the framework:
```bash
gcc -o distill distill.c -lm
```

To run knowledge distillation:
```bash
./distill <teacher_model_dir> <student_model_dir> <vocab_file> [distill_weight]
```

Example:
```bash
./distill ./teacher_models ./student_models ./vocab_model.txt 0.7
```

## Current Limitations

The current implementation is a framework only and does not perform actual distillation. To make it fully functional, you would need to:

1. Implement the forward pass for both teacher and student models
2. Generate logits from both models on training data
3. Compute the distillation loss using the provided KL divergence function
4. Implement backpropagation to update the student model parameters
5. Add training loop with multiple epochs

## Roadmap Integration

This framework aligns with the roadmap's "Knowledge Distillation Layer (The WhisperNet)" by providing the foundational components needed for:

- Teacher-Student model architecture
- Response distillation (soft labels)
- Feature distillation (hidden state matching)
- Combined loss functions (task loss + distillation loss)

## Next Steps

To fully implement the knowledge distillation system described in the roadmap:

1. Connect this framework with the existing neural network components
2. Implement actual forward passes for teacher and student models
3. Add gradient computation and parameter updates
4. Create specific distillation functions for each component (attention, MLP, output layer)
5. Add quantization and pruning capabilities for model compression