TEST-HARNESS - reusable, ops-based key-injection UX testing
Built 2026-07-26. Read this before writing a new test for this project,
or before copying this pattern into another project in the family.

================================================================================
WHY THIS EXISTS (vs. just writing another .sh script)
================================================================================

This project already had testing/test_multiuser_p2p.sh and
testing/test_real_ux_2users.sh - both work, but the actual key-injection
logic (append a KEY_PRESSED line, type a string char-by-char, find a
numbered menu item's current number and focus it, assert a frame
contains something) was written directly as bash functions INSIDE those
scripts. That means:
  - it can't be reused by anything except that one script
  - it can't be called directly by an agent poking around interactively
  - it can't be reused by a DIFFERENT kind of caller (a future CI job, a
    different scenario script, another project's own test harness)
  - the trickiest logic (multi-digit nav-jump - see
    #.haiku+/!.local-ux-testing-ai.txt Part 1) is buried in bash string
    manipulation instead of being a small, independently testable unit

This directory fixes that by following the SAME architecture the actual
app already uses: a thin button.sh entry point that never contains the
real logic, delegating to small reusable ops/ (self-contained C
binaries, one job each). Scenarios (like the 2-user chat demo) are then
just a SEQUENCE of calls to those ops - see scenarios/demo_2user_chat.sh,
which is barely more than a list of ops invocations with sleeps between
them.

================================================================================
LAYOUT
================================================================================

test-harn-same/
  button.sh                        - thin entry point (compile/demo/kill/help)
  ops/
    tk_inject_key.c                 - append one KEY_PRESSED line
    tk_type_text.c                  - type a string, one key per character
    tk_focus_item.c                 - find a menu item's CURRENT number by
                                       its label text in a frame, inject the
                                       digit-by-digit nav-jump for it
    tk_assert_contains.c            - check a file for a substring, print
                                       PASS/FAIL, exit 0/1
    +x/                             - compiled binaries (via `button.sh compile`)
  scenarios/
    demo_2user_chat.sh              - the reference scenario: 2 real users,
                                       real signup/login/room-join/typed
                                       message, verifying live cross-session
                                       push with zero action on the idle side
  README.txt                        - this file

================================================================================
USING THE OPS DIRECTLY (agent poking around, or a different caller)
================================================================================

Every op is a normal CLI tool, callable on its own, no scenario needed:

  ops/+x/tk_inject_key.+x <session_dir> <decimal_key_code>
  ops/+x/tk_type_text.+x <session_dir> "<text>"
  ops/+x/tk_focus_item.+x <session_dir> <frame_file> "<label_substring>"
      -> prints the item number it focused, or exits 1 if not found
  ops/+x/tk_assert_contains.+x <file> "<expected_substring>" ["<check_label>"]
      -> prints PASS/FAIL, exits 0/1

Example, poking at a live session by hand:
  SESS=$(ls -dt pieces/sessions/*/ | head -1)
  ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Create Account"
  ops/+x/tk_inject_key.+x "$SESS" 13
  cat "$SESS/pieces/display/current_frame.txt"

================================================================================
WRITING A NEW SCENARIO
================================================================================

Copy scenarios/demo_2user_chat.sh as a starting point. A scenario file
should:
  1. Launch whatever real session(s) it needs (via the PARENT project's
     own `button.sh run --pal`, backgrounded with `setsid ... & disown`
     - see #.haiku+/!.local-ux-testing-ai.txt Part 3 for why plain `&`
     is risky across agent tool-call boundaries).
  2. Sequence ops/+x/tk_* calls to drive the UI - focus an item by
     label (never hardcode a number, see local-ux-testing-ai.txt Part 1
     for why), activate it (Enter), type/act, exit or send.
  3. Use tk_assert_contains for every checkpoint, not just at the end -
     cheap and gives you a real PASS/FAIL trail instead of "did it look
     right at the very end."
  4. Save proof (cp the relevant frame/frame_history files somewhere
     under proof/) BEFORE cleanup runs, regardless of pass/fail.
  5. trap cleanup EXIT that calls `bash test-harn-same/button.sh kill` and
     is not fooled by its "clean" message - that message is itself only
     as reliable as kill_all.sh's own verification, which has known
     gaps (PITFALL 20/21/22 in #.haiku+/!.xyzos-pitfalls+1.txt). Always
     independently re-check with `ps aux | grep` if this matters to you.

================================================================================
KNOWN LIMITATION: NOT (currently) A REAL .pal SCENARIO
================================================================================

The parent app's own scenarios are expressed as .pal scripts run through
system/prisc+x (register machine + `exec`/`sleep` opcodes - see PITFALL
13 in xyzos-pitfalls for the `exec` vs `op` gotcha). This harness's
scenarios are plain bash instead, for one concrete, checked reason:
prisc+x's own `exec` opcode parses its arguments with
`sscanf(args, "%255s %255s %255s")` - i.e. WHITESPACE-DELIMITED, max 3
tokens, no support for a single argument containing spaces. A chat
message like "hello bob, this is a real typed message" cannot be passed
as one exec argument in native PAL syntax without a workaround (e.g.
writing the text to a temp file first via some non-PAL step, then having
an op read it from there instead of argv). For SINGLE-WORD actions
(most focus/key-code steps) a real .pal scenario built from these same
ops IS straightforward and would look like:

  exec ./ops/+x/tk_inject_key.+x SESSION_DIR 13
  sleep 500000

If a future agent wants genuine .pal-scenario parity (not just
ops-based bash), the missing piece is a `tk_type_text_from_file.c`
variant that reads its text from a file path (one exec-safe argument)
instead of argv - not yet built, flagged here rather than left
undiscovered.

================================================================================
SEE ALSO
================================================================================
#.haiku+/!.local-ux-testing-ai.txt   - the full interaction model (Enter to
                                        activate, ESC vs Enter-to-send,
                                        multi-digit nav) these ops encode
#.haiku+/!.xyzos-pitfalls+1.txt        - PITFALL 20 (orchestrator launch bug
                                        this whole testing effort found),
                                        21 (why op-level/isolated-node tests
                                        aren't a substitute for this), 22
                                        (keyboard_input CPU-spin, fixed)
../testing/test_multiuser_p2p.sh     - the faster, op-level-only P2P check
                                        (no UI navigation) - good as a quick
                                        first pass before running the full
                                        UX scenario here
