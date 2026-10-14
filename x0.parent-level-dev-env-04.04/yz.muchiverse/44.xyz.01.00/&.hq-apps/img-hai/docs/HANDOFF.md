# img-hai handoff (read this if you are taking the project over)

Written 2026-10-13 by claude (Sonnet 5.5) at the end of the first build session. It says what exists, what is proven, what is not, why things are the way they are, what to do next, and the traps. Where it says **proven** there is a run behind it that is named in `TESTING.md` or in the commit message; where it says **not verified** nobody has seen it work.

## 1. What this is and who asked for it
The owner (the house's human) asked, in these words: "a new h-ai for a text 2 image x11-hq gui interface for generating images / brixels", then "do it like palettes / canvas craft, where we can even mix tile live", then "we also want a place for word input, cli-io, text to image etc. gen image studio suite", then "test by generating things you need ... drive x11-hq through the relay as a human user as possible", then "whatever helps get more work done and delegates" and "think about legacy / handoff ... extensive and exhaustive dox".
It is the face of **W1** of `16.game/WORD-BANK-TO-IMAGE.md` (the off-line image pipeline for gta-hq's brixel parts) and roadmap item **G3a**. The long aim (owner): an in-house image model written as events/pal/ops, trained by hand with word banks and concept banks, with outside models only as drafters, claude and the owner as the two judges, nothing generated at game runtime.

## 2. Status table (2026-10-13)
| Piece | State | Evidence |
|---|---|---|
| window opens, tile palette with thumbnails | **proven** | screenshots; 44 tiles listed |
| place / move / rotate / flip / hide / delete / reorder tiles live on the canvas | **proven** (relay) | `img-hai-smoke`, `img-hai-mix` |
| words field (real typing) -> prompt | **proven** (relay, real cli_io) | `img-hai-human` first run |
| Draft on one host, status in the window, result in palette + ledger | **proven**, several real drafts (112-183 s each) | job.txt, ledger |
| Draft on ALL hosts in parallel (debil + Mac) | **proven** | `img-hai-both`: debil 119.4 s, Mac 118.5 s |
| unattended batch queue (both hosts pull jobs) | **proven** | default batch: 12 jobs, 12 done, 0 failed, ~14 min, no input |
| two-judge verdict buttons, Accept gated on both | **proven on a test tile** | `img-hai-judge` 14/14; the real `accept_candidates.pdl` does not exist (nothing accepted) |
| candidate-edit validation of bank changes, promotion with promoter != proposer | **proven** (command line, scratch copy) | `TESTING.md` section 4 |
| overlay trial (batch with a candidate tuning, nothing promoted) | **proven** (`tune-1`, 11 drafts) | section 7; the overlay prompt equals the promoted-bank prompt for all four parts |
| a picture is never overwritten (`_r2`, `_r3`) | **proven** | `retest` batch: the reruns arrived as `car_wheel_bk-sdm-tiny_s1_r2.png` / `_s2_r2.png` |
| clearing free words (submit an empty field or a space) | **proven** (relay) | `img-hai-words` 8/8 |
| Export mix -> new tile | **written, not driven** | - |
| cell-size, cursor, scan, Placed-list select, front/back, clear buttons | **written; some seen via the request op, not by the relay test** | - |
| window chrome (close / fullscreen / minimise), HQ or Toys menu entry | **not tested / not built** | - |
| owner's own use and verdicts | **none yet** (every owner verdict is blank) | `verdicts.txt` |

## 3. History (commits on branch `claude`; check `git log -- '&.hq-apps/img-hai'`)
`750bdd51c` first build (palette, canvas, words, Draft over ssh to debil) -> `ad22b6d1e` relay_drive driver + first tests -> `88bf1f792` verdict buttons, Accept, Mac host, Draft on ALL hosts -> `c71573a66` batch queue + contact sheet op -> the commit that holds this documentation: candidate-edit bank tooling (`img_bank.c`, `img_edit.c`), overlay in batches, batch selection, `relay_drive press/tapid` by element id, ids-based tests, docs/. Earlier the same day: the z-cell format (`shared/zcell`), W0 word bank and `img_prompt`, the first drafts and the W1 survey.

## 4. Decisions and why (each from the owner unless marked mine)
1. **Draw on other machines** (owner): the dev box is weak and the owner works on it live. Hence ssh jobs, `nice`, one job per host, nothing local.
2. **A window, not a script** (owner): the owner wants to see, mix and judge without a terminal; "delegate": batches run unattended.
3. **Tiles + live mixing like palettes** (owner): the canvas composes tile records so a mix can later feed brixel assemblies.
4. **Two judges, both must say yes** (owner: "both"): claude judges, the owner judges. Accept never writes the bank (mine, from the house rail "a model never approves its own output / no auto-promotion").
5. **Bank changes via candidate edits** (mine, following `concept-bank`'s existing EDIT/validate/promote pattern): a model proposes, a validator checks, a different human promotes; an **overlay** lets a trial batch use a proposal without promoting it.
6. **Python stand-in now, in-house model later** (owner): `img_draft.py` is a stand-in; the seam to replace it is clean (`EXTENDING.md` K).
7. **C ops and case-file tests, not shell** (house rule "window logic is C or pal"): `button.sh` is a launcher only (as every sibling is). The tests are `.pdl` data driven by a C op; a pal wrapper does not exist yet.
8. **Press by id, not number** (mine, after the numbers broke three tests): the frame dump lists ids.
9. **No added logging** (owner): state files and the ledger are the only record.
10. **Free models only** (owner): both models used are OpenRAIL-M (licence text to be read by the owner, see risks).

## 5. Rules the next person must keep (owner rules in force)
Commit only your own paths with an explicit pathspec after `git diff --cached --name-only`; never `git add -A`, no stash, no resets; never touch or stage `xyzfs/users`; never push `user/*`; no symlinks; no `pkill -f`/`pgrep -f` from a tool shell (use exact pids: `button.sh` does); do not restart or move the owner's live windows without asking; never report a number or "done" without a fresh build and observed evidence; use the marker (size growth), not mtime; heavy work under `nice -n 15 ionice -c3` and **on the remote hosts**; Ollama/Gemma LAN calls go to the Mac 10.0.0.144; merge to main and push only when the owner asks (this branch has been pushed on the owner's request in this session); commit trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; never print the process environment.

## 6. Image quality: what the models can and cannot do (the honest part)
Two small Stable-Diffusion-1.x derivatives (`nota-ai/bk-sdm-tiny`, `segmind/tiny-sd`), 512 px, 20 steps, CPU. Over ~25 drafts:
- **Worked:** a clear stylised **wheel** from the bank words (`car_wheel_bk-sdm-tiny_s2`; its 32 px 16-colour version still reads as a wheel). A clean red **cartoon car** (side/3-quarter) from free words. A red sports car (tiny-sd).
- **Did not work:** door panel, windshield, torso and road all came out as whole vehicles, photographs or grey stripe textures; nothing looks like deliberate pixel art except by accident. Same prompts gave different results on the two hosts only because seeds differed (not a like-for-like comparison).
- **Verdicts:** claude has said no to all but the wheel. **The owner has not judged any picture.**
- **Tuning experiment (`tune-1`):** nine candidate edits were run as an overlay batch of 11 drafts, nothing promoted. It lifted claude's yes count from 1 of 12 to 4 of 11; details in section 7.
Levers left, in order of cost: more word tuning; a larger model (minutes more per image); doing the 32 px reduction with our own op after a clean larger image; img2img from a hand-drawn base; training our own renderer on accepted pictures (W2). None is started.

## 7. The tune-1 experiment result
**It worked, clearly.** `tune-1` (nine candidate edits, run as an overlay trial, **nothing promoted**) was run as 11 drafts on both hosts (11 done, 0 failed, ~13 min) and compared with the untuned batch by eye (sheets `XO/25.games-plan/img/sheet_batch_default.png` and `sheet_tune1.png`):
| Part | Before (untuned bank) | After (tune-1) |
|---|---|---|
| car_door | whole cars | **s3: a flat red panelled door** (yes); s2 a double door (no); s1 texture (no) |
| car_windshield | pixel-style vehicles | **s1: a flat pale-blue glass pane in a frame** (yes); s2/s3 no |
| person_torso | photographs | **pixel-art people with outlines**; s2 the best (yes); s1 half-realistic, s3 lime-green (no) |
| car_wheel | s2 a clear wheel (yes) | s2 a clear wheel (yes, same quality); s1 broken (no) |
Claude's yes count went from **1 of 12** (untuned) to **4 of 11** (tuned). The change was only wording ("a single ... nothing else", negative words for whole cars and photographs, "crisp outline" 0.7 -> 0.9). It is still a trial: the owner has not judged any picture, and promotion (`EXTENDING.md` D step 5, by a human who is not the proposer) has not been done, so the real bank still holds the old words. If promoted, the default batch will use the new prompts.
**Same seed, same picture:** a rerun of the untuned wheel (seeds 1 and 2, possibly on different hosts than the originals) matched the originals within 0.6-0.9 of 255 mean pixel difference (the originals were only available as 256 px crops). So the ledger line (prompt, model, seed, steps, size) is a usable recipe.

## 8. Open items, most useful first
1. **Owner review**: open the window, press `o yes`/`o no` on the pictures (the wheel `car_wheel_bk-sdm-tiny_s2` is the first real Accept candidate: claude=yes), try the batch buttons. Everything above is claude's reading until then.
2. **Promote or drop `tune-1`** (a human other than claude: `EXTENDING.md` D step 5) depending on section 7.
3. **Tile list limit**: 256 tiles, two per draft, so ~120 drafts fill it. Needs paging/scrolling by day or a "hide 32 px copies" filter (change `scan_dir`/`MAXT`).
4. **Menu entry**: the HQ / Toys menu does not list img-hai. Toys read `@.apps/*/toy.pdl`; HQ apps are listed from `hq_ui.pdl`. Find how `co-lab-hai`/`chat-hai` are listed and mirror it (not investigated).
5. **Queue persistence**: the batch queue is in memory; closing the window drops it. A queue file (append-only, marker) would survive.
6. **Free-words clearing** and several buttons are untested (section 2).
7. **pal wrapper** for the relay cases (house wants harnesses as pal + case pdl); currently a C op runs the case file directly.
8. **Pixel-art step**: today's 32 px copy is a naive nearest-neighbour + 16-colour reduction of the big picture. A real pixel-art op (palette choice, outline cleanup, dithering off) is the next quality lever and belongs in C.
9. **Brixel link**: Accept -> concept row -> detail-level op is not built (`EXTENDING.md` J).
10. **Second opinion on the bank file shapes** from grok (the owner chose "without grok first", so none was asked).

## 9. Risks and traps
- **Pictures are not in git.** Losing `data/img/draft/*/img` loses the art (the ledger keeps the recipe; a rerun of two seeds matched within 1/255, but hosts and library versions are not pinned across time). Decide with the owner what to commit (`OPERATIONS.md` section 4).
- **Model licences:** both are CreativeML OpenRAIL-M (from the Hugging Face model cards). That licence carries use restrictions; the owner should read it before any art is shipped in a game. `stabilityai/sd-turbo` showed no licence in its card data and was not used.
- **Remote copies drift:** `img_draft.py` exists in three places (repo, debil, Mac). Re-copy after every edit. A version mismatch already cost one draft.
- **Version pins on the Mac** (torch 2.2.2 / diffusers 0.27.2) are fragile: a plain `pip install -U diffusers` there breaks it.
- **ssh keys:** drafts need non-interactive ssh to both hosts; if a key or an ip changes, jobs fail with `cannot reach host`.
- **Shared frame-dump path** `/tmp/entity-menu-frame.png*`: two tests at once corrupt each other.
- **Fragile layout:** the window works inside renderer limits (`CODE.md` section 7). A new row type can silently vanish; always take a screenshot after a layout change.
- **Dense code:** the manager is written in the house's one-line-per-statement-group style; read the header map first. Do not bulk-reformat (house comments policy: keep comments fresh, never bulk-strip).
- **Real owner windows:** launching this window puts a 1060x900 window on the owner's desktop. `button.sh stop` closes it; do not leave it open unasked.
- **The overwrite incident (fixed, but read it):** the first `tune-1` run reused the file names of earlier drafts (same part, model, seed) and **overwrote nine pictures**, including the wheel that had been judged yes. Recovered: three from the session scratch folder (512 px), six only from a contact sheet at 256 px (their 512 px originals are lost). Fixes: the job op never overwrites (adds `_r2`, `_r3`), and an overlay name is part of the picture name (`car_door_tune-1_bk-sdm-tiny_s3.png`). Lesson: any new naming scheme must be unique per run, and a contact sheet made right after a batch is cheap insurance. The incident is recorded in `data/img/draft/2026-10-13/verdicts.txt`.
- **Test data pollution:** forgetting to delete `verdicts.txt` / `accept_test.pdl` in the app folder after `img-hai-judge` makes the next fresh run start with verdicts set.

## 10. How to resume in ten minutes
1. `git log --oneline -- '&.hq-apps/img-hai' '&.widgits/concept-bank/data/img'` to see the latest work; read this file and `README.md`.
2. Build everything: `bash '&.hq-apps/img-hai/build.sh'` and `bash '&.widgits/concept-bank/ops/build_img_ops.sh'`; build the driver (`TESTING.md` section 1).
3. Check the hosts: `ssh -o BatchMode=yes debil@10.0.0.16 true` and the Mac; if a host's env is gone, `OPERATIONS.md` section 2.
4. Open the window (`button.sh run`), run `img-hai-smoke.pdl` (15 s) and `img-hai-judge.pdl` (40 s). If both pass, the app and the driver work.
5. Draft one picture (`img-hai-human.pdl`, ~3 min) to prove the remote path, then look at the ledger.
6. Pick the next item from section 8. Update this file and the status table in the same commit.

## 11. Where the owner and the others are
The owner reads `/home/no/Desktop/github/work/XO/25.games-plan/` (a non-git folder of copies: `9-IMG-HAI-DESIGN.md`, this documentation as `10-IMG-HAI-DOCS.html`, contact sheets under `img/`). Grok builds df-hq and the CDDA rules and has no part in img-hai; the colab room is session `1791851915` (post with `&.hq-apps/co-lab-hai/ops/colab_hai_post.sh`, needs the owner's Approve click). Related: `16.game/GAMES-ROADMAP.md`, `GTA-HQ-DESIGN.md`, `WORD-BANK-TO-IMAGE.md`, `IMG-HAI-DESIGN.md`, `02-architecture/RELAY-DRIVE.md`.

## 12. Addendum: pixel op (2026-10-13, after the first handoff)
`ops/img_pixel` added (EXTENDING.md recipe L): compared by eye on 5 pictures; no mode wins everywhere, so `pixel_modes.pdl` picks per part. **Wired into `img_hai_job`** (replaces the naive `_32px` copy); verified on 2 real remote drafts. Older pictures keep their naive copies. Not done: one shared palette across a part set, in-window re-reduce button, 8 colours.

## 13. Addendum (2026-10-14): what the window shows, and a gap
- The big canvas shows only what is **placed**. Drafts and 32 px copies land in the tile list; click a tile (two-step) to place it. At the default cell size 128 a 32 px picture is shown 4x.
- **The canvas layout is not saved.** Closing or relaunching the window empties it (placements are in memory). Export writes a canvas as a tile; there is no save/restore of placements yet. Open item: persist placements as an append-only file.
- Real generation exists: the wheel seen on 2026-10-13 (`car_wheel_bk-sdm-tiny_s2_r3`) was drafted by `nota-ai/bk-sdm-tiny` on the Mac (20 steps, 512 px, 107.9 s, seed 2, bank prompt for car_wheel), then reduced to 32 px by `img_pixel`. Ledger line in `data/img/draft/2026-10-13/ledger.txt`.
