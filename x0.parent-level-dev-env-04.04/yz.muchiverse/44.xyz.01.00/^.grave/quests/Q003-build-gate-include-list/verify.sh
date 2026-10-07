#!/bin/bash
# verify.sh - the deterministic SCORER for quest Q003 (build gate include list). Nothing is scored by anyone saying "done": this reads the
# real build scripts and the real #include lines and prints a verdict. It is also an FSM-plan assertion: exit 0 = PASS, 1 = FAIL, 2 = usage.
#
# What it checks (a hash gate is only as correct as its input list - hash_gate.sh: "pass every real input file explicitly"):
#   every file the binary's sources #include (quoted includes, resolved recursively through the text-included .c/.h files) must be
#   listed in the gate's source list, or editing it will not trigger a rebuild.
#   target core    : roots khtpm_core_render.c            ; list = CR_SRCS in build_core_render.sh
#   target manager : roots khtpm_taskbar_manager{,_main}.c; list = MGR_SRCS in build_khtpm_strip.sh
#
# Usage: verify.sh [core|manager|all]        (default all)
#        verify.sh --selftest                prove the scorer itself: it must FAIL a list with one file removed and PASS the complete list
# Output (stable, machine-readable): one `MISSING|<target>|<path>` line per uncovered file, then `VERDICT|<PASS|FAIL>|<target>|required=N|listed=N|missing=N`.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../../.." && pwd)"                        # .../44.xyz.01.00
OPS="$HOUSE/_.monads/_.livedesk-taskbar/ops"
SHARED="$HOUSE/&.widgits/_shared-lib"

resolve() {   # resolve <including-file-dir> <name>  -> prints the absolute path, or nothing
    local d="$1" n="$2" c
    for c in "$d/$n" "$OPS/$n" "$SHARED/$n" "$OPS/lib/$n"; do
        if [ -f "$c" ]; then readlink -f "$c"; return 0; fi
    done
    return 1
}

required_for() {   # required_for <root files...>  -> sorted unique absolute paths of every quoted include reachable from the roots (excluding the roots)
    local seen="" queue=("$@") f inc p d roots
    roots=" $(for f in "$@"; do readlink -f "$f"; done | tr '\n' ' ') "
    while [ ${#queue[@]} -gt 0 ]; do
        f="${queue[0]}"; queue=("${queue[@]:1}")
        d="$(dirname "$f")"
        while read -r inc; do
            p="$(resolve "$d" "$inc")" || continue
            # an ops/ (or ops/lib/) file that has a same-named twin in $SHARED is a build-time COPY of it (house rule: edit the shared-lib
            # original, the ops copy is overwritten): the gate must hash the original, so score against the shared path
            case "$p" in "$OPS/"*) [ -f "$SHARED/$(basename "$p")" ] && p="$(readlink -f "$SHARED/$(basename "$p")")";; esac
            case " $seen " in *" $p "*) continue;; esac
            seen="$seen $p"
            case "$p" in *.c|*.h) queue+=("$p");; esac
        done < <(grep -o '^[[:space:]]*#[[:space:]]*include[[:space:]]*"[^"]*"' "$f" 2>/dev/null | sed -E 's/.*"([^"]*)"/\1/')
    done
    for p in $seen; do case "$roots" in *" $p "*) ;; *) echo "$p";; esac; done | sort -u
}

listed_core()    { local line; line="$(grep -m1 '^CR_SRCS=' "$OPS/build_core_render.sh")" || return 1;  list_from "$line"; }
listed_manager() { local line; line="$(grep -m1 '^MGR_SRCS=' "$OPS/build_khtpm_strip.sh")" || return 1; list_from "$line"; }
list_from() {      # list_from 'NAME="a $SHARED/b c"' -> absolute paths (expanding $SHARED, relative names are in the ops dir)
    local v="${1#*=}" t
    v="${v%\"}"; v="${v#\"}"
    for t in $v; do
        t="${t//\$SHARED/$SHARED}"; t="${t//\"\$SHARED\"/$SHARED}"
        case "$t" in /*) readlink -f "$t";; *) readlink -f "$OPS/$t";; esac
    done | sort -u
}

score() {          # score <target> <required-file> <listed-file>
    local target="$1" req="$2" lst="$3" miss=0 nreq nlst p
    nreq=$(wc -l < "$req"); nlst=$(wc -l < "$lst")
    while read -r p; do
        [ -z "$p" ] && continue
        grep -qxF "$p" "$lst" || { echo "MISSING|$target|${p#$HOUSE/}"; miss=$((miss+1)); }
    done < "$req"
    echo "VERDICT|$([ "$miss" -eq 0 ] && echo PASS || echo FAIL)|$target|required=$nreq|listed=$nlst|missing=$miss"
    return $([ "$miss" -eq 0 ] && echo 0 || echo 1)
}

run_target() {
    local t="$1" req lst rc
    req="$(mktemp)"; lst="$(mktemp)"
    case "$t" in
        core)    required_for "$OPS/khtpm_core_render.c" > "$req"; listed_core > "$lst" ;;
        manager) required_for "$OPS/khtpm_taskbar_manager_main.c" "$OPS/khtpm_taskbar_manager.c" > "$req"; listed_manager > "$lst" ;;
        *) echo "unknown target $t" >&2; rm -f "$req" "$lst"; return 2 ;;
    esac
    score "$t" "$req" "$lst"; rc=$?
    rm -f "$req" "$lst"; return $rc
}

selftest() {       # the scorer must be right about KNOWN inputs before it may judge anything
    local req good bad rc1 rc2 bad_n
    req="$(mktemp)"; good="$(mktemp)"; bad="$(mktemp)"
    required_for "$OPS/khtpm_core_render.c" > "$req"
    [ "$(wc -l < "$req")" -ge 5 ] || { echo "SELFTEST FAIL: found only $(wc -l < "$req") required files (include scan broken)"; return 1; }
    cp "$req" "$good"; tail -n +2 "$req" > "$bad"          # same list minus its first file
    score selftest-good "$req" "$good" >/dev/null; rc1=$?
    score selftest-bad  "$req" "$bad"  >/dev/null; rc2=$?
    rm -f "$req" "$good" "$bad"
    if [ "$rc1" -eq 0 ] && [ "$rc2" -eq 1 ]; then echo "SELFTEST PASS (complete list passes, list missing one file fails)"; return 0; fi
    echo "SELFTEST FAIL (good rc=$rc1 want 0, bad rc=$rc2 want 1)"; return 1
}

case "${1:-all}" in
    --selftest) selftest; exit $? ;;
    core|manager) run_target "$1"; exit $? ;;
    all) rc=0; run_target core || rc=1; run_target manager || rc=1; exit $rc ;;
    *) echo "usage: verify.sh [core|manager|all|--selftest]" >&2; exit 2 ;;
esac
