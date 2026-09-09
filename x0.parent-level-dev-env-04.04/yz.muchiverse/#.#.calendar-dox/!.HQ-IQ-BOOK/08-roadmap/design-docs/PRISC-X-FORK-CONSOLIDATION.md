# prisc+x.c fork consolidation

**Status: Phase A done; Phase B ~done for the safe cluster (2026-09-09).**
`PRISC-X-FORK-CLASSIFICATION.md`. Prompted by the user: *"why so many
prisc+x forks? Fix that first... does it have KPIs for completion?"*

Phase B landed (each: `scripts/build.sh` compiles
`&.widgits/_shared-lib/system/prisc+x.c` via a walk-up
`PRISC_CANON_SHARED_LIB` resolver; vendored `system/prisc+x.c` `git
rm`'d + `.gitignore`d `**/system/prisc+x.c` with a
`!**/_shared-lib/...` exemption):
- **15 projects converted + A/B-verified** (`aa527edf`, `7fd2a427`):
  `041.pal-forum`, `041.pal-chain`, `044.pal-chat-irc`,
  `045.muchi-pal-agent`, `101.mutaclsym19.00` (muta-neo),
  `101.mutaclsym+18.0G`, `102.editor-00.00`, `0.user-pal/00.login-signup`,
  `0.user-pal/01.avatar-creation`, `002.zoo/…INK…PEN`, `*.START_BUTTON`,
  `@.apps/my-biotech`, `@.apps/my-chara-txt`, `@.apps/my-lawyer`,
  `@.apps/myne-qrypto/qtc`. Per project: every `.pal` in it run through
  the OLD fork binary vs the NEW canonical binary (stdout+stderr) —
  identical; rebuilds green via its own `scripts/build.sh`.
- **~11 projects were already on `$_SS = &.widgits/_shared-lib`**
  (civ-txt, tactics-txt, piececraft-hq/-xyz, aomorai-editor, yahoo-app/
  -broker, TSC_ELO, agy-txt, rpg-xyz, rtp-xyz) — untouched.
- **Deferred:** `014.wsr-pal` + `01.muchi-pals-egg` — fold their
  `#ifdef _WIN32` shims into the canonical first (see
  `PRISC-X-FORK-CLASSIFICATION.md`). `101.ledger-player-npc-simple+3` +
  `101.lpns+map+4` — no `scripts/build.sh`; prisc is built/spawned by
  their own `system/orchestrator.c` (TPMOS-style), a separate riskier
  change. `&.widgits/board-viewer/system/prisc+x` — binary only.
- **Not in scope of this pass:** registering the spawned prisc VM in the
  master-ledger. That's `chtpm_parser_pal.c`'s `<module>` launch (the
  legacy parser), broader blast radius — its own follow-up
  (PROC-LIFECYCLE §5).

TL;DR after reading every diff: it is the biggest **surface**
(≈20 projects) but **not** the hardest problem — the
`_shared-lib/system/prisc+x.c` canonical (1408 L) is genuinely the
**newest, most complete** VM; every fork is *behind* it (the 10-project
`30ab13606d3c` cluster even carries a real `original[128]`
path-truncation bug the canonical fixed). So this is *upgrade-everyone*,
not *merge-N-divergent-VMs*. Two forks (`wsr`, `egg`) additionally
carry `#ifdef _WIN32` shims to fold into the canonical first.

**Real blocker for Phase B:** nothing builds the canonical today, so
the first project switch is also the first live exercise of ~250 lines
of canonical-only VM code. Phase B step 1 = build the canonical
standalone and `diff` its output against each project's current binary
over a real `.pal` corpus, BEFORE editing any `build.sh`. That plus the
per-project pal-script smoke matrix makes Phase B a focused follow-up
effort, not a quick change.

Sequencing (unchanged): `SHARED-SOURCE-COMPILE-IN-PLACE.md` **done**
first (built the compile-in-place + `vendor-into.sh` mechanism this
reuses); proc-lifecycle wiring **done**; then Phase B here.

