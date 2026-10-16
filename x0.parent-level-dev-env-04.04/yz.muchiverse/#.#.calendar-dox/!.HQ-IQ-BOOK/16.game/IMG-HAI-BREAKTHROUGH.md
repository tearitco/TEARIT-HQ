# The img-hai breakthrough: a lesson (2026-10-13)

A class-style write-up. What happened in one day, why it matters, what is proven and what is not, the ten lessons, and exercises a newcomer can do. Companion to the handbook `44.xyz.01.00/&.hq-apps/img-hai/docs/` (README first) and the design `IMG-HAI-DESIGN.md`. Labels: **proven** = a run exists and is named; **not proven** = nobody has seen it work; **opinion** = claude's judgement.

## 1. The breakthrough in one paragraph
Before today the house could not make pictures: art for gta-hq parts was hand-drawn, borrowed, or absent. By the end of today the house has a **closed loop from words to a judged pixel-art picture, run through a window a human can use**: a word bank holds the words for a part (`car_wheel`), a prompt is built from it, a free image model on a spare machine (debil or the Mac, ssh, no GPU) draws it in about two minutes, our own C op reduces it to 32 px pixel art, two judges (claude and the owner) say yes or no, and the bank's wording can then be tuned through the same candidate/validate/promote path the rest of the house uses, with a trial that measurably improved the results (claude's yes rate 1/12 to 4/11) before anything was promoted. Nothing is generated at game runtime; the loop is off-line and every step leaves a text record.

## 2. Why it matters (opinion, with reasons)
1. **It unblocks gta-hq art.** Vehicles are grids of part cells; each part needs a picture at several detail levels. This is the first source of those pictures that is ours, repeatable and judged.
2. **It is the first working instance of the house's learning rails on real data**: a model proposes (drafts; candidate edits), real code validates, a different human promotes, nothing is auto-kept, every decision is a ledger/JUDGE row. The same rails planned for pets and FSMs now exist once in running form.
3. **It proves the offload rule**: heavy work ran on two other machines in parallel (119.4 s and 118.5 s for one press), unattended (12 jobs, 0 failed, ~14 min), while the dev machine stayed usable.
4. **It produced a reusable driver**: `relay_drive` lets any khtpm window be tested as a human would (keys, typing, two-step clicks, press by id). That helps every other window.
5. **It gives the in-house model a seed**: a ledger of prompt/seed/steps/seconds and judged pictures is exactly the training and grading data W2 (our own small renderer) needs.

## 3. What is proven and what is not
| Claim | Status | Evidence |
|---|---|---|
| Window with palette, live mixing canvas, words field | proven | relay cases smoke 9/9, mix 15/15, words 8/8; screenshots in `XO/25.games-plan/img/` |
| Draft on a remote machine from a click, result back in the palette | proven | ~25 real drafts; human case 15/15 with a real draft |
| Two machines drawing in parallel | proven | both-hosts case; 119.4 s and 118.5 s |
| Unattended batch queue | proven | 12/12 then 11/11 jobs, 0 failed |
| Two-judge gate, Accept needs both | proven on a throw-away tile | judge case 14/14 |
| Candidate-edit tuning with overlay trial, promotion refuses the proposer | proven (command line, scratch copy) | 9 bad candidates fail as designed; overlay prompt == promoted prompt |
| Tuning improved results | **proven by eye, one trial, claude judging only** | sheets `sheet_batch_default.png` vs `sheet_tune1.png` |
| Never overwrite a picture | proven | retest batch produced `_r2`, `_r3` |
| Own pixel-art reduction in the pipeline | proven on 2 real drafts; modes chosen by eye | `sheet_pixel_modes.png`; RGBA vs RGB copies |
| Quality good enough for the game | **not proven**; only a wheel is solid; door, glass, person usable in some seeds | contact sheets |
| The owner's verdict on any picture | **none yet** | `verdicts.txt` owner column blank |
| Same seed reproduces a picture | roughly (within ~1/255 on two reruns) | pixel difference measured |
| Canvas layout saved between runs | not built | placements vanish on relaunch |
| Brixel link, menu entry | not built | - |
Do not say "done" about quality; say "the loop works, the output is a first draft".

