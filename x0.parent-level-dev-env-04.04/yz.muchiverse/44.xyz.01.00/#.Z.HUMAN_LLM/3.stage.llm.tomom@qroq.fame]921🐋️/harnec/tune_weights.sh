#!/bin/bash
set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."

SUBJ="${1:?usage: tune_weights.sh <subject> <prompt> <desired_behavior>}"
PROMPT="${2:?prompt required}"
BEHAVIOR="${3:?behavior required}"
VOCAB="curriculum/$SUBJ/$SUBJ.txt"

if [ ! -f "$VOCAB" ]; then echo "ERROR: not found: $VOCAB"; exit 1; fi

BACKUP="harnec/${SUBJ}.vocab.bak"
cp "$VOCAB" "$BACKUP"
echo "Backup saved to $BACKUP"

EDITS="harnec/${SUBJ}.edits.txt"

python3 "harnec/tune.py" "$VOCAB" "$PROMPT" "$BEHAVIOR" "$EDITS"
echo "=== Edits file ==="
cat "$EDITS"

python3 - "$VOCAB" "$EDITS" << 'PYEOF'
import sys
vocab_file, edits_file = sys.argv[1], sys.argv[2]
edits = {}
with open(edits_file) as f:
    for line in f:
        parts = line.strip().split('\t')
        if len(parts) == 3:
            edits.setdefault(parts[0], {})[parts[1]] = float(parts[2])
if not edits:
    print("No edits to apply"); sys.exit(0)
with open(vocab_file) as f:
    lines = f.readlines()
header = lines[0].strip().split()
field_idx = {name: i for i, name in enumerate(header)}
out = [lines[0]]
modified = 0
for line in lines[1:]:
    parts = line.strip().split()
    if not parts:
        out.append(line); continue
    word = parts[1]
    if word in edits:
        for field, val in edits[word].items():
            idx = field_idx.get(field, -1)
            if 0 <= idx < len(parts):
                old = parts[idx]
                parts[idx] = f'{val:.6f}'
                print(f'  {word}.{field}: {old} -> {parts[idx]}')
                modified += 1
    out.append(' '.join(parts) + '\n')
with open(vocab_file, 'w') as f:
    f.writelines(out)
print(f'Applied {modified} edits')
PYEOF
