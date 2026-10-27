#!/bin/sh
# refresh.sh - store-hq MOCKUP backend: publishes state/ui.txt from mock_catalog.pdl + state/tab.txt, sel.txt, owned.txt.
# Same shape as _template-hq/refresh.sh. argv as a <module>: <house_root> <package_dir>; `--once <package_dir>` publishes one frame.
set -u
ONCE=0
[ "${1:-}" = "--once" ] && { ONCE=1; shift; }
HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
PKG="${2:-${KHTPM_PKG:-}}"
[ -n "$PKG" ] || PKG=$(cd "$(dirname "$0")" && pwd)
[ "$ONCE" = 1 ] && PKG="${1:-$PKG}"
REFRESH_SEC="${STORE_HQ_REFRESH_SEC:-30}"      # idle cost: one awk pass per 30 s; actions republish at once

# Cover art. A real cover is a PNG the creator provides at <PKG>/art/<id>/cover.png (the renderer loads .png sprites natively).
# Until then an auto-generated 48x48 placeholder (colours + stripes from a hash of the id) is written once to state/art/<id>/sprite.csv.
art_for() {
    id="$1"
    [ -f "$PKG/art/$id/cover.png" ] && { printf '%s' "$PKG/art/$id/cover.png"; return; }
    d="$PKG/state/art/$id"
    if [ ! -f "$d/sprite.csv" ]; then
        mkdir -p "$d"
        h=$(printf '%s' "$id" | cksum | cut -d' ' -f1)
        awk -v h="$h" 'BEGIN { r1=(h%200)+30; g1=(int(h/7)%200)+30; b1=(int(h/49)%200)+30; r2=255-r1; g2=255-g1; b2=255-b1; st=(h%5)+3
            print "# resolution=48"
            for (y=0;y<48;y++) for (x=0;x<48;x++) { f=(x+y)/94; k=(int((x+y)/st)%2); m=(k?0.82:1.0)
                printf "%d,%d,%d,255\n", (r1*(1-f)+r2*f)*m, (g1*(1-f)+g2*f)*m, (b1*(1-f)+b2*f)*m } }' > "$d/sprite.csv.tmp.$$" && mv -f "$d/sprite.csv.tmp.$$" "$d/sprite.csv"
    fi
    printf '%s' "$d"
}

publish() {
    mkdir -p "$PKG/state"
    tab=$(cat "$PKG/state/tab.txt" 2>/dev/null || echo store)
    sel=$(cat "$PKG/state/sel.txt" 2>/dev/null || echo "")
    last=$(cat "$PKG/state/last_action.txt" 2>/dev/null || echo "-")
    touch "$PKG/state/owned.txt"
    tmp="$PKG/state/ui.txt.tmp.$$"
    awk -F'|' -v tab="$tab" -v sel="$sel" -v last="$last" -v stamp="$(date '+%H:%M:%S')" -v owned="$PKG/state/owned.txt" '
        function t(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
        BEGIN { while ((getline l < owned) > 0) own[t(l)] = 1; close(owned) }
        $1 ~ /^GAME/ { id=t($2); n++; gid[n]=id; nm[id]=t($3); kd[id]=t($4); pr[id]=t($5); rt[id]=t($6); sc[id]=t($7); tg[id]=t($8); bl[id]=t($9) }
        $1 ~ /^REVIEW/ { rid=t($2); rc[rid]++; rv[rid, rc[rid]] = t($3) " (" t($4) "/5): " t($5) }
        END {
            shown = 0
            for (i = 1; i <= n; i++) {
                id = gid[i]; ok = 0
                if (tab == "library" && (id in own)) ok = 1
                else if (tab == "store" && kd[id] != "toy") ok = 1
                else if (tab == "toys" && kd[id] == "toy") ok = 1
                if (!ok) continue
                if (sel == "" ) sel = id
                row[shown] = id; shown++
            }
            if (!(sel in nm) || shown == 0) sel = (shown ? row[0] : "")
            insel = 0; for (r = 0; r < shown; r++) if (row[r] == sel) insel = 1
            if (!insel && shown) sel = row[0]
            print "stamp=" stamp
            print "tab=" tab
            print "last_action=" last
            for (r = 0; r < shown; r++) {
                id = row[r]; mark = (id == sel) ? "> " : "  "; st = (id in own) ? " [owned]" : ""
                print "row_" r "_id=" id
                print "row_" r "_text=" mark nm[id] "  -  " pr[id] st
            }
            print "rows_count=" shown
            print "no_rows=" (shown ? 0 : 1)
            if (sel != "") {
                print "d_id=" sel
                print "d_name=" nm[sel]
                print "d_line=" kd[sel] "  |  " pr[sel] "  |  rating " (rt[sel] > 0 ? rt[sel] " / 5" : "none yet")
                print "d_screen=screening: " sc[sel] (sc[sel] == "screened" ? "  (compatibility + safety checked)" : sc[sel] == "needs_review" ? "  (agent proposed, owner has not confirmed)" : "  (not checked: install blocked)")
                print "d_tags=" tg[sel]
                print "d_blurb=" bl[sel]
                print "d_action=" ((sel in own) ? "Play (mock)" : "Get (mock)")
                print "d_has=1"
                for (k = 1; k <= rc[sel] && k <= 3; k++) print "rev_" (k-1) "_text=" rv[sel, k]
                print "rev_count=" (rc[sel] > 3 ? 3 : rc[sel])
                print "no_rev=" (rc[sel] ? 0 : 1)
            } else { print "d_has=0"; print "d_name=(nothing here)"; print "d_line="; print "d_screen="; print "d_tags="; print "d_blurb=Library is empty: press Get on a game in the Store tab."; print "d_action="; print "rev_count=0"; print "no_rev=1" }
        }' "$PKG/mock_catalog.pdl" > "$tmp"
    for rid in $(sed -n 's/^row_\([0-9]*\)_id=.*/\1/p' "$tmp"); do
        echo "row_${rid}_art=$(art_for "$(sed -n "s/^row_${rid}_id=//p" "$tmp")")" >> "$tmp"
    done
    cur=$(sed -n 's/^d_id=//p' "$tmp")
    if [ -n "$cur" ]; then echo "d_art=$(art_for "$cur")" >> "$tmp"; else echo "d_art=" >> "$tmp"; fi
    echo "$sel" | grep -q . && printf '%s\n' "$(sed -n 's/^d_id=//p' "$tmp")" > "$PKG/state/sel.txt"
    mv -f "$tmp" "$PKG/state/ui.txt"
}

trap 'exit 0' TERM INT HUP
publish
[ "$ONCE" = 1 ] && exit 0
while :; do sleep "$REFRESH_SEC"; publish; done
