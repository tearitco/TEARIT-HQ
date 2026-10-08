#!/bin/sh
# nb_form_test.sh - hermetic regression test for the submit validation gate
# (nb_write_submit.sh). Builds a throwaway house root, drops a page.state with
# one required text field plus optional email/url/number fields into it, and
# drives the submit script directly for each case.
#
# Why hermetic instead of relay-driven: the live window churns (other agents,
# page loads, nav renumbering) which made relay tests flake. The gate is pure
# shell over page.state + fields.txt, so a sandbox house root tests the real
# code path with zero X11 and zero live-state pollution.
#
# Usage: sh nb_form_test.sh        exit 0 = all cases pass
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
HV="$(mktemp -d)"
trap 'rm -rf "$HV"' EXIT
mkdir -p "$HV/#.desktop"
STATE="$HV/#.desktop/network_browser_page.state.txt"
cat > "$STATE" <<'EOF'
URL|file:///tmp/form-types.html
TITLE|Type validation
INPUT|em|email||Email|0
INPUT|hm|url||Home|0
INPUT|qt|number||Qty|0
INPUT|nm|text||Name|1
INPUT|note|textarea||Notes|0
BUTTON|https://httpbin.org/get|get|Go
EOF
PASS=0
FAIL=0
last=""

run() {
    rm -f "$HV/#.desktop/network_browser_fields.txt" \
          "$HV/#.desktop/network_browser_console.txt" \
          "$HV/#.desktop/network_browser_request.txt" \
          "$HV/#.desktop/network_browser_checks.txt"
    [ -n "${1:-}" ] && printf '%b' "$1" > "$HV/#.desktop/network_browser_fields.txt"
    [ -n "${2:-}" ] && printf '%b' "$2" > "$HV/#.desktop/network_browser_checks.txt"
    sh "$PKG/ops/nb_write_submit.sh" submit "https://httpbin.org/get" get "$PKG" "$HV" >/dev/null 2>&1
    NOTE="$(tail -1 "$HV/#.desktop/network_browser_console.txt" 2>/dev/null)"
    REQ="$(tail -1 "$HV/#.desktop/network_browser_request.txt" 2>/dev/null)"
}

# expect_blocked <label> <fields> <expected reason substring>
expect_blocked() {
    run "$2"
    case "$NOTE" in
        *"blocked, invalid form"*"$3"*) ;;
        *) echo "FAIL: $1 - expected block mentioning '$3', note was: ${NOTE:-<none>}"; FAIL=$((FAIL+1)); return ;;
    esac
    [ -n "$REQ" ] && { echo "FAIL: $1 - blocked but a request was still written: $REQ"; FAIL=$((FAIL+1)); return; }
    echo "PASS: $1"; PASS=$((PASS+1))
}

# expect_submit <label> <fields> [<checks-file-contents>] <needle>
# The needle is always the LAST argument and the checks file is optional, so
# the 3-arg and 4-arg forms both work.
expect_submit() {
    case $# in
        3) run "$2" "" ;;
        *) run "$2" "$3" ;;
    esac
    for last in "$@"; do :; done   # last arg = needle
    NEEDLE="$last"
    case "$NOTE" in
        *blocked*) echo "FAIL: $1 - unexpected block: $NOTE"; FAIL=$((FAIL+1)); return ;;
    esac
    case "$REQ" in
        *"$NEEDLE"*) echo "PASS: $1"; PASS=$((PASS+1)) ;;
        *) echo "FAIL: $1 - request '$REQ' lacks '$NEEDLE'"; FAIL=$((FAIL+1)) ;;
    esac
}

echo "== submit validation gate"
expect_blocked "required text empty is refused"          ""                          "nm"
expect_blocked "email without host.tld is refused"        'nm\tBob\nem\tnotanemail\n' "em(email)"
expect_blocked "email without @ is refused"               'nm\tBob\nem\tplainword\n'  "em(email)"
expect_blocked "url without scheme is refused"            'nm\tBob\nhm\tftp:/x\n'     "hm(url)"
expect_blocked "number with trailing letters is refused"  'nm\tBob\nqt\t12abc\n'      "qt(number)"
expect_blocked "every offending field is reported"        'nm\tBob\nem\tx\nhm\ty\nqt\tz\n' "em(email) hm(url) qt(number)"
expect_submit "valid values submit"                       'nm\tBob\nem\tbob@ex.com\nhm\thttps://ex.com\nqt\t-3.5\n' "nm=Bob&em=bob%40ex.com&hm=https%3A%2F%2Fex.com&qt=-3.5"
expect_submit "optional format fields may stay empty"      'nm\tBob\n'                 "nm=Bob"
expect_submit "optional empty format field sends empty"    'nm\tBob\nem\t\n'           "em="

# Defaults: a browser submits the value= of an input the user never
# touched, and a pre-checked box submits even when nothing was toggled.
# Both regressed once (step 2 was gated on the checks file existing), so
# they are pinned here.
DEFSTATE="$HV/#.desktop/network_browser_page.state.txt"
cat > "$DEFSTATE" <<'EOF'
INPUT|cb|checkbox|checked|yes
INPUT|off1|checkbox||yes
INPUT|r2|radio|checked|r2-val
INPUT|note|textarea|hello world|Notes|0
INPUT|em|email|default@ex.com|Email|0
INPUT|nm|text||Name|1
HIDDEN|tok|abc123
EOF
echo "== defaults and toggles"
expect_submit "pre-checked box submits with no checks file" 'nm\tBob\n' "" "cb=yes"
# An unchecked box must NOT appear (negative check, so not via expect_submit).
run 'nm\tBob\n' ""
case "$REQ" in
    *off1=*) echo "FAIL: unchecked box off1 was submitted"; FAIL=$((FAIL+1)) ;;
    *)       echo "PASS: unchecked box stays out"; PASS=$((PASS+1)) ;;
esac
expect_submit "pre-checked radio submits its value"         'nm\tBob\n' "" "r2=r2-val"
expect_submit "textarea default submits"                    'nm\tBob\n' "" "note=hello+world"
expect_submit "text-ish default submits"                   'nm\tBob\n' "" "em=default%40ex.com"
expect_submit "hidden field always submits"                'nm\tBob\n' "" "tok=abc123"
expect_submit "typed value beats page default"             'nm\tBob\nem\ttyped@ex.com\n' "" "em=typed%40ex.com"
# Toggle off: cb must NOT appear (the page default was checked, the user
# switched it off). printf %b would expand \t inside the needle, so build
# it with a real tab.
# Toggle off must suppress a pre-checked default: the toggle is the live
# truth, the page default is only a fallback.
expect_submit "toggle off beats pre-checked default"       'nm\tBob\n' 'cb\toff\n' 'nm=Bob'
case "$REQ" in
    *cb=*) echo "FAIL: toggle off still submitted cb"; FAIL=$((FAIL+1)) ;;
    *)     echo "PASS: toggle off suppresses cb"; PASS=$((PASS+1)) ;;
esac
cat > "$STATE" <<'EOF'
URL|file:///tmp/form-types.html
TITLE|Type validation
INPUT|em|email||Email|0
INPUT|hm|url||Home|0
INPUT|qt|number||Qty|0
INPUT|nm|text||Name|1
INPUT|note|textarea||Notes|0
BUTTON|https://httpbin.org/get|get|Go
EOF

echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1