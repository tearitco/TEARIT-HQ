#!/bin/bash
set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."

SUBJ="${1:?usage: tune_output.sh <subject> <word1> <word2> ...}"
shift
BOOST_WORDS="$@"
VOCAB="curriculum/$SUBJ/$SUBJ.txt"
OUT_LAYER="curriculum/$SUBJ/output_layer.txt"

if [ ! -f "$VOCAB" ] || [ ! -f "$OUT_LAYER" ]; then
    echo "ERROR: files not found"; exit 1
fi

cp "$OUT_LAYER" "harnec/${SUBJ}.output_layer.bak"
echo "Backed up output_layer.txt"

python3 - "$VOCAB" "$OUT_LAYER" $BOOST_WORDS << 'PYEOF'
import sys
vocab_file = sys.argv[1]
layer_file = sys.argv[2]
boost_words = set(sys.argv[3:])

with open(vocab_file) as f:
    next(f)
    vocab_entries = [line.strip().split()[1] for line in f if line.strip().split() and len(line.strip().split()) >= 2]

vs = len(vocab_entries)
word_to_idx = {w: i for i, w in enumerate(vocab_entries)}
found = set(w for w in boost_words if w in word_to_idx)
print(f"Vocab: {vs}, Boosting: {found}")

if not found:
    print("No matching words found"); sys.exit(1)

with open(layer_file) as f:
    lines = f.readlines()

matrix = []
biases = []
for line in lines:
    vals = [float(x) for x in line.strip().split()]
    if len(vals) == vs:
        if len(matrix) < 32:
            matrix.append(vals)
        else:
            biases = vals
    elif vals:
        biases = vals

boost = 0.5
for w in found:
    idx = word_to_idx[w]
    for row in matrix:
        row[idx] += boost
    if biases: biases[idx] += boost
    print(f"  Boosted {w} col {idx} by +{boost}")

with open(layer_file, 'w') as f:
    for row in matrix:
        f.write(' '.join(f'{v:.9g}' for v in row) + '
')
    if biases:
        f.write(' '.join(f'{v:.9g}' for v in biases) + '
')
print(f"Updated {layer_file}")
PYEOF
echo "Use: ./+x/chatbot_moe_v1.+x curriculum_bank.txt "test prompt" 10 5"
