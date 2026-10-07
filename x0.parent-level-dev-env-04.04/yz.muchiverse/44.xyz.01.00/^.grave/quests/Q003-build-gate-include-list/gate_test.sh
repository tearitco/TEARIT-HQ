#!/bin/bash
# gate_test.sh - behavioural proof for Q003: with the REAL hash_gate.sh and the REAL CR_SRCS list (read from build_core_render.sh), editing an #included file must mark the binary
# stale. Works on a scratch COPY of the files; compiles nothing; touches no real source. Prints PASS/FAIL lines; exit 0 = all pass.
# Also shows the BUG the fix closes: the same edit goes unnoticed with the old 6-file list.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; HOUSE="$(cd "$HERE/../../.." && pwd)"
OPS="$HOUSE/_.monads/_.livedesk-taskbar/ops"; SHARED="$HOUSE/&.widgits/_shared-lib"
. "$SHARED/hash_gate.sh"
LIST="$(grep -m1 '^CR_SRCS=' "$OPS/build_core_render.sh")"; LIST="${LIST#CR_SRCS=\"}"; LIST="${LIST%\"}"; LIST="${LIST//\$SHARED/$SHARED}"   # bash substitution: the folder name contains '&', which sed would expand
OLD="khtpm_core_render.c $SHARED/khtpm_css_parser.c $SHARED/khtpm_ui_scale.c $SHARED/khtpm_render_core.c $SHARED/khtpm_draw_core.c $SHARED/khtpm_reparse_diff.c"
T="$(mktemp -d)"; fail=0
copy_list() {   # copy each listed file into $T/<n>_<name>, print the copies (same order)
    local n=0 f out=""; for f in $1; do case "$f" in /*) ;; *) f="$OPS/$f";; esac; n=$((n+1)); cp "$f" "$T/${n}_$(basename "$f")"; out="$out $T/${n}_$(basename "$f")"; done; echo "$out"
}
check_edit() {  # check_edit <label> <list> <victim basename> <want: stale|unnoticed>
    local copies bin="$T/bin.+x" man="$T/manifest" victim got
    rm -rf "$T"/[0-9]*; copies="$(copy_list "$2")"
    echo x > "$bin"; chmod +x "$bin"; rm -f "$man"
    hash_gate_commit "$man" "$bin" $copies
    hash_gate_stale "$man" "$bin" $copies && { echo "FAIL|$1|unchanged tree reported stale"; fail=1; return; }     # no-op run must NOT rebuild
    victim="$(ls "$T" | grep "_$3\$" | head -1)"
    [ -n "$victim" ] || { echo "SKIP|$1|$3 is not in this list (that IS the bug)"; copies="$copies"; }
    [ -n "$victim" ] && echo "// gate_test edit" >> "$T/$victim"
    if hash_gate_stale "$man" "$bin" $copies; then got=stale; else got=unnoticed; fi
    if [ "$got" = "$4" ]; then echo "PASS|$1|edit of $3 -> $got (expected)"; else echo "FAIL|$1|edit of $3 -> $got, expected $4"; fail=1; fi
}
for f in khtpm_nav_echo.c house_wait.h khtpm_css_parser.h kh_boot_mark.h kh_proc_registry.h; do check_edit "new-list" "$LIST" "$f" stale; done
check_edit "old-list-bug" "$OLD" khtpm_nav_echo.c unnoticed   # the old list has no nav_echo: no victim, so nothing it could notice
rm -rf "$T"
[ "$fail" = 0 ] && echo "VERDICT|PASS" || echo "VERDICT|FAIL"
exit "$fail"
