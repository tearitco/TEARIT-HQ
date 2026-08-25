#!/bin/bash

# Compile the knowledge distillation framework
gcc -o distill distill.c -lm

if [ $? -eq 0 ]; then
    echo "Knowledge distillation framework compiled successfully."
    echo "Usage: ./distill <teacher_model_dir> <student_model_dir> <vocab_file> [distill_weight]"
else
    echo "Error compiling knowledge distillation framework."
fi