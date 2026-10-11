# fleet / quartermaster

`ops/quartermaster.c` is the fleet budget gate from ROBOT-WORKFORCE-GAMEPLAN section 8: free quota,
CPU slots, review backlog, spawn limits. Build: `sh ops/build_quartermaster.sh` -> `ops/+x/quartermaster.+x`.

Files: `limits.pdl` (sample `LIMIT | provider | unit | max | window_seconds [| allow_unmeasured=1]`),
`fleet_ledger.txt` (append-only; created by the first `record`).
Every call takes `--ledger F --limits F --now $(date +%s)`; `--now` is required (the op never reads the clock).

## Around one attempt (runner)
    Q="quartermaster.+x --ledger fleet_ledger.txt --limits limits.pdl --now $(date +%s)"
    $Q check cpu slots 1              || wait      # exit 3 = DENY: wait, never fall back to paid
    $Q check review pending 1         || wait
    $Q check openrouter requests 1    || wait
    $Q record slot acquire G Q $$
    ... the attempt ...
    $Q record use openrouter requests 1 G Q
    $Q record slot release G Q $$
    $Q record review add ATTEMPT-ID                  # reviewer later: record review done ATTEMPT-ID
    # on HTTP 429:  $Q record exhausted openrouter free-models-per-day
Spawning: `$Q spawn-check <child_depth> <parent_children_incl_new> <total_incl_new>`.
`$Q status` lists every limit with usage, `QM|STATUS|ok|warnings=N|bad_lines=M` (warning at >= 80%).

## Output / exit
`QM|ALLOW|prov|used=U|max=M` (0), `QM|DENY|prov|reason|used=U|max=M|retry_after=S` (3), usage error (2).
Reasons: quota, unmeasured (max 0 without allow_unmeasured=1), exhausted, cpu-slots, review-backlog, no-limit-row.

## Rules
- A USE counts while `epoch > now - window`; retry_after = seconds until enough oldest USEs leave the window.
- A slot is live only if its pid still exists (kill(pid,0)); a dead pid with no release is stale.
- Ledger rows carry explicit epochs; file mtime/order is never used. Opened O_APPEND only, one write() per row.
- Bad ledger lines are skipped and counted (`bad_lines`). An EXHAUSTED row blocks until epoch+window (window 0 -> 3600s).
- Harness: `_shared-lib/harness/quartermaster.pal` (63 checks).
