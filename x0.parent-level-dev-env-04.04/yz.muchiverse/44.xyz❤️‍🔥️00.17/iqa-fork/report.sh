#!/usr/bin/env bash
# iqa-fork/report.sh - Summarize IQA distillation into reports, including
# CONVERSATION QUALITY examples (the model's own generated speech on the
# fixed prompt "How are you?") with a delta vs the previous report, so we
# can see if it's getting better at conversation over time.
# Usage:
#   report.sh            -> today's summary (last 24h window, default)
#   report.sh 24         -> last 24 hours summary
#   report.sh 168        -> last 7 days summary
# Writes iqa-fork/reports/REPORT-<date>.txt each run (overwrites per day).
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DIR/log.pdl"
RPT_DIR="$DIR/reports"
mkdir -p "$RPT_DIR"

HOURS="${1:-24}"

TS=$(date -u +%Y-%m-%dT%H:%M:%SZ)
TODAY=$(date -u +%Y-%m-%d)
NOW=$(date -u +%s)
CUT=$((NOW - HOURS * 3600))
OUT="$RPT_DIR/REPORT-$TODAY.txt"

# previous report of the same window for delta comparison
PREV=""
if [ "$HOURS" -ge 24 ]; then
    YESTERDAY=$(date -u -d "@$((NOW - 86400))" +%Y-%m-%d 2>/dev/null || true)
    [ -n "$YESTERDAY" ] && PREV="$RPT_DIR/REPORT-$YESTERDAY.txt"
    [ -f "$PREV" ] || PREV=""
fi

{
    echo "IQA_REPORT|$TS|window_hours=$HOURS"
    echo "------------------------------------"
    [ -f "$LOG" ] || { echo "no log.pdl yet"; exit 0; }

    awk -v cut="$CUT" -v now="$NOW" '
        function clean(g) {
            sub(/^" */, "", g); sub(/ *"$/, "", g);
            if (g ~ /Generation mode/) return "";
            if (g ~ /^<PAD>|<PAD>$/ || g == "" || g ~ /^ *$/) return "";
            return g;
        }
        $0 ~ /^IQA_DISTILL\|/ {
            split($0, a, "|");
            ts = a[2];
            if (ts < cut || ts > now + 300) next;
            total++;
            gen = "";
            loss = -1;
            topic = "?";
            for (i = 3; i <= 10; i++) {
                if (a[i] ~ /^topic=/) topic = substr(a[i], 7);
                if (a[i] ~ /^loss=/) { sub(/^loss=/, "", a[i]); if (a[i] ~ /^[0-9]/) loss = a[i] + 0; }
                if (a[i] ~ /^gen_sample=/) {
                    g = a[i]; sub(/^gen_sample=/, "", g);
                    g = clean(g);
                    if (g != "") gen = g;
                }
            }
            if (loss >= 0) { n++; sum += loss; if (loss < min || n == 1) min = loss; if (loss > max || n == 1) max = loss; }
            if (total == 1) first_sample = gen;
            if (gen != "") { last_sample = gen; last_ts = ts; last_topic = topic; }
        }
        END {
            if (total == 0) { print "no iterations in window"; exit 0; }
            printf "iterations=%d avg_loss=%.4f min_loss=%.4f max_loss=%.4f\n", n, n ? sum/n : 0, min, max;
            printf "first_sample=\"%s\"\n", first_sample;
            printf "last_sample=\"%s\"\n", last_sample;
        }
    ' "$LOG" 2>/dev/null

    # conversation examples: teacher goal + student attempt of the LAST iteration
    echo "------------------------------------"
    echo "latest_teacher=$(awk -F'|' '$1=="IQA_TEACH"{t=$3} END{print t}' "$LOG" 2>/dev/null)"
    echo "latest_student=$(awk -F'|' '$1=="IQA_STUD"{s=$3} END{print s}' "$LOG" 2>/dev/null)"

    # delta vs previous report
    if [ -n "$PREV" ]; then
        echo "------------------------------------"
        echo "delta_vs_prev_report=$PREV"
        echo "prev_loss=$(grep -oE 'avg_loss=[0-9.]+' "$PREV" | head -1 | cut -d= -f2)"
        echo "prev_sample=$(grep -oE 'last_sample="[^"]*"' "$PREV" | head -1 | sed 's/last_sample=//')"
    fi
} | tee "$OUT"
