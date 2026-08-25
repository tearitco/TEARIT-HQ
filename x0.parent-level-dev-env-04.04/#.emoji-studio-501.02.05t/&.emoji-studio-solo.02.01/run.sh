#!/bin/bash

# emoji-studio-standalone run script
# Compiles and launches the Manager and Host components.

echo "🔨 Compiling Emoji Studio..."

# Compile the Op
gcc -O2 emoji-xtract.c -o emoji-xtract -lm
if [ $? -ne 0 ]; then echo "❌ Failed to compile emoji-xtract"; exit 1; fi

# Compile the Manager
gcc -O2 manager/emoji-studio_manager.c -o manager/emoji-studio_manager
if [ $? -ne 0 ]; then echo "❌ Failed to compile manager"; exit 1; fi

# Compile the Host
gcc -O2 emoji-studio_host.c -o emoji-studio_host -I/usr/include/freetype2 -I/usr/include/libpng16 -lGL -lGLU -lglut -lm -lfreetype
if [ $? -ne 0 ]; then echo "❌ Failed to compile host"; exit 1; fi

echo "🚀 Launching Standalone Demo..."

# Ensure a clean session
rm -f session/history.txt session/frame_changed.txt
mkdir -p pieces session manager

# Start the Manager in the background
./manager/emoji-studio_manager &
MANAGER_PID=$!

# Function to cleanup on exit
cleanup() {
    echo "🛑 Shutting down..."
    kill $MANAGER_PID
    exit
}

# Trap exit signals
trap cleanup INT TERM

# Start the Host (this blocks until closed)
./emoji-studio_host

# Cleanup when Host exits
cleanup
