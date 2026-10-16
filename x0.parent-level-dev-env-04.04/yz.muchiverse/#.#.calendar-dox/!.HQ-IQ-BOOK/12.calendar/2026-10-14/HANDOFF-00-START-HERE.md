# 26.Oct15-lookahead: start here (written 2026-10-14 by claude, for an agent who has never seen this house)

You are inheriting a working repository, a lot of freshly built tools, and one new idea (the `auto` system). Read these four files in order; each is short enough to read whole.

| File | What it is |
|---|---|
| `00-START-HERE.md` (this) | rules, orientation, how to verify your footing in 10 minutes |
| `01-STOCK-TAKE-2026-10-14.md` | every change of the last two days, explained: what it is, how it was proven, what is NOT proven, where it lives, how to run it |
| `02-AUTO-SYSTEM-DESIGN.md` | the new design: per-user `auto` (on/off) and `auto-studio` (whole house account), driven by relay injection and tiered brains; how it is built, in slices with proofs |
| `03-AUTO-DRIVES-TODAYS-TOYS-AND-AGENT-LEVERAGE.md` | how `auto` operates the toys made this week, and the out-of-the-box ways it multiplies agents like you |

## Where things are
- Repo: `/home/no/Desktop/github/work/NNEST-12.00`, branch **`claude`** (your branch has its own name; never commit to another tool's branch, `main`, or the old all-agents branch).
- HOUSE = `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00` (the code). BOOK = `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK` (the docs; audio docs in `19.audio/`, handoffs in `12.calendar/`, agent learnings in `13.agent-coms/LEARNINGS.md`).
- XO = `/home/no/Desktop/github/work/XO` (not git; the owner's copy of docs and images: `25.games-plan/`, `25.games-plan/19-audio/` with demo wavs, `25.games-plan/img/` screenshots).
- Read first, in the repo: `AGENTS.md` (house rules), then the BOOK `03-pitfalls/OPERATIONAL-LANDMINES.md`.

## Rules that have already cost someone work (all from AGENTS.md or the owner)
1. **Commit only your own files, with an explicit pathspec**: `git commit -m ... -- <paths>`; run `git diff --cached --name-only` first. Never `git add -A`, never `git stash`, never `git reset` with anything staged you did not stage. Other agents (grok, kilo, opencode) work **in this same checkout**; their files appear uncommitted in your `git status`. Do not sweep them in. A `git commit -- path` commits the whole working-tree file, so if a shared file has someone else's uncommitted hunks, put your text in a new file.
2. **`xyzfs/users/` is live user data** (wallets, histories, api key files). Never `git add` it, never branch-switch/reset/clean from a branch that tracks it, never push `user/*` branches. This matters directly for `auto` (section 02).
3. **Never report a number or "done" you did not observe.** Fresh build + fresh run + evidence (diff, state file, PNG). Negative-test every assertion once (make it fail on purpose). `grep -c` on a missing file prints nothing, which reads as zero: confirm the file exists.
4. **Do not `pkill -f` / `pgrep -f` from a tool shell** (it matches and kills your own shell). Use pid files. Do not move or restart the owner's open windows without asking (a restart was asked for and done on 2026-10-14; that is not standing permission).
5. **Compute policy**: this machine is weak (load average was about 20 during testing). Heavy or GPU work goes over ssh to `debil@10.0.0.16` or the Mac `lfs.master@10.0.0.144`; wrap local multi-minute work in `nice -n 15 ionice -c3`. All Ollama/Gemma calls go to the **Mac** (URL in `#.desktop/ai_backend.pdl`), never local. **Free models only** (no paid credits).
6. **House code style**: window logic is C or pal (RISC-V `.pal` driven by prisc), not Python (Python only for one-off converters and off-line stand-ins). Change detection = marker files (append-only size growth), not `st_mtime`. Share code by inline / text-include (pure, 2+ users) / op + fork + IPC (stateful), never header + link. Harnesses are pal + case files, not new `.sh`.
7. **Honest audio claims**: nothing audio-related has been *heard* by the owner yet; every result is measured (numbers, spectra), not listened to.
8. Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` (use your own model name). Push and merge-to-main only when the owner asks (`git push origin claude`; fast-forward main only on "ff").

## Ten-minute footing check
```
cd /home/no/Desktop/github/work/NNEST-12.00 && git status --short | head      # expect other agents' dirty files; leave them
git log --oneline -8                                                          # newest: f7f8521ef merge of origin/grok
git ls-files --deleted | grep -v /pieces/sessions/ | wc -l                    # must be a few dozen at most; thousands = working tree wiped, STOP and read INCIDENT-2026-10-05
ps -eo pid,args | grep -c '[k]htpm_core_render'                               # how many windows are up
```
Compiled programs are in no branch: after a clone or branch switch run `sh '$.crypts/button.sh' build`; `sh '$.crypts/button.sh' reset` (hard kill + rebuild + relaunch) was run on 2026-10-14 after the grok merge and closed every window.

## State of the desktop at hand-off
Taskbar, world-manager, hotbar and the **voice-fx** window were running after the 2026-10-14 reset. img-hai and sfx-hai are closed (`sh '&.hq-apps/<app>/button.sh' run` / `stop`). `claude` was pushed through `f7f8521ef`; this folder's docs are committed after that.
