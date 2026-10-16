# img-hai: how it is built

For someone who has to read, debug or change the code. Source is small and in plain C; the window layout is one static template. Line counts on 2026-10-13: manager ~240 lines (dense, one statement-group per line, house style), three small ops, one template (~70 lines), one stylesheet. Read `docs/README.md` first for the overview.

## 1. Processes and lifecycle
```
button.sh run
   |-- stop_old(): kill -TERM the pids in state/renderer.pid / state/manager.pid (exact pids, never pgrep -f)
   |-- setsid nohup khtpm_core_render.+x <house> img-hai.xhtpm  ->  state/renderer.pid
         |-- the renderer parses img-hai.xhtpm, sees <module src="...img_hai_manager.+x">, forks it as its child
               |-- img_hai_manager.+x <house> <package_dir>      (long running; writes state/ui.txt, state/canvas.raw)
                     |-- fork+exec img_prompt.+x      (per draft with a bank prompt; waits up to 5 s)
                     |-- fork+exec img_hai_job.+x     (per running draft; detached with setsid; reaped with waitpid(WNOHANG))
                           |-- fork+exec ssh / scp    (each with a watchdog; no popen, no system)
button click -> renderer runs the item's action= string: '<pkg>/ops/+x/img_hai_req.+x' 'VERB' <package_dir> <house_root> [typed text]
                -> img_hai_req appends "VERB[:text]" to state/request.txt   -> the manager notices the file grew and handles it
```
- The manager **exits by itself** when the renderer's pid (`module_parent.pid` in the package dir) is gone. Closing the window therefore stops the manager, but **not** a draft already running on a host (the job op is detached) and it loses the batch queue.
- Only one window instance is expected (button.sh stops the old one first).

## 2. Files
| File | Role |
|---|---|
| `img-hai.xhtpm` | the static window template. Layout only; never generated at runtime. Reads variables from `state/ui.txt`. |
| `img-hai.css` | sizes and colours only. Layout rules live in the shared renderer. |
| `img_hai_manager.c` | the brain (see sections 3-6). Header comment has the verb list and function index. |
| `ops/img_hai_req.c` | the click side: appends one line to `state/request.txt`. |
| `ops/img_hai_job.c` | one draft on one remote host (section 6). |
| `ops/img_sheet.c` | contact sheet of many images (stand-alone; not used by the window). |
| `models.pdl` | HOST and MODEL rows (section 5). |
| `batches/*.pdl` | batch lists (section 5). |
| `button.sh`, `build.sh` | launch/stop; build the four programs into `+x/` and `ops/+x/`. |
| `toy.pdl` | a house menu stub (not wired to any menu yet). |
| `tests/*.pdl` | relay test cases (`TESTING.md`). |
| `tiles/` | your own images; Export writes `mix_<n>.png` here. |
| `state/` | runtime only, **not in git**: `ui.txt`, `canvas.raw`, `canvas.receipt.txt`, `request.txt`, `job.txt`, `job_<id>.out`, `prompt_*.txt`, `thumbs/`, `renderer.pid`, test screenshots. |
| `audit/img-hai.log` | renderer stdout/stderr. |
Outside the folder (the word bank, shared on purpose): `&.widgits/concept-bank/data/img/{words.pdl,parts.pdl,candidates/,draft/,accept_candidates.pdl}`, `&.widgits/concept-bank/ops/{img_bank.c,img_prompt.c,img_edit.c}`, and the remote script `data/img/draft/img_draft.py`.

