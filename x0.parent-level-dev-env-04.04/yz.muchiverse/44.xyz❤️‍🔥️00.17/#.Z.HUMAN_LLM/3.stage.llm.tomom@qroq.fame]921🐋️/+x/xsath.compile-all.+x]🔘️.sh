#!/bin/bash

# xshin.compile-all.+x🥅️🌐️📺️🔘️.sh - Universal C compiler into +x/

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define project root as parent of +x/, assuming script is inside +x/
PROJECT_ROOT="$SCRIPT_DIR/.."

# Output directory is +x/ (same as script's dir)
OUTPUT_DIR="$SCRIPT_DIR"

# Check if +x/ exists, create if not
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "Creating $OUTPUT_DIR/"
    mkdir -p "$OUTPUT_DIR"
fi

# Check if there are any .c files in project root
C_FILES=("$PROJECT_ROOT"/*.c)
if [ ! -f "${C_FILES[0]}" ]; then
    echo "No .c files found in $PROJECT_ROOT/"
    exit 1
fi

# Loop over all .c files in project root
for file in "$PROJECT_ROOT"/*.c; do
    if [ ! -f "$file" ]; then
        continue
    fi

    # Extract basename without extension
    basename=$(basename "$file" .c)
    executable_name="${basename}.+x"

    # Compile into +x/ directory
    gcc "$file" -o "$OUTPUT_DIR/$executable_name" \
        -pthread -lm -lssl -lcrypto \
        -lGL -lGLU -lglut -lfreetype \
        -lavcodec -lavformat -lavutil -lswscale \
        -lX11 \
        -I/usr/include/freetype2 -I/usr/include/libpng12 \
        -L/usr/local/lib -lOpenCL

    if [ $? -eq 0 ]; then
        echo "✅ Built: $executable_name"
    else
        echo "❌ Failed: $file"
    fi
done

echo "🎯 Compilation complete. Executables in: $OUTPUT_DIR/"
