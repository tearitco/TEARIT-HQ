TEST-HARNESS - user-pal login GUI (key-injection UX testing)
Built 2026-07-27. Same family pattern as 044.pal-chat-irc/test-harn-same/
and 041.pal-forum/test-harn-same/. Read this before extending.

================================================================================
WHY / DECISION
================================================================================

0.a-z-pets-plan midterm (2026-07-27) puts **user login** as the active
near-term focus before avatar/xyzfs/shell. 00.login-signup already has a
real GUI (login.chtpm + userpal_* ops + session-isolated button.sh).
What it lacked: an AI-runnable harness that drives that GUI with real
keystrokes and asserts frames/files.

This harness proves that path first. Deliberately deferred here:
  - avatar creation auto-launch (01.avatar-creation stays separate)
  - orchestrator/kill_all 101 conversion (nice-to-have; blunt kill works)
  - passwords (family v1 rule: no auth layer unless asked)

Done in this slice:
  - UUID mint at Create Account
  - xyzfs/users/<uuid>/{home,projects,meta.txt} multi-user trees
  - current_login carries current_user_uuid + current_xyzfs

#.haiku+ docs are older (Jul 19-21) for *priorities* but the testing
methodology and tk_* UX injection rules remain the reference.

================================================================================
LAYOUT
================================================================================

test-harn-same/
  button.sh                     - thin entry (compile/demo/kill)
  ops/
    tk_inject_key.c             - one KEY_PRESSED line
    tk_type_text.c              - type string char-by-char
    tk_focus_item.c             - focus numbered item by label
    tk_assert_contains.c        - PASS/FAIL substring check
    +x/                         - compiled binaries
  scenarios/
    demo_login_signup.sh        - create -> logout -> login -> logout
                                  + unknown-user refuse
  README.txt                    - this file

================================================================================
USAGE
================================================================================

  cd 0.user-pal👤️/00.login-signup
  ./test-harn-same/button.sh compile
  ./test-harn-same/button.sh demo

Ops alone (live session poke):
  SESS=$(ls -dt pieces/sessions/*/ | head -1)
  ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Log In"
  ops/+x/tk_inject_key.+x "$SESS" 13
  cat "$SESS/pieces/display/current_frame.txt"

Proof frames land in: proof/harness-<timestamp>/

================================================================================
SCENARIO CHECKPOINTS
================================================================================

1. Start with cleared current_login.txt -> frame "Not logged in"
2. Fill User ID + Display Name, Create Account -> auto-login message +
   users/<id>/profile.txt + current_login.txt
3. Log Out -> "Not logged in"
4. Log In same id -> "Logged in as: <id>"
5. Log Out again
6. Log In unknown id -> "No such user", still not logged in

Never hardcode menu item numbers - always tk_focus_item by label.
Always clear cli_io fields before typing (backspace spam).

================================================================================
LIVE BUG FIXED WHILE BUILDING THIS (2026-07-27)
================================================================================

First paint showed only "[Map Loading...]" even though
pieces/apps/player_app/view.txt already had the real login chrome.
Cause: chtpm_parser_pal caches game_map on first load_vars before the
pal loop writes view.txt; the compose marker often does not "grow"
again until a keypress. Fix: button.sh pre-runs
ops/+x/userpal_compose_frame.+x under PRISC_PROJECT_ROOT=session before
starting chtpm. Verified: harness initial assert "Not logged in" now
passes without injecting a dummy key.
