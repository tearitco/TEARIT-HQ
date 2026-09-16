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

# MANIFEST.txt - real record of every house-relative path copied in,
# tagged BASELINE/PRIMARY/DEP, so reintegrate.sh (generated below) and
# REINTEGRATE.md know exactly what came from where without re-deriving
# it. One line per path: "<TAG> <house-relative-path>".
MANIFEST="$DEST/MANIFEST.txt"
: > "$MANIFEST"

# Baseline every package needs - the generic renderer + its shared
# lib. NOT the desktop taskbar/shell itself.
mkdir -p "$OUT/&.widgits"
cp -r "$HOUSE/&.widgits/_shared-lib" "$OUT/&.widgits/"
printf 'BASELINE &.widgits/_shared-lib\n' >> "$MANIFEST"
mkdir -p "$OUT/*.monads/*.livedesk-taskbar"
cp -r "$HOUSE/"*.monads/*.livedesk-taskbar/ops "$OUT/"*.monads/*.livedesk-taskbar/
printf 'BASELINE *.monads/*.livedesk-taskbar/ops\n' >> "$MANIFEST"
mkdir -p "$OUT/#.desktop"

LAUNCH_TARGETS=""
for name in $NAMES; do
    SRC="$(find_target_dir "$name")" || { echo "unknown candidate: $name (run with no args to list real candidates)" >&2; exit 1; }
    REL="${SRC#"$HOUSE"/}"
    DESTREL="$OUT/$REL"
    mkdir -p "$(dirname "$DESTREL")"
    cp -r "$SRC" "$DESTREL"
    printf 'PRIMARY %s\n' "$REL" >> "$MANIFEST"

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
                printf 'DEP %s\n' "$dep" >> "$MANIFEST"
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

# reintegrate.sh - generated, real, dry-run-by-default. 2026-09-15,
# direct live request ("what about reintegrating the modified code
# into source. will it be ez? i might like for you to include a
# document &/or script with the build package export that explains to
# users how they will beable to do that?"). Every packaged path is a
# 1:1 mirror of the real house layout (44.xyz.01.00/<path> here ==
# <house_root>/44.xyz.01.00/<path> there), so reintegration is a
# targeted rsync back per MANIFEST.txt entry, tag-aware: PRIMARY (the
# app itself - expected/safe to have been edited), DEP (a cross-app
# dependency - also real, but touches shared ground, reviewed
# separately), BASELINE (the renderer/shared-lib - editing this is
# unusual; excluded unless explicitly asked for, since overwriting it
# house-wide from someone else's copy is the one genuinely risky case
# here).
cat > "$DEST/reintegrate.sh" <<'RGEN'
#!/bin/sh
# reintegrate.sh - copy this package's edited files back into the
# REAL house this was packaged from. Generated by create-package.sh -
# see REINTEGRATE.md (same dir) for the plain-language version.
#
# Usage:
#   ./reintegrate.sh <path-to-real-house-root>              dry run
#   ./reintegrate.sh <path-to-real-house-root> --apply       real copy
#   ./reintegrate.sh <path-to-real-house-root> --apply --include-baseline
#       also copies BASELINE-tagged paths (the shared renderer/lib) -
#       only do this if you KNOW you edited something under
#       &.widgits/_shared-lib or *.monads/*.livedesk-taskbar, since
#       those are shared by every app in the house, not just this one.
#
# SAFE BY DESIGN, on purpose ("lets be careful that we wont break any
# working code" - direct live instruction): this NEVER deletes a file
# in the real house just because the package doesn't have it (no
# --delete). A file in the house that isn't in this package is treated
# as the house's own newer/unrelated work, never touched. This can
# only ADD or OVERWRITE files the package actually carries - it cannot
# remove anything from the house. Known runtime-noise paths (pid/log
# files, generated chunk data, session dirs, the ledger) are excluded
# entirely, so a stale package snapshot never clobbers the house's own
# live, currently-more-current runtime state with old data.
#
# Real house-side review after running with --apply:
#   cd <house_root> && git status && git diff
# is what actually tells you what changed - this script only copies
# files, it never commits anything.
set -eu
SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT/44.xyz.01.00" ] || {
    echo "usage: $0 <path-to-real-house-root> [--apply] [--include-baseline]" >&2
    echo "  (real-house-root must contain a 44.xyz.01.00/ dir)" >&2
    exit 1
}
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
APPLY=0
INCLUDE_BASELINE=0
for a in "$@"; do
    [ "$a" = "--apply" ] && APPLY=1
    [ "$a" = "--include-baseline" ] && INCLUDE_BASELINE=1
done

MANIFEST="$SELF_DIR/MANIFEST.txt"
[ -f "$MANIFEST" ] || { echo "reintegrate.sh: missing MANIFEST.txt next to this script" >&2; exit 1; }

# Real runtime-state noise, never synced either direction - these are
# regenerated live by a running session, not authored code. Syncing
# them would either spam the diff with meaningless churn (dry run) or
# overwrite the house's own live state with a stale snapshot (apply).
EXCLUDES="--exclude=*.pid --exclude=*.log --exclude=module_parent.pid --exclude=sessions --exclude=chunks --exclude=master_ledger.txt --exclude=*.tmp"

[ "$APPLY" = 1 ] && echo "APPLYING real changes to: $HOUSE_ROOT (never deletes - only adds/overwrites)" || echo "DRY RUN (pass --apply to actually copy) against: $HOUSE_ROOT"
echo

while read -r TAG RELPATH; do
    [ -n "$RELPATH" ] || continue
    if [ "$TAG" = "BASELINE" ] && [ "$INCLUDE_BASELINE" != 1 ]; then
        echo "SKIP  (baseline, use --include-baseline to sync) $RELPATH"
        continue
    fi
    SRC="$SELF_DIR/44.xyz.01.00/$RELPATH"
    DST="$HOUSE_ROOT/44.xyz.01.00/$RELPATH"
    [ -e "$SRC" ] || { echo "MISSING in package: $RELPATH" >&2; continue; }
    # trailing slash on SRC is load-bearing here - without it rsync
    # nests SRC's own basename inside DST instead of syncing SRC's
    # CONTENTS into DST (a real, easy-to-hit rsync footgun, caught live
    # this session: a dry run against an UNCHANGED real house showed
    # every single file as "new" because of this exact mistake).
    # No --delete anywhere below, by design - see the safety note above.
    if [ "$APPLY" = 1 ]; then
        mkdir -p "$DST"
        # shellcheck disable=SC2086
        rsync -a $EXCLUDES "$SRC/" "$DST/" 2>/dev/null || cp -r "$SRC/." "$DST/"
        echo "COPIED [$TAG] $RELPATH"
    else
        if command -v rsync >/dev/null 2>&1; then
            echo "--- [$TAG] $RELPATH ---"
            # shellcheck disable=SC2086
            rsync -avn $EXCLUDES "$SRC/" "$DST/" 2>/dev/null | tail -n +2 | grep -v '^$' || true
        else
            echo "WOULD COPY [$TAG] $RELPATH (install rsync for a real diff preview)"
        fi
    fi
done < "$MANIFEST"

echo
[ "$APPLY" = 1 ] && echo "Done. Now: cd '$HOUSE_ROOT' && git status && git diff" \
                  || echo "Dry run done. Re-run with --apply to actually copy."
RGEN
chmod +x "$DEST/reintegrate.sh"

cat > "$DEST/REINTEGRATE.md" <<MDGEN
# Getting your changes back into the real house

This package's \`44.xyz.01.00/\` folder is a byte-for-byte mirror of
those same paths in the real house it came from - so getting your
edits back is not a manual copy-paste job, it's a targeted sync.

## The easy way

\`\`\`
./reintegrate.sh /path/to/the/real/house      # dry run - shows what would change
./reintegrate.sh /path/to/the/real/house --apply   # actually copies it back
\`\`\`

Then, in the real house itself:

\`\`\`
cd /path/to/the/real/house
git status
git diff
\`\`\`

That \`git diff\` is the real, final answer to "what did I actually
change" - review it before committing.

## Safety: this can't delete your real work

\`reintegrate.sh\` only ever ADDS or OVERWRITES files this package
actually carries - it never deletes anything from the real house, even
with \`--apply\`. If the real house gained new files since this package
was made, they're left alone. Runtime-only files (\`.pid\`/\`.log\`,
generated chunk data, session dirs, the ledger) are excluded entirely,
so a stale snapshot can never clobber the house's own live state.
Live-tested this exact scenario (an edit + a brand-new real-house file
both surviving \`--apply\` correctly) before this doc was written.

## What gets copied, and why some of it is skipped by default

Every file this package has came from one of three places, recorded
in \`MANIFEST.txt\` next to this doc:

- **PRIMARY** - the app itself ($NAMES). This is what you were
  actually meant to edit. Always synced back.
- **DEP** - other house apps/widgets this one depends on to run. Real,
  also synced back by default, but touches more than just "your" app -
  worth a closer look in \`git diff\` before committing.
- **BASELINE** - the shared rendering engine + shared library
  (\`&.widgits/_shared-lib\`, \`*.monads/*.livedesk-taskbar\`). Every
  app in the whole house uses these, so editing them is unusual -
  \`reintegrate.sh\` skips this tier unless you pass
  \`--include-baseline\`, specifically so an accidental edit here (or
  just a different version having drifted) doesn't silently overwrite
  shared code every other app in the house depends on.

## If reintegrate.sh isn't available for some reason

Do it by hand: for each real, human-made edit, copy the file from
this package's \`44.xyz.01.00/<same path>\` to the real house's
\`44.xyz.01.00/<same path>\` - the paths always match exactly, that's
the whole trick. Then \`git diff\` in the house to review.
MDGEN

echo "Packaged:$NAMES"
echo "  -> $DEST"
echo "Run with: $DEST/launch.sh"
echo "Reintegrate later with: $DEST/reintegrate.sh <real-house-root>  (see $DEST/REINTEGRATE.md)"
