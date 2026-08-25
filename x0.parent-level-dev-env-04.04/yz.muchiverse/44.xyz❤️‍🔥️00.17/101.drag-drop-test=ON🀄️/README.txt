DRAG-DROP TEST HARNESS - reusable ops-based visual drag-drop testing
Built 2026-07-26. Tests X11 Xdnd drag-drop from muchi-pals egg_window
to mutaclsym gl_mirror, verifying pet import.

================================================================================
WHY THIS EXISTS
================================================================================

The drag-drop feature needs visual testing: position two windows, simulate
a drag, verify the pet was imported. This harness provides:

1. Ops to position windows by writing to drag_drop_test.pdl (which the
   windows poll for position updates)
2. Ops to simulate drag-drop via xdotool
3. Ops to verify results (pet imported, file exists, etc.)
4. A scenario that sequences these ops

Each op is independently callable - an agent can poke at a live session
by calling ops directly, without running a full scenario.

================================================================================
LAYOUT
================================================================================

drag-drop-test/
  button.sh                        - thin entry point (compile/demo/kill/help)
  ops/
    dd_set_positions.c             - write window positions to config file
    dd_find_window.c               - find window ID by name
    dd_move_window.c               - move window to position via xdotool
    dd_drag_drop.c                 - simulate drag-drop via xdotool
    dd_assert_file.c               - check file exists and contains substring
    dd_check_import.c              - verify pet was imported to mutaclsym
    +x/                            - compiled binaries
  scenarios/
    test_basic_import.sh           - basic drag-drop import test
  README.txt                       - this file

================================================================================
USING THE OPS DIRECTLY
================================================================================

Every op is a normal CLI tool:

  ops/+x/dd_set_positions.+x <config_file> <gl_x> <gl_y> <egg_x> <egg_y>
      -> writes positions to config, windows poll and update

  ops/+x/dd_find_window.+x <window_name>
      -> prints window ID to stdout

  ops/+x/dd_move_window.+x <window_id> <x> <y>
      -> moves window via xdotool

  ops/+x/dd_drag_drop.+x <start_x> <start_y> <end_x> <end_y> [steps] [delay_ms]
      -> simulates drag-drop: mousedown, move, mouseup

  ops/+x/dd_assert_file.+x <file> "<expected_substring>" ["<label>"]
      -> prints PASS/FAIL, exits 0/1

  ops/+x/dd_check_import.+x <exchange_dir> <pet_id>
      -> checks if pet was imported, prints PASS/FAIL

Example, testing manually:

  # Position windows
  ops/+x/dd_set_positions.+x drag_drop_test.pdl 100 100 800 100

  # Wait for windows to poll (1 second)
  sleep 2

  # Find gl_mirror window
  GL_WID=$(ops/+x/dd_find_window.+x "mutaclsym RGB mirror")
  echo "gl_mirror window: $GL_WID"

  # Find egg_window window
  EGG_WID=$(ops/+x/dd_find_window.+x "pet egg_1")
  echo "egg_window window: $EGG_WID"

  # Simulate drag-drop
  ops/+x/dd_drag_drop.+x 840 140 420 202 20 50

  # Check if pet was imported
  ops/+x/dd_check_import.+x ../exchange egg_1

================================================================================
WRITING A NEW SCENARIO
================================================================================

Copy scenarios/test_basic_import.sh as a starting point. A scenario should:

1. Launch mutaclsym and muchi-pals (via their button.sh run)
2. Open a pet window in muchi-pals
3. Use dd_set_positions to position windows
4. Wait for windows to update
5. Use dd_drag_drop to simulate the drag
6. Use dd_assert_file and dd_check_import to verify results
7. Clean up (kill both apps)
8. Print PASS/FAIL summary

================================================================================
SEE ALSO
================================================================================
#.haiku+/tpmos-re-dox/_.0.aigent-testing-k3.txt  - agent testing guide
044.pal-chat-irc👥️+2/test-harn-same/             - key-injection test harness
