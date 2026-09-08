# sql-hq

Query `.csv` and `.pdl` files with **real SQL** (joins, GROUP BY, CTEs,
window functions — the full sqlite3 grammar). `.sql` / `.db` support may
come later. Design: `08-roadmap/design-docs/SQL-HQ-DESIGN.md`.

## Status

- **Done:** the engine (`ops/sql_hq_engine.c`) + the CSV/PDL adapters
  (`ops/sql_hq_adapters.c`) + a `sqlite3`-shell-style REPL.
- **Not built yet:** the x11-hq window (`sql-hq.xhtpm` + projector +
  `sql_hq_action.sh`), the macro sidebar, the db-cell menu wiring.

## Build

```
sh ops/build_sql_hq.sh          # ~15s first time (compiles vendored sqlite3.c once)
```
Produces `ops/+x/sql_hq.+x` — one binary, no runtime deps.

## Use — command line / REPL

```
# quick one-file query
ops/+x/sql_hq.+x data.csv "SELECT name, age FROM data WHERE age >= 25 ORDER BY age DESC"

# interactive REPL (like the sqlite3 shell, but over csv/pdl)
ops/+x/sql_hq.+x repl
  sql-hq> .open people.csv
  sql-hq> .open cities.csv
  sql-hq> .tables
  sql-hq> SELECT p.name, c.country FROM people p JOIN cities c ON p.city = c.city;
  sql-hq> UPDATE people SET age = 26 WHERE name = 'Alice';
  sql-hq> .commit          -- writes changed tables back to their files
  sql-hq> .quit

# one-shot from a workspace manifest (what the GUI window uses)
ops/+x/sql_hq.+x run   <workspace_dir> <query_file> [out_grid_file]
ops/+x/sql_hq.+x commit <workspace_dir>
```

### REPL dot-commands
| cmd | |
|---|---|
| `.open <file> [as name]` | load a `.csv` / `.pdl` as a table |
| `.tables` | list loaded tables (marks `*dirty*`) |
| `.schema [table]` | show `CREATE TABLE` |
| `.commit` / `.rollback` | write dirty tables to disk / reload from disk |
| `.save <table> [to path]` | dump one table now |
| `.grid on\|off` | aligned grid vs raw TSV |
| `.quit` | |

## How files map to tables

| file | table shape |
|---|---|
| `x.csv` | first line = column names; rows below. (MVP splitter: no RFC-4180 quoted commas yet.) |
| flat `x.pdl` (`SECTION\|key\|value`, `COLOR\|bg\|#111`) | 3 columns `(tag, key, value)`, one row per line |
| record `x.pdl` (`CLOCK\|id\|scope=…\|desc=…`) | columns `_tag, _id`, then the union of every `k=` key |

`#` comments and blank lines are dropped and **not preserved** on
write-back — so only ever `.commit` a pdl sql-hq created, never a
hand-authored config file. Writes are staged in memory; disk is only
touched by `.commit` (unlike the reference `PURE.c`, which rewrote
files instantly).

## Safety

- In-memory sqlite3 (`:memory:`); source files untouched until `.commit`.
- `.commit` writes atomically (tmp file + `rename`).
