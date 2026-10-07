#!/bin/bash
# verify.sh - the deterministic SCORER for quest Q006 (one generic board layout). Renders the board HEADLESS (no window, no display) with the real renderer and reads its ascii frame.
# SANDBOX: two throwaway instances (vtest-quests, vtest-ghosts) stamped from the ONE template, pointed (absolute paths) at scratch data in /tmp; their files are deleted afterwards.
# PASS/FAIL lines, then VERDICT; exit 0 = PASS, 1 = FAIL, 2 = prerequisites missing. `--selftest`: a deliberately broken template (no <repeat> rows) must make it FAIL.
set -u
if [ "${1:-}" = "--selftest" ]; then   # a template with the rows removed must FAIL; the real one must PASS
    ME="$(cd "$(dirname "$0")" && pwd)"; B="$(mktemp)"
    awk '/<repeat count="\$\{n_rows\}"/ {skip=1} !skip {print} /<\/repeat>/ && skip {skip=0}' "$ME/board.xhtpm" > "$B"
    grep -q 'n_rows}" bind' "$B" && { echo "SELFTEST FAIL (could not build the broken template)"; rm -f "$B"; exit 1; }
    BOARD_TEMPLATE="$B" bash "$0" >/dev/null 2>&1; rcb=$?; rm -f "$B"
    bash "$0" >/dev/null 2>&1; rcg=$?
    if [ "$rcb" = 1 ] && [ "$rcg" = 0 ]; then echo "SELFTEST PASS (a template with no rows fails; the real one passes)"; exit 0; fi
    echo "SELFTEST FAIL (broken rc=$rcb want 1, real rc=$rcg want 0)"; exit 1
