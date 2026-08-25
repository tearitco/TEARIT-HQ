#!/bin/bash
# compile-runner.sh - house-wide compile runner. Finds every build
# script in the house (each project's own scripts/build.sh plus any
# ops/build_*.sh), runs each one from its own project dir, and drops one
# consolidated report.
#
# This is the verification half of the "house-wide build/distribution
# tool" flagged in sim-smell-fix.md (2026-08-20) - NOT the
# compile-once-distribute-everywhere tool itself. House rule (direct user
# instruction, see any build.sh header): every project stays self-
# contained and solo-shippable, so this runner deliberately invokes each
# project's OWN build script rather than building shared sources once and
# copying binaries around. What it buys you is the single command + single
# consolidated report, same shape as harness-runner.sh does for tests.
#
# WHAT THIS DOES:
#   - Finds every scripts/build.sh and ops/build_*.sh in the house
#     (excludes pieces/sessions/*, _BACKUP*, .backup*, .pre-symlink-swap
#     frozen copies).
#   - Runs each from its OWN directory (build scripts assume that), with
#     a per-script timeout, capturing full output to its own log under a
#     dated report directory.
#   - Classifies PASS / FAIL / TIMEOUT by exit code (builds print no
#     OVERALL verdict markers, so unlike harness-runner there is no
#     marker-grep tier - exit code is the real verdict for a compiler).
#   - Writes REPORT.md - one row per script, failure log tails inlined,
#     plus a summary count.
#
# WHAT THIS DOES NOT DO:
#   - Does not distribute binaries anywhere. Each build writes only into
#     its own project (normal build side effects).
#   - Does not judge WHY a build failed - read the inlined log tail or
#     the full log file. Known-good baseline as of 2026-08-21: all 44
#     scripts PASS (see sim-smell-fix.md MANAGER HANDOFF); any FAIL
#     against that baseline is a real regression until proven otherwise.
#   - Does not run EMERGENCY_KILL between builds (compilers don't leak
#     processes; harness-runner needs that, this doesn't).
#
# USAGE:
#   ./compile-runner.sh                  # run every build script found
#   ./compile-runner.sh <substring>      # only scripts whose path contains <substring>
#   ./compile-runner.sh --list           # just list what would run, don't run anything
#
# Per-script timeout defaults to 300s (override: BUILD_TIMEOUT=600 ./compile-runner.sh).

set -u
(set -o pipefail) 2>/dev/null && set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT_SECS="${BUILD_TIMEOUT:-300}"

MODE="run"
FILTER=""
for arg in "$@"; do
    if [ "$arg" = "--list" ]; then
        MODE="list"
    else
        FILTER="$arg"
    fi
done

REPORT_DIR="$HOUSE_DIR/\$.crypts/build-reports/$(date +%Y%m%d-%H%M%S)"

if [ "$MODE" != "list" ]; then
    mkdir -p "$REPORT_DIR"
fi

echo "=== compile-runner.sh - house-wide compile sweep ==="
echo "House: $HOUSE_DIR"
[ "$MODE" != "list" ] && echo "Report dir: $REPORT_DIR"
echo ""

RESULTS_FILE="$REPORT_DIR/results.tsv"
[ "$MODE" != "list" ] && > "$RESULTS_FILE"

PASS=0; FAIL=0; TIMEOUT=0

while IFS= read -r bsh; do
    [ -z "$bsh" ] && continue
    rel="${bsh#$HOUSE_DIR/}"
    if [ -n "$FILTER" ]; then
        case "$rel" in
            *"$FILTER"*) ;;
            *) continue ;;
        esac
    fi

    if [ "$MODE" = "list" ]; then
        echo "WOULD BUILD  $rel"
        continue
    fi

    projdir="$(cd "$(dirname "$bsh")" && pwd)"
    # ops/build_*.sh live inside the project already; scripts/build.sh too.
    echo "--- $rel ---"
    safelog="$REPORT_DIR/$(echo "$rel" | tr '/ ' '__').log"

    (
        cd "$projdir" || exit 1
        timeout "$TIMEOUT_SECS" bash "$(basename "$bsh")"
    ) > "$safelog" 2>&1
    ec=$?

    if [ "$ec" -eq 0 ]; then
        status="PASS"; PASS=$((PASS+1))
    elif [ "$ec" -eq 124 ]; then
        status="TIMEOUT"; TIMEOUT=$((TIMEOUT+1))
    else
        status="FAIL (exit $ec)"; FAIL=$((FAIL+1))
    fi
    echo "  -> $status   (log: ${safelog#$HOUSE_DIR/})"
    printf '%s\t%s\t%s\n' "$status" "$rel" "${safelog#$HOUSE_DIR/}" >> "$RESULTS_FILE"
done << BUILDEOF
$(find "$HOUSE_DIR" \( -name "build.sh" -o -name "build_*.sh" \) -type f 2>/dev/null \
    | grep -vE '/pieces/sessions/|_BACKUP|\.backup|\.pre-symlink-swap' | sort -u)
BUILDEOF

[ "$MODE" = "list" ] && exit 0

# --- write REPORT.md ---
{
    echo "# House-wide compile report — $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "**Summary: $((PASS+FAIL+TIMEOUT)) scripts — PASS=$PASS FAIL=$FAIL TIMEOUT=$TIMEOUT**"
    echo ""
    echo "| Status | Script | Log |"
    echo "|---|---|---|"
    while IFS="$(printf '\t')" read -r st rel log; do
        echo "| $st | $rel | $log |"
    done < "$RESULTS_FILE"
    echo ""
    if [ "$FAIL" -gt 0 ] || [ "$TIMEOUT" -gt 0 ]; then
        echo "## Failure log tails"
        echo ""
        while IFS="$(printf '\t')" read -r st rel log; do
            case "$st" in PASS*) continue ;; esac
            echo "### $rel — $st"
            echo '```'
            tail -15 "$HOUSE_DIR/$log" 2>/dev/null
            echo '```'
            echo ""
        done < "$RESULTS_FILE"
    fi
} > "$REPORT_DIR/REPORT.md"

echo ""
echo "=== DONE: $((PASS+FAIL+TIMEOUT)) scripts — PASS=$PASS FAIL=$FAIL TIMEOUT=$TIMEOUT ==="
echo "Report: ${REPORT_DIR#$HOUSE_DIR/}/REPORT.md"
