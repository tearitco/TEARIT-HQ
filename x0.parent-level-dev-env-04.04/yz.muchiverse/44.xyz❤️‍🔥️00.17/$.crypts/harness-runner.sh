#!/bin/bash
# harness-runner.sh - house-wide test-harness runner. Finds every
# test-harn*/ directory in the house, compiles its own small C ops (if it
# has a `compile`/`build` action), runs its own real scenario action
# (auto-detected - whichever case-label in its button.sh actually invokes
# a scenarios/*.sh script), and drops one consolidated report.
#
# This is "future work item 2" from sim-smell-fix.md's own HANDOFF section
# ("a house-wide test-harness runner... drops a single consolidated
# report") - see that doc for the full context on why this exists and how
# it fits into the house's own symlink-migration effort.
#
# WHAT THIS DOES:
#   - Finds every test-harn*/button.sh in the house (excludes
#     pieces/sessions/*, and any dir whose path contains "_BACKUP" or
#     ".backup" - those are frozen backups, not live projects).
#   - For each: runs `button.sh compile` (or `build`/`c`) if that action
#     exists, then auto-detects and runs the real scenario action (the
#     case-label whose body invokes `scenarios/*.sh` - NOT hardcoded to
#     "demo", since different harnesses use different verbs: demo, pvp,
#     run, etc.)
#   - Captures full output per harness to its own log file under a
#     dated report directory, and classifies PASS/FAIL/TIMEOUT/SKIP/ERROR
#     by grepping the output for known markers (see classify_harness()).
#   - Runs `button.sh kill` after each harness regardless of outcome, so a
#     failed/hung harness doesn't leave stray processes for the next one.
#   - Writes REPORT.md - one row per harness, plus a short summary count.
#
# WHAT THIS DOES NOT DO:
#   - Does not know whether a FAIL is a real regression or a pre-existing,
#     unrelated bug (see sim-smell-fix.md's own "pre-existing-bug-vs-
#     regression" technique - TSC_ELO's pvp_duel and 102.editor's CLEAR
#     bug are both known, confirmed-pre-existing examples). A human or
#     agent still has to read the report and make that call per failure.
#   - Does not touch any button.sh, symlinks, or project state. Read-only
#     except for the report directory and whatever each harness itself
#     writes as a side effect of running (session dirs, its own proof/
#     dir, etc. - normal for a real run).
#   - Does not run projects that have NO test-harn*/ dir at all - those
#     still need the manual-test protocol in sim-smell-fix.md.
#
# USAGE:
#   ./harness-runner.sh                  # run every harness found, ~House-wide
#   ./harness-runner.sh <substring>      # only run harnesses whose path contains <substring>
#   ./harness-runner.sh --list           # just list what would run, don't run anything
#
# Per-harness timeout defaults to 240s (override: HARNESS_TIMEOUT=300 ./harness-runner.sh).

set -u
(set -o pipefail) 2>/dev/null && set -o pipefail   # bash/ksh only; dash treats an unknown `set -o`
                                                    # arg as fatal even under `||`, so test in a
                                                    # subshell first rather than risk killing the script

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT_SECS="${HARNESS_TIMEOUT:-240}"

MODE="run"
FILTER=""
for arg in "$@"; do
    if [ "$arg" = "--list" ]; then
        MODE="list"
    else
        FILTER="$arg"
    fi
done

REPORT_DIR="$HOUSE_DIR/\$.crypts/harness-reports/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$REPORT_DIR"

# find_scenario_action <button.sh> - print the case-label whose body
# invokes a scenarios/*.sh script, or nothing if none found.
find_scenario_action() {
    local f="$1"
    awk '
        /^\s*[A-Za-z0-9_|-]+\)[[:space:]]*$/ {
            line=$0; gsub(/^[[:space:]]*/, "", line); gsub(/\)[[:space:]]*$/, "", line);
            label=line; next
        }
        /scenarios\// { if (label != "") { print label; exit } }
    ' "$f" | cut -d'|' -f1
}

# has_action <button.sh> <action> - 0 if the case statement has this label
has_action() {
    local f="$1" act="$2"
    grep -qE "^\s*($act)(\||\))" "$f"
}

# classify_harness <logfile> <exit_code> -> PASS|FAIL|TIMEOUT|ERROR
#
# Priority order matters: a scenario's own "=== OVERALL: PASS/FAIL ==="
# verdict (many harnesses in the house use this convention) is checked
# FIRST, before the outer exit code - because a scenario can print its own
# real, definitive PASS verdict and then hang/get killed during its own
# TRAILING cleanup (its EXIT trap calling `button.sh kill`), which makes
# the outer `timeout` kill it with a nonzero exit code even though the
# actual test already fully passed. Trusting exit code over the verdict
# in that case is a real, confirmed false-negative (caught live 2026-08-20
# testing my-chara-txt's own demo_end_turn.sh: exit 143 from a cleanup-
# phase kill, but "=== OVERALL: PASS ===" already printed with all 13
# checks passing). Only fall back to exit-code/FAIL-line heuristics when
# no explicit OVERALL verdict is present at all.
classify_harness() {
    local log="$1" ec="$2"
    if grep -qE '^=== OVERALL: PASS' "$log"; then
        echo "PASS"; return
    fi
    if grep -qE '^=== OVERALL: FAIL' "$log"; then
        echo "FAIL"; return
    fi
    if [ "$ec" -eq 124 ]; then
        echo "TIMEOUT"; return
    fi
    if grep -qiE 'FSM TIMEOUT|FSM FAILED' "$log"; then
        echo "FAIL"; return
    fi
    if grep -qE '^=== FAILED' "$log" || grep -qE '^FAIL[: ]' "$log"; then
        echo "FAIL"; return
    fi
    if grep -qiE '^FAIL$|failed to compile|FAIL   ' "$log"; then
        echo "FAIL"; return
    fi
    if [ "$ec" -ne 0 ]; then
        echo "ERROR (exit $ec)"; return
    fi
    if grep -qE '^PASS|=== (PASSED|OK|DONE)|all checks passed' "$log"; then
        echo "PASS"; return
    fi
    echo "PASS (no explicit marker, exit 0)"
}

