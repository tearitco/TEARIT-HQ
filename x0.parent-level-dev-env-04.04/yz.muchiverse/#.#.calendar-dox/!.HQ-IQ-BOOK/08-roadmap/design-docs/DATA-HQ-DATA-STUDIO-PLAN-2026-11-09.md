# data-hq: a generic, dynamic, AI-integratable data studio (the final plan, 2026-11-09)

Owner, 2026-11-09, in two steps. First: a house tool to view one or more games' data like a spreadsheet, auto-charts, history for trends and training, sessions auto-loaded in a sidebar like chat-hai. Then, widening it: "a tool like blueprintz, layout studio, file explorer, an x11-hq app; it does not have to be .csv, .pdl is fine, make it easy; not Teropoly only; it adapts its layout to the data, a dynamic but generic dashboard; put it under the `db` cell (toolbar cell 10) as `data-hq`; a very versatile, AI-integratable data studio, like RStudio or Python notebooks." This file REPLACES the first design (`16.game/GAME-DATA-HQ-...`, same day, moved here; decision D58 was revised in place). Status: **PLAN ONLY, nothing built.** Labels: **FACT** (read on 2026-11-09), **PROPOSAL**, **OPEN** (owner decision or unverified).

## 0. What changed from the first design (the answer to "does that change the plan?")

| Before (Teropoly viewer) | Now (data studio) |
|---|---|
| A game's CSV tables and charts | ANY dataset: csv, tsv, `.pdl` tables, pipe-delimited ledgers, other apps' state files; a game is just the first "provider" |
| Sidebar = games | Sidebar = notebooks + auto-discovered datasets + history |
| Auto charts | Auto DASHBOARD whose layout is chosen from the data's schema, plus hand-built notebooks |
| Queries handed to sql-hq | The studio calls sql-hq's engine itself for SQL cells; notebooks are first-class |
| Training export | An AI side (explain, propose cells, propose SQL) and a training/offload path |
| Toys menu (`toy.pdl`) | The `db` menu, row `data-hq` (see section 2) |
| One window | One window with three modes: dashboard, notebook, table |

## 1. What exists (FACT) and what we reuse

| Piece | FACT | Reuse for |
|---|---|---|
| `sql-hq` | Vendors `sqlite3.c`, compiles its own engine (`ops/sql_hq_engine.c`) with CSV and PDL adapters (`sql_hq_adapters.c`); there is NO system `sqlite3` binary here or on debil. | All SQL cells. Call it as a process (op + files); do not link and do not write a second engine. |
| `blueprintz` | book/pages/events editor: manager + `books.pdl` + `.xhtpm`. | The "document of ordered parts" shape: a notebook is a book of cells. |
| layout studio | Per-game layouts with ids (layers 1 and 2 built, memory note; `layouts.pdl` exists in `@.apps/deskopoly`). | Saved dashboard layouts; OPEN: verify the file format fits before reuse. |
| `file-explorer` (widget) | Folder browsing widget. | Picking/importing data files; the dataset browser. |
| `eden-viewer-hq`, `csv-lab-hq` | Read-only game window; sidebar of tabs and filters over a CSV pipeline. | Window shell, tab and filter pattern. |
| `chat-hai` | Disk-persisted, deletable session list in a left sidebar. | Notebook and dataset sidebar. |
| h-ai (`hai_op.c`) | locate / context / ask / propose / apply: the model may only write a whitelisted file shape, nothing happens until the owner says `/apply`, key read from a gitignored file and never printed, quota guards (3 s between asks, 200 a day on the free tier). | The AI side: same proposal-then-apply contract. |
| The renderer | No chart element; a canvas can show an RGBA file. | Charts are pictures from a C op. |
| db menu | Rows `db_menu_N_label` / `db_menu_N_cmd` in `#.desktop/livedesk_taskbar.pdl` (today 1 db-ez, 2 db-hq, 3 sql-hq, 4 Cancel); each `livedesk:open-*` command has a branch in `khtpm_taskbar_manager.c`. | Adding `data-hq` = rows + a small launch branch copied from `open-sql-hq`. |
| House rules | NO Python, no new `.sh`; the ledger is the only authority; derived files are projections; free models only; heavy work offloaded (debil 12 cores, Mac); capped runtime files; one process registry. | Constraints on every phase below. |

## 2. Where it lives (PROPOSAL)

`&.hq-apps/data-hq/` (manager C, `.xhtpm`, `.css`, `README.md`, `ops/`, `state/`); db menu row 4 `data-hq` (Cancel moves to 5) via two data rows in `livedesk_taskbar.pdl` and one launch branch in `khtpm_taskbar_manager.c` shaped like `livedesk:open-sql-hq` (that shared file collides often: one tiny branch, own commit). Shown to the user as "Data Studio".

## 3. The data model: datasets (PROPOSAL, kept easy)

