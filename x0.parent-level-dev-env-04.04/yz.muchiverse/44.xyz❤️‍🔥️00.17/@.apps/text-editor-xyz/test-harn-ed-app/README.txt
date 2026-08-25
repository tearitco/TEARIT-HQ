test-harn-ed-app — level-2 harness for @.apps/text-editor-xyz
==============================================================

Proves: editor + file-menu widget actually LOAD and SAVE text through
REAL entry points and REAL key injection — not the op-level shortcut
%.harnesses/file-menu+editor/scenarios/demo_load_save.sh already
proved (that one calls ops/+x/fm_set_focus.+x and fm_enqueue_cmd.+x
directly, never through button.sh, never through prisc+x's custom-op
dispatch, never through a real keypress — see !.xyzos-standards+1.txt
§36.6 and !.xyzos-pitfalls+1.txt PITFALL 21/50 for why that's not enough
on its own).

Name: "test-harn-ed-app" — test-harn- prefix (greppable, per §36.1),
descriptive suffix naming what's actually under test (this app).
Lives here, not inside 102.editor-📄️00.00/ or &.widgits/file-menu/,
because it tests the COMBINED app (@.apps/text-editor-xyz), the entity
that actually encompasses both modules — matches §36.3's own guidance
on where cross-project stories belong.

MODULAR BY DESIGN (direct instruction): demo-load and demo-save are
fully independent. Neither depends on the other having run, neither
leaves state the other assumes. Run either alone, or both.

Run
---
  ./button.sh compile     - check dependencies exist
  ./button.sh demo-load   - LOAD only
  ./button.sh demo-save   - real EDIT + SAVE_AS only
  ./button.sh demo        - both, sequentially
  ./button.sh kill        - EMERGENCY_KILL.sh (PITFALL 55 — fixed
                            2026-07-30, safe to use now)

Before running: read the handoff's own "START HERE" block —
../EDITOR-WIDGET-INTEGRATION-HANDOFF.txt — especially PITFALL 53
(never background these launchers manually; this harness's own
common.sh already redirects stdin from /dev/null when it backgrounds
the app, which sidesteps that specific trap, but running ANYTHING
else in this family with a bare trailing `&` from your own shell will
still hit it).

What each scenario actually does (§36.6 level-2 mechanics)
-----------------------------------------------------------
demo_load.sh:
  1. Writes a fixture file with a unique marker string.
  2. Launches @.apps/text-editor-xyz/button.sh run for real.
  3. Injects real keys into file-menu's OWN pieces/keyboard/
     history.txt: '4' (LOAD) -> arrow to FILE field -> types the
     fixture's absolute path -> arrow to CONFIRM -> Enter. Same
     [TS] KEY_PRESSED: <code> format and same relay path
     (chtpm_parser_pal reads history.txt, populates
     interact_relay.txt, prisc+x's PAL loop reads that) a human
     keystroke actually goes through.
  4. Asserts BOTH: editor's own editor_buffer.txt state file matches
     the fixture byte-for-byte, AND editor's own rendered
     current_frame.txt actually contains the marker string — the
     second check is the part no op-level test can ever catch (a
     state file can be correct while the frame a real user would see
     is stale).

demo_save.sh:
  1. Launches the app for real.
  2. Injects Enter on editor's own default-selected "EDIT TEXT
     (INTERACT)" button — a REAL chtpm onClick="INTERACT" engagement,
     not a shortcut (confirmed: editor_menu_input.c's own digit
     dispatch has no handler for piece.pdl's "EDIT" command string;
     this only happens via chtpm_parser_pal's own native INTERACT
     handling on that specific button).
  3. Waits for pieces/display/active_gui_is_typing.txt to actually
     flip to 1 (a real synchronization point, not a guessed sleep).
  4. Types a unique marker string as real keys — this actually EDITS
     the buffer, it does not just pass through the untouched seed
     content.
  5. ESC to exit INTERACT, '4' (FILE MENU) to signal file-menu.
  6. Injects real keys into file-menu: '3' (SAVE_AS) -> arrow to FILE
     field -> types an absolute output path -> arrow to CONFIRM ->
     Enter.
  7. Asserts the FILE ON DISK at that path actually contains the
     marker string that was typed in step 4 — proves real editing +
     real save, not a static buffer pass-through.

Both scenarios also assert pieces/system/widget_cmds/status.txt shows
the expected last_cmd/result, and both write full evidence (fixture,
injected key tail, buffer/frame/status snapshots) to proof/<scenario>-
<timestamp>/ for post-hoc review without re-running.

Relationship to %.harnesses/file-menu+editor/
------------------------------------------------
That harness is NOT superseded or replaced by this one — per §36.6,
level-1 (op-level) coverage is a real, valuable SUPPLEMENT (faster,
more precise once something's already known to be broken), never a
replacement. This harness is the level-2 floor that makes a genuine
"LOAD/SAVE work" claim valid; that one still isolates bugs faster once
this one finds something wrong.
