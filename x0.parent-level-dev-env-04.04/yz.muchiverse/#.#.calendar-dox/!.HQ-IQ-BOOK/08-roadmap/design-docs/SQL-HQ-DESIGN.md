# sql-hq — design doc

**Status: DESIGN.** Written 2026-09-08. The user wants an in-house SQL
tool that loads `.csv` and `.pdl` files as tables and queries them with
SQL (`.sql` / `.db` files "later, if ever"), with a macro-button
sidebar UI like the SQLite web playground (`/home/no/Desktop/siql.png`).

Reference implementation the user pointed at:
`/media/no/…/♓]HALO:Pi.c]EZ.db]⚠️/2.Pi.c.ez.db]SQL♓⚠️]d5]PURE.c`
(also `+x/1.sql]Pi.c.ez.db♓⚠️]d5.+x`, `xdb/*.c` earlier drafts, a
`data.csv`, a `history/` dir). Its own README is emphatic about the
philosophy:

> "it's a SQL `./+x/MODULE.+x` (cli can be built around it) … more like
>  an NPM PACKAGE."

i.e. **the query engine is a small standalone binary; the CLI / UI /
orchestrator is a separate layer that shells out to it.** sql-hq keeps
that split.

---

## 1. What the reference `PURE.c` actually does (the baseline)

~760 lines, single file, `gcc PURE.c -o sql.+x`, **zero libraries**.
One-shot:

```
sql.+x <input.csv> "<query>" [output.csv]
```

| supported | notes |
|---|---|
| `SELECT col,… \| * FROM t [WHERE col OP val [AND\|OR …]]` | `OP` ∈ `= > < !=`; `>`/`<` numeric (`atoi`), `=`/`!=` string. Output to stdout or `output.csv`. |
| `INSERT INTO t (cols) VALUES (vals)` | **appends** a row to the CSV. |
| `UPDATE t SET col=val,… [WHERE …]` | **rewrites the CSV in place.** |
| `DELETE FROM t WHERE …` | **rewrites the CSV in place.** |

Row 0 of the file = headers. The table name in the query is ignored
(the file *is* the table). Bare tokens only — `David`, not `'David'`.

**Everything else is absent.** No JOIN, `ORDER BY`, `LIMIT/OFFSET`,
`GROUP BY`, aggregates, `DISTINCT`, `LIKE`, `BETWEEN`, `IN`, `IS NULL`,
subqueries, `CREATE/DROP/ALTER`, views, indexes, multi-file, schema
introspection, transactions, quoting, `NULL`, type coercion, aliases,
qualified `t.col`. It will always be a strict subset of SQL.

---

## 2. What the playground sidebar (and `PURE.c`) are MISSING

The `siql.png` sidebar — CREATE TABLE, DROP TABLE, INSERT INTO, UPDATE,
DELETE, SELECT, SELECT DISTINCT, LIKE, BETWEEN, MIN/MAX, COUNT/AVG/SUM,
GROUP BY, HAVING, INNER JOIN — is a **SQL-syntax cheat-sheet**, not a
**tool control panel**. It has no way to *open* anything, *see* what's
loaded, or *switch* between tables/DBs. Missing, grouped:

### A. Data source / connection — "open db" and friends
| macro | SQL / meaning | why it matters here |
|---|---|---|
| **Open / Import CSV** | load a `.csv` → a table | the whole point |
| **Import PDL** | load a `.pdl` → a table (see §5) | the whole point |
| **Open Folder** | attach every `.csv`/`.pdl` in a dir as tables | multi-table queries / JOINs need >1 table |
| **New DB** / **Save DB** / **Close DB** | an in-memory workspace holding N tables; save = write all dirty tables back to their files | the reference has no "DB", only one file |
| **Attach / Detach** | `ATTACH DATABASE 'x' AS y` / `DETACH y` | later, for real `.db` files |
| **List sources** | show attached files + their table names | orientation |

