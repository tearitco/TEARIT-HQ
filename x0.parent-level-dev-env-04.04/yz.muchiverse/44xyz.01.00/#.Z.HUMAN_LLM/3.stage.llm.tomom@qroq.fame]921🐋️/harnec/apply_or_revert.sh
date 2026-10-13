#!/bin/bash
set -eu

SUBJ="${1:?usage: apply_or_revert.sh <subject> <old_loss_file>}"
OLD_LOSS_FILE="${2:?old loss file path}"

TOMOM_DIR="/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44xyz.01.00/#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️"
VOCAB="$TOMOM_DIR/curriculum/$SUBJ/$SUBJ.txt"
BACKUP="$TOMOM_DIR/harnec/${SUBJ}.vocab.bak"
RESULTS="$TOMOM_DIR/harnec/${SUBJ}.results.txt"

# Read old loss
OLD_LOSS=$(tail -1 "$OLD_LOSS_FILE")
echo "Old loss: $OLD_LOSS"

# Clean trained state for this subject
echo "Cleaning trained matrices for $SUBJ..."
rm -f "$TOMOM_DIR/curriculum/$SUBJ/attention_model.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/mlp_model.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/output_layer.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/optimizer_state.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/loss.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/"*.m.txt "$TOMOM_DIR/curriculum/$SUBJ/"*.v.txt \
      "$TOMOM_DIR/curriculum/$SUBJ/predictions.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/grad_*.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/attn_scores*.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/context.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/hidden_state.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/"q.txt "$TOMOM_DIR/curriculum/$SUBJ/k.txt" \
      "$TOMOM_DIR/curriculum/$SUBJ/v.txt"

# Retrain with 5 epochs (quick eval)
echo "Retraining $SUBJ for 5 epochs..."
cd "$TOMOM_DIR"

# Temporarily override epochs to 5
ORIG_CONFIG="$TOMOM_DIR/curriculum/$SUBJ/config.txt"
TEMP_CONFIG="$TOMOM_DIR/curriculum/$SUBJ/config.txt.bak"
cp "$ORIG_CONFIG" "$TEMP_CONFIG"
cat > "$ORIG_CONFIG" << 'EOF'
epochs=5
learning_rate=0.001
beta1=0.9
beta2=0.999
max_gradient_norm=1.0
attention_dropout=0.0
mlp_dropout=0.0
attention_noise=0.0
attention_weights_noise=0.0
causal_attention=0
EOF

./+x/trainer.+x "curriculum/$SUBJ/$SUBJ.txt" 2>/dev/null
NEW_LOSS=$(tail -1 "curriculum/$SUBJ/loss.txt")
echo "New loss: $NEW_LOSS"

# Restore original config
mv "$TEMP_CONFIG" "$ORIG_CONFIG"

# Compare and apply/revert
python3 -c "
old = float('$OLD_LOSS')
new = float('$NEW_LOSS')
print(f'Loss comparison: old={old:.6f} new={new:.6f} delta={new-old:.6f}')
if new < old:
    print('RESULT: KEEP — loss improved')
    with open('$RESULTS', 'a') as f:
        f.write(f'keep\t{old}\t{new}\n')
else:
    print('RESULT: REVERT — loss did not improve, restoring backup')
    import shutil
    shutil.copy2('$BACKUP', '$VOCAB')
    with open('$RESULTS', 'a') as f:
        f.write(f'revert\t{old}\t{new}\n')
"
