# auto (slices A0-A3, 2026-10-14)
The conductor for the `auto` / `auto-studio` system. Design: BOOK `17.ai/AUTO-AND-AUTO-STUDIO-DESIGN.md`. Dev log + proofs: BOOK `12.calendar/2026-10-14/DEVLOG-auto-A0-A3.md`. Class: `1-1.HARNECIENT.SMOL/NIGHT_50_*.txt`.
Build: `sh ops/build.sh` (ops land in `ops/+x/`, gitignored). Ops: `auto_ctl` (switches, STOP), `auto_act` (one verb into one window, checked), `auto_observe` (marker/hash change detection), `auto_plan` (list of verbs), `case_run` (generic proof runner).
Proof: with a fresh sfx-hai window up: `A=<ops/+x> U=<scratch user dir> M=<sfx-hai manifest> H=<house root> R=<relay file of that window> NEG=<tests dir> ops/+x/case_run.+x tests/auto_cases.pdl` (34 cases). Use a scratch user dir, never `xyzfs/users`.
A window opts in with `auto.manifest.pdl` next to its `button.sh` (example: `&.hq-apps/sfx-hai/auto.manifest.pdl`). Not built: brains (A4), studio scheduler (A5), sub training (A6), auto-hq window (A7).
