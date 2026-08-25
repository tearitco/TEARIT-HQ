#!/usr/bin/env bash
# iqa-fork/run.sh - ONE distillation + train + eval iteration.
# IQA distillation: teacher (llama3-groq-tool-use) authors a curriculum
# sentence on a topic; student (gemma3:270m) attempts the same; the
# teacher's line is distilled into the iqabod corpus on Linux; iqabod
# trains; we generate and log KPIs. All model compute on LAN (Mac API),
# training on Linux node, orchestration only on this box.
#
# Usage: run.sh [epochs] [topic]    (defaults: epochs=10, topic=rotated)
# Exit 0 = ok, 1 = infra fail.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
TOPICS="$DIR/topics.txt"
LOG="$DIR/log.pdl"
EPOCHS="${1:-10}"
TOPIC="${2:-}"
TEACHER="llama3-groq-tool-use"
STUDENT="gemma3:270m"
LINUX="jb@10.0.0.187"
LPASS="root"
STORE="conv-g1"

SSH() { sshpass -p "$LPASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=8 "$LINUX" "$@"; }

# --- pick topic ---------------------------------------------------------
if [ -z "$TOPIC" ]; then
    [ -f "$TOPICS" ] || { echo "no topics.txt"; exit 1; }
    TOPIC=$(grep -v '^#' "$TOPICS" | sed -n '1p')
    if [ -z "$TOPIC" ]; then echo "no topics left"; exit 1; fi
    grep -v '^#' "$TOPICS" | tail -n +2 > "$TOPICS.tmp"
    echo "$TOPIC" >> "$TOPICS.tmp"
    { grep '^#' "$TOPICS"; cat "$TOPICS.tmp"; } > "$TOPICS.rot"
    mv "$TOPICS.rot" "$TOPICS"
fi
TS=$(date -u +%s)

# --- teacher + student attempts ----------------------------------------
TEACH=$(bash "$DIR/ask.sh" "$TEACHER" "Write one short, simple, factual sentence a beginner should learn, about: $TOPIC. One sentence only, no preamble.")
STUD=$(bash "$DIR/ask.sh" "$STUDENT" "Write one short, simple, factual sentence a beginner should learn, about: $TOPIC. One sentence only, no preamble.")
[ -n "$TEACH" ] || TEACH="ASK|ERROR"
[ -n "$STUD" ] || STUD="ASK|ERROR"

# --- distill teacher line into corpus on Linux --------------------------
if echo "$TEACH" | grep -q "ASK|ERROR"; then
    echo "RUN|$TS|ERROR|teacher_failed topic=$TOPIC"
    exit 1
fi
SANIT=$(printf '%s' "$TEACH" | tr -d '\r' | tr '\n' ' ' | LC_ALL=C sed 's/[^ -~]/ /g; s/  */ /g')
echo "$SANIT" > "$DIR/.distill_line.txt"
if ! sshpass -p "$LPASS" scp -o StrictHostKeyChecking=no -o ConnectTimeout=8 "$DIR/.distill_line.txt" "$LINUX:~/iqabod-store/$STORE/distill_line.txt" 2>/dev/null; then
    echo "RUN|$TS|ERROR|scp_failed topic=$TOPIC"
    exit 1
fi
if ! SSH "cd ~/iqabod-store/$STORE && cat distill_line.txt >> corpus.txt && rm -f distill_line.txt" 2>/dev/null; then
    echo "RUN|$TS|ERROR|corpus_append_failed topic=$TOPIC"
    exit 1
fi

# --- lock: one trainer at a time (two trainers corrupted weights -> nan) -
LOCK="~/iqabod-store/$STORE/.train.lock"
if ! SSH "mkdir $LOCK 2>/dev/null"; then
    echo "RUN|$TS|SKIP|trainer_busy topic=$TOPIC (another iteration is training)"
    exit 3
fi

# --- train on Linux (holding the lock) ----------------------------------
TRAIN_OUT=$(SSH "cd ~/iqabod-store/$STORE && ./+x/main_orchestrator.+x train corpus.txt $EPOCHS 2>&1" 2>/dev/null)
LOSS=$(echo "$TRAIN_OUT" | grep -oE "Average Loss: (nan|[0-9.e+-]+)" | tail -1 | sed 's/Average Loss: //')
if [ -z "$LOSS" ] || [ "$LOSS" = "nan" ] || [ "$LOSS" = "-nan" ]; then LOSS="NA"; fi

# --- release the lock ----------------------------------------------------
SSH "rmdir ~/iqabod-store/$STORE/.train.lock" 2>/dev/null

# --- eval: generate on a FIXED in-vocab prompt so reports compare over time -
GEN_PROMPT="How are you?"
GEN_SAMPLE=$(SSH "cd ~/iqabod-store/$STORE && ./+x/generation_module.+x generate curriculum/corpus/corpus.txt 1.0 0.9 80 \"$GEN_PROMPT\" 2>/dev/null" 2>/dev/null | grep -v "Generation mode\|Loading model\|Loaded model\|Generating text\|Generating attention\|Attention map\|Generation completed" | sed -n 's/.*Final generated text: //p' | tr '\n' ' ' | head -c 140)

# --- log KPI ------------------------------------------------------------
TLEN=${#TEACH}; SLEN=${#STUD}
echo "IQA_DISTILL|$TS|topic=$TOPIC|teacher_len=$TLEN|student_len=$SLEN|loss=$LOSS|gen_sample=\"$GEN_SAMPLE\"" | tee -a "$LOG"
echo "IQA_TEACH|$TS|$TEACH" | tee -a "$LOG"
echo "IQA_STUD|$TS|$STUD" | tee -a "$LOG"
exit 0