## 3. The three contracts with the renderer
### 3.1 `state/ui.txt` (manager -> window)
Plain `key=value` lines, rewritten atomically (write `.tmp`, rename) only when something changed (`dirty`). The template refers to them as `${key}`. A repeated block uses `<repeat count="${X_count}" bind="t">` and the keys `t_<i>_<field>` (bind name, index, field): that naming is the renderer's rule. A row with `show=` that is `0` or empty is dropped.
| Key | Meaning |
|---|---|
| `status_line` | window title text |
| `canvas_raw` | path of the pixel buffer (also named in the template's `<canvas sprite=...>`) |
| `tiles_count`, `t_<i>_label`, `t_<i>_sprite`, `t_<i>_action`, `t_<i>_show` | the tile palette (label includes the `c:`/`o:` tags; `show`=0 when no thumbnail could be made) |
| `place_count`, `p_<i>_label`, `p_<i>_class`, `p_<i>_action` | the Placed list; class `active` = selected |
| `prompt_line`, `job_line`, `queue_line`, `judge_line`, `part_line`, `batchsel_line`, `msg` | text lines |
| `*_action` (words, draft, draftall, batchrun, batchstop, batchsel, part, model, accept, jc_yes, jc_no, jo_yes, jo_no, mv_l.., rot, flip, vis, up, down, del, clear, export, rescan, c32, c64, c128, cu_l..) | the full command for each button: `'<pkg>/ops/+x/img_hai_req.+x' 'VERB'` (the renderer appends package_dir and house_root) |
### 3.2 `state/request.txt` (window -> manager)
One line per click: `VERB` or `VERB:argument`. Append-only; the manager remembers a byte offset (`req_off`) and reads only the new, newline-terminated lines (a half-written line is left for the next tick). The offset starts at the file's size when the manager starts, so old requests are not replayed. Change detection is **size growth**, never mtime (house rule: `prefer-marker-files-not-mtime`). The full verb list is in the manager's header comment and in section 4.
### 3.3 `state/canvas.raw` + `canvas.receipt.txt` (the pixels)
`canvas.raw` is 640 x 480 x 4 bytes, **RGBA, row-major**, written to `.tmp` then renamed. `canvas.receipt.txt` says `overlay_w=640` / `overlay_h=480`. The renderer's `<canvas>` reads it and blits it 1:1, centred, cropped if bigger. A live canvas makes the renderer redraw at about 30 fps (a known CPU cost of any canvas window).

## 4. The manager, section by section (`img_hai_manager.c`)
1. **Tiles** `scan_tiles()` lists images in `draft/*/img/` and `tiles/` (sorted by path), then `make_thumb()` writes `state/thumbs/s<index>_<filesize>.png`: scaled so the longest side is 72 px (box average down; whole-number nearest up for small pictures) and **red/blue swapped** because the renderer paints `.png` sprites as BGRA. The thumb name contains the source file size, so a changed file gets a new thumb; old thumbs are not deleted.
2. **Judges** `load_judges()` reads every `draft/<day>/verdicts.txt` and `<package>/verdicts.txt`, takes `JUDGE | label | who | yes/no | day` rows (latest wins) into `J[]`. A VERDICT request appends a JUDGE line to the verdict file of the tile's own folder (`<day>/verdicts.txt`; for a tile in `tiles/`, `<package>/verdicts.txt`) and reloads. ACCEPT checks both verdicts and appends a CAND line (labels starting `test_` go to `state/accept_test.pdl` so tests never touch real data).
3. **Hosts, models, parts** `load_models()` (models.pdl), `load_parts()` (parts.pdl).
4. **Prompts** `build_prompt(part, outfile, overlay)` forks `img_prompt.+x`, stdout to a file, waits at most 5 s. With an overlay it passes `0.30 <overlay path>`.
5. **Drafts** `start_on(host, model, part, words, seed, overlay)` makes the prompt file (free words -> `PROMPT: ...`/`NEGATIVE: text, watermark, blurry`; else `build_prompt`), forks the job op with `setsid`, records `job_pid[host]`. `start_draft` (selected model's host), `start_all` (each host's first model). One job per host at a time.
6. **Batches** `scan_batches` (alphabetical list), `load_batch(name)` expands rows to `Q[]` jobs (seeds 1..n); `dispatch_queue()` (every tick) gives each idle host the next waiting job whose model is `any` (host's first model) or named and on that host; `poll_job()` reaps children, marks the job done/failed from the exit status, rescans tiles, and shows the last new line of `state/job.txt` as `job_line`.
7. **Canvas** `draw_place()` composites one placement (fit to cell, rotation 0-3 x 90, flip, alpha), `write_canvas()` paints checker + grid, all visible placements in order, and the gold cursor frame.
8. **Window** `write_ui()` (section 3.1). **Input** `handle()` (one `if/else if` per verb), `poll_requests()`. **Lifecycle** `parent_alive()`, `bye()` (SIGTERM), `main()`.
Limits (compile-time): 256 tiles, 64 placements, 512 judge labels, 256 batch jobs, 16 models, 4 hosts, 16 parts. Past 256 tiles the extra images are not listed.

## 5. Data formats (pipe-separated rows, `#` = comment; same family as the house's `.pdl` files)
```
models.pdl     HOST  | name  | ssh target        | remote dir | python path       | command prefix (low priority)
               MODEL | model id (Hugging Face) | host name | licence | steps | size px | note
batches/*.pdl  BATCH | label | part or free | words ("-" = from the word bank) | seeds | model id or any | overlay (name under data/img/candidates/ without .cand, "-" = none)
words.pdl      WORD  | id | text | kind (subject material colour angle style size negative) | weight 0..1
parts.pdl      PART  | brixel id | word ids (comma list) | note
*.cand         EDIT  | id=.. | type=img_word_weight_delta|img_word_text|img_part_words|img_word_add | target=.. | (delta=|text=|words=|kind=,text=,weight=) | reason="..." | proposer=.. | status=candidate
ledger.txt     LEDGER | part | model | seed | steps | size | seconds | file name | prompt
verdicts.txt   JUDGE | tile label | claude or owner | yes or no | day        (older free-text VERDICT rows may sit beside them; only JUDGE rows are read)
accept_candidates.pdl   CAND | tile label | image path | claude yes | owner yes | day
promotions.pdl          PROMOTED | edit id | type | target | proposer | promoter | day      (written by img_edit promote)
state/job.txt  one status line per event, append-only: running / copying the image back / done: <id> in <s> s / failed: <reason>
```
Tile label = image file name without `.png`, normally `<part>_<modelname>_s<seed>` (free words: `free_<modelname>_s<seed>`; batch free rows: `free_<rowlabel>_<modelname>_s<seed>`; an overlay trial: `<part>_<overlayname>_<modelname>_s<seed>`; a rerun of an existing name: `..._r2`; `_32px` copies are separate tiles). Verdicts attach to the label, so a changed label is a new picture.

## 6. The remote draft pipeline (`ops/img_hai_job.c`, `draft/img_draft.py`)
`img_hai_job <pkg> <house> <ssh target> <remote dir> <python> <model> <steps> <size> <seed> <part> <prompt file> [prefix]`:
1. status `running: <id> on <target> (about 2 min)`.
2. `ssh <target> "mkdir -p <rdir>/jobs/out"` (timeout 30 s), `scp prompt -> <rdir>/jobs/<id>.txt` (60 s).
3. `ssh <target> "cd <rdir> && <prefix> env IMG_THREADS=6 <python> img_draft.py --prompt-file jobs/<id>.txt --part <part> --model <model> --out jobs/out --steps <n> --size <px> --seed <s>"` (timeout 1500 s). `<prefix>` is `nice -n 15 ionice -c3` on Linux, `nice -n 15` on macOS (no ionice).
4. `img_draft.py` (on the host): loads `StableDiffusionPipeline` in float32 on CPU (safety checker off: offline art drafting, not a public service), generates, saves `<id>.png` and a nearest-neighbour 32 px 16-colour `<id>_32px.png`, prints `done <secs>s <path>`.
5. status `copying the image back`; `scp` both PNGs into `draft/<day>/img/`; append the LEDGER line; status `done: <id> in <s> s`. Any step failing writes `failed: <reason>` and exits 1 (the manager marks the job failed; `state/job_<id>.out` keeps the remote output).
The remote name (and the job id) is `<part>_<modelname>_s<seed>`, where `<part>` is the part, `free`, `free_<batch row label>` or `<part>_<overlay name>` (an overlay trial gets its own names). **Locally a picture is never overwritten**: if `<id>.png` already exists in the day's `img/` folder the new one is saved as `<id>_r2.png`, `_r3` ... and the ledger line and the `done:` status use that local name. (This was added after a batch overwrote nine pictures; see `HANDOFF.md` section 9.)
Why a prompt *file*, not a command-line string: prompts contain commas, quotes and pipes; a file avoids every quoting problem.

## 7. Renderer quirks this code works around (each was met and fixed)
| Quirk | Consequence in this app |
|---|---|
| `sprite=` shows **PNG only**, drawn **unscaled** into the item box, and as **BGRA** | the manager makes small PNG thumbnails and swaps red/blue in them; any other format would not show |
| `<canvas>` shows a raw RGBA file 1:1, centred | big picture = canvas; the canvas forces a ~30 fps redraw |
| the window's rows are **numbered** and the numbers shift when rows are added | tests press by id (`relay_drive press <id>`), not by number |
| clicks are **two-step** (focus, then activate) | a human clicks twice; tests use `tap` / `press` |
| a `cli_io` inside a sidebar sits glued to the bottom and was **covered** by a long scroll list | the words field lives in the canvas' bottom overlay strip, which reserves space for a typed line |
| `<tabbar>` / `<row class="toolbar">` inside a `<panel>` with a canvas are **not laid out** | tools are canvas overlay rows: `canvas-overlay-right ov-top` (column) and `canvas-overlay-bottom` (strip); each item takes its css width/height (`.ov-btn`) |
| two scroll lists in one sidebar collapsed the first | only one `<scrolllist>` (the tiles); the placed list is plain rows |
| the frame dump (key `p`) is written to the **shared path** `/tmp/entity-menu-frame.png*` | drive one window at a time |
| keys through the relay are **decimal codes** | `relay_drive` converts |

## 8. Build, run, files not in git
`bash build.sh` builds the manager, req op, job op and sheet op (gcc, `-lm`, stb headers from `&.widgits/_shared-lib/`). The word-bank ops build with `bash '&.widgits/concept-bank/ops/build_img_ops.sh'`. Binaries (`+x/`, `ops/+x/`) and `state/` are in `.gitignore`; after a clone or a wipe run both build scripts. The renderer itself builds with `sh _.monads/_.livedesk-taskbar/ops/build_core_render.sh` (button.sh does it if missing).

## 9. Safety properties (checked in code, not by luck)
- No credentials anywhere: ssh uses keys; the login note stays outside the repo.
- Every child process is fork+exec with a time limit; the window cannot freeze on a hung ssh.
- The word bank is written only by `img_edit promote`, which refuses promoter == proposer and refuses any edit that does not validate (`EXTENDING.md` D).
- `test_*` tiles can never write to real verdict/candidate files.
- Nothing is generated at game runtime; the only callers of image models are the Draft job and the batch queue.