## 4. The day in order (what was done, what failed)
1. z-cell format v1.1 written and checked (`16.game/Z-CELL-FORMAT.md`), then the gta design with the owner's eight answers.
2. W0 word bank and `img_prompt`; survey of what the hosts can run (neither has a GPU; CPU float32, ~2 min/image).
3. First img-hai window; first remote draft; one draft **lost** because an old script copy sat on the remote host.
4. `relay_drive` built so the window could be tested like a person; first scripted scenario 18/19 (my wrong guess of a row number).
5. Verdict buttons, gated Accept, Mac as second host, Draft on ALL hosts.
6. Batch queue; default batch of 12: claude yes 1/12 (the wheel).
7. Candidate-edit tooling (validate, overlay, promote); a bad candidate with a `|` inside a value **slipped past** the first validator and was fixed.
8. `tune-1` trial: 4/11. **Incident:** the trial reused file names and overwrote nine pictures (three recovered, six only as 256 px crops). Fixed and tested.
9. Handbook (7 docs, ~10k words), relay regression, `img_pixel` op, per-part modes, default cell 128.

## 5. Ten lessons (each from something that actually went wrong or right today)
1. **A new naming scheme must be unique per run.** Overwriting nine pictures came from reusing `<part>_<model>_s<seed>`. Check "could this name already exist?" before any batch. Make a contact sheet right after a batch: it saved six pictures.
2. **Test the validator with bad input and read the exit code.** The pipe-in-value case passed until it was negative-tested. An `echo $(basename ...)` can hide a failing exit code.
3. **Numbers shift; ids do not.** Three tests broke on row numbers in one afternoon. Press by id from the frame dump.
4. **Offload, and say so.** Heavy compute went to debil/Mac; one local `find /` still broke the rule. Look at where a command runs before running it.
5. **Pin versions per platform.** The Mac needs torch 2.2.2 and diffusers 0.27.2; the newest diffusers fails with a `torch.xpu` error. Record the working pins when you find them.
6. **Keep copies of remote scripts in step.** A stale `img_draft.py` on one host lost a draft.
7. **Small models need tuned words, not more steps.** Wording changes ("a single … nothing else", negative words for whole cars and photographs) lifted the yes rate; more drafting did not.
8. **No single reduction mode wins.** Look at the picture, choose per part, keep the choice in a data file (`pixel_modes.pdl`) so it can change without a rebuild.
9. **Rails are cheap if built in from the start.** Proposer is not promoter, candidates are validated, overlays try before promoting: these cost little and made the experiment safe (nothing in the real bank changed).
10. **Say what you did not look at.** The words field cleared test, the relaunch emptying the canvas, the zoom: each time, an honest "I only read the state file" kept trust.

## 6. How to use it today (five minutes)
`sh '&.hq-apps/img-hai/button.sh' run`; click `Draft image (remote)` twice (two-step); wait ~2 min; press `scan` if needed; click the new tile twice to place it; click `c yes`/`o yes` to judge. Details: `USER-GUIDE.md`. Tune the words: `EXTENDING.md` D. Run unattended: `batches/`.

## 7. Exercises (for a student or a new agent)
1. Run `img-hai-smoke.pdl` and read every PASS line; then edit it so one expectation is wrong and confirm the run fails.
2. Write a candidate that lowers the weight of "pixel art" by 0.2 on one part; validate it; try to break the rule with a bad copy; run it as an overlay trial with 2 seeds; make a contact sheet; write your verdicts.
3. Add a batch row for a new free-words subject; predict whether the picture file will collide with an existing name; check.
4. Change the pixel mode of one part in `pixel_modes.pdl`, draft one picture, compare against the old mode by eye.
5. Read `ops/img_pixel.c`'s header; change the colour count to 8 and describe what you expect before running it.

## 8. What to do next (priority, opinion)
1. The owner judges pictures (`o yes`/`o no`); promote or drop `tune-1`.
2. Shared palette across a part set; canvas persistence; menu entry.
3. A second tuning round on doors and torsos with more seeds; a larger model only if wording stops helping.
4. Link accepted pictures to brixel concept rows (not built), then the detail-level op (G2).
5. Start W2 (own renderer) only when there are accepted pictures to learn from.

## 9. Where everything is
Handbook `44.xyz.01.00/&.hq-apps/img-hai/docs/`; design `IMG-HAI-DESIGN.md`; pipeline plan `WORD-BANK-TO-IMAGE.md`; driver `02-architecture/RELAY-DRIVE.md`; day log `12.calendar/2026-10-13/`; picture sheets `XO/25.games-plan/img/`; one-page docs `XO/25.games-plan/10-IMG-HAI-DOCS.html`.
