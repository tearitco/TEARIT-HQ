# data-hq (Data Studio): phases P0 (dataset layer) and P1 (charts, panel chooser, picture spike)

Plan: BOOK `08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md` (D58, D61, D64). **Status 2026-11-09: P0 built (harness case `data_hq_p0`, 148 checks) and P1 built (case `data_hq_p1`: `chart_op`, `data_op autodash`, the picture spike); there is NO studio window yet** (P2 window and db menu row, P3 notebooks and the rest are plan only; `spike/` is a fixture prototype that shows ONE chart).
The studio only READS game data. Every table it makes is a projection: delete it and make it again, the bytes come back.

## What exists

| Piece | Where | What it does |
|---|---|---|
| `data_op` | `ops/data_op.c` -> `ops/+x/data_op.+x` (one line in `&.widgits/_shared-lib/harness/ops/build_harness_ops.sh`) | `list`, `sniff`, `check`, `show`, `stats`, `pick`, `provider`, `index`, `autodash` (header of the source has the exact usage) |
| `chart_op` | `ops/chart_op.c` (+ `ops/font57.inc`) -> `ops/+x/chart_op.+x` (one line in `build_harness_ops.sh`) | `chart_op <spec.pdl> <data> <out.png> [--id X] [--size WxH]`: ONE spec row `CHART \| id \| title \| type \| x \| y \| group \| filter \| agg` + a table -> a PNG (or, for an out name ending `.raw`, RGBA + `<base>.receipt.txt`, which is what the renderer's generic `<canvas>` blits). Types: `line` (x numeric or YYYY-MM-DD, a group = one line each, `y=a+b` = one line per column of a wide table), `bar` (HORIZONTAL, category x measure, `top=N` + `other (K)`, groups = clusters), `hist` (whole numbers get a bin each, a group stacks), `scatter` (marker shape differs per group). Axes with 1-2-5 ticks, grid, legend column, title that drops from 3x to 2x before it is cut, 5x7 board font, Okabe-Ito colours, white ground. Empty selection = a grey `NO DATA` tile of the same size (exit 0); a bad spec = exit 2 and NOTHING written. Facts on stdout for tests: `LAYOUT`, `LEGEND`, `POINT` (where each mark went). Same input = same bytes. `chart_op pixel <png> x y` reads a pixel back with stb_image (not the writer's code) |
| `data_op autodash` | `ops/data_op.c` | `data_op autodash <table or exporter folder> <out dir> [--schema s.pdl] [--max N] [--size WxH]`: picks panels from the column ROLES (schema beside the file wins, else the sniffer; the DASH row says which), writes `<out>/dash.pdl` (`DASH` row with the grid, `CHART` rows = chart_op specs, `PANEL` rows = grid cells) and renders every panel with `chart_op.+x`. Rules, in order: time x measure = a line (one per group only when the time values repeat: the engine's `rounds.csv` has one row per player per round, a snake's direction is a state and is NOT a series); category x measure = bars (`sum` for a flow such as rent or paid or a count, `mean` for a balance such as cash); no category but an id and at most 24 rows = bars per id; a measure = a histogram (5 rows or more); two measures = a scatter coloured by a category of at most 8 values. Measures are ordered by distinct values, a constant column is never charted. Default 6 panels (12 for a folder, at most 3 per table), cell 640x400 |
| `spike/` | `spike/spike.xhtpm`, `spike/spike.css` | the PICTURE-IN-WINDOW SPIKE (plan section 5, decision D64): a fixture window of the shared renderer that shows one chart with ZERO renderer C. See "The picture route" below. NOT the studio window (P2) |
| `datasets.pdl` | here | the registry: `DATASET \| id \| path \| kind \| class \| provider \| note` (paths relative to the house root; the longest matching row wins) |
| `fixtures/` | here | **FIXTURES, not game data**: hand-made tables in the shape of Chutes and Ladders (`chutes_markov`, `chutes_games`), Snake (`snake_runs`), Teario (`teario_runs`, `teario_deaths`) and two generic tables, each with the schema the sniffer is expected to reproduce. They prove the studio handles those shapes before the games exist |
| the first exporter | `@.apps/deskopoly/ops/deskopoly.c`, verb `export` | `deskopoly <app> <state> export <out>`: replays the ledger with the engine's own replay and `check` (a ledger that does not reproduce itself, an oversize ledger, a mini game, an out folder that holds a ledger.txt: refused, nothing written) and writes `game.csv` (1 row), `rounds.csv` (player x round), `events.csv` (1 row per ledger row), `tiles.csv`, `trades.csv`, `auctions.csv`, `schema.pdl` |

## Verbs (all deterministic, no network, no daemon)

