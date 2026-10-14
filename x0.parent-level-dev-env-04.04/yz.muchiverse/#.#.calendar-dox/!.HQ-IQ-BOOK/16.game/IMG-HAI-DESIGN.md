# img-hai: a text-to-image window for brixels (design, 2026-10-13)

Owner (SAID, 2026-10-13): "a new h-ai for a text 2 image x11-hq gui interface for generating images / brixels". Status: **first build running (2026-10-13): palette + mixing canvas + words input + Draft job; verdict buttons, Accept and the Mac host are not built.** Parent plan: `WORD-BANK-TO-IMAGE.md` (W0 prompt op and W1 drafts exist; this window is the face for them). Labels: READ = from files; MINE = proposal.

## 1. What it is
A new app `&.hq-apps/img-hai/`, built like its siblings `chat-hai` and `co-lab-hai` (READ: static `.xhtpm` template, a small C manager that writes `state/ui.txt`, `button.sh` that starts the shared `khtpm_core_render.+x`, `ops/`, a design doc). One window to take a brixel part from words to a judged image:
1. **Pick a part** (`car_door` ... from `data/img/parts.pdl`); the window shows its words and weights and the prompt `img_prompt` builds.
2. **Pick a host and model** (debil, later the Mac; model from a short list) and press **Draft**. The manager runs the python stand-in on that host over ssh and shows status (queued, running, seconds, done/failed). It never blocks the window.
3. **Look at the results** as a contact sheet of the part's drafts, with their ledger lines.
4. **Judge:** buttons yes / no for the owner and for claude (two separate rows, because both must say yes). Verdicts append to `verdicts.txt` in the draft folder.
5. **Accept:** only when both verdicts are yes does an Accept button enable. Accept writes a *candidate* concept row linking the image to the brixel part for the normal human promotion path; it does not edit the bank directly.
6. **Edit words:** changing a word weight or a part's word list produces a candidate EDIT line (checked, then a human promotes), not a direct write. A model never sets a weight.

