#!/usr/bin/env python3
import sys, os

tomom = os.environ.get("TOMOM", ".")
os.chdir(tomom)

subj = sys.argv[1]
boost_words = set(sys.argv[2:])

vocab_file = f"curriculum/{subj}/{subj}.txt"
layer_file = f"curriculum/{subj}/output_layer.txt"

with open(vocab_file) as f:
    next(f)
    entries = []
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 2:
            entries.append(parts[1])

vs = len(entries)
word_to_idx = {w: i for i, w in enumerate(entries)}
found = set(w for w in boost_words if w in word_to_idx)
missing = boost_words - found
print(f"Vocab size: {vs}")
print(f"Boosting: {sorted(found)}")
if missing:
    print(f"Missing: {sorted(missing)}")

if not found:
    print("No matching words found")
    sys.exit(1)

with open(layer_file) as f:
    lines = f.readlines()

HIDDEN_DIM = 32
matrix = []
biases = []
for line in lines:
    vals = [float(x) for x in line.strip().split()]
    if len(vals) == vs:
        if len(matrix) < HIDDEN_DIM:
            matrix.append(vals)
        else:
            biases = vals
    elif vals:
        biases = vals

print(f"Matrix: {len(matrix)} rows, biases: {len(biases) if biases else 0}")

boost = 0.5
for w in found:
    idx = word_to_idx[w]
    for row in matrix:
        row[idx] += boost
    if biases:
        biases[idx] += boost
    print(f"  Boosted '{w}' (col {idx}) by +{boost}")

with open(layer_file, "w") as f:
    for row in matrix:
        f.write(" ".join(f"{v:.9g}" for v in row) + "\n")
    if biases:
        f.write(" ".join(f"{v:.9g}" for v in biases) + "\n")
print(f"Updated {layer_file}")
