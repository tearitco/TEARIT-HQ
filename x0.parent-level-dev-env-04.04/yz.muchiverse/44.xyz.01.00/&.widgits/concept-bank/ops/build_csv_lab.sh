#!/bin/sh
# build_csv_lab.sh - compile csv_lab (self-contained C: jobs, batch, prompt, parse, lint, call, quota, merge, ui, pipeline). -Wno-unknown-warning-option first so clang on macOS ignores gcc-only flags.
# The Groq path also needs the transport it calls: ^.hai-horn/ops/+x/horn_chat_backend.+x (built by ^.hai-horn/scripts/build.sh).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O2 -o +x/csv_lab.+x csv_lab.c
echo "OK +x/csv_lab.+x"
