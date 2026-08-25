#!/bin/bash
# TPMOS Yahoo Ops Compiler
output_dir="+x"
mkdir -p "$output_dir"

for file in *.c; do
    basename=${file%.c}
    executable_name="${basename}.+x"
    # Basic compilation - some may need -lm or other flags
    gcc "$file" -o "$output_dir/$executable_name" -D_POSIX_C_SOURCE=200809L -lm -pthread 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "Successfully compiled $file"
    else
        # Try again with more flags if it failed (e.g. for fetch_stock or options_pricing)
        gcc "$file" -o "$output_dir/$executable_name" -D_POSIX_C_SOURCE=200809L -lm -pthread -lssl -lcrypto 2>/dev/null
        if [ $? -eq 0 ]; then
             echo "Successfully compiled $file (with extra flags)"
        else
             echo "Error compiling $file"
        fi
    fi
done
