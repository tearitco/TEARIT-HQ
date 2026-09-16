#!/bin/sh
# create-package.sh - real, standalone packager for handing a single
# house app to someone else as an independent, runnable tree (same
# real shape as the work-co/seulgi-s14 and work-co/seulgi-palcraft-15
# packages a past session built by hand for the browser and pc-hq).
#
# 2026-09-15, direct live request ("today we were going to make
# 'package' to send to seulgi... is there a x11-hq app we could make
# create-package, by letting us choose from all possible package
# candidates... export it as an independent package. the script should
# also be able to run solo thru sh by simply taking arg... read from
# script so user knows what's available").
#
# Candidate list is NOT a separately maintained registry - it's the
# SAME real, live scan the taskbar's own Toys dropdown already does
# (khtpm_taskbar_manager.c's livedesk_build_toys_menu()/
# toys_scan_one_root(): house_root + @.apps + &.widgits + &.hq-apps,
# one level deep, opt-in by a real toy.pdl file's presence - direct
# live clarification: "im talking about the apps in toys tb drop down.
# it doesn't matter where they are from"). Whatever shows up in that
# dropdown is exactly what this script can package - no drift possible
# between the two lists because they're the same scan.
#
# toy.pdl gets one new, optional, backward-compatible key this session:
#   deps = comma-separated, house-root-relative extra paths to also
#          copy (an app that reaches into other widgets - board-viewer,
#          file-explorer, etc. - the way piececraft-hq does declares
#          them here; no deps= means "my own dir is self-contained",
#          the common case).
#
# Usage:
#   create-package.sh                          list real candidates
#   create-package.sh <name...> <dest-dir>     package one or more
#
# A package is the SAME real shape the browser/pc-hq packages already
# use: 44.xyz.01.00/ containing &.widgits/_shared-lib + the renderer
# binary from *.monads/*.livedesk-taskbar (the generic rendering
# ENGINE every khtpm window needs - NOT the desktop taskbar/shell
# itself, a separate thing per direct live correction: "she doesn't
# need tb, that's a separate thing") + the chosen app(s) + their
# declared deps, plus a generated launch.sh at the top that invokes
# each target directly (same shape as seulgi-s14/launch.sh).
set -eu
SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SELF_DIR/../.." && pwd)"

pdl_val() {
    # $1 = toy.pdl path, $2 = key. Real pipe-delimited "SECTION|KEY|VALUE"
    # read (same convention as pc_generate_chunk.c's extrusion table),
    # tolerant of either "SECTION" or "META" in column 1 - both real
    # shapes exist across this house's own toy.pdl files.
    sed -n "s/^[^|]*|[[:space:]]*$2[[:space:]]*|[[:space:]]*//p" "$1" 2>/dev/null | head -1 | sed 's/[[:space:]]*$//'
}

list_candidates() {
    for root in "$HOUSE" "$HOUSE/@.apps" "$HOUSE/&.widgits" "$HOUSE/&.hq-apps"; do
        [ -d "$root" ] || continue
        for d in "$root"/*/; do
            [ -f "${d}toy.pdl" ] || continue
            name="$(basename "$d")"
            title="$(pdl_val "${d}toy.pdl" title)"
            [ -n "$title" ] || title="$name"
            printf '  %-28s %s\n' "$name" "$title"
        done
    done
}

find_target_dir() {
    want="$1"
    for root in "$HOUSE" "$HOUSE/@.apps" "$HOUSE/&.widgits" "$HOUSE/&.hq-apps"; do
        if [ -d "$root/$want" ] && [ -f "$root/$want/toy.pdl" ]; then
            echo "$root/$want"
            return 0
        fi
    done
    return 1
}

if [ $# -lt 2 ]; then
    echo "Real, live candidates (same list the taskbar's own Toys dropdown scans):"
    echo
    list_candidates
    echo
    echo "Usage: $0 <name...> <dest-dir>"
    exit 0
fi

n=$#
i=1
DEST=""
eval "DEST=\${$n}"
NAMES=""
while [ "$i" -lt "$n" ]; do
    eval "a=\${$i}"
    NAMES="$NAMES $a"
    i=$((i + 1))
done

[ -e "$DEST" ] && { echo "refusing to overwrite existing path: $DEST" >&2; exit 1; }
mkdir -p "$DEST/44.xyz.01.00"
OUT="$DEST/44.xyz.01.00"

# Baseline every package needs - the generic renderer + its shared
# lib. NOT the desktop taskbar/shell itself.
mkdir -p "$OUT/&.widgits"
cp -r "$HOUSE/&.widgits/_shared-lib" "$OUT/&.widgits/"
mkdir -p "$OUT/*.monads/*.livedesk-taskbar"
cp -r "$HOUSE/"*.monads/*.livedesk-taskbar/ops "$OUT/"*.monads/*.livedesk-taskbar/
mkdir -p "$OUT/#.desktop"

LAUNCH_TARGETS=""
for name in $NAMES; do
    SRC="$(find_target_dir "$name")" || { echo "unknown candidate: $name (run with no args to list real candidates)" >&2; exit 1; }
    REL="${SRC#"$HOUSE"/}"
    DESTREL="$OUT/$REL"
    mkdir -p "$(dirname "$DESTREL")"
    cp -r "$SRC" "$DESTREL"

    DEPS="$(pdl_val "$SRC/toy.pdl" deps)"
    if [ -n "$DEPS" ]; then
        OLDIFS="$IFS"; IFS=','
        for dep in $DEPS; do
            IFS="$OLDIFS"
            dep="$(echo "$dep" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
            [ -n "$dep" ] || continue
            if [ -e "$HOUSE/$dep" ]; then
                mkdir -p "$OUT/$(dirname "$dep")"
                cp -r "$HOUSE/$dep" "$OUT/$dep"
            else
                echo "WARNING: $name declares dep '$dep' but it doesn't exist - skipped" >&2
            fi
            IFS=','
        done
        IFS="$OLDIFS"
    fi

    LAUNCH="$(pdl_val "$SRC/toy.pdl" launch)"
    [ -n "$LAUNCH" ] || LAUNCH="button.sh"
    LAUNCH_TARGETS="$LAUNCH_TARGETS $REL/$LAUNCH"
done

cat > "$DEST/launch.sh" <<'HDR'
#!/bin/bash
# launch.sh - generated by create-package.sh. Same real shape as
# work-co/seulgi-s14/launch.sh - runs each packaged app directly,
# no desktop taskbar/shell involved.
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$DIR/44.xyz.01.00"
mkdir -p "$ROOT/#.desktop"
HDR
for t in $LAUNCH_TARGETS; do
    printf 'sh "$ROOT/%s" run\n' "$t" >> "$DEST/launch.sh"
done
chmod +x "$DEST/launch.sh"

echo "Packaged:$NAMES"
echo "  -> $DEST"
echo "Run with: $DEST/launch.sh"
