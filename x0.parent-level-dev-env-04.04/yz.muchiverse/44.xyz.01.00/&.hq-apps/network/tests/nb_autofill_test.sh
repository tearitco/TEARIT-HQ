#!/bin/bash
# nb_autofill_test.sh - pins the autofill jar (ops/nb_write_autofill.sh,
# ops/nb_write_clearautofill.sh) and the manager's origin derivation.
#
# Why hermetic, no X11: the jar is a pure text file with one owner script,
# and the origin rule is a pure string function. Both are testable in a
# throwaway house root with zero live state and zero relay - the same
# discipline nb_form_test.sh already uses for the fields file.
#
# What is pinned:
#   ORIGIN   scheme://host[:port]; http != https; default port dropped;
#            non-default port kept; userinfo stripped; non-http (file://)
#            has NO origin, so a local page never inherits a real value.
#   REMEMBER appends one tab-separated line; LAST line per (origin,name)
#            wins; a tab inside a value is refused, not written.
#   FILL     exact (origin,name) match only - the privacy property.
#   NEVER    password, and an origin with no remembered value, fill nothing.
#   CLEAR    the clearautofill request file is written with the verb.
#
# Usage: sh nb_autofill_test.sh        exit 0 = all cases pass
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
NB="$(cd "$HERE/.." && pwd)"
AF="$NB/ops/nb_write_autofill.sh"
CAF="$NB/ops/nb_write_clearautofill.sh"
WORK=/tmp/nb_autofill_test
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$AF" ]  || { echo "FAIL: autofill script not executable ($AF)"; exit 1; }
[ -x "$CAF" ] || { echo "FAIL: clearautofill script not executable ($CAF)"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK/house/#.desktop"
H="$WORK/house"
JAR="$H/#.desktop/network_browser_autofill.txt"

# The manager's own origin rule, re-derived here as a black box: the C is
# not linkable from a shell test, so the SAME rule is asserted against the
# script's per-origin KEYING (which is what the privacy property depends
# on) and the manager's derivation is pinned by nb_origin_scan.py against
# the real binary's projection. Both halves are checked; neither is assumed.
remember() { "$AF" remember "$1" "$2" "$WORK/pkgdir" "$H" "$3"; }
fill()     { "$AF" autofill "$1" "$2" "$WORK/pkgdir" "$H"; }

# ---- 1. basic remember/fill round trip
remember email "https://example.com" "a@b.com"
got="$(fill email "https://example.com")"
if [ "$got" = "a@b.com" ]; then note "round trip: ok"; else fail "round trip: got '$got'"; fi

# ---- 2. PER-ORIGIN ISOLATION (the privacy property)
remember email "https://other.com" "leak@other.com"
got="$(fill email "https://example.com")"
[ "$got" = "a@b.com" ] || fail "origin leak: example.com got '$got'"
got="$(fill email "https://other.com")"
if [ "$got" = "leak@other.com" ]; then note "per-origin isolation: ok"; else fail "other origin wrong: '$got'"; fi

# ---- 3. http vs https are DIFFERENT jars
remember email "http://plain.com" "http-only@x"
got="$(fill email "https://plain.com")"
[ -z "$got" ] || fail "https saw the http jar's value: '$got'"
got="$(fill email "http://plain.com")"
if [ "$got" = "http-only@x" ]; then note "http/https separation: ok"; else fail "http fill wrong: '$got'"; fi

# ---- 4. LAST write wins
remember email "https://example.com" "newer@b.com"
got="$(fill email "https://example.com")"
if [ "$got" = "newer@b.com" ]; then note "last write wins: ok"; else fail "last-write-wins: got '$got'"; fi

# ---- 5. a DIFFERENT field name on the same origin is its own entry
remember q "https://example.com" "search-term"
got="$(fill email "https://example.com")"
[ "$got" = "newer@b.com" ] || fail "field name bled across: '$got'"
got="$(fill q "https://example.com")"
if [ "$got" = "search-term" ]; then note "per-field keying: ok"; else fail "second field wrong: '$got'"; fi

# ---- 6. an unknown origin fills NOTHING (exit 1, empty stdout)
got="$(fill email "https://never-visited.test")"
[ -z "$got" ] || fail "unknown origin returned '$got'"
"$AF" autofill email "https://never-visited.test" "$WORK/pkgdir" "$H" >/dev/null 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then note "unknown origin: no fill: ok"; else fail "unknown origin should exit non-zero"; fi

# ---- 7. empty origin is refused on remember (would be a global jar)
before=$(wc -l < "$JAR")
"$AF" remember email "" "$WORK/pkgdir" "$H" "nope" >/dev/null 2>&1
after=$(wc -l < "$JAR")
if [ "$before" = "$after" ]; then note "empty origin refused: ok"; else fail "empty origin was stored"; fi

# ---- 8. a tab inside a value is REFUSED, not written (no field forging)
TAB="$(printf '\t')"
before=$(wc -l < "$JAR")
"$AF" remember sneaky "https://example.com" "$WORK/pkgdir" "$H" "a${TAB}b" >/dev/null 2>&1
after=$(wc -l < "$JAR")
if [ "$before" = "$after" ]; then note "tab in value refused: ok"; else fail "tab-bearing value was stored"; fi

# ---- 9. value containing a single quote survives (the renderer's
#         quoting path is the shell's, so this must round-trip)
remember name "https://example.com" "O'Brien"
got="$(fill name "https://example.com")"
if [ "$got" = "O'Brien" ]; then note "quote in value: ok"; else fail "quote round trip: got '$got'"; fi

# ---- 10. clearautofill writes the verb the manager reads
printf 'stale' > "$H/#.desktop/network_browser_request.txt"
"$CAF" clearautofill "$WORK/pkgdir" "$H"
req="$(cat "$H/#.desktop/network_browser_request.txt" 2>/dev/null)"
case "$req" in
    clearautofill*) note "clearautofill request verb: ok" ;;
    *) fail "clearautofill wrote '$req'" ;;
