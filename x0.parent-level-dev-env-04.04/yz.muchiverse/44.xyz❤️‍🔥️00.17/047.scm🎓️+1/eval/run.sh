#!/bin/sh
# eval/run.sh — SCM probe-bank baseline gate (design §9).
# For each probe: run `select --all` over SEEDS seeds. From the SCORE dump we
# know every phrase's score (uniform baseline = mean over all phrases) and the
# argmax phrase. Gate: the mean score of actually-SELECTED phrases must beat the
# uniform mean (sampling with temperature ~0.9, near-tied top scores, so the
# right bar is "better than random on average", not "argmax every time").
set -e
cd "$(dirname "$0")/.."
B=+x/scm_cli
LOG=eval/last_run.log
SEEDS=25
: > "$LOG"

data=0
while IFS='|' read -r msg cur feats; do
    [ -z "$msg" ] && continue
    case "$msg" in \#*) continue;; esac
    data=$((data+1))
done < eval/probes.txt

echo "probe | argmax-hit% | mean-selected | mean-uniform | verdict" | tee -a "$LOG"
echo "------|-------------|---------------|--------------|--------" | tee -a "$LOG"
passed=0; failed=0

while IFS='|' read -r msg cur feats; do
    [ -z "$msg" ] && continue
    case "$msg" in \#*) continue;; esac
    all=$(./"$B" select "$cur" "$msg" --seed 1 --all 2>/dev/null)
    argmax=$(printf '%s\n' "$all" | awk -F'\t' '$1=="SCORE" && ($2+0)>mx {mx=$2+0; t=$3} END{print t}')
    argmax_score=$(printf '%s\n' "$all" | awk -F'\t' '$1=="SCORE" && $3==a {print $2}' a="$argmax")
    uniform_mean=$(printf '%s\n' "$all" | awk -F'\t' '$1=="SCORE"{s+=$2; n++} END{printf "%.4f", s/n}')
    n=$(printf '%s\n' "$all" | awk -F'\t' '$1=="SCORE"{n++} END{print n}')
    top1=0; ssum=0
    for s in $(seq 1 "$SEEDS"); do
        out=$(./"$B" select "$cur" "$msg" --seed "$s" --all 2>/dev/null)
        sel=$(printf '%s\n' "$out" | awk -F'\t' '$1=="SCORE"{sc[$3]=$2} $1!="SCORE"{print}' )
        sel=$(printf '%s\n' "$out" | tail -1)
        [ "$sel" = "$argmax" ] && top1=$((top1+1))
        sscore=$(printf '%s\n' "$out" | awk -F'\t' -v t="$sel" '$1=="SCORE" && $3==t {print $2}')
        ssum=$(awk -v a="$ssum" -v b="$sscore" 'BEGIN{printf "%.4f", a+b}')
    done
    top1pct=$((top1 * 100 / SEEDS))
    sel_mean=$(awk -v s="$ssum" -v S="$SEEDS" 'BEGIN{printf "%.4f", s/S}')
    verdict=PASS
    if awk -v a="$sel_mean" -v u="$uniform_mean" 'BEGIN{exit !(a > u)}'; then
        passed=$((passed+1))
    else
        verdict=FAIL; failed=$((failed+1))
    fi
    echo "$msg | $top1pct% | $sel_mean | $uniform_mean | $verdict" | tee -a "$LOG"
done < eval/probes.txt

echo "---"
echo "PASS=$passed FAIL=$failed (probes=$data, seeds=$SEEDS, gate=mean(selected) > mean(uniform))"
[ "$failed" -eq 0 ]
