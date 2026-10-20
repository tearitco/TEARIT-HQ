#!/bin/bash
set -e
TOMOM="$(cd "$(dirname "$0")/.." && pwd)"
cd "$TOMOM"

for subj in "$@"; do
    echo ""
    echo "=== Tuning $subj ==="
    
    # Get baseline loss
    base_loss=$(grep '^[0-9]' "curriculum/$subj/loss.txt" 2>/dev/null | tail -1 || echo "0")
    echo "  Baseline loss: $base_loss"
    
    # Backup current vocab
    cp "curriculum/$subj/$subj.txt" "harnec/${subj}.vocab.before_tune.bak"
    
    # Get LLM suggestions using tune.py
    python3 "harnec/tune.py" \
        "curriculum/$subj/$subj.txt" \
        "Boost the weight field (5th column) for the most central domain-specific words in this subject to values between 0.85-1.0" \
        "boost_central_words" \
        "harnec/${subj}.edits.txt" 2>&1 | tail -3
    
    # Check if edits were made
    if [ -s "harnec/${subj}.edits.txt" ]; then
        echo "  Applying edits..."
        # Apply the edits
        python3 << PYEOF
import sys, os
os.chdir("$TOMOM")
subj = "$subj"
edits_file = f"harnec/{subj}.edits.txt"
vocab_file = f"curriculum/{subj}/{subj}.txt"

with open(vocab_file) as f:
    lines = f.readlines()

header = lines[0]
content = lines[1:]
updated = []
for line in content:
    parts = line.strip().split()
    if len(parts) >= 9:
        word = parts[1]
        # Check if this word has an edit
        with open(edits_file) as ef:
            for eline in ef:
                eline = eline.strip()
                if eline and word in eline:
                    eparts = eline.split()
                    if len(eparts) >= 3:
                        try:
                            new_val = float(eparts[-1])
                            if 0 <= new_val <= 1.5:
                                parts[4] = f"{new_val:.6f}"  # weight field
                        except: pass
    updated.append(" ".join(parts) + "\n")

with open(vocab_file, "w") as f:
    f.write(header)
    f.writelines(updated)
print("  Edits applied")
PYEOF
        
        # Fine-tune for 10 epochs
        sed -i 's/epochs=50/epochs=10/' "curriculum/$subj/config.txt"
        echo "  Training 10 epochs..."
        ./"+x/trainer.+x" "curriculum/$subj/$subj.txt" 2>&1 | grep "Loss" | tail -1
        
        new_loss=$(grep '^[0-9]' "corpus/$subj/loss.txt" 2>/dev/null | tail -1 || grep '^[0-9]' "curriculum/$subj/loss.txt" 2>/dev/null | tail -1)
        echo "  New loss: $new_loss"
        
        # Compare
        python3 -c "
base = float('$base_loss')
new = float('$new_loss')
delta = new - base
if delta < 0:
    print(f'  IMPROVED: {base:.6f} -> {new:.6f} (delta={delta:.6f})')
    with open('harnec/${subj}.results.txt', 'a') as f:
        f.write(f'tuned\t{base:.8f}\t{new:.8f}\n')
elif delta > 0:
    print(f'  REVERT: {base:.6f} -> {new:.6f} (delta={delta:.6f})')
    cp harnec/${subj}.vocab.before_tune.bak curriculum/$subj/$subj.txt
    # Restore trained matrices would need backup... for now just print
    with open('harnec/${subj}.results.txt', 'a') as f:
        f.write(f'revert\t{base:.8f}\t{new:.8f}\n')
else:
    print(f'  SAME: {base:.6f} == {new:.6f}')
"
    else
        echo "  No edits suggested, skipping training"
    fi
    
    # Reset config
    sed -i 's/epochs=10/epochs=50/' "curriculum/$subj/config.txt"
done
