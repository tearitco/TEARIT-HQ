# install_ledger_op - spec (locked; the cases in `_shared-lib/harness/cases/install_ledger.pdl` are the judge)

One small C program, `install-harness/ops/install_ledger_op.c`, std C11 + POSIX, no shell, no network, no absolute paths, builds with `gcc -std=c11 -O2 -w -o <out> install_ledger_op.c`. Part of the labeled-install harness (`MACHINE-USERS-SCHOOLS-AND-FARM-ANIMATION-PLAN-2026-10-08.md` section 9). A label is `<name>-v<N>` (e.g. `mac-v3`); it is the product name given to `install.sh`.

Ledger file: append-only, one row per line, fields split by `|`:
- `INSTALL|<label>|<host>|<epoch seconds>|<payload_ref>|<result>`
- `REMOVE|<label>|<epoch seconds>`
Hosts file (data, hand-edited): `HOST|<name>|<target>` rows; `#` lines and blank lines ignored.

Commands (argv[1]); output on stdout; exit 0 ok, 1 refused (rule broken, not found), 2 usage or bad input:
1. `next <ledger> <name>` prints `<name>-v<N>` where N = 1 + the highest N among INSTALL rows whose label is `<name>-v<digits>` (REMOVE rows never free a number). Missing or empty ledger prints `<name>-v1`. Never writes.
2. `append <ledger> INSTALL <label> <host> <payload_ref> <result>` appends an INSTALL row with the current time. Refuses (exit 1, nothing written) if the label already has an INSTALL row (removed or not). Creates the file if missing.
3. `append <ledger> REMOVE <label>` appends a REMOVE row. Refuses (exit 1, nothing written) if the label has no INSTALL row or already has a REMOVE row.
4. `list <ledger>` prints one line per label in first-install order: `<label> <host> <result> <state>` where state is `kept` or `removed`. Missing ledger prints nothing, exit 0.
5. `resolve <hosts> <name>` prints the target of the `HOST|<name>|...` row (first match). Not found: print nothing, exit 1.
Input rules (exit 2, nothing written): a label must match `[a-z][a-z0-9]*-v[1-9][0-9]*`; no argument may contain `|`, a newline, or be empty; wrong argument count; unknown command.
Other rules: never modify an existing line (append only); a ledger line that does not parse is skipped, never fatal.
