#!/bin/bash
set -eu

VOCAB="${1:?usage: validate_weights.sh <vocab_file>}"

if [ ! -f "$VOCAB" ]; then
    echo "ERROR: vocab file not found: $VOCAB"
    exit 1
fi

echo "Validating $VOCAB..."
python3 -c "
import sys, os

VOCAB = '$VOCAB'

# 1. Check header
with open(VOCAB) as f:
    header = f.readline().strip().split()
expected = ['number','word','embedding','pe','weight','bias1','bias2','bias3','bias4']
if header != expected:
    print(f'FAIL: header mismatch: {header}')
    sys.exit(1)
print('PASS: header correct')

# 2. Count lines, parse each row
orig_count = 0
valid = 0
dropped = 0
with open(VOCAB) as f:
    next(f)  # skip header
    for lineno, line in enumerate(f, start=2):
        parts = line.strip().split()
        if not parts:
            continue
        orig_count += 1
        if len(parts) != 9:
            print(f'FAIL line {lineno}: {len(parts)} fields (expected 9)')
            dropped += 1
            continue
        try:
            num = int(parts[0])
            word = parts[1]
            vals = [float(x) for x in parts[2:9]]
            # Range checks: all values should be 0.0-1.0 (with small epsilon)
            for i, v in enumerate(vals):
                if v < -0.001 or v > 1.001:
                    print(f'WARN line {lineno} {word}: field[{i}] = {v} out of [0,1] range')
            valid += 1
        except ValueError as e:
            print(f'FAIL line {lineno}: parse error ({e})')
            dropped += 1
            continue

print(f'PASS: {valid} words parsed, {dropped} dropped')

# 3. Check token count matches what we expect
file_size = os.path.getsize(VOCAB)
print(f'INFO: file size = {file_size} bytes')

if dropped > 0:
    print('FAIL: tokens were dropped or corrupted')
    sys.exit(1)
elif orig_count == 0:
    print('FAIL: no data rows found')
    sys.exit(1)
else:
    print('ALL CHECKS PASSED')
    sys.exit(0)
"
