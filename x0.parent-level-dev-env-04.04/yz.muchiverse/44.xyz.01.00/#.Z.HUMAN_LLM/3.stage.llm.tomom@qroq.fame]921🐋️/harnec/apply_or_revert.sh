#!/bin/bash
set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."
SUBJ="${1:?usage: apply_or_revert.sh <subject> <old_loss>}"
OLD_LOSS="${2:?old loss}"
BACKUP="harnec/${SUBJ}.vocab.bak"
RESULTS="harnec/${SUBJ}.results.txt"
echo "Old loss: $OLD_LOSS"
echo "Cleaning..."
rm -f "curriculum/$SUBJ/attention_model.txt" "curriculum/$SUBJ/mlp_model.txt" \
      "curriculum/$SUBJ/output_layer.txt" "curriculum/$SUBJ/optimizer_state.txt" \
      "curriculum/$SUBJ/loss.txt" "curriculum/$SUBJ/"*.m.txt "curriculum/$SUBJ/"*.v.txt \
      "curriculum/$SUBJ/predictions.txt" "curriculum/$SUBJ/grad_*.txt" \
      "curriculum/$SUBJ/attn_scores*.txt" "curriculum/$SUBJ/context.txt" \
      "curriculum/$SUBJ/hidden_state.txt" "curriculum/$SUBJ/q.txt" \
      "curriculum/$SUBJ/k.txt" "curriculum/$SUBJ/v.txt"
ORIG_CONFIG="curriculum/$SUBJ/config.txt"
TEMP_CONFIG="harnec/${SUBJ}.config.bak"
cp "$ORIG_CONFIG" "$TEMP_CONFIG" 2>/dev/null || true
cat > "$ORIG_CONFIG" << 'CFG'
epochs=10
learning_rate=0.001
beta1=0.9
beta2=0.999
max_gradient_norm=1.0
attention_dropout=0.0
mlp_dropout=0.0
attention_noise=0.0
attention_weights_noise=0.0
causal_attention=0
CFG
echo "Retraining for 10 epochs..."
./+x/trainer.+x "curriculum/$SUBJ/$SUBJ.txt" 2>/dev/null || true
NEW_LOSS=$(tail -1 "curriculum/$SUBJ/loss.txt" 2>/dev/null || echo "$OLD_LOSS")
echo "New loss: $NEW_LOSS"
cp "$TEMP_CONFIG" "$ORIG_CONFIG" 2>/dev/null || true
python3 -c "
old=float('$OLD_LOSS'); new=float('$NEW_LOSS')
print(f'old={old:.6f} new={new:.6f} delta={new-old:+.6f}')
if new < old:
    print('RESULT: KEEP'); open('$RESULTS','a').write(f'keep\t{old}\t{new}\n')
else:
    print('RESULT: REVERT'); import shutil; shutil.copy2('$BACKUP','curriculum/$SUBJ/$SUBJ.txt'); open('$RESULTS','a').write(f'revert\t{old}\t{new}\n')
"
