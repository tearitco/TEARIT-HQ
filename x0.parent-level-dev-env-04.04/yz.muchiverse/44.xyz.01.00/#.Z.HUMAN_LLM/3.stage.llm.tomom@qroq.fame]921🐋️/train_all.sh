#!/bin/bash
cd "/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️"

for vocab in $(cat curriculum_bank.txt); do
    dir=$(dirname "$vocab")
    if [ -f "$dir/loss.txt" ]; then
        echo "[SKIP] $(basename $vocab)"
        continue
    fi
    echo "[START] $vocab"
    ./+x/trainer.+x "$vocab" 2>&1 | grep "Epoch 50/"
    echo "[DONE] $vocab"
done