---

## 1. What `prisc+x` is

`prisc+x` (a.k.a. "prisc") is **the PAL virtual machine** — a small
RISC-V-flavoured bytecode interpreter (`regs[0]` hardwired to 0, integer
+ string register files, opcodes for file/history/env/format ops). Every
`.pal` script in the house runs on it: mutaclysm, piececraft-xyz/-hq,
board-viewer, `041.pal-*` (chain/forum/chat-irc), `@.apps/*` toys
(civ-txt, tactics-txt, my-lawyer, my-biotech, aomorai-editor, …),
`0.user-pal`, editors, TSOTS. It is **not** a fork of TPMOS code — the
file's own header says so: *"this file is OUR OWN VM — genuine fixes
belong here directly."*

## 2. Why there are so many copies

`find` shows **~26 `prisc+x.c` files** under `44.xyz.01.00/` (excluding
`pieces/sessions/` runtime copies), in **7 distinct content variants**:

| md5 (short) | lines | # copies | representative |
|---|---|---|---|
| canonical `_shared-lib/system/` | **1408** | 1 | the source of truth |
| `30ab13606d3c` | 1094 | **10** | `041.pal-chain⛓️`, pal-forum, pal-chat-irc, editor, user-pal ×2, my-biotech, my-lawyer, myne-qrypto, START_BUTTON, zoo |
| `a2c7e1c2675a` | 1176 | 2 | `101.mutaclsym…19.00` (muta-neo), `@.apps/my-chara-txt` |
| `d333b84ff495` | 1119 | 2 | `045.muchi-pal-agent`, `101.ledger-player-npc-simple` |
| `cd840d0777a2` | 1130 | 1 | `101.lpns+map+4` |
| `580b1196c144` | 1228 | 1 | `101.mutaclsym…+18.0G` (old muta) |
| `d40e3b9e292a` | 1255 | 1 | `014.wsr-pal💸️📌️+2` |
| `676f5a72e0b1` | 1133 | 1 | `01.muchi-pals-🥚️-13.01` |

Plus `&.widgits/board-viewer/system/prisc+x` — a **binary only**, no
`.c` beside it, copied from `014.wsr-pal` at some point.

**How it got this way:**
1. **House convention: self-contained subtrees.** Every pal-using
   project vendors its whole `system/` toolkit (`prisc+x.c`,
   `chtpm_parser_pal.c`, …) with **zero shared runtime include paths**,
   so the project can be zipped / installed / shared standalone
   (`04.harnecient-fresh-install-design.md` §5.1). prisc is required by
   all of them.
2. **New projects are bootstrapped by copying an existing project's
   `system/` dir wholesale** — each new project snapshots whatever
   `prisc+x.c` version its source project had that day.
3. **A real consolidation happened once** — the header comment:
   *"moved here after confirming byte-identical copies across all 4 pal
   projects"* — into an old `2.muchi-verse/shared-ops/` path. After the
   2026-09-01 house restructure the canonical landed in
   `&.widgits/_shared-lib/system/` and **kept receiving VM fixes**
   (string-register subsystem, the 3-arg `read_history`, `sreg_idx`,
   `OP_BNE`, ~20 `OP_S*` string ops), growing to 1408 lines — while
   **no build script ever pulls from it.** Every project still compiles
   its own vendored `system/prisc+x.c`, so the copies fossilised at 7
   different points in the same lineage.

## 3. The key finding — canonical is a SUPERSET

Opcode inventory, canonical vs each fork variant:

| fork variant | opcodes in the fork but **not** in canonical |
|---|---|
| `30ab13606d3c` (the 10-cluster) | **none** — canonical is a strict superset |
| `a2c7e1c2675a` (muta-neo, my-chara-txt) | **none** |
| `d333b84ff495` | **none** |
| `cd840d0777a2` | **none** |
| `580b1196c144` (old muta) | **none** |
| `676f5a72e0b1` (muchi-pals-egg) | **none** |
| `d40e3b9e292a` (wsr-pal) | `OP_READ_HISTORY_`, `OP_READ_HISTORY_STR` — almost certainly an **earlier name** for the canonical's `read_history <path> xD, xS` 3-arg form (header comment describes exactly that work landing properly); needs a 5-minute confirm, not a merge |

