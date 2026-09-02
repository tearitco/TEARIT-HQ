#!/bin/bash
# START_BUTTON_44.xyz — house root launcher
# Launches the START_BUTTON from the house root directory.
cd "$(dirname "$0")" || exit 1
exec "*.START_BUTTON"/button.sh run
