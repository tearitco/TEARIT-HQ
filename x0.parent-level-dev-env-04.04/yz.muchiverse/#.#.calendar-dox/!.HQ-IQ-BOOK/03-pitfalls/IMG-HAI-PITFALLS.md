# img-hai pitfalls (2026-10-13)

Full context: `16.game/IMG-HAI-BREAKTHROUGH.md` and the app handbook `44.xyz.01.00/&.hq-apps/img-hai/docs/`.
- **File-name collisions in batch output.** Any name built from `<part>_<model>_s<seed>` repeats across runs; a batch silently overwrote nine pictures. Never write an output without checking it does not exist; include the experiment name in the file name.
- **Validator passes bad input quietly.** Test each rule with a deliberately bad file and check `$?` directly (not inside `echo $(...)`).
- **Row numbers in relay tests.** Use ids from the frame dump (`relay_drive press <id>`).
- **Armed cli_io eats keys** (including `p` for the frame dump): Esc first.
- **A relaunch empties the img-hai canvas** (placements are in memory only).
- **Stale remote script.** After editing `img_draft.py`, copy it to every host.