Every fork's diff-vs-canonical (252–649 lines) is **real code, not
whitespace** (`diff -w` barely shrinks it) — but it is all *canonical
has more*, not *fork has different*. So:

**This is "upgrade every project to the canonical VM", not "reconcile
divergent VMs".** Risk is dominated by (a) semantics of an *existing*
opcode changing between a fork and canonical, and (b) a project's own
`.pal` scripts depending on old behaviour — both checkable per project,
not open-ended.

## 4. Why do `SHARED-SOURCE-COMPILE-IN-PLACE.md` first

The clean end state here is: **every project's `build.sh` compiles
`"$SHARED/system/prisc+x.c"` with `-I "$SHARED/system"`, keeping only
its binary local** — and the packaging step vendors a real copy into
each shipped `system/` subtree. That is *exactly* the mechanism
`SHARED-SOURCE-COMPILE-IN-PLACE.md` builds (compile canonical in place,
`vendor-into.sh` at package time). Land that on the 3–5 khtpm build
scripts first — small, low-risk, low blast radius — and this becomes
"apply the same proven pattern to one more file across ~20 projects",
not "invent the pattern under maximum blast radius."

Recommended order overall: (1) shared-source compile-in-place, (2)
proc-lifecycle wiring, (3) **this**.

## 5. Plan

