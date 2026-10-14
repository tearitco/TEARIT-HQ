#!/bin/bash
# nb_focus_safety_test.sh - G1.4: an unsolicited republish must never steal
# the address bar or reset the cursor mid-typing.
#
# This is the last G1 risk the push protocol introduced. Once the manager
# can republish on the worker's say-so, a page that updates itself rewrites
# the projection while the user is halfway through typing a URL. If that
# reparse re-hydrates the armed <cli_io> from scratch, the keystrokes go
# somewhere else and the text is silently scrambled - the worst possible
# failure mode for an address bar, because it looks like the browser
# ignoring you.
#
# WHERE THE RISK ACTUALLY LIVES: not in the manager (it only rewrites the
# projection file - that half is proven by nb_tick_live_test.sh, where the
# page's tick row advances with no input) but in the RENDERER's
# reparse_chtpm_if_changed(). That is what this suite drives, hermetically:
# own fixture window, own cli_io, and a projection that is rewritten under
# it on a timer, exactly as a ticking page would.
#
# TWO ASSERTIONS, and the second is the one that matters:
#   1. the typed text survives the churn;
#   2. the CURSOR is still at the end - proved by typing one more character
#      afterwards and requiring it to APPEND ("helloX"), not to land at
#      position 0 ("Xhello"). A reset cursor is invisible to assertion 1
#      and is precisely the bug this KPI is about.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$RENDER" ] || { echo "FAIL: renderer not built ($RENDER)"; exit 1; }

WORK="$(mktemp -d /tmp/nb_focus.XXXXXX)"
RPID=""
cleanup() { [ -n "$RPID" ] && kill "$RPID" 2>/dev/null; rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

cat > "$WORK/focus.xhtpm" <<'EOF'
<window label="FocusProbe" class="focus-fixture database-window">
  <page id="main">
    <text id="m1" class="lbl" label="address bar below"/>
    <cli_io id="addr" label="addr"/>
  </page>
</window>
EOF
cat > "$WORK/focus.css" <<'EOF'
window { width: 700px; height: 200px; }
EOF

# The renderer publishes the armed field's buffer to
# <workdir>/cli_io_state.txt as "<armed>:<id>=<text>". That is where the
# assertion reads from - the ascii mirror shows the element LABEL, not the
# buffer, so it cannot see what was typed.
state() { cat "$WORK/cli_io_state.txt" 2>/dev/null; }

# rewrite the projection, the way a pushed page render makes the manager
# rewrite it: same document, new content, new mtime.
republish() {
    cat > "$WORK/focus.xhtpm" <<EOF
<window label="FocusProbe" class="focus-fixture database-window">
  <page id="main">
    <text id="m1" class="lbl" label="address bar below (render $1)"/>
    <cli_io id="addr" label="addr"/>
  </page>
</window>
EOF
}

type_() {   # type_ <string> -> relay codes 32..126 are literal characters
    # POSIX on purpose: nb_all_tests.sh runs these with `sh`, and
    # ${1:$i:1} is a bashism that dash rejects outright.
    _n=$(printf '%s' "$1" | wc -c)
    _i=1
    while [ "$_i" -le "$_n" ]; do
        _c=$(printf '%s' "$1" | cut -c"$_i")
        _code=$(printf '%d' "'$_c")
        printf '%s\n' "$_code" >> "$RELAY"
        sleep 0.25
        _i=$((_i + 1))
    done
}

setsid "$RENDER" "$HR" "$WORK/focus.xhtpm" "$WORK" >"$WORK/render.log" 2>&1 &
RPID=$!
sleep 6
kill -0 "$RPID" 2>/dev/null || { echo "FAIL: fixture renderer died"; tail -5 "$WORK/render.log"; exit 1; }
RELAY="$HR/#.desktop/entity_menu_history/$RPID.txt"

printf 'FOCUS_CLI_IO\n' >> "$RELAY"; sleep 1.5
type_ "hello"; sleep 1.5

ARMED="$(state)"
echo "  after typing: $ARMED"
case "$ARMED" in
  *addr=hello) echo "PASS: the address bar is armed and holds the typed text" ;;
  *) fail "arming or typing failed (got '$ARMED'); the rest of this suite proves nothing"; exit 1 ;;
esac

# ---- the churn: eight republishes, as a ticking page would produce -------
for n in 1 2 3 4 5 6 7 8; do
    republish "$n"
    sleep 0.7
done
sleep 1.5

AFTER="$(state)"
echo "  after 8 unsolicited republishes: $AFTER"
# VACUITY GUARD, and it is the one that matters most here: "the text
# survived" is worthless if the churn never happened. The fixture's label
# carries the render number, so if the mirror shows "render 8" then the
# renderer really did reparse eight times underneath the armed field.
MIRROR="$HR/#.desktop/ascii_frames/$RPID.frame.txt"
if grep -q 'render 8' "$MIRROR" 2>/dev/null; then
    echo "PASS: the projection really was reparsed under the armed field"
else
    fail "the projection never reparsed - the churn proved nothing, and the "\
"survival assertion below is vacuous"
    exit 1
fi
if [ "$AFTER" = "$ARMED" ]; then
    echo "PASS: the typed text survived 8 unsolicited republishes"
else
    fail "an unsolicited republish changed the typed text: '$ARMED' -> '$AFTER'"
fi

# ---- the cursor: one more character must APPEND, not prepend -------------
type_ "X"; sleep 1.5
FINAL="$(state)"
echo "  after typing one more char: $FINAL"
case "$FINAL" in
  *addr=helloX) echo "PASS: the cursor is still at the end (appended, not prepended)" ;;
  *addr=Xhello) fail "the cursor jumped to position 0 on a republish: '$FINAL'" ;;
  *addr=hello)  fail "the field lost ARM focus across a republish: the text is "\
"intact but the next keystroke went nowhere (still '$FINAL'). This is the "\
"real G1.4 failure - verified by disabling the reparse re-arm on purpose, "\
"which produces exactly this." ;;
  *) fail "unexpected buffer after the cursor check: '$FINAL'" ;;
esac

[ "$FAIL" -eq 0 ] && echo "nb_focus_safety_test.sh PASS"
exit $FAIL