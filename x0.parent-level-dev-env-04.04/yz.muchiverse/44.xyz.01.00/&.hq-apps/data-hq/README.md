# data-hq (Data Studio): phase P0, the dataset layer

Plan: BOOK `08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md` (D58, D61). **Status 2026-11-09: P0 built and tested (harness case `data_hq_p0`, 148 checks); there is NO window yet** (P1 charts, P2 window and db menu row, P3 notebooks and the rest are plan only).
The studio only READS game data. Every table it makes is a projection: delete it and make it again, the bytes come back.

## What exists

| Piece | Where | What it does |
|---|---|---|
| `data_op` | `ops/data_op.c` -> `ops/+x/data_op.+x` (one line in `&.widgits/_shared-lib/harness/ops/build_harness_ops.sh`) | `list`, `sniff`, `check`, `show`, `stats`, `pick`, `provider`, `index` (header of the source has the exact usage) |
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
