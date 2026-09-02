#!/bin/bash

# Compile the association visualization tool
gcc -std=c99 -Wall -Wextra -O2 -o "+x/visualize_associations🥰]b1.+x" "visualize_associations🥰]b1.c" -lGL -lGLU -lglut -lm -lfreetype

if [ $? -eq 0 ]; then
    echo "Successfully compiled visualize_associations🥰]b1.c"
    echo "Usage: ./+x/visualize_associations🥰]b1.+x <associations.csv>"
    echo "Example: ./+x/visualize_associations🥰]b1.+x emoji_associations.csv"
else
    echo "Compilation failed"
fi