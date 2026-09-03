#!/bin/sh
# tearit-install — uninstaller. Removes a tearit-hq install completely.
#
#   curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/uninstall.sh | sh -s -- tearit-hq
# or, if you still have the install:
#   tearit-hq uninstall
#   sh ~/tearit-hq/start.sh uninstall
#
# Positional arg 1 : product name (default: tearit-hq)
# Env: PREFIX (install dir, default $HOME/<product>), BINDIR
#      (launcher dir, default $HOME/.local/bin), YES=1 (no prompt)

set -eu

PRODUCT="${1:-tearit-hq}"
PREFIX="${PREFIX:-$HOME/$PRODUCT}"
BINDIR="${BINDIR:-$HOME/.local/bin}"

say() { printf '\033[1m[tearit-uninstall]\033[0m %s\n' "$*"; }

say "product : $PRODUCT"
say "prefix  : $PREFIX"

# Prefer the install's own uninstall path (it knows how to stop the
# running desktop cleanly and finds its own launcher).
if [ -f "$PREFIX/start.sh" ]; then
    say "handing off to $PREFIX/start.sh uninstall"
    exec sh "$PREFIX/start.sh" uninstall "$( [ "${YES:-0}" = 1 ] && echo -y )"
fi

# Fallback: install dir already gone or incomplete — just clean up.
say "no start.sh at $PREFIX — cleaning up directly"

# stop any strip parser whose argv[1] is this PREFIX
for p in /proc/[0-9]*; do
    [ -r "$p/cmdline" ] || continue
    a0=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | sed -n 1p)
    a1=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | sed -n 2p)
    case "$a0" in */khtpm_core_render.+x|khtpm_core_render.+x) ;; *) continue ;; esac
    [ "$a1" = "$PREFIX" ] && { say "killing pid ${p#/proc/}"; kill -TERM "${p#/proc/}" 2>/dev/null || true; }
done

# remove any launcher pointing at this PREFIX
for d in "$BINDIR" "$HOME/bin" "/usr/local/bin"; do
    [ -d "$d" ] || continue
    for f in "$d"/*; do
        [ -f "$f" ] || continue
        if grep -qF "$PREFIX/start.sh" "$f" 2>/dev/null; then
            rm -f "$f" && say "removed launcher: $f"
        fi
    done
done
# also try the obvious name
[ -f "$BINDIR/$PRODUCT" ] && { rm -f "$BINDIR/$PRODUCT" && say "removed launcher: $BINDIR/$PRODUCT"; } || true

if [ -d "$PREFIX" ]; then
    if [ "${YES:-0}" != "1" ]; then
        printf 'delete %s ? [y/N] ' "$PREFIX"
        read ans
        case "$ans" in y|Y|yes|YES) ;; *) say "kept $PREFIX"; exit 0 ;; esac
    fi
    rm -rf "$PREFIX"
    say "removed $PREFIX"
fi
say "done."
