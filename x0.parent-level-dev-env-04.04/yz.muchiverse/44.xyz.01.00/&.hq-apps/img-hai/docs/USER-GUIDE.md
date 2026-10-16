# img-hai user guide

Everything you can do in the window, what each control does, and where the results go. Controls have a stable **id** (used by the tests; you can ignore it). Nothing here is hypothetical: every control below exists. Which ones have been driven by the automated relay test and which have only been seen working is in `TESTING.md` and `HANDOFF.md` section 4.

## 1. Opening and closing
`sh '&.hq-apps/img-hai/button.sh' run` opens the window (it rebuilds the programs if they are missing). `... stop` closes it. The window is 1060 x 900. The title shows `img-hai - img-hai  tiles=N  placed=N  cell=NNpx  cursor=x,y`.
The three small boxes at the top right are the usual chrome: `_` minimise, `!` fullscreen, `x` close (not tested by the automated tests).
The window shows nothing a file does not say: if something looks wrong, the truth is in `state/ui.txt`.

## 2. How clicking works in this house (read once)
- **Two steps:** the first click on a row only *focuses* it (a purple frame appears); the second click activates it. Double-click-style behaviour is built into the house, not into img-hai.
- **Numbers:** every clickable row shows a small number like `[ ]17.`. Typing that number and pressing **Enter** activates it, from the keyboard, even when the row has scrolled out of sight of the mouse. The numbers shift when rows are added (placing a tile adds a row), so read them off the screen each time.
- **Typing field:** the `words >` field at the bottom of the canvas swallows every key while it is armed (click it to arm it; **Esc** disarms it). While armed, digits are text, not row numbers.

## 3. The screen
```
+--------------------------+-------------------------------------------+
| Words (text to image)    |                                [m<][m>]... |  <- right column: placement tools
| <your words / prompt>    |    the CANVAS: a grid of cells             |
| <job status line>        |    (tiles you place appear here)           |
| part: car_door model: .. |                                            |
| next model               |                                            |
| Draft image (remote)     |                                            |
| Draft on ALL hosts       |        [c<][c>][c^][cv][32][64][128][scan] |  <- bottom strip: cursor, cell size
| batch file: default      |        [ words >  type here, Enter      ]  |  <- the words field
| Run selected batch       |                                            |
| Stop batch               |                                            |
| <queue status line>      +--------------------------------------------+
| <judge status line>      |  message bar (last thing that happened)    |
| Placed on canvas         |
|  * 1 tile @1,1 r0        |
| Tiles (click to place)   |
|  [thumbnail] label c:no o:? ...  (scrolls)
+--------------------------+
```

## 4. Making a picture (Draft)
1. **Choose what to draw.**
   - *From the word bank:* click `part: ...` (id `partsel`) to cycle the part (car_door, car_wheel, car_windshield, person_torso, ...). The prompt is built from the bank words for that part (`data/img/words.pdl` + `parts.pdl`).
   - *Your own words:* click the `words >` field, type a prompt, press **Enter**. The line under "Words" shows it. Your words **replace** the bank prompt for every Draft until you clear them (click the field and press Enter with nothing typed, or type a space: the message bar says "words cleared"). Drafts made from your own words are tagged part `free` in the ledger and file names.
2. **Choose a model** with `next model` (id `modelsel`). The part line shows the model; each model belongs to a host (machine) listed in `models.pdl`.
3. Press **Draft image (remote)** (id `draft`) to draft on that model's host, or **Draft on ALL hosts** (id `draftall`) to start one draft on every host at once (each with the first model of its host). A draft takes about two minutes on these CPU-only machines. The status line shows `running: <id> on <host>` and later `done: <id> in <s> s` or `failed: ...`. The window stays usable while it runs; a host can run one draft at a time (a second press says `<host> is busy`).
4. When it is done the new picture appears in the **tile palette** (press **scan** if it does not). It is also written to `data/img/draft/<today>/img/` with a `_32px` copy (nearest-neighbour, 16 colours: what a 32-pixel game sprite would look like) and one line in `data/img/draft/<today>/ledger.txt`.

Seeds: a single Draft uses a seed derived from the clock; batch rows use seeds 1..n so runs can be compared. **A picture is never overwritten:** if the same name already exists the new one is saved with `_r2`, `_r3`... A batch that tests a candidate tuning puts the tuning's name in the picture name (`car_door_tune-1_...`).

