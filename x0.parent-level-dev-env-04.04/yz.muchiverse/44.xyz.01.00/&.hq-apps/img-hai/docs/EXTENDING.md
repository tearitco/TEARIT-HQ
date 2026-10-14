# Extending and modifying img-hai

Recipes, each with the exact files to touch, the commands to run, and how to prove the change. Do them in the order written; every recipe ends with a test, because a change nobody drove through the real window is not done (house rule). Paths are from the house root `44.xyz.01.00/`.

General rules for any change: commit only your own paths with an explicit pathspec; keep `state/` and binaries out of git; do not add logging (owner's rule); no new `.sh` for window logic (C or pal); if the renderer behaves oddly, read `CODE.md` section 7 before touching the shared renderer (never add per-app C there).

## A. Add a button (a new verb)
1. **Manager:** in `img_hai_manager.c` `handle()` add `else if (!strcmp(cmd, "MYVERB")) { ...; }` (verbs with an argument use `strncmp(cmd, "MYVERB:", 7)`); set `g_msg` to say what happened. `dirty = 1` at the end of `handle()` already refreshes the window.
2. **Action string:** in `write_ui()` add the verb to the two arrays next to `jo_yes ... batchsel` (`jk[]` = the ui key stem, `jvb[]` = the request text) and raise the loop count. The key becomes `<stem>_action`.
3. **Template:** in `img-hai.xhtpm` add `<item id="mybutton" label="..." action="${<stem>_action}"/>` in the right place: sidebar rows for general actions; the canvas overlay rows (`tb-place` column, `tb-cursor` strip) for canvas actions (give it `class="ov-btn"` there so it has a size). Keep ids short, unique and stable: tests refer to them.
4. **Docs:** add it to `USER-GUIDE.md` and the verb list in the manager's header comment.
5. **Prove it:** `bash build.sh`, relaunch (`sh button.sh run`), then drive it: `relay_drive.+x <pid> press mybutton` and read the effect in `state/ui.txt`. Add a case to `tests/` (recipe H).

## B. Add a host (another machine that draws)
1. **Access:** ssh by key must work non-interactively: `ssh -o BatchMode=yes <user>@<ip> true`. Never put a password in any file; the owner's login note stays outside the repo. Prefer machines with a GPU; none of ours has one (`OPERATIONS.md` section 1).
2. **Remote environment** (python venv with torch + diffusers, the draft script): follow `OPERATIONS.md` section 2 exactly. Use the version pins listed there for the platform (an old torch needs an old diffusers).
3. **models.pdl:** add one `HOST | name | user@ip | imggen | venv/bin/python | <prefix>` row (prefix `nice -n 15 ionice -c3` on Linux, `nice -n 15` on macOS) and at least one `MODEL | <model id> | <name> | <licence> | <steps> | <size> | <note>` row for it. The first MODEL listed for a host is what "Draft on ALL hosts" and `any` use there.
4. **Prove it:** copy the script (`scp '&.widgits/concept-bank/data/img/draft/img_draft.py' user@ip:imggen/`), relaunch the window, select the model, press Draft; expect `done:` and a new ledger line with the real seconds. Record the seconds in the model's note.
Limit: 4 hosts compile-time (`H[4]`, `job_pid[4]`).

## C. Add or change a model
1. Read the model card **licence** and record it in the MODEL row (free models only; owner rule). Check the weights size against the host's disk (`du -sh ~/.cache/huggingface`).
2. Only Stable-Diffusion-1.x-style pipelines work with `img_draft.py` as written (`StableDiffusionPipeline`). A different pipeline (SDXL, a Turbo model) needs a branch in `img_draft.py` keyed on the model id, then a copy of the script to every host.
3. Add a `MODEL` row; run Draft; compare against the ledger's existing seconds and a contact sheet (`ops/+x/img_sheet.+x`).
Do not run an unfamiliar model on the dev machine (owner's offload rule).

## D. Tune the word bank (the main creative lever)
The bank is `data/img/words.pdl` (words) and `data/img/parts.pdl` (which words describe which part). It changes only through **candidate edits**: a model or person *proposes*, the validator *checks*, a different human *promotes*. Never edit the two files by hand and never let a model write them.
1. **Write a candidate file** `&.widgits/concept-bank/data/img/candidates/<name>.cand`, one `EDIT` per line (format in `CODE.md` section 5 and the header of `ops/img_bank.c`). Example (a real one is `tune-1.cand`):
   `EDIT | id=t1-03 | type=img_word_text | target=w_car_door | text="a single red metal door panel, nothing else" | reason="car door panel was read as a car" | proposer=claude | status=candidate`
   Types: `img_word_weight_delta` (target word, `delta` within +-0.5, result must stay 0..1), `img_word_text` (new words, 1-95 chars, no `|`), `img_part_words` (target part, `words=` id list, all must exist, at least one `subject`), `img_word_add` (new id, `kind`, `text`, `weight`).
2. **Validate:** `&.widgits/concept-bank/ops/+x/img_edit.+x validate data/img/words.pdl data/img/parts.pdl data/img/candidates/<name>.cand` (exit 0 = valid). Edits apply in file order on an in-memory copy, so a later edit can use a word added earlier.
3. **Look at the prompt without changing the bank:** `img_prompt.+x data/img/words.pdl data/img/parts.pdl car_door 0.30 data/img/candidates/<name>.cand`. (Proved equal to the promoted result for every part: see `HANDOFF.md`.)
4. **Trial it:** make `batches/<name>.pdl` with the overlay name in the last column (recipe E), run the batch, judge the pictures (`USER-GUIDE.md` section 6). Compare against the same seeds of the untuned batch.
5. **Promote (a human, not the proposer):** `img_edit.+x promote data/img/words.pdl data/img/parts.pdl data/img/candidates/<name>.cand <last edit id to apply> <your name> data/img/promotions.pdl`. It applies every edit up to that id, rewrites the two bank files (comments kept), appends `PROMOTED` rows. It refuses when your name equals a proposer and refuses edits that no longer validate. Then rebuild nothing: the next prompt uses the new bank. Commit the two changed bank files and the ledger.
6. **Add a bad case** to `data/img/candidates/bad/` for any new rule you add to `img_bank.c`, and watch it fail before the rule exists.
Why not edit directly: weights and wording feed every later draw and (later) a learner; an unreviewed model-written change is the failure mode the house design rails forbid (`NOTE-FROM-CLAUDE-to-opencode-and-grok-pipeline-direction.md`).

## E. Add a batch
Create `&.hq-apps/img-hai/batches/<name>.pdl` (it appears in the window's batch cycle after `scan`/relaunch; it is alphabetical):
```
BATCH | label | part or free | words ("-" = from the bank) | seeds | model id or any | overlay
BATCH | wheel | car_wheel | - | 3 | any | -
BATCH | sign  | free | red stop sign, flat game sprite, white background | 2 | any | -
```
`free` rows are stored as part `free_<label>` so two free rows never overwrite each other. Seeds are 1..n. `any` = each host's first model; a named model only runs on a host that has it. The default and `tune-1` batches are examples. Prove it with a relay case that presses `batchsel`/`batchrun` and expects the `queue_line` (see `tests/img-hai-tune.pdl`).

## F. Show another image format
The window shows what `stb_image` decodes: png, jpg, bmp, gif (first frame), tga, psd, hdr, pnm. To add one (webp, avif, svg): convert it to png **before** it reaches `tiles/` (e.g. `python3 -c "from PIL import Image; Image.open('a.webp').save('a.png')"` on a remote host or any machine with Pillow), or add a decoder to `make_thumb()`/`draw_place()` (both call `stbi_load`; keep the two in step) and add its extension to `has_img_ext()`. Remote drafts always return PNG, so only hand-dropped files are affected.

## G. Change sizes
- Canvas pixels: `CW`/`CH` in the manager **and** the canvas width/height in `img-hai.css` **and** the window size (`window { width; height }`). The renderer centres a smaller canvas and crops a larger one.
- Cell sizes: the `CELL:` verb accepts 16..256; add buttons as in recipe A.
- Thumbnail size: `THUMB` (72). The renderer draws a `.png` sprite unscaled into its box (sprite-flow tile 168x96 minus padding), so keep it at or below the box, and use whole numbers for pixel art.
- Tile/placement/job limits: the `#define MAXT/MAXP` constants and array sizes at the top of the manager.

## H. Add a test (a relay case)
Cases are `.pdl` files driven by `relay_drive` (`TESTING.md`). Add `tests/img-hai-<name>.pdl` with `STEP | press | <id>`, `STEP | expect | <file> | contains | <text> | <timeout s>`, `STEP | shot | <png>`. Always keep one case that must FAIL (`img-hai-negative.pdl`) so you know the driver can fail. State which window state a case needs (fresh or not). If you need a new driver verb, add it to `&.widgits/_shared-lib/ops/relay_drive.c` (it is shared by every khtpm window), rebuild it, and document it in `02-architecture/RELAY-DRIVE.md`.

## I. Change the layout or look
Edit `img-hai.xhtpm` / `img-hai.css` only; do not add a layout branch to the shared renderer (`khtpm-house-standards`: reuse an existing shape, clip never translate, number only visible rows). If a control vanishes, check `CODE.md` section 7 first: most vanishing acts were the renderer not laying out a tag in that container. After a layout change **re-take a screenshot** (`relay_drive.+x shot <winid> out.png`) and re-run the relay cases (they use ids, so they should still pass).

## J. Connect accepted pictures to the game (not built yet)
Accept writes `data/img/accept_candidates.pdl`. The intended next steps, none implemented: (1) a human promotes a CAND into the brixel concept row (`shared/brixels.pdl` `CONCEPT | brixel | image | pixelmap | does`), (2) the pixel-map step (which pixels belong to which sub-part) is done by hand or a worker, (3) the detail-level op (roadmap G2) picks the 32 px or the large picture by distance. Start from `BOOK 16.game/BRIXELS-DESIGN.md` and the roadmap.

## K. Replace the python stand-in with an in-house model (the owner's aim)
`img_draft.py` is a stand-in. The plan (`WORD-BANK-TO-IMAGE.md` W2) is a small in-house renderer trained on accepted pictures with their bank rows, run as a C op, driven by events/pal. The seam is clean: anything that takes `PROMPT:`/`NEGATIVE:` text and writes `<id>.png` + `<id>_32px.png` into `jobs/out` can replace `img_draft.py` without touching the window. Do not start W2 before there are accepted pictures to learn from.

## L. The pixel-art reduction op (`ops/img_pixel`)
`img_pixel <in> <out.png> [size=32] [colors=16] [mode=mode|avg]` turns a large picture into small pixel art: median-cut palette for the whole picture, every pixel mapped to its nearest palette colour, then each size x size block becomes ONE palette colour (`mode` = most frequent colour in the block, crisp; `avg` = block mean snapped to the palette, smooth). Deterministic, no network, runs locally in well under a second. It is **not yet used by the draft job**: `img_draft.py` still makes the old naive `_32px` copy (one sample per block, then PIL 16-colour quantize).
Observed 2026-10-13 on the four tuned yes-pictures plus the wheel (sheet `XO/25.games-plan/img/sheet_pixel_modes.png`, columns original / old / mode / avg): both modes remove the old version's stray noisy blocks; `avg` is cleanest for flat things (door, glass); `mode` keeps thin dark outlines best (wheel rings, person); `avg` blurs outlines and washes out the person; `mode` leaves a few dark specks on the door. **No single winner**: pick per part, or add a `mode` column to `parts.pdl`. To try it: run it on any tile and drop the result into `tiles/`. To change a rule, edit the three numbered steps in the file header and re-run the comparison.
Not verified: an in-window button, 16 vs 8 colours, palette shared across a part's pictures (needed so a set of parts matches), transparent-background pictures (code handles alpha, never run on a real one).
