%.harnesses/file-menu+editor — multi-project harness
=====================================================

Proves: &.widgits/file-menu ops × 102.editor widget cmd bus
(LOAD / SAVE_AS / NEW without a second human terminal).

House law: xyzos-standards §36 (cross-project harness under %.harnesses/).

Layout
------
  button.sh
  ops/hm_assert_*.c
  scenarios/demo_load_save.sh
  fixtures/
  proof/
  workdir/

Run
---
  ./button.sh compile
  ./button.sh demo

What it does
------------
  1. Boots an editor session (session-isolated)
  2. file-menu fm_set_focus → that session
  3. fm_enqueue LOAD fixture → editor_widget_cmds drains inbox
  4. Asserts buffer == fixture
  5. SAVE_AS via file-menu → disk matches
  6. NEW via file-menu → empty buffer

GL UI for file-menu is later; this harness is the cmd-bus contract.
