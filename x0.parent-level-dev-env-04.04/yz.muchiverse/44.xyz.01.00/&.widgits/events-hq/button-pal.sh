#!/bin/sh
# button-pal.sh - PARALLEL launcher for the static-xhtpm events editor
# (EVENTS-HQ-XHTPM-PORT.md). The old button.sh + pieces/dashboard.chtpm
# stay untouched as rollback until the dumps match.
#
#   sh button-pal.sh <package_dir> [house_root]
#
# Difference from button.sh:
#   - renders events-hq.xhtpm (class="events-hq-pal", NOT events-hq-window)
#   - starts khtpm_events_hq_manager.+x itself (background) - the generic
#     <module> launcher can't pass it the event_pkg dir as argv
#   - the xhtpm's one <module> is the projector; it reads KHTPM_ARG3
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/../../*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
PROJ="$HERE/ops/+x/evhq_projector.+x"
MGR="$HERE/ops/+x/khtpm_events_hq_manager.+x"
XHTPM="$HERE/events-hq.xhtpm"

[ -x "$BIN" ]  || (cd "$OPS_DIR" && sh build_core_render.sh) || true
[ -x "$PROJ" ] || (cd "$HERE/ops" && sh build_evhq_projector.sh) || true
[ -x "$MGR" ]  || (cd "$HERE/ops" && sh build_events_hq_manager.sh) || true
for f in "$BIN" "$PROJ" "$MGR" "$XHTPM"; do
    [ -e "$f" ] || { echo "events-hq-pal: missing $f" >&2; exit 1; }
done

PACKAGE_DIR="${1:?usage: button-pal.sh <package_dir> [house_root]}"
PACKAGE_DIR="$(cd "$PACKAGE_DIR" && pwd)"
H="${2:-}"
if [ -z "$H" ] || [ ! -d "$H" ]; then H="$(cd "$HERE/../.." && pwd)"; fi
PKG_DIR="$PACKAGE_DIR/event_pkg"
LABEL="$(basename "$PACKAGE_DIR")"
mkdir -p "$PKG_DIR/.hq_manager"
printf '%s\n' "$LABEL" > "$PKG_DIR/.hq_manager/label.txt"

# Same "only replace an instance already open on THIS event_pkg" guard as
# button.sh's same_entity_pids - byte-compare /proc/<pid>/cmdline, no regex
# (house paths carry emoji/parens). Covers both the renderer and the manager.
same_entity_pids() {
    for pid in $(pgrep -f 'khtpm_core_render\.\+x|khtpm_events_hq_manager\.\+x' 2>/dev/null || true); do
        if [ -r "/proc/$pid/cmdline" ]; then
            if tr '\0' '\n' < "/proc/$pid/cmdline" 2>/dev/null | grep -qxF "$PKG_DIR"; then
                echo "$pid"
            fi
        fi
    done
}

pids="$(same_entity_pids)"
if [ -n "$pids" ]; then
    echo "events-hq-pal: replacing instance on $LABEL: $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(same_entity_pids)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

# 1. the IR/compile manager (needs house + event_pkg + label, same 3 args
#    the old shell used).
setsid nohup "$MGR" "$H" "$PKG_DIR" "$LABEL" \
    >/tmp/events-hq-pal-mgr-"$LABEL".log 2>&1 < /dev/null &
disown 2>/dev/null || true

# 2. the shared renderer on the static template. argv[3]=$PKG_DIR is a
#    directory -> the renderer's instance-dir hook: ${ARG3}=$PKG_DIR,
#    KHTPM_ARG3 exported to the <module> projector, and
#    $PKG_DIR/.hq_manager/ui.txt appended to the template's vars sources.
setsid nohup "$BIN" "$H" "$XHTPM" "$PKG_DIR" "$LABEL" \
    >/tmp/events-hq-pal-"$LABEL".log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(same_entity_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" -ge 2 ] 2>/dev/null; then
    echo "events-hq-pal launched for $LABEL (pids $(echo $pids | tr '\n' ' ')) log=/tmp/events-hq-pal-$LABEL.log"
else
    echo "events-hq-pal: launch incomplete for $LABEL ($n proc) - check /tmp/events-hq-pal-$LABEL.log" >&2
    cat /tmp/events-hq-pal-"$LABEL".log 2>/dev/null >&2
    exit 1
fi