### B. Schema / navigation
| macro | SQL | note |
|---|---|---|
| **Show Tables** | `SELECT name FROM sqlite_master WHERE type='table'` / `.tables` | |
| **Describe Table** | `PRAGMA table_info(t)` / `.schema t` | column names + types |
| **Set Active Table** | *(UI only — SQL just uses `FROM t`)* | a picker so `SELECT *` / macros know the default `t`. This is the "change table" the user was reaching for. |
| **Rename Table** | `ALTER TABLE a RENAME TO b` | |
| **CREATE VIEW** / **CREATE INDEX** | | views = saved queries; indexes = perf (free with SQLite) |

### C. Common query clauses the playground skipped
`ORDER BY … ASC/DESC` · `LIMIT n [OFFSET m]` · `WHERE … IN (…)` ·
`IS NULL` / `IS NOT NULL` · `LEFT JOIN` · `UNION` / `UNION ALL` ·
`INSERT INTO t SELECT …` · `ALTER TABLE t ADD/RENAME/DROP COLUMN` ·
`CASE WHEN … THEN … END` · `CAST(x AS type)` · `COALESCE(…)` ·
`AS` aliases · qualified `t.col` · `DISTINCT` as a modifier (not only
`SELECT DISTINCT`).

### D. Session / result ops (tool-level, not SQL keywords)
| macro | note |
|---|---|
| **Run** | ✅ in the image |
| **Run Selection** | run only the highlighted statement(s) |
| **Explain** | `EXPLAIN QUERY PLAN …` |
| **Begin / Commit / Rollback** | **critical.** `PURE.c` rewrites the CSV *the instant* you hit UPDATE/DELETE — no undo. sql-hq must stage writes in the in-memory workspace and only touch files on **Commit**. |
| **Export Results** | results grid → `.csv` / `.pdl` / clipboard |
| **Query History** | the reference already keeps a `history/` dir — persist + re-run |
| **Beautify** / **Clear** | ✅ in the image |

### E. PDL specifics (new — no precedent in the reference)
The house `.pdl` is `SECTION | key | value` rows. Two real shapes:
1. **Flat config pdl** (`livedesk_taskbar.pdl`, `livedesk_theme.pdl`):
   → a 2-column table `(key, value)`, one row per `SECTION` line.
2. **Record pdl** (`clocks.pdl`: `CLOCK|<id>|scope=…|desc=…`,
   `reminders.pdl`): first token = row id, `k=v` pairs = columns →
   a real multi-column table (union of all keys seen = the schema).
The importer detects which by sniffing the first N non-comment lines
(does every row have the same `|`-field pattern with `k=v`? → record;
else → flat). Comment lines (`#…`) skipped. **Write-back** re-emits the
same shape.

---

## 3. Engine choice

Two routes:

### Route A — extend the pure-C engine (`PURE.c` lineage)
Keep hand-rolling: add ORDER BY / LIMIT / GROUP BY / aggregates / JOIN /
quoting / NULL. **Pros:** zero deps, matches the house's anti-linker
rule, fully ours, tiny. **Cons:** a real SQL engine is *thousands* of
lines and years of edge cases; it will always be a subset; JOIN +
GROUP BY + subqueries are where hand-rolled parsers go to die.

### Route B — SQLite amalgamation, in-memory (RECOMMENDED)
`sqlite3.c` + `sqlite3.h` is **one .c file, ~250 KB, public domain,
zero external deps** (libm + libpthread only — already linked
everywhere). Flow:

```
1. open  :memory:  db
2. for each loaded file:  CREATE TABLE <name>(...);  bulk INSERT parsed rows
3. run the user's query verbatim  (FULL SQL — joins, GROUP BY, CTEs, everything)
4. results -> a text grid the projector renders
5. on Commit: for each table the query dirtied, SELECT * and rewrite its
   backing .csv / .pdl
```

This is the standard "SQL over CSV" pattern (`q`, `csvsql`, DuckDB's
CSV reader all do a version of it). We get correctness for free and
spend our effort on the **adapters** (CSV/PDL ↔ rows) and the **UI**,
which is where the actual product value is. The `PURE.c` binary stays
as the no-deps fallback / the "module you can npm-install" the README
wants.

**Decision needed from the user:** A or B. The rest of this doc assumes
**B** (note where A would differ).

