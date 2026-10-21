#!/bin/bash
# cpu-quota.sh <cmd...>: run a heavy job (build, test suite, TTS, ffmpeg) under a CPU cap so it cannot pin the machine.
# Background: the machine hard-froze four times 2026-10-15..17 under sustained load (03-pitfalls/hq-cpu-safety.md section 7).
# Env: QUOTA_CPU = cap in percent of ONE core (default 300 = 3 of 8 cores), QUOTA_MEM (default 3G), QUOTA_NICE (default 15).
# Tier 1: kernel cgroup quota (needs the cpu controller delegated to the user: the root drop-in in the doc; NOT the case on this machine 2026-10-17).
# Tier 2: ops/cpu_cap.+x, a process-group limiter (SIGSTOP/SIGCONT against a cumulative budget). Measured: cap 300 -> 305 %, cap 200 -> ~330 %, cap 100 -> ~150-200 %.
# Tier 3: nice + ionice only (no cap). The tier used is printed on stderr.
HERE="$(cd "$(dirname "$0")" && pwd)"
CPU="${QUOTA_CPU:-300}"; MEM="${QUOTA_MEM:-3G}"; NI="${QUOTA_NICE:-15}"
[ $# -gt 0 ] || { echo "usage: cpu-quota.sh <cmd...>" >&2; exit 2; }
SC=""; command -v systemd-run >/dev/null 2>&1 && systemd-run --user --scope --quiet -p MemoryMax="$MEM" true >/dev/null 2>&1 && SC="systemd-run --user --scope --quiet -p MemoryMax=$MEM"
if [ -n "$SC" ] && $SC -p CPUQuota=100% sh -c '[ -r /sys/fs/cgroup$(cut -d: -f3 /proc/self/cgroup)/cpu.max ]' 2>/dev/null; then
  echo "cpu-quota: tier 1 (cgroup CPUQuota=${CPU}%, MemoryMax=$MEM)" >&2
  exec $SC -p "CPUQuota=${CPU}%" -p CPUWeight=20 -p IOWeight=20 nice -n "$NI" ionice -c3 "$@"
fi
if [ -x "$HERE/ops/+x/cpu_cap.+x" ]; then
  echo "cpu-quota: tier 2 (cpu_cap ${CPU}% of one core${SC:+, MemoryMax=$MEM})" >&2
  exec $SC nice -n "$NI" ionice -c3 "$HERE/ops/+x/cpu_cap.+x" "$CPU" "$@"
fi
echo "cpu-quota: tier 3 (nice only, NO cap: build ops/cpu_cap.c first)" >&2
exec nice -n "$NI" ionice -c3 "$@"
