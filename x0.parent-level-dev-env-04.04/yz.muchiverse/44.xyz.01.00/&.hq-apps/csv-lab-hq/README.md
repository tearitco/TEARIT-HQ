# csv-lab-hq

X11-HQ window "CSV Lab": a LIVE view of the CSV / Chinese translation pipeline. Top: run id, bank,
stage and a progress bar (done/total), then three quota bars (Groq requests, Groq tokens/min, Mac
calls). Left sidebar: a bank tab per distinct bank in the feed plus a status filter (ALL / needs
review = lint_fail + low_score + translated-and-not-yet-reviewed / accepted / rejected). Main
scrolling list: one row per JOB (`English -> 中文 -> pinyin -> score -> status`), colored by status
(lint_fail red, low_score amber, translated blue, ok/accepted calm green, rejected grey, new dim).
Click a row and it opens as a card with `[ ACCEPT ]` / `[ REJECT ]` rows; click it again to collapse.
Below the quota bars: the LOG pane (last 6 feed lines, **newest at the bottom**), then a `cmd>` cli_io.

Shape = `knowledge-hq`: `csv-lab-hq.xhtpm` + `.css` + ONE manager (`ops/csv_lab_manager.c`, built by
`ops/build_csv_lab_manager.sh` into git-ignored `ops/+x/`). No renderer or shared khtpm change.
Launch: `sh open_csv_lab_hq.sh <house_root>` (no taskbar shim; shared taskbar files are off limits).

## Files (paths in `csv_lab_config.pdl`, relative to the house root; defaults shown)
| role | default | access |
|---|---|---|
| feed | `&.widgits/concept-bank/state/csv_lab_ui.txt` | read only, written by the pipeline |
| review | `&.widgits/concept-bank/state/csv_lab_review.txt` | append only, `REVIEW\|<bank>\|<id>\|accept` or `...\|reject` (exactly 4 fields, no timestamp) |
| cmd | `&.widgits/concept-bank/state/csv_lab_cmd.txt` | append only, `CMD\|<text>` per line typed in the cli_io (a `\|` in the text becomes a space); the pipeline reads it later |

## Feed format ('|' separated)
```
HEAD|<run_id>|<bank>|<stage>|<done>|<total>        stage: extract|batch|translate|lint|score|merge|done|error
QUOTA|groq_req|<used>|<limit>   QUOTA|groq_tok_min|<used>|<limit>   QUOTA|mac_calls|<n>|0
JOB|<bank>|<id>|<en>|<zh>|<pinyin>|<score 1-5 or ->|<status>   status: new|translated|lint_fail|low_score|ok|accepted|rejected
LOG|<HH:MM:SS>|<short line>                         last 40 kept, last 6 shown
```
A row that does not parse (field count, non-numeric count, score outside 1-5/`-`, unknown status,
unknown type) is skipped and counted in the status line (`skipped bad rows: N`). A later JOB row with
the same bank+id replaces the earlier one. The owner's review outranks the pipeline's status in the
list (an accepted job leaves "needs review"). No feed file = "waiting for pipeline".

## Behavior
- Change detection: content hash of the feed + size of the review/cmd files, polled at 10 Hz; never st_mtime.
- Feed-driven republishes of `csv_lab_ui.txt` (the `vars=` file) happen at most every 500 ms; clicks publish at once;
  a republish whose text equals the file on disk is skipped (`--once` prints `ui=written` / `ui=unchanged`).
- Verbs (`csv_lab_action.txt`, `seq=<n>` / `cmd=<VERB[:arg]>`): `SEL:<n>`, `BANK:<n>` (0 = ALL), `FILTER:<f>`, `ACCEPT`, `REJECT`, `CMD:<text>`, `RELOAD`.

## CJK font
`csv-lab-hq.css` sets `font-family: Noto Sans CJK SC` (`fc-list :lang=zh` here lists NotoSansCJK-*.ttc, NotoSerifCJK-*.ttc and
AR PL UMing). DejaVu Sans has no Han glyphs, so on a machine without a CJK font the Chinese shows as boxes.
The manager passes UTF-8 bytes through untouched (harness-checked).

## Limits
- The log pane is fixed rows (6), not scrollable; the renderer lays one `<scrolllist>` per panel, and it is the job list.
- Accept/Reject buttons are rows inside the job list (in the opened card), not fixed buttons.
- Sized with the `ui` unit (window 1280x800, sidebar 170); font sizes are raw px.
- The cli_io only records commands (`CMD|...`) and shows `last command: ... [queued]`; nothing consumes them yet.

## Harness
`_shared-lib/harness/csv_lab_hq.pal` + `cases/csv_lab_hq.pdl` + `fixtures/csv_lab_feed_fixture.txt` (45 JOBs, 3 banks, CJK rows, a
lint_fail, a low_score, QUOTA, LOG): parse of every row type, malformed rows, filters, 4-field review rows, idempotent republish,
waiting state, CJK bytes, a daemon section (republish on feed/review growth + action file) and six source mutants that must be detected.
Run from the harness folder with a built prisc: `gcc -O2 -w -o /tmp/prisc_csv ../system/prisc+x.c -lm`, delete
`results/csv_lab_hq.txt*`, `/tmp/prisc_csv csv_lab_hq.pal`, read `results/csv_lab_hq.txt.verdict.txt`.
