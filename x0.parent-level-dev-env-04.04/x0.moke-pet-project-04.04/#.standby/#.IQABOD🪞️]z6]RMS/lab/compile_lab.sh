#!/bin/bash

# This script compiles the programs in the lab/ directory.
# It places the executables in the root '+x/' directory to maintain consistency.

output_dir="../+x"

echo "Compiling lab programs..."

# Create the +x/ directory if it doesn't exist at the root
if [ ! -d "$output_dir" ]; then
    mkdir "$output_dir"
fi

# Compile probe_model.c
gcc lab/probe_model.c -o "$output_dir/probe_model.+x" -lm
if [ $? -eq 0 ]; then
    echo "Successfully compiled lab/probe_model.c into $output_dir/probe_model.+x"
else
    echo "Error compiling lab/probe_model.c"
fi

# Compile visualize_attention.c
gcc lab/visualize_attention.c -o "$output_dir/visualize_attention.+x" -lm
if [ $? -eq 0 ]; then
    echo "Successfully compiled lab/visualize_attention.c into $output_dir/visualize_attention.+x"
else
    echo "Error compiling lab/visualize_attention.c"
fi

echo "Lab compilation complete. Run executables from the root directory."