### Phase A — classify (read-only, can start now, parallel with #1)
- For each of the 7 variants: full `diff` vs canonical, bucket every
  hunk as **behind** (canonical has it, fork doesn't),
  **cosmetic** (comments/formatting), or **genuinely divergent**
  (fork changed an existing opcode's behaviour / added a real op).
  Expectation from §3: almost everything is "behind", a handful
  cosmetic, ~0 genuinely divergent.
- Resolve the wsr-pal `OP_READ_HISTORY_*` question (rename vs real).
- Grep each project's `.pal` scripts for opcodes/mnemonics whose parse
  or semantics changed between its fork and canonical (esp.
  `read_history`, any `OP_S*`, `regs[0]` assumptions). Produce a
  per-project "scripts touched / not touched" list.
- Output: `PRISC-X-FORK-CLASSIFICATION.md` — a table of
  project → variant → verdict (`clean upgrade` / `upgrade + watch op X`
  / `keep forked because …`).

### Phase B — the easy 15 (one project per commit)
The 10-cluster + the 2×`a2c7e1c2` + the 2×`d333b84f` + `cd840d07` +
`580b1196` = 16 projects that are a clean "behind → upgrade":
1. Point that project's `build.sh` at `"$SHARED/system/prisc+x.c"`
   (`-I "$SHARED/system"`); delete its vendored `system/prisc+x.c`
   (`git rm` if tracked), `.gitignore` `**/system/prisc+x.c` scoped
   away from `_shared-lib`.
2. Rebuild; run the project; drive its own `.pal` scripts through a
   representative flow (each project has a `button.sh`/harness).
3. KPI set §6 A–D for that project.

### Phase C — the divergent singletons
`014.wsr-pal` (521), `01.muchi-pals-egg` (649). After Phase A's verdict:
either the same clean upgrade, or — if a real per-project op/behaviour
is found — **fold that op into the canonical** (the header's own rule:
"genuine fixes belong here directly"), then upgrade. Never leave a
project on a fork "because it's different" without a one-line reason
recorded in `PRISC-X-FORK-CLASSIFICATION.md`.

### Phase D — board-viewer's binary
`&.widgits/board-viewer/system/prisc+x` (binary, no source). Give it a
`build.sh` that compiles the canonical, or have board-viewer's existing
build produce it. Then it rides the same source as everyone else.

### Phase E — cleanup + docs
- One `_shared-lib/system/build_prisc_x.sh` (canonical build recipe;
  flags, `-lm`, `_GNU_SOURCE`) that every project's `build.sh` calls or
  copies its compile line from.
- Update `_shared-lib/README.md`, `TWO-PARSER-FAMILIES.md`, the compact
  doc's "prisc+x pal VM — ~10 drifted copies" line, and
  `prisc-x-popen-custom-op-freeze` memory (the fork+exec fix must land
  in the canonical, not per-fork).

## 6. Testable KPIs for completion

### Per project (run during Phase B/C)
- **A — opcode parity.** `nm`/`strings` or a
  `prisc+x --dump-opcodes` (add a tiny debug flag if absent): the
  post-switch binary exposes **every** opcode the old vendored binary
  did (superset is fine, missing one is a fail).
- **B — script behaviour parity (the real test).** Pick the project's
  representative `.pal` flow (its `button.sh` demo, or its harness).
  Capture the project's own output artifacts — published state files
  / frame text / ledger lines — with the **old** binary, then with the
  **canonical**. `diff` → identical (or every difference explained and
  intended, e.g. a bug the canonical fixes).
- **C — no crash / no hang.** The project runs its flow to completion;
  no non-zero exit, no `wchan=pipe_read` freeze (the custom-op-exec
  freeze class — `prisc-x-popen-custom-op-freeze`), no new stderr.
- **D — source is gone, build is clean.** The project's
  `system/prisc+x.c` no longer exists / is git-ignored;
  `git status` shows no `prisc+x.c` churn after a build; the
  `build.sh` names `"$SHARED/system/prisc+x.c"`.

### Whole-effort (done-when)
- **E — one source.** `find 44.xyz.01.00 -name 'prisc+x.c' -not -path
  '*/_shared-lib/*' -not -path '*/pieces/sessions/*'` returns
  **nothing**.
- **F — one build recipe.** Every `build.sh`/`button.sh` that produces
  a `prisc+x` binary compiles `"$SHARED/system/prisc+x.c"`; a
  `grep -rn "prisc+x" **/*.sh` audit finds no local-copy compile.
- **G — house smoke.** `button.sh reset`; then run, in turn:
  mutaclysm, piececraft-hq, a board-viewer session, `041.pal-chain`
  (mine + send + history), `041.pal-chat-irc` (2-instance P2P),
  `041.pal-forum`, one `@.apps/*` pal toy (civ-txt), one `0.user-pal`
  flow — each reaches its normal first interactive state and completes
  one real action. Zero regressions vs a pre-consolidation capture of
  the same runs.
- **H — a VM fix propagates for free.** Land a one-line no-op marker
  (`fprintf(stderr,"PRISC_CANON\n")` in VM init) in
  `_shared-lib/system/prisc+x.c`; rebuild any two projects; the marker
  appears for both; revert. Proves there is genuinely one source now.
- **I — packaging still self-contained.** After `vendor-into.sh` runs
  in the package/zip path, a shipped project's `system/` contains a
  real `prisc+x.c` and builds with no `-I`.
- **J — classification recorded.** `PRISC-X-FORK-CLASSIFICATION.md`
  exists with a verdict line per project; any project deliberately
  left on a fork names the reason and an owner.

## 7. Related
- `08-roadmap/design-docs/SHARED-SOURCE-COMPILE-IN-PLACE.md` (the
  prerequisite mechanism).
- `&.widgits/_shared-lib/system/prisc+x.c` (canonical; read its header).
- `&.widgits/_shared-lib/README.md`,
  `02-architecture/TWO-PARSER-FAMILIES.md`.
- `03-pitfalls/` + the `prisc-x-popen-custom-op-freeze` auto-memory —
  the fork+exec/watchdog custom-op fix must land in the canonical.
- `04.harnecient-fresh-install-design.md` §5.1 (self-contained
  subtrees — the goal kept).
- `08-roadmap/OPEN-ITEMS.md` #18 (parent), this becomes #19.
