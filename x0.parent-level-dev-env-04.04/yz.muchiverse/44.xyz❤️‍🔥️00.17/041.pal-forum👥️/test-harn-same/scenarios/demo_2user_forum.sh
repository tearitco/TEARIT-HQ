#!/bin/bash
# demo_2user_forum.sh - reference SCENARIO for pal-forum, built from
# test-harn-same/ops/ (same generic tk_* primitives 044.pal-chat-irc/
# 041.pal-chain's own harnesses use - no project-specific logic in the
# ops themselves).
#
# Proves the REAL key-injected UX pipeline works for 2 concurrent
# same-install forum sessions: signup(auto-login) -> post -> follow ->
# refresh feed -> like, via actual keystrokes through the actual menu
# system (not direct op invocation). Unlike pal-chain, SIGNUP here
# auto-logs in on the SAME screen (no separate Back-to-Login round
# trip) - see forum_menu_input.c's own SIGNUP handler.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill
}
trap cleanup EXIT

key()    { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_()  { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }
focus()  { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.2; }
check()  { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }
# Real, live-caught bug (found building 041.pal-chain's own harness): a
# cli_io field can carry over content from a previous screen visit -
# re-activating it and typing again APPENDS instead of replacing.
# Always clear first.
fill_field() {
    local sess="$1" frame="$2" label="$3" value="$4"
    focus "$sess" "$frame" "$label"; key "$sess" 13
    for _ in $(seq 1 20); do "$OPS/tk_inject_key.+x" "$sess" 127; done
    type_ "$sess" "$value"; key "$sess" 27
}

echo "=== pal-forum 2-user real key-injected UX scenario ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

cd "$PROJECT_DIR"
NO_GL=1 setsid bash button.sh run > /tmp/th_forum_sess_a.log 2>&1 < /dev/null & disown
sleep 1
NO_GL=1 setsid bash button.sh run > /tmp/th_forum_sess_b.log 2>&1 < /dev/null & disown
sleep 3

SESS_A=$(ls -dt pieces/sessions/*/ | sed -n '2p'); SESS_A="${SESS_A%/}"
SESS_B=$(ls -dt pieces/sessions/*/ | sed -n '1p'); SESS_B="${SESS_B%/}"
FRAME_A="$SESS_A/pieces/display/current_frame.txt"
FRAME_B="$SESS_B/pieces/display/current_frame.txt"
USER_A="uforumA_$$"
USER_B="uforumB_$$"

signup_and_home() {
    local sess="$1" frame="$2" user="$3" dispname="$4"
    fill_field "$sess" "$frame" "User ID" "$user"
    fill_field "$sess" "$frame" "Display Name" "$dispname"
    focus "$sess" "$frame" "Create Account"; key "$sess" 13
    sleep 0.5
    check "$frame" "logged in as $user" "user '$user' signed up + auto-logged-in"
    focus "$sess" "$frame" "Continue to Home"; key "$sess" 13
    sleep 0.5
    check "$frame" "H O M E" "user '$user' reached home screen"
}

echo "--- user A ($USER_A): signup (auto-login) + reach home ---"
signup_and_home "$SESS_A" "$FRAME_A" "$USER_A" "ForumA"
cp "$FRAME_A" "$PROOF_DIR/userA_at_home.txt"

echo "--- user B ($USER_B): signup (auto-login) + reach home (SAME install, concurrent) ---"
signup_and_home "$SESS_B" "$FRAME_B" "$USER_B" "ForumB"
cp "$FRAME_B" "$PROOF_DIR/userB_at_home.txt"

POST_TEXT="hello from the ops-based forum harness $$"
echo "--- user A composes and posts a real message via real keystrokes ---"
focus "$SESS_A" "$FRAME_A" "Compose Post"; key "$SESS_A" 13
sleep 0.5
fill_field "$SESS_A" "$FRAME_A" "Post text" "$POST_TEXT"
# NOT "Post" bare - that's a substring of item 1's OWN label "Post
# text", so tk_focus_item's plain strstr() would match the field again
# instead of the button (real, live-caught bug building this scenario).
# Buttons render as "[Label]" (bracketed); fields render as
# "Label: [value]" (only the value is bracketed) - "[Post]" is
# unambiguous.
focus "$SESS_A" "$FRAME_A" "[Post]"; key "$SESS_A" 13
sleep 1
cp "$FRAME_A" "$PROOF_DIR/userA_post_result.txt"
check "$FRAME_A" "Posted (id=" "user A's real post was accepted (real forum_post.+x wrote a real wall.txt entry)"

# Get the real post id directly from A's own wall.txt (more robust than
# scraping the frame text) to drive the Like step below.
POST_ID=$(tail -1 "$PROJECT_DIR/users/$USER_A/wall.txt" 2>/dev/null | cut -d'|' -f2)
echo "real post id (from users/$USER_A/wall.txt): $POST_ID"

echo "--- user B follows user A via real keystrokes ---"
# B is already on home.chtpm right after signup (never navigated away) -
# no "Back to Home" click needed/available here.
focus "$SESS_B" "$FRAME_B" "Follow / Unfollow"; key "$SESS_B" 13
sleep 0.5
fill_field "$SESS_B" "$FRAME_B" "User ID" "$USER_A"
# "Follow" (not "Unfollow") - piece.pdl lists Follow before Unfollow so
# it's the first substring match; "Follow" is also not a substring of
# any OTHER visible label on this screen at this point.
focus "$SESS_B" "$FRAME_B" "Follow"; key "$SESS_B" 13
sleep 1
cp "$FRAME_B" "$PROOF_DIR/userB_follow_result.txt"
check "$FRAME_B" "Now following $USER_A" "user B followed user A via real keystrokes (real forum_follow.+x wrote a real following.txt entry)"

echo "--- user B refreshes feed and should see user A's real post ---"
focus "$SESS_B" "$FRAME_B" "Back to Home"; key "$SESS_B" 13
sleep 0.5
focus "$SESS_B" "$FRAME_B" "Feed"; key "$SESS_B" 13
sleep 0.5
focus "$SESS_B" "$FRAME_B" "Refresh Feed"; key "$SESS_B" 13
sleep 1
cp "$FRAME_B" "$PROOF_DIR/userB_feed.txt"
# The feed row display box has a fixed column width (~60 chars) and
# truncates long lines - checking for the full POST_TEXT (which
# includes a long unique $$ suffix) fails even though the post
# genuinely appears, just visually cut off. Check a short, still-
# unique prefix instead (real, live-caught while building this).
check "$FRAME_B" "hello from the ops-based forum harness" "user B's real feed (via real Refresh Feed click) shows user A's real post text - cross-user, same-install, real UI"

echo "--- user B likes user A's post (real post id typed from the feed) ---"
if [ -n "$POST_ID" ]; then
    fill_field "$SESS_B" "$FRAME_B" "Post ID" "$POST_ID"
    focus "$SESS_B" "$FRAME_B" "Like"; key "$SESS_B" 13
    sleep 1
    cp "$FRAME_B" "$PROOF_DIR/userB_like_result.txt"
    check "$FRAME_B" "Liked $POST_ID" "user B liked user A's real post via real keystrokes (real forum_like.+x wrote a real likes.txt entry)"
else
    fail "could not determine real post id from users/$USER_A/wall.txt - skipped Like step"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