```
data_op list <root...> [--registry f.pdl] [--house dir]   FOUND | path | kind | rows | cols | bytes | class | why | provider
data_op sniff <file> [--out schema.pdl] [--against expected.pdl]
data_op check <file> <schema.pdl>          same columns in order, measure columns numeric, time columns numeric or dates
data_op show <file> [--schema s.pdl] [--head N] [--where col>=val]... [--cols a,b]
data_op stats <file> [col]                 n min p05 q25 median q75 p95 max mean distinct (linear-interpolated quantiles)
data_op pick <file> <outcol> --where ...   exactly one matching row or exit 1 (used by the harness to tie tables to the engine)
data_op provider <app_dir> <state_dir> <out_dir>   run the app's `GAME | export` row
data_op index <exporter out dir...> [--out index.csv]   one row per game: id source ruleset seed players rounds rows outcome winner winner_driver human_ever hash
```

Formats read: csv and tsv (RFC 4180 quotes), `.pdl` pipe tables (a header line then a dashed line), pipe ledgers (`EVENT|round|actor|verb|target|at|result`), a folder holding `ledger.txt` (listed as `ledgerdir`).
Caps (refused with the reason, exit 2): file 8 MiB, 200000 rows, 64 columns; `list` 2000 datasets, depth 8; `provider` 60 s and 64 KiB of exporter output (only the first 4000 bytes are printed); `index` 512 folders.

## Schema files and roles

`schema.pdl` rows: `TABLE | file | what`, then `COL | name | role | unit | hint`; roles `time category measure id text`; `ABSENT | name | why` lists what the data-hq plan wanted and the engine does not have (no column is invented).
With no schema the sniffer guesses: a date, or an ascending integer column whose name is time-like (tick, round, seq, step...), is `time`; other numbers are `measure`; an ascending integer column of unique values with no time-like name is `id` (the plan says "ascending integers are time"; that is as likely a row id, so P0 says id); a name ending in `_id`, `hash`, `seed`... is `id`; few repeated short strings are `category`; the rest is `text`.
A sniff is a guess and a `schema.pdl` beside the data wins (on the real 103-row archived game the sniffer and the exporter schema differ on 18 of the 87 exported columns, for example a tile index sniffs as a measure: plan section 12).

## Public and private (data, fail closed)

1. a `datasets.pdl` row (longest path prefix) says it;
2. a path under `xyzfs/users/` is private;
3. a ledger (or a folder with `ledger.txt`) with a human seat is private, one without is public;
4. a folder with an exporter `game.csv` follows its `human_ever` column;
5. otherwise **private**: only a positive reason makes a dataset public.
Private rows never reach the AI side (plan section 7; the AI side is not built).

## How a game opts in: `GAME | export`

One optional data row in the app's `game.pdl` (documented there for Teropoly):

```
GAME   | export       | {book}/ops/+x/deskopoly.+x {book} {state} export {out}
```

`{house}` and `{book}` (the app folder) are replaced like the `driver` row of the editor's Play; `{state}` is the game's state folder (holds `ledger.txt`), `{out}` the folder the tables go to. Arguments are split on single spaces (no quoting).
The program must be read-only on `{state}`, exit 0 on success, exit 1 if the ledger does not reproduce itself, and write only inside `{out}`. `data_op provider <app> <state> <out>` runs it (fork/exec, no shell, 60 s, output capped, killed on a breach; it is short-lived, so no process registry row is needed). Exit 3 = the app has no export row.
**The editor's Play ignores the row**: `petslike_core.c game_row()` is asked for exactly four keys (`title`, `driver`, `chat`, `clock`) and skips every other `GAME` row (read 2026-11-09, `&.widgits/_shared-lib/petslike_core.c` lines 190 to 200). Case `dsk_chathook` starts the REAL editor against the shipped `game.pdl` (with the export row) and still passes 47/47.
A new game (Chutes, Snake, Teario) needs only its own exporter that writes `game.csv` (one row with at least `game_id`), its tables and a `schema.pdl`; the studio needs no change (P7 proves it with a second provider).

## Files the studio writes (runtime-file standard)

Nothing at rest, no daemon, nothing on a timer. `deskopoly export` writes 7 files (59 KB for the 831-row pinned game, 14 KB for a 103-row one) into the folder you name, at most 8 MiB, refused beyond; `data_op index --out` one small file; `data_op sniff --out` one schema. Rows in `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`. The `state/out` cache of the later phases does not exist yet.

## Tests

`sh`-free: `cd &.widgits/_shared-lib/harness; prisc+x data_hq_p0.pal` then read `results/data_hq_p0.txt.verdict.txt`. 148 checks, scratch only; the owner's archived Teropoly games are only read.

## What P0 showed (2026-11-09)