fi
HERE="$(cd "$(dirname "$0")" && pwd)"; HOUSE="$(cd "$HERE/../.." && pwd)"
BIN="$HOUSE/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"; OP="$HERE/ops/+x/board_vars_op.+x"
[ -x "$BIN" ] && [ -x "$OP" ] || { echo "missing renderer or board_vars_op (sh ops/build_board_vars_op.sh)" >&2; exit 2; }
T="$(mktemp -d)"; fail=0; N=0
ck() { N=$((N+1)); if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
PIDS=""
cleanup() { for p in $PIDS; do kill "$p" 2>/dev/null; done; sleep 0.5; rm -rf "$T" "$HERE"/board-vtest-*.xhtpm "$HERE"/board-vtest-*.css "$HERE"/state/vtest-*; }   # kill OUR renderers by pid (never pkill -f)
trap cleanup EXIT
# scratch data: a quest index (3 rows, links to files with a Log) and a roster (2 ghosts + a template folder)
mkdir -p "$T/quests/QA" "$T/quests/QB" "$T/quests/QC" "$T/roster/_template" "$T/roster/ghost-b" "$T/roster/ghost-a"
printf '| id | quest | tier | size | status | assignee |\n|---|---|---|---|---|---|\n| [QA](QA/QUEST.md) | alpha job | w | S | open | - |\n| [QB](QB/QUEST.md) | beta job | w | S | done | x |\n| [QC](QC/QUEST.md) | gamma job | w | S | open | - |\n\nnext ids\n' > "$T/quests/INDEX.md"
for q in QA QB QC; do printf '# %s\n## Log\nline one of %s\nline two of %s\nthe last line of %s\n' "$q" "$q" "$q" "$q" > "$T/quests/$q/QUEST.md"; done
printf '1 started\n2 did a thing for ghost-a\n' > "$T/roster/ghost-a/history.txt"; printf '1 started\n2 did a thing for ghost-b\n' > "$T/roster/ghost-b/history.txt"
stamp() {   # stamp <name> <pdl-text>
    sh "$HERE/ops/new_board.sh" "$1" >/dev/null; printf '%s\n' "$2" > "$HERE/state/$1/board.pdl"; rm -f "$HERE/state/$1/ui.txt" "$HERE/state/$1/selected.txt"
}
stamp vtest-quests "BOARD  | title    | Test quests
BOARD  | subtitle | scratch
SOURCE | md-table | $T/quests/INDEX.md | cols=0,1,4
DETAIL | link-tail | 5
ACTION | Done | echo done {id}"
stamp vtest-ghosts "BOARD  | title    | Test ghosts
BOARD  | subtitle | scratch roster
SOURCE | dirs     | $T/roster
DETAIL | dir-file-tail | history.txt | 5"
frame_of() {   # frame_of <pid> -> the latest frame block of that renderer
    local f="$HOUSE/#.desktop/ascii_frames/$1.frame_history.txt"; [ -f "$f" ] || return 1
    awk '/^--- page:/ {buf=""} {buf = buf $0 "\n"} END {printf "%s", buf}' "$f"
}
run_board() {   # run_board <instance> <seconds> -> sets RPID; leaves the renderer running under timeout
    cd /tmp || return 1
    DISPLAY= timeout "$2" nice -n 15 "$BIN" "$HOUSE" "$HERE/board-$1.xhtpm" --headless >"$T/$1.log" 2>&1 &
    local tp=$!; cd "$HERE" || return 1; sleep 7
    RPID="$(ps -o pid= --ppid "$tp" 2>/dev/null | tr -d ' ' | head -1)"      # this run's own renderer, not "the newest frame file"
    PIDS="$PIDS $tp $RPID"; for m in $(ps -o pid= --ppid "$RPID" 2>/dev/null); do PIDS="$PIDS $m"; done
}
relay() { printf 'KEY_PRESSED: %s\n' "$2" >> "$HOUSE/#.desktop/entity_menu_history/$1.txt"; }

run_board vtest-quests 30; FR="$(frame_of "$RPID")"
echo "$FR" | grep -q 'Test quests' && ck title-from-pdl ok || ck title-from-pdl bad "title missing: $(echo "$FR" | head -3 | tr '\n' '/')"
echo "$FR" | grep -q 'scratch - 3 rows' && ck row-count-from-source ok || ck row-count-from-source bad "no '3 rows' line"
[ "$(echo "$FR" | grep -cE '\] [0-9]+\. Q[ABC]  ')" = 3 ] && ck rows-nav-numbered ok || ck rows-nav-numbered bad "expected 3 numbered rows: $(echo "$FR" | grep -c 'job')"
echo "$FR" | grep -q 'alpha job  open' && echo "$FR" | grep -q 'beta job  done' && ck row-text-from-chosen-columns ok || ck row-text-from-chosen-columns bad "cols 0,1,4 not shown"
echo "$FR" | grep -q 'the last line of QA' && ck detail-tail-of-selected-row ok || ck detail-tail-of-selected-row bad "detail pane lacks QA's last line"
echo "$FR" | grep -q 'Done' && ck action-button-from-pdl ok || ck action-button-from-pdl bad "action button missing"
# select row 2 with the keyboard through the real relay (digit 2 = nav number 2, then Enter): the detail pane must switch to QB
relay "$RPID" 50; sleep 1; relay "$RPID" 13; sleep 4; FR2="$(frame_of "$RPID")"
echo "$FR2" | grep -q 'the last line of QB' && ! echo "$FR2" | grep -q 'the last line of QA' && ck select-switches-detail ok || ck select-switches-detail bad "detail did not switch to QB after nav 2 + Enter"
[ "$(cat "$HERE/state/vtest-quests/selected.txt" 2>/dev/null)" = 1 ] && ck selection-saved ok || ck selection-saved bad "selected.txt=$(cat "$HERE/state/vtest-quests/selected.txt" 2>/dev/null)"
run_board vtest-ghosts 20; FG="$(frame_of "$RPID")"
echo "$FG" | grep -q 'Test ghosts' && [ "$(echo "$FG" | grep -cE '\] [0-9]+\. ghost-[ab]')" = 2 ] && ck second-source-roster-rows ok || ck second-source-roster-rows bad "roster rows: $(echo "$FG" | grep -c ghost)"
! echo "$FG" | grep -q '_template' && ck template-folder-hidden ok || ck template-folder-hidden bad "_template listed"
echo "$FG" | grep -q 'did a thing for ghost-a' && ck second-source-detail ok || ck second-source-detail bad "roster detail missing"
# ONE layout: both instances are the template with only the name substituted; one css edit restyles both
diff <(sed 's/vtest-quests/NAME/g' "$HERE/board-vtest-quests.xhtpm") <(sed 's/vtest-ghosts/NAME/g' "$HERE/board-vtest-ghosts.xhtpm") >/dev/null && ck instances-differ-only-by-name ok || ck instances-differ-only-by-name bad "instances differ beyond the name"
cp "$HERE/board.css" "$T/css.bak"; echo '.act-btn { color: #ff00ff; }' >> "$HERE/board.css"; sh "$HERE/ops/new_board.sh" vtest-quests >/dev/null; sh "$HERE/ops/new_board.sh" vtest-ghosts >/dev/null
grep -q 'ff00ff' "$HERE/board-vtest-quests.css" && grep -q 'ff00ff' "$HERE/board-vtest-ghosts.css" && ck one-css-edit-restyles-both ok || ck one-css-edit-restyles-both bad "css edit did not reach both"
cp "$T/css.bak" "$HERE/board.css"
git -C "$HERE" diff --quiet -- board.css 2>/dev/null; true
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)|checks=$N"; exit "$fail"
