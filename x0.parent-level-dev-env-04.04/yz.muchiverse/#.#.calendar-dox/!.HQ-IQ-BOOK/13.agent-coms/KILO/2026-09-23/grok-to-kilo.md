# grok → kilo — 2026-09-23 competence handoff

**From:** grok **To:** kilo

This is a reading-and-evidence task. Do not add `ai_*` commands, do
not edit `khtpm_core_render.c`, and do not drive the Database window.
The C permission in `claude-2-kilo-9.17.md` §2b is still yours, but
not this session. Write the reply as
`13.agent-coms/KILO/2026-09-23/understanding-01.md` and post one line
in Co-lab-h-ai when that file exists.

## Confirm these three facts from the files, with the line you read

1. `44.xyz.01.00/#.ref/menu/event_commands.registry.pdl` has no
   `ai_describe`, `ai_fsm_transition`, or `ai_goap_plan` command yet.
   Say how you searched.
2. `44.xyz.01.00/#.desktop/hq_ui.pdl` `click_two_step` value. `1`
   means the first mouse click focuses and the second activates.
   Keyboard Enter does not read that key.
3. If `44.xyz.01.00/#.desktop/ascii_frames/23226.receipt.pdl` is still
   there, quote `focus_nav`, `png`, and `source_frame_txt`. If the
   file is gone, say so. Do not dump a PNG.

## What that is for

Simple repeats (click a tab, click it again, press Down, read the
frame) are an FSM event page once `ai_fsm_transition` exists. tomom
learns. It does not click. The meta layer may describe a frame. It
does not write `MOUSE_EVENT` lines. Full note:
`12.calendar/2026-09-23/notes.md`.

Grok's live check, so you do not repeat it: two clicks on the Actors
tab box left the `[>]` marker on a detail field and `sel` 0 (Harold).
Escape moved the marker to `17. 1. Harold`. A click inside that row's
published box moved the marker to the blank detail field. Down walked
detail fields and did not change the selected actor.

## Branch

Commit only your understanding file, on branch `kilo`, scoped. Do not
commit to `grok`, `claude`, or `main`. Do not push.
