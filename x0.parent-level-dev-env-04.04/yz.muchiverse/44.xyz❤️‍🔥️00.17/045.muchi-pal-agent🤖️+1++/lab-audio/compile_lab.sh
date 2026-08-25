#!/bin/bash
# Compiles lab-audio/'s programs into lab-audio/+x/, matching IQABOD's own
# lab/compile_lab.sh convention (see #.ref/^.IQABOD-llm-06.00/lab/) - these
# are exploratory tools, not yet promoted into muchi-pal-agent's own ops/+x/
# build (see ROADMAP-models.txt §10.4/§10.2 - integration shape isn't
# decided yet).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
output_dir="$SCRIPT_DIR/+x"
mkdir -p "$output_dir"

echo "Compiling lab-audio programs..."

for name in wav_io fft synth; do
    gcc -Wall -Wextra -O2 "$SCRIPT_DIR/$name.c" -o "$output_dir/$name.+x" -lm
    if [ $? -eq 0 ]; then
        echo "  OK   $name.c -> +x/$name.+x"
    else
        echo "  FAIL $name.c"
    fi
done

echo "Done. Try: ./+x/wav_io.+x gen_test /tmp/test.wav 440 1.0 && ./+x/fft.+x /tmp/test.wav 2048 1024 /tmp/pitch.csv"
