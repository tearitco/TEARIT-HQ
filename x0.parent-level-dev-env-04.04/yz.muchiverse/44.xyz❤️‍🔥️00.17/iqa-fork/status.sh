#!/usr/bin/env bash
# iqa-fork/status.sh - BOSS CHECK-IN. Report all LAN thread health + KPIs.
# Usage: status.sh   (read-only, safe to run any time)
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
echo "=== MAC ollama (compute) ==="
timeout 8 curl -s http://10.0.0.144:11434/api/tags -o /dev/null -w "mac_api: up (%{http_code})\n" 2>/dev/null || echo "mac_api: DOWN"
echo "=== LINUX node (training) ==="
timeout 10 sshpass -p root ssh -o StrictHostKeyChecking=no -o ConnectTimeout=6 jb@10.0.0.187 \
  'echo "linux_ssh: up"; echo "train_proc: $(pgrep -f main_orchestrator | wc -l)"; tail -1 ~/iqabod-store/conv-g1/loss.txt 2>/dev/null | sed "s/^/loss_tail: /"' 2>/dev/null || echo "linux_ssh: DOWN"
echo "=== THIS box threads ==="
echo "builder: $(pgrep -f 'builder/run.sh' | wc -l) proc(s)"
echo "iqa_loop: $(pgrep -f 'loop\.sh' | wc -l) proc(s)"
echo "=== last IQA KPIs (log.pdl) ==="
[ -f "$DIR/log.pdl" ] && tail -3 "$DIR/log.pdl" || echo "no iqa-fork log yet"
echo "=== last builder/progress notes ==="
[ -f "$DIR/../046.open-gema🤖️+1/ARCH-DOC.txt" ] && echo "open-gema arm present" 
echo "=== disk (this box, daily ship!) ==="
df -h "$DIR" | tail -1 | awk '{print "free: "$4" of "$2}'
