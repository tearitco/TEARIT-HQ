#!/bin/bash
set -e

TOMOM="$(cd "$(dirname "$0")/.." && pwd)"
cd "$TOMOM"

SUBJECTS=$(awk -F'/' '{print $2}' curriculum_bank.txt | sort -u)

for subj in $SUBJECTS; do
    echo ""
    echo "=== Tuning $subj ==="

    VOCAB="curriculum/$subj/$subj.txt"
    LOSS_FILE="curriculum/$subj/loss.txt"

    # Check if already tuned in Phase G
    if [ -f "harnec/${subj}.phaseG.results.txt" ]; then
        echo "  Already tuned (Phase G), skipping"
        continue
    fi

    # Get baseline loss (last line of loss.txt)
    base_loss=$(tail -1 "$LOSS_FILE" 2>/dev/null || echo "0")
    echo "  Baseline loss: $base_loss"

    # Backup vocab AND trained model files for full auto-revert
    cp "$VOCAB" "harnec/${subj}.vocab.before_tune.bak"
    cp "curriculum/$subj/attention_model.txt" "harnec/${subj}.attention.trained.bak" 2>/dev/null || true
    cp "curriculum/$subj/mlp_model.txt" "harnec/${subj}.mlp.trained.bak" 2>/dev/null || true
    cp "curriculum/$subj/output_layer.txt" "harnec/${subj}.output_layer.trained.bak" 2>/dev/null || true
    cp "curriculum/$subj/optimizer_state.txt" "harnec/${subj}.optimizer.trained.bak" 2>/dev/null || true

    # Get LLM suggestions using tune.py
    echo "  Requesting LLM suggestions..."
    python3 "harnec/tune.py" \
        "$VOCAB" \
        "Boost the weight field (5th column) for the most central domain-specific words in this subject to values between 0.85-1.0" \
        "boost_central_words" \
        "harnec/${subj}.edits.txt" 2>&1 | tail -5

    # Check if edits were made
    if [ -s "harnec/${subj}.edits.txt" ]; then
        echo "  Applying edits..."
        python3 << PYEOF
import sys, os, shutil
os.chdir("$TOMOM")
subj = "$subj"
edits_file = f"harnec/{subj}.edits.txt"
vocab_file = f"curriculum/{subj}/{subj}.txt"
loss_file = f"curriculum/{subj}/loss.txt"
results_file = f"harnec/{subj}.phaseG.results.txt"

with open(vocab_file) as f:
    lines = f.readlines()
with open(edits_file) as f:
    edit_lines = [l.strip() for l in f.readlines()]

header = lines[0]
content = lines[1:]
edit_map = {}
for eline in edit_lines:
    if not eline:
        continue
    parts = eline.split('\t')
    if len(parts) >= 3:
        word = parts[0].strip().lower()
        field = parts[1].strip().lower()
        try:
            val = float(parts[2])
            if 0 <= val <= 1.5:
                edit_map[(word, field)] = val
        except: pass

updated = []
for line in content:
    parts = line.strip().split()
    if len(parts) >= 9:
        word = parts[1].strip().lower()
        for (ew, ef), ev in edit_map.items():
            if word == ew and ef == 'weight':
                try:
                    parts[4] = f"{ev:.6f}"
                except: pass
    updated.append(" ".join(parts) + "\n")

with open(vocab_file, "w") as f:
    f.write(header)
    f.writelines(updated)
print("  Edits applied")

# Train 10 epochs from trained baseline
import shutil as sh2
sh2.copy2(vocab_file.replace(f"{subj}.txt", "config.txt"), f"harnec/{subj}.config.bak")
config_file = f"curriculum/{subj}/config.txt"
with open(config_file) as f:
    orig_config = f.read()
with open(config_file, "w") as f:
    f.write(orig_config.replace("epochs=50", "epochs=10"))
PYEOF

        echo "  Training 10 epochs..."
        ./"+x/trainer.+x" "$VOCAB" 2>&1 | grep "Loss" | tail -1

        # Restore config
        cp "harnec/${subj}.config.bak" "curriculum/$subj/config.txt"

        new_loss=$(tail -1 "$LOSS_FILE" 2>/dev/null || echo "$base_loss")
        echo "  New loss: $new_loss"

        # Compare and auto-revert if worse
        python3 << PYEOF
import shutil
subj = "$subj"
base = float('$base_loss')
new = float('$new_loss')
delta = new - base
print(f'  delta={delta:+.6f}')
results_file = f'harnec/{subj}.phaseG.results.txt'
vocab_file = f'curriculum/{subj}/{subj}.txt'
if new < base:
    print(f'  IMPROVED: {base:.6f} -> {new:.6f}')
    with open(results_file, 'w') as f:
        f.write(f'keep\t{base:.8f}\t{new:.8f}\n')
elif new > base:
    print(f'  REVERT: {base:.6f} -> {new:.6f}')
    shutil.copy2(f'harnec/{subj}.vocab.before_tune.bak', vocab_file)
    shutil.copy2(f'harnec/{subj}.attention.trained.bak', f'curriculum/{subj}/attention_model.txt')
    shutil.copy2(f'harnec/{subj}.mlp.trained.bak', f'curriculum/{subj}/mlp_model.txt')
    shutil.copy2(f'harnec/{subj}.output_layer.trained.bak', f'curriculum/{subj}/output_layer.txt')
    shutil.copy2(f'harnec/{subj}.optimizer.trained.bak', f'curriculum/{subj}/optimizer_state.txt')
    with open(results_file, 'w') as f:
        f.write(f'revert\t{base:.8f}\t{new:.8f}\n')
else:
    print(f'  SAME: {base:.6f}')
    with open(results_file, 'w') as f:
        f.write(f'same\t{base:.8f}\t{new:.8f}\n')
PYEOF
    else
        echo "  No edits suggested, skipping training"
        echo -e "skip\t${base_loss}\t${base_loss}" > "harnec/${subj}.phaseG.results.txt"
    fi
done

echo ""
echo "=== Phase G Complete ==="
echo "Results summary:"
for f in harnec/*.phaseG.results.txt; do
    [ -f "$f" ] && echo "  $(basename $f .phaseG.results.txt): $(cat $f)"
done