echo "=== harness-runner.sh - house-wide test harness sweep ==="
echo "House: $HOUSE_DIR"
echo "Report dir: $REPORT_DIR"
echo ""

RESULTS_FILE="$REPORT_DIR/results.tsv"
> "$RESULTS_FILE"

while IFS= read -r hdir; do
    [ -z "$hdir" ] && continue
    case "$hdir" in
        *_BACKUP*|*.backup-*) continue ;;
    esac
    bsh="$hdir/button.sh"
    [ -f "$bsh" ] || continue
    rel="${hdir#$HOUSE_DIR/}"
    if [ -n "$FILTER" ]; then
        case "$rel" in
            *"$FILTER"*) ;;
            *) continue ;;
        esac
    fi

    action="$(find_scenario_action "$bsh")"
    if [ -z "$action" ]; then
        echo "SKIP  (no scenario action found)  $rel"
        printf '%s\t%s\t%s\n' "SKIP" "$rel" "no scenario action in button.sh" >> "$RESULTS_FILE"
        continue
    fi

    if [ "$MODE" = "list" ]; then
        echo "WOULD RUN  action=$action  $rel"
        continue
    fi

    echo "--- $rel  (action: $action) ---"
    safelog="$REPORT_DIR/$(echo "$rel" | tr '/ ' '__').log"

    # Run with CWD = the PROJECT root (one level up from the harness dir),
    # not the harness dir itself. House convention is inconsistent: some
    # scenarios `cd "$PROJECT_DIR"` themselves (my-chara-txt), others use
    # bare relative paths (data/blockchain.txt, pieces/sessions/*/) that
    # silently assume the CALLER's CWD is already the project root
    # (041.pal-chain's own demo_2wallet_ux.sh, "ported unmodified" from
    # 044.pal-chat-irc - confirmed live, 2026-08-20: running from the
    # harness dir itself broke it with "cannot open /pieces/keyboard/
    # history.txt", a bare absolute-looking path with no project prefix).
    # cd'ing to the project root satisfies both conventions.
    PROJECT_ROOT="$(cd "$hdir/.." && pwd)"
    (
        cd "$PROJECT_ROOT" || exit 1
        if has_action "$hdir/button.sh" "compile" || has_action "$hdir/button.sh" "build" || has_action "$hdir/button.sh" "c"; then
            bash "$hdir/button.sh" compile
        fi
        timeout "$TIMEOUT_SECS" bash "$hdir/button.sh" "$action"
    ) > "$safelog" 2>&1
    ec=$?

    (cd "$PROJECT_ROOT" && bash "$hdir/button.sh" kill >/dev/null 2>&1 || true)

    # REAL FIX 2026-08-20 - confirmed live: running all ~18 harnesses
    # back-to-back left enough real contention (leaked GL/board-viewer/
    # widget processes from an earlier harness, general system load)
    # that LATER harnesses' own session-launch readiness checks timed
    # out at 30s even though the exact same harness passed cleanly in
    # isolation seconds earlier - a real infra flake, not a regression.
    # A single project's own `button.sh kill` (above) only kills ITS
    # own processes, not strays from a DIFFERENT project's widgets. Run
    # the house's own broad EMERGENCY_KILL.sh between every harness so
    # each one starts from a genuinely clean slate.
    if [ -x "$HOUSE_DIR/EMERGENCY_KILL.sh" ]; then
        bash "$HOUSE_DIR/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
    fi
    sleep 1

    status="$(classify_harness "$safelog" "$ec")"
    echo "  -> $status   (log: ${safelog#$HOUSE_DIR/})"
    printf '%s\t%s\t%s\n' "$status" "$rel" "${safelog#$HOUSE_DIR/}" >> "$RESULTS_FILE"
done << HARNEOF
$(find "$HOUSE_DIR" -iname "test-harn*" -type d 2>/dev/null | grep -v '/pieces/sessions/' | sort -u)
HARNEOF

[ "$MODE" = "list" ] && exit 0

# --- write REPORT.md ---
{
    echo "# House-wide harness report — $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "Generated by \`\$.crypts/harness-runner.sh\`. See \`sim-smell-fix.md\`'s own HANDOFF section"
    echo "for what PASS/FAIL/SKIP/TIMEOUT/ERROR mean here and, critically, for how to tell a REAL"
    echo "regression apart from a pre-existing, unrelated bug before acting on any FAIL below."
    echo ""
    echo "| Status | Harness | Log |"
    echo "|---|---|---|"
    while IFS=$'\t' read -r status rel log; do
        echo "| $status | \`$rel\` | \`$log\` |"
    done < "$RESULTS_FILE"
    echo ""
    total=$(wc -l < "$RESULTS_FILE")
    pass=$(grep -c '^PASS' "$RESULTS_FILE" || true)
    fail=$(grep -c '^FAIL' "$RESULTS_FILE" || true)
    skip=$(grep -c '^SKIP' "$RESULTS_FILE" || true)
    other=$((total - pass - fail - skip))
    echo "**Summary**: $total harness(es) found — $pass PASS, $fail FAIL, $skip SKIP, $other other (TIMEOUT/ERROR)."
} > "$REPORT_DIR/REPORT.md"

echo ""
echo "=== done. Report: $REPORT_DIR/REPORT.md ==="
cat "$REPORT_DIR/REPORT.md"
