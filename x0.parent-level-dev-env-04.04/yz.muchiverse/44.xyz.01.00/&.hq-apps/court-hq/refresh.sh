#!/bin/sh
# refresh.sh - court-hq MOCKUP backend: publishes state/ui.txt from mock_cases.pdl + policy.pdl + state/tab.txt, sel.txt. Same shape as store-hq/refresh.sh.
# argv as a <module>: <house_root> <package_dir>; `--once <package_dir>` publishes one frame. Idle cost: one awk pass per 30 s.
set -u
ONCE=0
[ "${1:-}" = "--once" ] && { ONCE=1; shift; }
HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
PKG="${2:-${KHTPM_PKG:-}}"
[ -n "$PKG" ] || PKG=$(cd "$(dirname "$0")" && pwd)
[ "$ONCE" = 1 ] && PKG="${1:-$PKG}"
REFRESH_SEC="${COURT_HQ_REFRESH_SEC:-30}"
CHAIN="${COURT_HQ_CHAIN:-chain:cones-test}"     # which chain's POLICY rows to show (mockup)

publish() {
    mkdir -p "$PKG/state"
    tab=$(cat "$PKG/state/tab.txt" 2>/dev/null || echo docket)
    sel=$(cat "$PKG/state/sel.txt" 2>/dev/null || echo "")
    last=$(cat "$PKG/state/last_action.txt" 2>/dev/null || echo "-")
    tmp="$PKG/state/ui.txt.tmp.$$"
    awk -F'|' -v tab="$tab" -v sel="$sel" -v last="$last" -v stamp="$(date '+%H:%M:%S')" -v chain="$CHAIN" -v pol="$PKG/policy.pdl" '
        function t(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
        BEGIN { while ((getline l < pol) > 0) { n = split(l, f, "|"); if (t(f[1]) == "POLICY") { sc = t(f[2]); k = t(f[3]); if (sc == "*" ) pdef[k] = t(f[4]); else if (sc == chain) pchn[k] = t(f[4]) } } close(pol) }
        function pv(k) { return (k in pchn) ? pchn[k] : ((k in pdef) ? pdef[k] : "?") }
        $1 ~ /^CASE/ { id=t($2); n++; gid[n]=id; ti[id]=t($3); tb[id]=t($4); am[id]=t($5); st[id]=t($6); pa[id]=t($7); rs[id]=t($8); sm[id]=t($9) }
        END {
            shown = 0
            for (i = 1; i <= n; i++) { id = gid[i]; if (tb[id] == tab) { row[shown] = id; shown++ } }
            insel = 0; for (r = 0; r < shown; r++) if (row[r] == sel) insel = 1
            if (!insel) sel = (shown ? row[0] : "")
            print "stamp=" stamp; print "tab=" tab; print "last_action=" last
            for (r = 0; r < shown; r++) { id = row[r]; print "row_" r "_id=" id; print "row_" r "_text=" ((id == sel) ? "> " : "  ") ti[id] "  -  " am[id] }
            print "rows_count=" shown; print "no_rows=" (shown ? 0 : 1)
            print "pol_line=policy (" chain "): window " pv("dispute_window_days") " days | small <= " pv("small_amount") " | owner liability " pv("liability_share_pct") "% | judge bias " pv("judge_bias")
            if (sel != "") {
                small = pv("small_amount") + 0
                how = (rs[sel] == "rule" && am[sel] + 0 <= small) ? "rule-resolved (amount within the small-amount threshold)" : (am[sel] + 0 > small ? "goes to court case files, then owner confirms (above threshold)" : "owner reviews")
                print "d_id=" sel; print "d_name=" ti[sel]; print "d_line=" st[sel] "  |  amount " am[sel] "  |  " pa[sel]
                print "d_how=handling: " how; print "d_blurb=" sm[sel]; print "d_has=1"
                print "d_action=" ((tb[sel] == "jail") ? "Post bail (mock)" : (tb[sel] == "docket" ? "Apply (mock)" : "View ruling (mock)"))
            } else { print "d_id="; print "d_name=(nothing here)"; print "d_line="; print "d_how="; print "d_blurb=No cases in this tab."; print "d_has=0"; print "d_action=" }
        }' "$PKG/mock_cases.pdl" > "$tmp"
    sid=$(sed -n 's/^d_id=//p' "$tmp"); printf '%s\n' "$sid" > "$PKG/state/sel.txt"
    mv -f "$tmp" "$PKG/state/ui.txt"
}

trap 'exit 0' TERM INT HUP
publish
[ "$ONCE" = 1 ] && exit 0
while :; do sleep "$REFRESH_SEC"; publish; done
