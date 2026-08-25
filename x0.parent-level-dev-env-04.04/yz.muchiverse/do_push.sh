#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"
gh repo create tearitco/TEARIT-HQ --public --source=. --remote=origin --push
echo "--- done ---"
gh repo view tearitco/TEARIT-HQ --web=false --json url -q .url
