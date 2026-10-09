#!/bin/bash
# nb_history_test.sh - pins the visit-log bound.
#
# The sidebar visit log grew unboundedly while both readers take only
# the FIRST 256 lines (oldest-first), so past 256 entries every new
# visit was stored yet never shown. visit_log_append() now trims to
# the newest 256 on every append.
#
# Method: back up the live history file, flood 300 dated fake entries,
# trigger one real fetch (mini-article - its own visit lands newest),
# then assert: file holds <= 256 lines, the newest is the real visit,
# the oldest fake is gone. The backup is restored on ANY exit (trap),
# because this file is live user data, not a fixture.
#
# Usage: sh nb_history_test.sh    (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
PF="$HR/#.desktop/network_browser_page.state.txt"
UI="$HR/#.desktop/network-browser-hq_ui.txt"
HIST="$HR/#.desktop/network_browser_history.log.txt"
FAIL=0

[ -f "$UI" ] || { echo "FAIL: no browser running"; exit 1; }

BACKUP="$(mktemp /tmp/nb_hist_backup_XXXXXX.txt)"
cp "$HIST" "$BACKUP" 2>/dev/null || true
restore_history() { cp "$BACKUP" "$HIST" 2>/dev/null || true; rm -f "$BACKUP"; }
trap restore_history EXIT

FX="$HERE/fixtures/mini-article.html"
i=1
while [ "$i" -le 300 ]; do
    printf 'https://history-flood-%d.example.com/page\n' "$i" >> "$HIST"
    i=$((i + 1))
done
BEFORE="$(wc -l < "$HIST" | tr -d ' ')"
echo "flooded history to $BEFORE lines"

printf 'go:file://%s\n' "$FX" > "$REQ"
for _ in $(seq 1 40); do
    grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
        && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
    sleep 0.5
done
sleep 2

AFTER="$(wc -l < "$HIST" | tr -d ' ')"
if [ "$AFTER" -le 256 ]; then
    echo "PASS: history bounded ($BEFORE -> $AFTER lines)"
else
    echo "FAIL: history has $AFTER lines after a fetch (expected <= 256)"; FAIL=1
fi
if grep -q "history-flood-1\.example" "$HIST" 2>/dev/null; then
    echo "FAIL: oldest flood entry survived the trim"; FAIL=1
else
    echo "PASS: oldest entries dropped first"
fi
# newest must be a real visit (the fetch just made), not a flood line.
# The manager appends the bare URL for file:// loads.
if tail -1 "$HIST" 2>/dev/null | grep -q "mini-article.html"; then
    echo "PASS: newest entry is the real visit"
else
    echo "FAIL: newest entry is not the real visit ($(tail -1 "$HIST" 2>/dev/null | cut -c1-60))"; FAIL=1
fi

restore_history
trap - EXIT
echo "history restored: $(wc -l < "$HIST" | tr -d ' ') lines"
echo
[ "$FAIL" -eq 0 ] && echo "HISTORY PASS" || echo "HISTORY FAIL"
exit $FAIL
