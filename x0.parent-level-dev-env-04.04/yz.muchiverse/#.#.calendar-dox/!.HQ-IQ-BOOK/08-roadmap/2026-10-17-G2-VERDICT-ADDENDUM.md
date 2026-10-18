# 2026-10-17 — G2 verdict addendum

Companion to `2026-10-14-G1-SPRINT-VERDICT.md`. Three days on: **G2.1 and
G2.2 are closed**, `origin/claude` is merged, and the desk runs the merged
tree. Full measured record in
`design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md` §9.12–9.14.
 — G2 closed, and the advice above got sharper

Three days on. **G2.1 and G2.2 are closed** (`100 %`, wired into
`nb_all_tests.sh`), `origin/claude` is merged, and the desk runs the merged
tree. Full measured record in
`design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md` §9.12–9.14.

**The standing advice above was right, and it was not strong enough.**

It said: *spend one cheap measurement before concluding why something fails.*
Correct — but the bigger lesson was not "measure before theorising", it was
**that the measuring device itself was the bug, repeatedly**:

- `nb_gbcr_test.sh` reported a confident `0/12`. It had been comparing the
  browser **chrome** (7 nav buttons + 5 tabs = also exactly 12 rows), not the
  content. The count matched by coincidence.
- `frame_geom.py`'s stdout truncates the class column to 18 chars, so
  `from-top,nb-content` could **never** be matched as a substring.
- `browser_pid.sh` printed every pid instead of one, so callers built relay
  paths like `.../100153 2367044.txt`. And `--require-one` counted **lines**
  in a one-line string — a guard that *cannot fail*.
- `xclipget.py` returns whatever the clipboard holds; read right after Ctrl+C
  it can succeed with the **previous** owner's bytes.
- My own gutter negative control was an assignment the next line overwrote —
  a dead statement, so it broke nothing and "passed".

Four of those were caught **only** by negative-testing, and one by diffing a
passing run against a failing run and finding the traces **byte-identical** —
which eliminated the entire suspected subsystem in one step.

So the sharpened form, and it is the thing I would hand a new lane:

> **A green number here has not been measured until you have shown it can go
> red, and shown that the device taking it is looking at the thing you think
> it is looking at.** Where a subsystem is suspected, instrument it and diff a
> passing run against a failing one before forming a theory — if the traces
> match, the bug is not in the instrumented code.

Two things also worth carrying forward because they cost real time:

- **Do not trust a count read off a live or conflicted index.** "17 files
  under `xyzfs/users`" came from `ls-files` mid-merge; the truth from
  `ls-tree` on a commit was **1,749**. The bad instrument nearly caused the
  wrong call on the merge.
- **Prove the recovery path before taking the risk, not after.** The
  `xyzfs/users` fast-forward really did delete 1,741 files and 18 wallets from
  the live house. Nothing was lost only because the restore had been dry-run
  and hashed against a pre-reset tarball beforehand. A restore tested after the
  fact is a hope.

Open and unowned: `nb_span_drag_test.sh` still fails ~60 % of runs at T2 — the
drag never produces a highlight band, which is the mouse-motion path
(`khtpm_core_render.c:15597`), not the keyboard path fixed in §9.13.
