# compact-store-install.md — taskbar Store tab + GitHub-backed toy install (planned, not built)

Quick orientation for the upcoming `tb` Store tab work. Real docs
exist already — this is a compact pointer + the one real decision
made 2026-09-17, not a replacement for them.

## What's being built (owner's own framing, 2026-09-17)

The taskbar's **Store** header cell (`strip-cell-13`, real label
"store" — currently a genuinely inert cell, confirmed live during
today's taskbar-numbering bug fix, no menu builder dispatches there
yet) becomes a real catalog: every listed item shows a price
("free" or a real price), and picking one downloads the app from
GitHub and installs it into the user's house under the right real
location:

- **toys, plugins** — most items land here (`&.hq-apps/`, `@.apps/`,
  `&.widgits/` — wherever a real `toy.pdl` already puts things, see
  `create-package.sh`'s own 4-root scan for the real, current
  candidate-location convention).
- **palettes** — installed palettes go into the real palettes tree,
  not toys.
- **entire files/desks** — sometimes a store item is a whole
  project (pc-hq's own "file=dir, desk=map" convention), not a single
  app.
- Newly installed toys/palettes start **empty** (no bundled sample
  content) — the store item IS the content.

## Real decision made 2026-09-17

**Each store item is its own individual GitHub repo, under the
`tearit-co` org, for now.** Not one central catalog repo listing
download locations per item — literal `git clone`/`pull` per item.
Recorded in `07-install-and-ship/PHONDO_INSTALL_IDEAS.md` §6 item 3
(that doc's own open-question list) — read that doc's full §3/§4/§5
before building anything, it has the real surrounding context this
file deliberately doesn't repeat.

"For now" is load-bearing — this is the real starting shape, not a
permanent architecture lock-in. "Just document for now" — direct
instruction, 2026-09-17: no code yet, this doc + the
PHONDO_INSTALL_IDEAS.md update are the whole deliverable today.

## The honest gap, before anyone starts building

Per `PHONDO_INSTALL_IDEAS.md` §3/§4 (verified current as of that
doc's own last real read-through): **this house has zero network
transport today.** Every real, working install/versioning mechanism
(gitlet, install v1) is explicitly local-path-only by design. Wiring
Store → real GitHub clone/pull is the first time this house would
talk to the actual internet as a product feature. Budget for that
being a materially bigger task than "add a taskbar cell."

Real, already-adjacent infrastructure worth reusing, not
reinventing:
- `&.hq-apps/create-package/create-package.sh` — already does the
  "package a real house app into an independent, runnable tree"
  half (though it packages the OWNER's own local house content, not
  a third-party download) — its `MANIFEST.txt`/`reintegrate.sh`
  pattern (tagged PRIMARY/DEP/BASELINE, safe non-destructive sync) is
  a real, tested precedent for "get a whole app + its deps into a
  self-contained tree," the same shape a Store install ultimately
  needs in reverse (network repo → local house, not local house →
  package).
- `toy.pdl`'s own real, already-declared identity file convention
  (`title`/`launch`/`deps`) — a downloaded store item should almost
  certainly ship with one, so it's real toy-scan-discoverable the
  moment it lands, no separate registration step.

## Still-open, from PHONDO_INSTALL_IDEAS.md §6 (not yet answered)

- Who's the real target user for the first working version — owner
  testing on a second machine, a friend, or a stranger off GitHub?
  (Testing proceeds in that order regardless, per §5's own recorded
  decision — but the FIRST build target still needs picking.)
- What does "approved" mean concretely — manual review, an automated
  KPI-harness check, both, or anything-goes for the alpha channel?

## If you're picking this up

1. Read `07-install-and-ship/PHONDO_INSTALL_IDEAS.md` in full first —
   this file is a pointer, not a substitute.
2. Read `07-install-and-ship/USER-JOURNEY-COMPLETION-GRAPH.md` §7/§8
   for the real, current completion-graph status of install+store
   together (they share infrastructure, per that doc's own finding).
3. The real taskbar-side starting point: `khtpm_taskbar_manager.c`'s
   `ktb_hq_open()` dispatch chain has no `which==13`/store branch at
   all yet — that's the real, first, small, provable wiring step
   (an empty or stub menu is enough to prove the cell isn't inert
   anymore), matching this same session's own live-verified
   `ktb_cell_id()`-first pattern (see `08-roadmap/OPEN-ITEMS.md` and
   today's `04-bugs/bug_bounty.md` CLOSED entry for the real,
   current convention to build any new cell's dispatch against).