## 2. Rules it must keep
- Big/GPU work runs on debil or the Mac over ssh (owner's offload rule), never on the dev machine; local work is only prompt-building, copying a PNG back, and drawing.
- One ledger line per draft and per verdict, append-only, size-growth markers rather than mtime for change detection (house rule). No extra logging beyond that.
- The ssh job runs from a forked child with a watchdog and no `popen`-to-EOF (the known freeze in the house's prisc custom-op path); the manager polls a marker.
- No credentials in tracked files. Free models only; model licence recorded in the ledger/model list.
- Nothing is generated while a game runs; this window is the only caller.

## 2b. Owner's shape (SAID, 2026-10-13, mid-build)
"i'd do it like palettes / canvas craft, where we can even mix tile live" and "we also want a place for word input, cli-io, text to image etc. gen image studio suite". So the window is a **studio**, not only a reviewer: a tile **palette** (every image under the draft folders and `tiles/`, any format the loader reads), a **canvas** where tiles are placed and mixed live (move, rotate, flip, order, hide, delete, export a mix back as a new tile), a **words** input line (cli_io) that feeds a **Draft** (text to image) job, and the part/model selectors.

## 2c. Built and observed (2026-10-13)
`&.hq-apps/img-hai/` (manager `img_hai_manager.c`, request op `ops/img_hai_req.c`, job op `ops/img_hai_job.c`, `models.pdl`, `img-hai.xhtpm`/`.css`, `button.sh run|stop`, `build.sh`). Screenshots were taken with `dump_frame_png_op` on the real window.
- **Tile palette:** 9 tiles listed with thumbnails after the second Draft (the 5 car_door drafts from earlier + new ones). Thumbnails are PNG made by the manager because the renderer's `sprite=` path draws PNG only; the manager reads png, jpg, bmp, gif (first frame), tga, psd, hdr and pnm. **Not supported:** webp, svg, avif (the loader does not read them). Red/blue are swapped in the thumbnails on purpose: the renderer blits a `.png` sprite as BGRA, which showed true red as blue until swapped (observed).
- **Live mixing:** three tiles placed on the canvas at cells (1,1), (2,1) and (3,2), one rotated 90 degrees and one flipped, with the placed list on the left (screenshot). The canvas is composited from placement records, so a tile can be moved or turned later; the export verb writes the mix to `tiles/mix_<n>.png` and rescans (export click not yet exercised by me).
- **Words + Draft:** the words line set the prompt ("red car door, flat top-down game sprite, white background, pixel art"); Draft started an ssh job on debil; the window showed `running: car_door_bk-sdm-tiny_s53883 on debil ... (about 2 min)` and then `done ... in 119 s`; a ledger line was appended and the image copied back; the palette went from 7 to 9 tiles. Per-draft cost 117 to 119 s on debil's CPU.
- **Part/model selectors:** buttons cycle the part (from `parts.pdl`) and the model (from `models.pdl`).
- **Bugs found and fixed while building:** the job status never reached the window (wrong path to `job.txt`); the remote script was an older copy that named files differently, so one finished image had to be fetched by hand and its ledger line added by hand (it is marked in the ledger by its seed).

## 2d. Tested through the input relay (2026-10-13)
Driver and findings: `02-architecture/RELAY-DRIVE.md`. Cases: `&.hq-apps/img-hai/tests/`.
- **`img-hai-human.pdl` (18 of 19 steps passed, first run):** clicked the words field, typed "flat top-down pixel art sprite of a red sports car, white background", Enter (the real cli_io), jumped to Draft by number and pressed Enter, waited for the remote job (120 s on debil), saw the ledger line, placed the first tile, then the rotate step FAILED because I had guessed the wrong number (29 is "move left"; rotate is 33). Fixed in the case file; the mixing steps were re-run on their own and passed (`img-hai-mix.pdl`, 11 of 11) and again from a fresh window (`img-hai-smoke.pdl`, 14 of 14, no remote job).
- A negative case (`img-hai-negative.pdl`) fails with exit 1 as it must.
- **Generated through the UI:** two red sports-car drafts (bk-sdm-tiny 120 s, tiny-sd 123 s). tiny-sd's is a clean red convertible, 3/4 view, game-asset look; neither is top-down pixel art. Verdicts in `data/img/draft/2026-10-13/verdicts.txt`, owner column blank.
- Bugs found by driving it like a person: the words field sat under the tile list and was hidden (moved into the canvas's bottom strip); the tool column covered the canvas (moved to the right side); a free-words draft was logged under part car_door (now tagged `free`).
- Still not driven by the relay: Export, scan, cell-size buttons, hide/delete/front/back, the part and model buttons through the number path.

## 3. Slices (each ends with a screenshot + a state file the owner can read)
| # | Slice | Proof | Status |
|---|---|---|---|
| H1 | Window opens; lists parts/tiles and the built prompt | window screenshot; ui state | built (parts list shows the current part only; ledger/verdict text not shown in the window yet) |
| H2 | Show images in the window | tile palette with thumbnails; canvas for mixing | built (PNG thumbnails; big preview of one draft is the canvas) |
| H3 | **Draft** button: ssh job to debil with status | a new ledger line appears and the new image shows, while the window stays responsive during the ~2 min run | built and observed once (seed 53883, 119 s); window stayed live; a failed copy path was fixed before |
| H4 | Verdict buttons (owner row, claude row) and Accept gated on both | a verdict line per click; Accept stays disabled with one yes; with two yes it writes a candidate row | planned |
| H5 | Second host (Mac) and a model list | same part drafted on both, two ledger lines with host and seconds | planned |
| H6 | Brixel view: show the accepted part in an assembly | the accepted door on the car assembly | planned |

## 4. Open questions
1. **Name and place:** `img-hai` under the `14.h-ai` group like `chat-hai` (MINE: yes), or a different name?
2. **Showing PNGs inside a khtpm window:** the shared renderer's swatch-grid / sprite paths exist (READ in the khtpm standards), but I have not confirmed an image element that loads an arbitrary PNG. If there is none, H2 needs the existing sprite path or a small shared addition (which must follow the khtpm rule: reuse, never a new per-app branch). To check before H2.
3. **Who owns the model list** (which free models, licences): proposal is a `models.pdl` beside the words, reviewed by you.
4. Should Accept also scale the image to the part's 32 px cell, or only store the large image and let the detail-level op (G2) scale it? (MINE: store both, the op decides.)

## 5. Not verified
No click on a window button was made by a human or by the relay: all state changes were sent through the request op that the buttons call, and the screenshots confirm the result. Verdict buttons (H4), the Mac host (H5), the assembly view (H6), the words-field Enter path through the real cli_io (I set words through the request op), the export button and a menu entry (the window starts with `sh button.sh run`; it is not in the HQ/Toys menu yet) are not done. Layout is rough: the placement tools overlay a column over part of the canvas.