See the plan, section 12. Short version: the editor archive holds 3 archived games (60, 81 and 103 rows, 4 to 8 rounds) plus the live one (78 rows); none was played to the end (every game.csv says `open`), all are seed 7 with 3 players and a human at p1 (so all private), and all four replay and export under today's engine; the engine has no wall-clock time, no per-player income account and no pet-policy id (the three `ABSENT` rows); the sniffer and the exporter schema differ on 18 of 87 columns of the 103-row game, so a schema.pdl is not optional for the dashboard.

## The picture route (P1 spike, 2026-11-09; decision D64)

**Answer: the shared renderer already shows a chart, with no renderer change.** `<canvas id="screen" sprite="${canvas_raw}"/>` (`khtpm_draw_core.c kh_draw_canvas`) reads a RAW RGBA file whose size is in `<base>.receipt.txt` (`frame_w=`, `frame_h=`); `chart_op <spec> <data> <x>.raw --size WxH` writes exactly that pair. A frame the size of the canvas box is blitted 1:1 (sharp: 10 of 10 probed pixels of the window equal the chart's own pixels); a different size is scaled "cover" (aspect kept, nearest neighbour, the excess cropped), so the studio should ask the window for the box size (as the pc-hq board does) and render at that size. The file is re-read only when its inode, size or mtime change, and the producer replaces it by rename: rewriting the chart in place showed the new chart within 2 s without a relaunch. In a window the var `canvas_raw` is published by the window's manager (`vars=` file); one `<canvas>` per window (the decoded frame is a single static cache).
**The other route does not work for charts:** an `<item sprite="x.png"/>` draws the PNG through `stbi_load` UNSCALED and clipped to the item box, reloads it on every repaint, and shows RED AND BLUE SWAPPED (the RGBA bytes go to an X image that wants BGRA: the orange series came out sky blue in the spike). Use it for nothing that has colour meaning; a generic fix (swap R and B, cache, scale to fit) would be a renderer capability for every window, not a data-hq branch.
**Measured (scratch house `/tmp`, the shared `khtpm_core_render.+x` on `spike.xhtpm`, DISPLAY :0, no browser asked of, 10 s idle, window showing the chart):** renderer 0.7% of one core (7 clock ticks in 10 s), 45 wakeups per second (the canvas keeps the 30 fps tick: `g_has_canvas`), 1 thread, 0 children, 17 MB resident; one process, registered in the scratch registry by `kh_spawn.sh`. This is the cost of a window that HAS a canvas even when nothing changes; P2 must say it again with the real manager and a browser open.
Run it yourself (scratch, never the live house): copy `spike/spike.xhtpm` + `spike.css` to a folder, put `canvas_raw=<abs path to chart.raw>` in `spike_ui.txt` there, write `chart.raw` + `chart.receipt.txt` with `chart_op`, then `sh &.widgits/_shared-lib/kh_spawn.sh <scratch house> <owner pid> data-hq-spike -- env DISPLAY=:0 <khtpm_core_render.+x> <scratch house> <folder>/spike.xhtpm`, find the window by size (`xwininfo -root -tree`, windows have no name) and photograph it with `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <hex id> <png>`; end it with `kill -TERM <that pid>`.

## P1 findings (2026-11-09)

1. **A schema or a hint is needed for "series" in an interleaved long table.** The engine's `events.csv` has one row per ledger row and a unique `seq`, so a time axis with an `actor_cash` column draws ONE scribble through all actors (the autodash rule only groups when the time values repeat). Charting that table needs the exporter to say which category is the series key (a word in the `hint` column of its `schema.pdl` is the smallest way); not done.
2. The default bar aggregate is a guess made from the column NAME and unit (rent, paid, income, fee, a `count` unit = sum; anything else = mean). Right for the Teropoly tables and the fixtures, a heuristic anywhere else: the dashboard is a notebook the owner can edit (plan section 4), so a wrong default costs one edit.
3. PNG size: a stored-block PNG is W x H x 3 bytes (1.15 MB at 800x480, 0.77 MB at 640x400), so a 12-panel dashboard is 9 MB. Capped by `--max` and the 8 MiB file cap of the reader; the `state/out` cache of P2 must carry its own cap (RUNTIME-FILE-GROWTH-STANDARD row to be added with it). A fixed-Huffman encoder would cut it about 20x; not needed yet.
4. The lowercase `p`, `g`, `q`, `y` of the 5x7 board font have no descenders (they look like small capitals: "Per player"). It is the board's font, kept so labels match the game; a real fix is a font table of the studio's own.
5. Not built (plan section 5 lists them): stacked bars, heat map, summary tiles, a log axis, tooltips (a picture has none: P2 may overlay text rows).
