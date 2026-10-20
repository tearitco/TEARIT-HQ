#!/bin/bash
set -eu
TIM="/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️"
cd "$TIM"

for subj in Mathematics Astronomy Biology Chemistry Economics Geography History Literature Physics Programming; do
    echo ""
    echo "=== Processing $subj ==="
    
    # Get baseline loss (last value in loss.txt before any revert)
    base_loss=$(grep '^[0-9]' "corpus/$subj/loss.txt" 2>/dev/null | tail -1 || true)
    if [ -z "$base_loss" ]; then
        base_loss=$(grep '^[0-9]' "curriculum/$subj/loss.txt" 2>/dev/null | tail -1 || echo "0")
    fi
    
    # Check if we have a trained baseline backup
    if [ -f "harnec/${subj}.vocab.trained.bak" ]; then
        echo "  Has trained backup, proceeding"
        
        # Get LLM suggestions for weight boosts
        words=$(tail -n +2 "curriculum/$subj/$subj.txt" | awk '{print $2}' | head -100 | tr '\n' ' ')
        
        # Ask llama3 for central words to boost
        boosts=$(curl -s --max-time 30 http://127.0.0.1:11434/api/generate -d "$(python3 -c "
import json
prompt = 'List 5-10 central words from this vocabulary (one per line, no numbers): $words'
print(json.dumps({'model':'llama3','prompt':prompt,'stream':False}))
")" | python3 -c "
import json,sys
try:
    r=json.load(sys.stdin)['response']
    for line in r.split('\n'):
        w=line.strip().split()[0].lower() if line.strip() else ''
        if w and not w.replace('.','').replace(',','').isalpha(): continue
        if len(w)>2: print(w)
except: pass
" 2>/dev/null | head -10)
        
        if [ -n "$boosts" ]; then
            echo "  LLM suggested boosts: $boosts"
            # Apply weight boosts (set weight field to 0.95 for each)
            python3 << PYEOF
import os, sys
os.chdir(os.environ.get('TIM', "$TIM"))
boosts = """$boosts""".strip().split('\n')
boosts = [b.strip() for b in boosts if b.strip()]

vocab_file = "curriculum/$subj/$subj.txt"
with open(vocab_file) as f:
    lines = f.readlines()
    
header = lines[0]
updated = []
for line in lines[1:]:
    parts = line.strip().split()
    if len(parts) >= 9:
        word = parts[1]
        if word in boosts:
            parts[4] = "0.950000"  # Boost weight to 0.95
    updated.append(" ".join(parts) + "\n")

with open(vocab_file, "w") as f:
    f.write(header)
    f.writelines(updated)
print(f"  Applied {len(boosts)} weight boosts")
PYEOF
        fi
        
        # Fine-tune for 10 epochs
        sed -i 's/epochs=50/epochs=10/' "curriculum/$subj/config.txt"
        ./+x/trainer.+x "curriculum/$subj/$subj.txt" 2>&1 | grep "Loss" | tail -1
        
        # Compare loss
        new_loss=$(grep '^[0-9]' "corpus/$subj/loss.txt" 2>/dev/null | tail -1 || grep '^[0-9]' "curriculum/$subj/loss.txt" 2>/dev/null | tail -1)
        echo "  $subj: baseline=$base_loss new=$new_loss"
        
        if [ "$new_loss" != "" ] && [ "$base_loss" != "0" ]; then
            python3 -c "
base = float('$base_loss')
new = float('$new_loss')
delta = new - base
print(f'  delta={delta:.6f}')"
        fi
    else
        echo "  No trained backup, skipping"
    fi
    
    # Reset config
    sed -i 's/epochs=10/epochs=50/' "curriculum/$subj/config.txt" 2>/dev/null
done