---

## 4. Architecture — sql-hq as an x11-hq app

Follows `CENTROID_GOLD_STD.md` + the same shape as `db-hq-pal` /
`events-hq` (static template + projector + one action script + a
manager/engine binary), rendered by the shared `khtpm_core_render.+x`.

```
&.hq-apps/sql-hq/
  sql-hq.xhtpm            static template:
                           <sidebar> = the macro buttons (§ list below)
                           <panel>   = editor region (cli_io / text_area)
                                     + toolbar row (Run / Beautify / Clear / Export)
                                     + <scrolllist> results grid (<repeat> rows)
                           <tabbar>  = one <tab> per loaded table (active-table picker)
  sql-hq.css
  ops/
    sql_hq_engine.c        Route B: bundles sqlite3.c; argv:
                             <workspace_dir> <query_file> <out_file>
                           reads workspace/manifest.pdl (which files ->
                           which tables), builds :memory:, runs, writes
                           out_file (grid) + a dirty-tables list.
    sql_hq_adapters.c      csv_load/csv_dump, pdl_load/pdl_dump (detect
                           flat vs record), shared by engine + action.
    sql_hq_projector.c     reads workspace state + last result grid,
                           writes state/ui.txt (${var} / <repeat> data)
    sql_hq_action.sh       verbs: open-csv <path> | open-pdl <path> |
                           open-folder <dir> | run | run-sel | export
                           <fmt> | set-table <name> | begin | commit |
                           rollback | macro <name>  (inserts snippet)
    build_sql_hq.sh
    +x/  (sqlite3.c vendored here, or in &.widgits/_shared-lib/sql/)
  state/
    manifest.pdl           SECTION | table:<name> | <abs file path>|csv|pdl|dirty
    query.txt              current editor buffer
    result.grid.txt        last result (TSV-ish, projector -> <repeat>)
    history/               one file per run (reference already does this)
  toy.pdl
  button.sh                launches khtpm_core_render.+x on sql-hq.xhtpm
```

**Wiring into the db cell menu:** the placeholder is already in
`livedesk_taskbar.pdl` as `db_menu_3_label | sql-hq`. When built:
```
SECTION | db_menu_3_cmd | livedesk:open-sql-hq        # + a launcher_sql row in livedesk_launchers.pdl
```
(or a bare `sh &.hq-apps/sql-hq/button.sh` — match how `db-hq` does it).

**Safety:** the workspace is in-memory / staged. Files on disk are
**only** touched by `commit`. `PURE.c`'s instant-rewrite behaviour is
explicitly not carried over.

---

## 5. The macro sidebar (proposed full list)

