#!/bin/bash

# Define the output directory
output_dir="+x"

# Create the +x/ directory if it doesn't exist
if [ ! -d "$output_dir" ]; then
    mkdir "$output_dir"
    if [ $? -ne 0 ]; then
        echo "Error: Could not create directory $output_dir"
        exit 1
    fi
fi

# Loop through all .c files in the current directory
for file in *.c; do
    # Check if any .c files exist (if no matches, *.c becomes literal)
    if [ ! -f "$file" ]; then
        echo "No .c files found in the current directory."
        exit 1
    fi

    # Extract the filename without the .c extension
    basename=${file%.c}

    # Append .+x to the executable name
    executable_name="${basename}.+x"

    # Compile the .c file into an executable in the +x/ directory with .+x suffix
    gcc "$file" -o "$output_dir/$executable_name" -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lfreetype -lavcodec -lavformat -lavutil -lswscale  -lX11 -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL

    # Check the exit status of gcc
    if [ $? -eq 0 ]; then
        echo "Successfully compiled $file into $output_dir/$executable_name"
    else
        echo "Error compiling $file"
    fi
done

echo "Compilation complete."