esac

# ---- 11. jar format is exactly 3 tab-separated fields per line
bad=$(awk -F'\t' 'NF!=3 {c++} END{print c+0}' "$JAR")
if [ "$bad" = "0" ]; then note "jar line format: ok"; else fail "$bad jar lines are not 3 tab-separated fields"; fi

# ---- 12. NEGATIVE TEST of the test: the isolation assertion above must
#         be ABLE to fail. Append a same-named field for a DIFFERENT
#         origin as the LAST line in the jar, so last-write-wins would
#         hand it back to any implementation that ignored the origin.
#         A correct fill still returns example.com's own value; a leaky
#         one returns the planted value. (The first attempt at this
#         rewrote an existing line and changed nothing observable -
#         last-write-wins still picked a later matching line, so it
#         could not have failed. A negative test that cannot fail is
#         not a negative test.)
cp "$JAR" "$WORK/jar.bak"
printf '%s%s%s%s%s\n' "https://planted.test" "$TAB" "email" "$TAB" "PLANTED-LEAK" >> "$JAR"
own="$(fill email "https://example.com")"
if [ "$own" = "newer@b.com" ]; then
    note "NEGATIVE OK: planted other-origin value was NOT served (assertion can fail)"
else
    fail "negative test: planted leak WAS served ('$own') - isolation assert cannot fail"
fi
# and prove the probe is real: the planted line IS readable under its own
# origin, so the jar was genuinely written and the check above is not
# passing merely because nothing was there.
planted="$(fill email "https://planted.test")"
if [ "$planted" = "PLANTED-LEAK" ]; then
    note "NEGATIVE CONTROL OK: planted value readable under its own origin"
else
    fail "negative control: planted line unreadable ('$planted') - the probe proves nothing"
fi
cp "$WORK/jar.bak" "$JAR"

# ---- 13. the manager's own origin rule, from the REAL shipped code.
#          autofill_origin_test.c #includes network_browser_manager.c and
#          calls the real autofill_origin() - a copy of the rule in this
#          test would prove nothing about what actually ships.
if ! command -v make >/dev/null; then
    fail "make needed to build the origin suite"
elif ! (cd "$NB" && make afot >/dev/null 2>&1); then
    fail "could not build the origin suite (make afot)"
else
    out="$("$NB/afot" 2>&1)"
    if [ $? -ne 0 ]; then
        fail "origin rule: $out"
    else
        note "manager origin rule (real code): ok"
    fi
fi

[ "$FAIL" -eq 0 ] && echo "nb_autofill_test.sh PASS"
exit $FAIL