Grouped; clicking a macro inserts a template snippet at the cursor
(the playground's behaviour), except the ★ ones which are tool actions.

```
SOURCE            SCHEMA              QUERY                 WRITE            RESULT
★ Import CSV      ★ Show Tables       SELECT                INSERT INTO      ★ Run
★ Import PDL      ★ Describe Table    SELECT DISTINCT       INSERT … SELECT  ★ Run Selection
★ Open Folder     ★ Set Active Table  WHERE                 UPDATE           ★ Explain
★ New DB          CREATE TABLE        ORDER BY              DELETE           ★ Export CSV
★ Save DB         CREATE VIEW         LIMIT / OFFSET        ALTER TABLE      ★ Export PDL
★ Close DB        CREATE INDEX        LIKE                  ★ Begin          ★ Beautify
★ List Sources    DROP TABLE          BETWEEN              ★ Commit         ★ Clear
                  RENAME TABLE        IN (…)               ★ Rollback       ★ History
                                      IS NULL
                                      GROUP BY / HAVING
                                      COUNT / AVG / SUM
                                      MIN / MAX
                                      INNER JOIN
                                      LEFT JOIN
                                      UNION
                                      CASE WHEN
                                      CAST
```

Trim to a first-cut subset for the MVP (see step plan). The snippet
text for each lives in a `sql_hq_macros.pdl` (`SECTION | macro:select |
SELECT * FROM ${table} WHERE ;`) so grok can add/reorder without a
recompile — same pattern as the strip menus.

---

## 6. Phased plan + KPIs (grok's requested "10-step + KPIs")

| # | step | done-when (KPI) |
|---|---|---|
| 1 | **Adapters**: `sql_hq_adapters.c` — `csv_load`/`csv_dump`, `pdl_load`/`pdl_dump` (flat + record detect). Standalone, unit-tested. | round-trips `data.csv`, `livedesk_theme.pdl` (flat), `clocks.pdl` (record) byte-stable through load→dump. |
| 2 | **Engine (Route B)**: vendor `sqlite3.c`; `sql_hq_engine.c` builds `:memory:` from a manifest, runs a query file, writes a TSV grid + dirty-table list. | `SELECT * FROM data` on the CSV matches `PURE.c`; a real `INNER JOIN` across two CSVs returns correct rows. |
| 3 | **CLI orchestrator** (the reference's own next-step): `sql_hq` REPL — `\open x.csv`, type SQL, see the grid, `\commit`. No GUI yet. | 10 mixed queries (SELECT/JOIN/GROUP BY/UPDATE+commit) run in one session; committed files correct on disk. |
| 4 | **`sql-hq.xhtpm` + projector + button.sh** — static window: sidebar (subset of §5), editor `text_area`, Run, results `<repeat>` grid. `class="sql-hq database-window"`. | window opens via `button.sh`; typing a query + Run shows rows in the grid; `khtpm_png_dump` proof. |
| 5 | **`sql_hq_action.sh`** wired to every ★ macro + snippet insertion for the rest. | each sidebar button does the right thing; snippet macros land at the cursor. |
| 6 | **Active-table tabbar** — one `<tab>` per manifest table; `set-table` updates `${table}`; `Import CSV/PDL` adds a tab. | open 3 files → 3 tabs; switching changes which table `SELECT *` targets. |
| 7 | **Begin/Commit/Rollback** — staged writes; `commit` calls the adapters' dump; `rollback` discards. | UPDATE then Rollback = file unchanged; UPDATE then Commit = file changed, still valid CSV/PDL. |
| 8 | **Export + History** — grid → CSV/PDL/clipboard; every run appended to `state/history/`, re-runnable. | export produces a file another tool can re-open; history row re-runs identically. |
| 9 | **db cell wiring** — `db_menu_3_cmd` + `launcher_sql`; open from `[ ]9.db → sql-hq`. | clicking the row opens the window. |
| 10 | **Docs + polish** — `sql-hq/README.md` (user), update `pc-hq-bugs.md`-style notes, `CENTROID_GOLD_STD.md` app list; keyboard nav in the grid; error surface (bad SQL → a red message row, not a crash). | a rusty-SQL user can load `data.csv` and run SELECT/WHERE/ORDER BY/JOIN from the sidebar with no docs. |

Optional later: `.db` file support (`ATTACH`), `EXPLAIN` visualiser,
`function_bank.txt` (the reference's `sum ./+x/sum.+x` idea — only
relevant if Route A), json/xlsx→csv importers (grok note: "we can
easily turn json/xl 2 .csv").

---

## 7. Open questions for the user

1. **Route A (hand-rolled, subset, zero deps) or Route B (vendor
   sqlite3.c, full SQL)?** Recommendation: B. The `PURE.c` module stays
   either way as the dependency-free fallback.
2. **PDL write-back**: for a *flat* config pdl edited via SQL, do we
   preserve comments / row order, or is "regenerate from the table"
   acceptable? (Matters for `livedesk_taskbar.pdl` etc. — probably
   sql-hq should refuse to write those and only write pdls it created.)
3. **Scope of "table"**: a single file = one table (reference model),
   or a folder = a database of many tables? The plan assumes the
   latter (needed for JOINs).
4. **Where does sqlite3.c live** if Route B — `&.hq-apps/sql-hq/ops/` or
   a shared `&.widgits/_shared-lib/sql/`? (Shared if anything else
   might want SQL over house data — e.g. db-hq, stats-hq.)
5. First-cut sidebar: which of the §5 macros ship in the MVP (step 4)?
