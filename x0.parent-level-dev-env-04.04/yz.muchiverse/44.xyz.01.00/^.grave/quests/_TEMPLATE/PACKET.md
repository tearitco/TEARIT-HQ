# Quest Packet (what a delegable quest folder holds)

Design: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/ROBOT-WORKFORCE-GAMEPLAN-AND-PLAYBOOK.md` section 3.

```
^.grave/quests/Q0nn-<slug>/
  QUEST.md       status header, prompt, onboarding, log
  scope.txt      the ONLY paths a worker may write (one glob row each)
  harness/       the judge: <name>.pal + cases/<name>.pdl
  LOCK.sha256    sha256 of every harness file, written BEFORE assignment
  reference/     optional known-good solution, sealed from the worker
  budget.pdl     max attempts, max wall seconds, max free-quota units, escalate_to
  attempts/001/  each try (append-only; numbered without gaps)
```

Formats:
- `scope.txt`: `#` comments, blank ok; `dir/` = everything under it, trailing `*` = prefix, else exact path. Paths are repo-relative.
- `LOCK.sha256`: sha256sum format, paths relative to `harness/`: `cd harness && find . -type f | sort | xargs sha256sum`.
- `budget.pdl`: rows `BUDGET | max_attempts | 3`, `max_seconds`, `max_quota_units` (positive integers), `escalate_to | <word>`.

Run the gate (build once with `sh '&.widgits/quest-pilot/ops/build_quest_check.sh'`):

    '&.widgits/quest-pilot/ops/+x/quest_check.+x' <quest_dir> <worktree_dir> <target_branch_ref>

Prints `QCHECK|PASS/FAIL|<check>|<detail>` per check and `QCHECK|VERDICT|...|checks=N|failed=M`.
Exit 0 = valid; else a bitmask: 1 LOCK, 2 SCOPE, 4 BASE (stale), 8 BUDGET, 16 ATTEMPTS.
The packet's harness/, LOCK.sha256 and scope.txt are always out of scope for a worker diff.
Harness for the checker itself: `&.widgits/_shared-lib/harness/quest_q011_quest_check.pal`.
