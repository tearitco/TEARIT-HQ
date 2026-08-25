#!/bin/bash

# TPMOS REMOTE OLLAMA FIX
# Run this on the Mac (10.0.0.144) to allow other machines to connect.

echo "--- Stopping any existing Ollama processes ---"
killall Ollama 2>/dev/null
killall ollama 2>/dev/null
sleep 2

echo "--- Setting OLLAMA_HOST to 0.0.0.0 (Allowing Remote Connections) ---"
export OLLAMA_HOST=0.0.0.0

echo "--- Starting Ollama in the background ---"
# We use nohup so it keeps running even if you close the terminal
nohup ollama serve > ollama_remote.log 2>&1 &

echo ""
echo "Ollama is now starting on 0.0.0.0:11434"
echo "Check 'ollama_remote.log' for details."
echo "You can now try connecting from TPMOS again."
