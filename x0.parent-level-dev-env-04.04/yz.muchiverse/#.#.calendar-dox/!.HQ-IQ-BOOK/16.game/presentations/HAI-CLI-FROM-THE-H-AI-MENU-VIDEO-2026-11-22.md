# hai-cli: opened from the H-AI menu, typed in the window by relay injection (video proof, 2026-11-22)

The video is `hai-cli-typed-in-window-en-voice-ja-subs.mp4` in the owner's XO folder `31.Nov/nov-22/12.hai-cli-typed-in-window-video-en-voice-ja-subs/` (media stays out of the repo, per the presentations standard). 59.9 s, 1280x770, h264 + aac, English voice (Kokoro `af_heart`) with Japanese subtitles (rule 3b), the narrator face Actor1 left of the words (rule 3c), the whole window in frame.

Labels: **RAN** = I ran it and saw it. **NOT DONE** = a gap, said plainly.

## What the footage is (RAN)
| Slide | Photograph | What it shows |
|---|---|---|
| 1 | the top bar, right part | cell 15, `h-ai` |
| 2 | the H-AI dropdown | the new row `hai-cli` (row 12 of 13) |
| 3 | the hai-cli window | opened by clicking that row; empty ledger; a typing box at the bottom |
| 4 to 8 | the same window | typed IN the window by relay injection: `hi` (r_greet 83%), `/grade 4`, `help` (75%), `weather` (r_fallback 0%), `/grade 4`, `hi` again (**90%**, it rose from 83% after the grade) |
| 9 | the same window | the limits, said in the voice |

Stills are real window photographs (`dump_frame_png_op`), padded to 1280x640 with ffmpeg; the top-bar slide is a 2x crop of the bar. The typing is `relay_drive` steps (`tapid composer`, `type`, `key enter`) with an `expect` on the window's own `state/ui.txt` before each photograph.

## What was built to make it (RAN, details in the hai repo `book/21-hai-cli-in-the-h-ai-menu.md`)
- Menu row `hai-cli` (`ai_menu_12`, command `livedesk:open-hai-cli`), launcher registry row `launcher_hai_cli`, launcher `&.hq-apps/hai-cli/button.sh` (package path in `hai_home.pdl`), a 12-line branch in `khtpm_taskbar_manager.c` (manager rebuilt under `cpu_gate`, desktop reset).
- W1 in the hai repo: `<cli_io id="composer">`, `window/hai_send.c`, manager shows confidence and grades.
- Tests: hai `cases/hai_send.pdl` (13/13, mutant red 5), `window/tests/hai-window-type.pdl` (relay injection, 16/16 fresh, a re-run fails line 12 as it should).

## The ledger behind it (RAN)
`TURN hi r_greet 83`, `GRADE 1 r_greet 4`, `TURN help r_help 75`, `TURN weather r_fallback 0`, `GRADE 3 r_fallback 4`, `TURN hi r_greet 90`. `hai_chat calib` printed `BIN lo=0 hi=9 n=1 good=1 rate=1.0000 mean_conf=0.0000`, `BIN lo=80 hi=89 n=1 good=1 rate=1.0000 mean_conf=0.8300`, `ECE=0.5850 graded=2`. Hand check: 0.5 x 1.0 + 0.5 x 0.17 = 0.585. Two grades prove nothing about calibration; the slide says so.

## Changes to the presentation tool (RAN)
`make_presentation_video.py`: a manifest caption may be `SUBTITLE ## SPOKEN` (the strip shows the first, the voice speaks the second: this is rule 3b, which the script could not do before), and `--narrator <faces sheet png>` puts the Actor1 face left of the words (rule 3c). Both were used here. Without the new parts the script behaves as before.

## NOT DONE / flaws, said plainly
- The face covers the "5/9" slide counter at the top left of the caption strip (cosmetic, not fixed).
- Not a continuous screen recording: nine photographs with narration, each taken after the window's projection confirmed the step.
- The reply TEXT is not shown in the window, only the reply id and confidence.
- The menu was driven by pixel click on the row. Typed digits in a dropdown are ROW INDEXES, not the badge numbers; one wrong digit entry launched tomom by mistake and that window is still open (an attempt to close it by pid was denied). Details: `02-architecture/LYFEDESK-ENTITIES-INPUT-MENUS-BOTTOM-BAR-REFERENCE-2026-11-16.md` section 6.
- The Japanese lines are mine, not reviewed by a native speaker; no Japanese voice was tried.
