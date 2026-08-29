# #.haiku+/ — Haiku Agent Context Directory

**CORRECTED (2026-08-29 doc-audit pass, updated same day after a
follow-up instruction):** this README described a `for-user/`+`for-agent/`
split where `for-user/` held live, actively-maintained files. That's no
longer true — `for-user/` was archived earlier this pass because all 4
of its files described a pre-reorg topology (`shared-ops/`,
`pal-scripts/`, `2.muchi-verse-0.0/`) that no longer exists, and it
references files (`zest-er-summary.txt`, `!.xyzos-standards.txt`
without the `+1`) that don't exist under those names anymore. **The
`archive/` folder itself was then DELETED from disk** (direct
instruction, same day) — so `for-user/` and everything else mentioned
below as "archived" is gone entirely now, not just moved aside.
Recoverable only via `git log --diff-filter=D` in this repo. This
README is rewritten below to describe what's actually here now.

## What's actually in this directory today

- **`for-agent/`** — Haiku's still-live working files:
  `testing_methodology.txt` (current, corroborated by
  `!.TPMOS_ONBORD_BIBLE_10.md` §7) and `sonnet-handoff.txt` (a
  template, use as needed). `agent.txt`/`gotchas_by_project.txt`
  (pre-reorg paths) and `debug_findings_2026-07-21.md` (resolved
  one-time incident report) are DELETED, not just moved.
- **`30.jul-30-handoff/`** — a live, actively-cited architecture
  library, NOT a dated one-off despite the folder name. `!.xyzos-2do.txt`
  (current top-level onboarding doc) points agents to
  `1.ngn/todo-j30.txt` inside it as "the plan, do not re-derive it."
- **`tpmos-re-dox/`** — 3 files cited directly from real current source
  code (`khtpm_strip_parser.c`, `compose_rgb_frame.c`) and
  `!.HOUSE_STDS.md`.
- **`sonnet/`** — mostly current; `wildcard-completion-handoff.md` is
  DELETED (its target task was never implemented and the project has
  moved on).
- `for-user/` (whole folder), `#.temp-dox/`, `#.calendar-dump/` — all
  DELETED, no `archive/` folder exists anymore in this directory.

## Current top-level references

- `!.xyzos-2do.txt`, `!.xyzos-pitfalls+1.txt`, `!.xyzos-standards+1.txt`
  (the live standards doc — note the `+1`, the old `!.xyzos-standards.txt`
  without it doesn't exist) — all directly in `#.haiku+/`.
- The house-wide index is `1.^V-hq/INDEX.md` under
  `#.#.✅️.cal-user-sum/` — start there, not from a parent-directory
  file that no longer exists.

---

**Corrected:** 2026-08-29 (previous "Last updated: 2026-07-20" content
retired to `archive/` reasoning above; this file itself is not archived
since a directory README should describe the current directory).