## 5. The tile palette and the canvas (mixing)
- **Tiles** are every image under the draft folders and `&.hq-apps/img-hai/tiles/`. Drop any image there (png, jpg, bmp, gif first frame, tga, psd, hdr, pnm; **not** webp, svg, avif) and press **scan**. Each tile shows a thumbnail, its label and its verdict tags `c:` (claude) and `o:` (owner): `?` = not judged.
- **Click a tile** (twice, see section 2) to **place** it on the canvas at the **cursor** cell (the gold square). The cursor moves with `c< c> c^ cv` (bottom strip).
- **Cell size:** `32`, `64`, `128` (bottom strip, ids `c32 c64 c128`) set how big a cell is on the 640x480 canvas. A tile is fitted into its cell (whole-number zoom for small pictures, averaged down for large ones). Default 128 (changed from 64 on 2026-10-14: 32 px art reads better 4x up; the canvas then holds 5 x 3 cells).
- **The Placed list** (left, "Placed on canvas") lists what is on the canvas: `* 1 label @x,y r90 flip` (`*` visible, `-` hidden; `r` is rotation in degrees). Click a row to **select** it; the right-column tools act on the selected placement.
- **Right column tools** (ids in brackets): move `m< m> m^ mv` (`ml mr mu md`), `rot` rotate 90 degrees, `flip` mirror, `hid` hide/show (`vis`), `top` draw it above the others (`up`), `bot` below (`down`), `del` delete, `clear` remove everything, `exp` export (`export`).
- **Export** writes the whole canvas as a new picture `&.hq-apps/img-hai/tiles/mix_<n>.png`; it appears in the palette like any tile, so a mix can be placed in a bigger mix or judged. (Export has been seen in code, not driven by an automated test.)
- Placements are records, not pixels: moving or turning a tile later is lossless.

## 6. Judging and Accept
Two judges look at every picture: **claude** and **the owner**. Buttons act on the *selected placement's* tile: select it first.
| Button (id) | Meaning |
|---|---|
| `c yes` / `c no` (`jc_yes`, `jc_no`) | claude's verdict. Only claude should press these. |
| `o yes` / `o no` (`jo_yes`, `jo_no`) | the owner's verdict. Only the owner should press these. |
| `accept` | writes one candidate line to `data/img/accept_candidates.pdl` **only if both said yes**; otherwise the message bar says `accept needs BOTH yes (claude=? owner=?)`. |
Each press appends `JUDGE | <tile label> | claude|owner | yes|no | <date>` to that day's `verdicts.txt` (the latest line per judge wins, so a verdict can be changed). The tags in the palette update at once. Accept never edits the bank or the game: a human promotes a candidate later (`HANDOFF.md` section 8).

## 7. Batches (unattended drafting)
A batch is a text file in `&.hq-apps/img-hai/batches/`: one row per kind of draft, with a seed count. Machines take jobs from the list themselves.
1. Click `batch file: ... (click to change)` (id `batchsel`) to cycle through the files (alphabetical; `default`, `tune-1`, ...).
2. Click **Run selected batch** (id `batchrun`). The line under the buttons shows `batch <name>: N waiting, N running, N done, N failed/cancelled`.
3. **Stop batch** (id `batchstop`) cancels the waiting jobs; the running ones finish.
4. Come back later, press **scan**, look at the new tiles, judge them. A contact sheet of any image list: `&.hq-apps/img-hai/ops/+x/img_sheet.+x out.png <columns> <thumb px> <files...>` (prints which row/column is which file).
The queue lives in the window's process: **if you close the window the queue is lost** (drafts already running on a host still finish and are saved).
Batch file format and examples: `EXTENDING.md` recipe E.

## 8. Tuning the words (without the window)
The word bank is edited through *candidate edits*, never by hand and never by a model directly. A candidate file can be tried in a batch without promoting it (the batch row's last column names it), and a human promotes the winners. Step by step: `EXTENDING.md` recipe D.

## 9. Where things end up (all under the house root `44.xyz.01.00/`)
| Thing | Path |
|---|---|
| pictures and their 32 px copies | `&.widgits/concept-bank/data/img/draft/<day>/img/` (**not in git**: too big; see `OPERATIONS.md` backups) |
| one line per draft (part, model, seed, steps, size, seconds, file, prompt) | `&.widgits/concept-bank/data/img/draft/<day>/ledger.txt` |
| verdicts | `.../draft/<day>/verdicts.txt` (JUDGE rows) |
| accepted candidates | `&.widgits/concept-bank/data/img/accept_candidates.pdl` (created by the first Accept) |
| exported mixes, your own tiles | `&.hq-apps/img-hai/tiles/` |
| running state (text the window shows) | `&.hq-apps/img-hai/state/` (not in git) |

## 10. Common questions
- *Nothing happens when I click:* it is two-step: click again. Or the words field is armed: press Esc.
- *I typed numbers and they went into the field:* the field is armed. Esc, then type the number, then Enter.
- *The picture is not what I asked for:* the models are small and CPU-sized; see `HANDOFF.md` section 6 for what works (a wheel) and what did not (door, glass, person, road) and the tuning idea.
- *Thumbnails look right but I see colour swaps in other tools:* the thumbnails are deliberately red/blue swapped to look right in this renderer (`CODE.md` section 7).
- *A draft says failed:* `state/job.txt` has the reason; the full remote output is in `state/job_<id>.out`. `OPERATIONS.md` section 5 lists known causes.