- A **dataset** = a named table (header + rows) plus optional `schema.pdl` (`COL | name | role | unit | hint`). Roles: `time`, `category`, `measure`, `id`, `text`. With no schema the studio SNIFFS: numbers are measures, ascending integers or dates are time, few repeated strings are categories.
- **Providers** read what the house already has: csv/tsv; `.pdl` tables (`SECTION | KEY | VALUE` with the dashed line); pipe-delimited row files (ledgers: `EVENT|seq|actor|...`); a folder of such files as one dataset with a `source` column; and **exporters**: an app opts in with a data row in its `game.pdl` or app config, `GAME | export | <program> <args>`, whose program replays the app's authority (the ledger) and writes tables. No app name lives in the studio.
- **Authority rule:** the studio READS. Every table it makes is a projection (derived by a named verb, safe to delete, byte-identical on regeneration). A notebook is the owner's document (a preference-class file, like a layout), never game state.
- **Classes** on a dataset: `public` (harness, pet-vs-pet games) or `private` (a human's play, user desk data). Class decides what the AI side may see (section 7).

## 4. The window and its three modes (PROPOSAL)

```
+----------------------------------------------------------------------------------+
| Data Studio            [dashboard | notebook | table]      [AI] [export] [save]  |
+------------------+---------------------------------------------------------------+
| NOTEBOOKS        |  (dashboard) Teropoly games, 3 selected                       |
|  teropoly-trends |  [games 3] [rounds 78] [trades 3] [winner B]   <- summary tiles|
|  + new           |  net worth by round (line per player, game overlaid)          |
| DATASETS         |  rent collected by colour group (bars)                        |
| [x] live game    |  auction price vs list (scatter)   cash at round 20 (hist)    |
| [x] game-...607  |                                                               |
| [ ] harness: s6  |  (notebook) cells run top to bottom:                          |
| [ ] +import file |   text | data | query(SQL) | chart | stat | ai | export       |
| HISTORY          |  (table) sortable, paged, filter box, "open as SQL cell"      |
|  trends          |                                                               |
+------------------+---------------------------------------------------------------+
```
- **Sidebar** is loaded by scanning, like chat-hai's history: notebooks (`state/notebooks/*.pdl`), datasets from every provider and exporter, history (archived runs). Check boxes select 1..n datasets (overlay, compare). New files are noticed by a marker-file size change, never by a timer or mtime.
- **Dashboard (the dynamic generic layout):** `autodash` picks panels from the selected datasets' schema roles: summary tiles for counts and extremes; time x measure = line per group; category x measure = bars (sorted); one measure = histogram; two measures = scatter; many categories = top-N + other. Panels fill a grid chosen from the panel count and window size. The generated dashboard is itself a notebook the owner can edit and save (so "dynamic" never means "uneditable").
- **Notebook:** a `.pdl` of ordered cells `CELL | id | kind | ...`: `text`, `data` (load a dataset), `query` (SQL through the sql-hq engine, read-only), `chart` (declarative spec), `stat` (a fixed vocabulary of C ops: mean, median, quantiles, correlation, linear regression, group-by aggregates, moving average, rank), `ai` (section 7), `export` (write a table). No Python, R or JS cells (house rule); the extension path is a new small C op registered in `stats.pdl`.
- **Outputs** are projections cached under capped `state/out/`; running a cell again overwrites; deleting the folder loses nothing.
- **CPU:** idle near zero; a cell or panel runs on demand only; `chart_op` and long stats run niced behind `cpu_gate`; heavy jobs offload to debil or the Mac.

## 5. Charts (PROPOSAL)

`chart_op <spec> <data> <out>`: C, no libraries, embedded bitmap font (same kind as the board labels), line, bar, stacked bar, histogram, scatter, heat map; fixed accessible palette with labels (skill `dataviz`); same input = same bytes. **OPEN, unverified:** how a window shows a picture. The board and pets show a canvas fed from an RGBA file; I did NOT verify this for an `.xhtpm` toy-style window. Phase P1 starts with a one-day spike (canvas fed by RGBA; fallback: PNG file shown in the canvas element) and a decision-log row with the result.

## 6. History and trends (PROPOSAL)

Every archived run of any provider adds one row to that provider's `index.csv` (a projection). Trend panels read the index: for games win rate by seat and by driver, game length, cash at round N, first-tile choices vs winning, rent by colour group, trades and auctions per game, card frequencies. This is the balance and tuning instrument; for other apps it is whatever their exporter publishes.

## 7. The AI side (PROPOSAL; same contract as hai_op)

1. **Context file** (`state/ai_context.txt`, under 2 KB, whole file, no secrets): dataset names, columns, roles, row counts, three summary statistics per measure; NEVER raw rows of a `private` dataset.
2. **Three verbs:** `explain` (answer in the chat pane), `propose` (the model may only emit cells from the whitelist: `chart`, `stat`, `query` with SELECT only, `text`; nothing else is written), `sql` (a proposed SELECT). Nothing is added to the notebook until the owner says `/apply`; `/no` drops it; the previous notebook is kept as `.bak`.
3. **Which model:** free models only (house rule). Cloud free tier with hai_op's guards (a gap between asks, a daily cap, key from a gitignored file, never printed); local Ollama via the Mac (`ai_backend.pdl`) for `private` data. Never loop to a rate limit.
4. **Validation:** a proposed cell is parsed with the same strict parser as a hand-written one and run on a LIMITed copy first; a cell that fails validation is shown as an error, never applied.
5. **Training path:** an `export` cell can write a feature table (one row per decision: state features by replay, action, outcome); training jobs run on debil or the Mac; their metrics come back as a dataset and show in the dashboard. Human-seat games enter training only on an explicit flag.

## 8. Phases and acceptance (what a human must see, and the proof that can fail)

| Phase | Deliverable | Proof |
|---|---|---|
| P0 | Dataset layer: csv/tsv/pdl/ledger providers, schema sniffing, `schema.pdl`; first exporter `deskopoly export` (game, rounds, events, tiles, trades, auctions, index) | case: replay reproduces tables byte for byte; hand-edited tables change nothing; row counts equal ledger rows; cash column ties to `status`; sniffing picks the expected roles on 5 fixtures; negative controls |
| P1 | `chart_op` + `autodash` panel chooser (and the picture-in-window spike) | case with pixel probes (series colour at the expected point, axes, legend), same input = same bytes, empty data = "no data" tile; PNGs opened |
| P2 | Window shell, db menu row, sidebar (datasets), dashboard and table modes | live PNG of the window opened from the db menu with real archived games in the sidebar; `see` steps; a newly archived game appears without a relaunch |
| P3 | Notebook: text, data, query, chart, stat cells, save and load, multi-dataset overlay | case: a fixture notebook renders identical output twice; a bad SQL cell shows its error and stops; PNG of 3 games overlaid |
| P4 | History and trends from `index.csv`; open table as SQL cell | trend chart over at least 10 harness games |
| P5 | AI cells: context file, `explain`, `propose`, `/apply`, validation, quota guards | case with a stub model (no network): a proposal outside the whitelist is refused, `private` rows never in the context, `/no` leaves the notebook byte-identical; ONE real free-model call shown live (counted) |
| P6 | Training export + offloaded job + metrics dataset | case: features at a decision equal the replayed state; no human rows without the flag; one real small job on debil |
| P7 | A second provider (a petslike game or Eden) with no studio code change | the sidebar lists it; its dashboard is generated |
Each phase ends with a compendium entry (what to look for), a decision-log OBS, a harness case that can fail, a PNG opened (owner priority: visible before/after plus the click path), and a scoped commit.

## 9. Risks (honest)

- Scope: this is a product, not a feature; P0 to P3 is the usable core, P5 to P6 can wait.
- No Python or R means no arbitrary scripting; the answer is SQL + declarative cells + C stat ops. That is less flexible than a notebook with Python, and the studio must say so on its first screen.
- The picture-in-window route is unverified (section 5).
- The taskbar manager file is a shared, high-collision file; keep the change to one branch.
- A "dynamic" dashboard must not hide the data: every panel offers "open as table" and "open as SQL".

## 10. Open decisions for the owner

1. Name and wording: `data-hq`, shown as "Data Studio"; db menu row 4.
2. Notebook file format: `.pdl` (proposal) so the studio is editable by hand and by blueprintz-style tools.
3. Do human-seat games count in trends and training? (Proposal: shown in the studio; training only on a flag; `private` data never leaves the machine except to the local model.)
4. Local model for `private` data: which one on the Mac?
5. Which second provider after Teropoly (P7)?
6. Reuse the layout-studio files for saved dashboards if the format fits (to be verified), or keep notebooks as the only saved form?

## 11. External references

Repo (house root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/`): `#.desktop/livedesk_taskbar.pdl` (db menu rows), `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` (`livedesk_build_db_menu`, `livedesk:open-sql-hq`), `&.hq-apps/sql-hq/` (README, `ops/sql_hq_engine.c`, adapters), `&.hq-apps/blueprintz/`, `&.hq-apps/eden-viewer-hq/`, `&.hq-apps/csv-lab-hq/`, `&.hq-apps/chat-hai/chat-hai-design.md`, `&.widgits/file-explorer/`, `@.apps/rpg-edit-hq/ops/hai_op.c`, `@.apps/deskopoly/ops/deskopoly.c` (replay, `status`, `talk`, `ui`), `@.apps/deskopoly/layouts.pdl`. BOOK: `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D58), `06-testing/WHAT-TO-LOOK-FOR-ACCEPTANCE-COMPENDIUM-PER-PROJECT.md`; `1-1.HARNECIENT.SMOL/DAY_27_THE_DATA_STUDIO.txt` (the class that explains it). Skill `dataviz`.
