#!/bin/sh
# eval/edge.sh — SCM edge-case + robustness checks. Each must exit 0.
# The check is the "no crash, sane result" property; failures print a line.
set -u
cd "$(dirname "$0")/.."
B=+x/scm_cli
fails=0

expect_rc() { # want_rc name cmd...
    local want="$1"; shift
    local name="$1"; shift
    "$@" >/dev/null 2>&1
    local rc=$?
    if [ "$rc" -eq "$want" ]; then echo "OK   $name"; else echo "FAIL $name (rc=$rc want $want)"; fails=$((fails+1)); fi
}

echo "--- edge cases ---"
expect_rc 1 "missing corpus dir exits nonzero, no crash" ./"$B" select corpuses/does_not_exist "hi" --seed 1
expect_rc 0 "empty message ok" ./"$B" select corpuses/small-talk "" --seed 1
expect_rc 0 "unknown /rp pack falls back" ./"$B" select corpuses/tavern_rp "hi" --seed 1
expect_rc 0 "meta missing file ok" ./"$B" select corpuses/small-talk "hi" --meta no_such_situation --seed 1
expect_rc 0 "route command" ./"$B" route "what do you know about cats"
expect_rc 0 "route /rp unknown" ./"$B" route "/rp bogus"
expect_rc 0 "list intents" ./"$B" list
expect_rc 0 "slot template" ./"$B" select corpuses/tavern_rp "i need a drink" --seed 3 --rp

# repeatability: same seed -> same answer
a=$(./"$B" select corpuses/small-talk "hello there!" --seed 42)
b=$(./"$B" select corpuses/small-talk "hello there!" --seed 42)
if [ "$a" = "$b" ]; then echo "OK   seed repeatability"; else echo "FAIL seed repeatability ($a vs $b)"; fails=$((fails+1)); fi

# memory overflow: push 40 entries via demo, must not crash
if printf 'hello\n%.0s' $(seq 1 40) | printf 'hello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\nhello\n/quit\n' | ./"$B" demo --seed 1 >/dev/null 2>&1; then
    echo "OK   memory ring 40 pushes"
else
    echo "FAIL memory ring 40 pushes"; fails=$((fails+1))
fi

echo "---"
if [ "$fails" -eq 0 ]; then echo "EDGE: all pass"; else echo "EDGE: $fails failures"; fi
[ "$fails" -eq 0 ]
