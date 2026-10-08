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
BUTTON|https://httpbin.org/get|get|Go
EOF
PASS=0
FAIL=0

run() {
    rm -f "$HV/#.desktop/network_browser_fields.txt" \
          "$HV/#.desktop/network_browser_console.txt" \
          "$HV/#.desktop/network_browser_request.txt"
    [ -n "${1:-}" ] && printf '%b' "$1" > "$HV/#.desktop/network_browser_fields.txt"
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

# expect_submit <label> <fields> <expected query substring>
expect_submit() {
    run "$2"
    case "$NOTE" in
        *blocked*) echo "FAIL: $1 - unexpected block: $NOTE"; FAIL=$((FAIL+1)); return ;;
    esac
    case "$REQ" in
        *"$3"*) echo "PASS: $1"; PASS=$((PASS+1)) ;;
        *) echo "FAIL: $1 - request '$REQ' lacks '$3'"; FAIL=$((FAIL+1)) ;;
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

echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1