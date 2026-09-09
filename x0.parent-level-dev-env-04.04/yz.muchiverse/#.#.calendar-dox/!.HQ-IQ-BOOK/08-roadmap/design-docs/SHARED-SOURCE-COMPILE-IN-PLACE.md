# Shared source: compile the canonical file, move only the binary

**Status: DONE for the khtpm shared source (2026-09-09).** Prompted by
the user: *"we compile a local version of shared code by copying it,
so someone edits the copy instead of the definitive file. Why not
compile the definitive code and just move the binary?"*

Landed:
- `build_core_render.sh`, `build_khtpm_strip.sh`,
  `tile-picker/scripts/build.sh`, `livedesk-clock/ops/build_lc_clock.sh`
  now compile `&.widgits/_shared-lib/khtpm_{css_parser.c,css_parser.h,
  render_core.c,draw_core.c}` in place via `-I`; no `cp` into `ops/`.
- Every tracked `ops/` copy `git rm`'d (incl. the fully-dead
  chat-hai/ + events-hq/ ones left from the CENTROID merge);
  `.gitignore` `**/ops/khtpm_{css_parser.c,css_parser.h,render_core.c,
  draw_core.c}`.
- `&.widgits/_shared-lib/vendor-into.sh` — the one blessed copy path,
  for the install/packaging step only.
- KPIs A–E + G verified (see §5): every affected binary rebuilds
  BYTE-IDENTICAL; zero `ops/` source churn on build; a `#warning` in
  the canonical fires during the build; `vendor-into.sh` yields a
  no-`-I` build byte-identical to the `-I` build.

Out of scope (kept as-is): `stb_image_write.h` copies (frozen vendored
3rd-party header, ~0 drift risk); `build_db_hq.sh` (already dead — refs
a nonexistent `khtpm_hq_render.c`, no callers). Follow-on: the ~26
`prisc+x.c` forks — `PRISC-X-FORK-CONSOLIDATION.md`.

---

## 1. The problem, concretely

Several build scripts **`cp` shared `.c`/`.h` source out of
`&.widgits/_shared-lib/` into a local `ops/` directory and then
compile in that directory:**

| build script | copies into its own dir |
|---|---|
| `*.monads/*.livedesk-taskbar/ops/build_core_render.sh` | `khtpm_css_parser.c/.h`, `khtpm_render_core.c`, `khtpm_draw_core.c`, `lib/stb_image_write.h` |
| `*.monads/*.livedesk-taskbar/ops/build_khtpm_strip.sh` | `khtpm_css_parser.c/.h` |
| `*.monads/*.livedesk-taskbar/ops/build_db_hq.sh` | `khtpm_css_parser.c/.h`, `khtpm_render_core.c`, `lib/stb_image_write.h` |
| `&.widgits/tile-picker` , `&.widgits/open-hai/ops` , `&.widgits/livedesk-clock/ops` build scripts | same family of files |

Result: the same file name exists as a **canonical copy** in
`_shared-lib/` **and** as N build-generated copies in consumer `ops/`
dirs:

```
&.widgits/_shared-lib/khtpm_css_parser.c        <- canonical
*.monads/*.livedesk-taskbar/ops/khtpm_css_parser.c   <- copy (git-TRACKED)
&.widgits/events-hq/ops/khtpm_css_parser.c           <- copy
&.hq-apps/chat-hai/ops/khtpm_css_parser.c            <- copy
&.widgits/livedesk-clock/ops/khtpm_css_parser.c      <- copy
… (same for khtpm_render_core.c, khtpm_draw_core.c)
```

### Why this is a real bug, not a style nit

1. **Drift has already happened.** As of 2026-09-09, `cmp` shows **6
   of the copies already differ from the canonical file**:
   - `chat-hai/ops/khtpm_css_parser.c`, `events-hq/ops/khtpm_css_parser.c`,
     `livedesk-clock/ops/khtpm_css_parser.c`
   - `chat-hai/ops/khtpm_render_core.c`, `tile-picker/ops/khtpm_render_core.c`,
     `events-hq/ops/khtpm_render_core.c`
   Whether canonical moved ahead or a copy was hand-edited, the tree
   now contains several divergent files with identical names and no
   single visibly-authoritative one.
2. **A dev edit to the wrong copy is silently discarded.** Edit
   `ops/khtpm_css_parser.c`, rebuild, the `cp` overwrites it with zero
   error. This is `03-pitfalls/HOUSE_CODE_PITFALLS.md` #1-adjacent and
   `OPERATIONAL-LANDMINES.md` #1 — and it has cost real debugging time
   more than once (an agent edited a copy, "the fix didn't work").
3. **`git status` is permanently noisy.** The strip's copy is
   git-tracked, so canonical + copy both show diffs; the other copies
   show as untracked/dirty forever. Real changes drown.
4. **`X11-HQ-APP-DESIGN-WISDOMS.md` §8 already documents the
   workaround** ("the `ops/` copies are build-generated and show up
   dirty in `git status` forever — don't stage them"). That is a note
   apologising for the antipattern, not a design.

## 2. Why it was done this way (and why it doesn't hold up)

`&.widgits/_shared-lib/README.md` gives the rationale — two real
concerns:

- **(a) Self-contained install units.**
  `xyz-installer-dev/dev-doc/04.harnecient-fresh-install-design.md`
  §5.1 copies each widget's `ops/` dir as its own subtree; a shared
  *runtime* include path would make two top-level install units reach
  across each other by relative path — the fragility class
  `!.HOUSE_STDS.md` #20 has burned this house before.
- **(b) No in-house `.h` files.** The house convention (checked against
  TPMOS `wraith-alpha/ops/*.c`) is zero in-house headers; sharing a
  struct across standalone binaries is done by **text-including a
  `.c`** (`#include "khtpm_render_core.c"`), not writing a header.

Neither concern actually requires copying source into `ops/` **at dev
build time**:

- A **compiler `-I` flag** is not a runtime cross-reference. After the
  link, the output binary is exactly as self-contained as it is today —
  the `-I` path is consulted only while `cc` runs on the dev machine.
  Concern (a) is about *shipping a tree*, not *building on a dev box*.
- **Text-include still works with `-I`.** `#include "khtpm_render_core.c"`
  resolves against `-I &.widgits/_shared-lib` just as it resolves
  against the local dir. The "no `.h`" convention is untouched — we are
  not adding a header, we are pointing the include search at the one
  real `.c`.
- The **install/packaging** need (a genuinely self-contained `ops/`
  subtree in a shipped zip) is met by doing the copy **in the
  packaging step**, against the shippable tree, once — not on every
  developer build. One script, one call site.

So: keep the *goal* (self-contained shipped subtrees), drop the
*mechanism* (copy-on-every-dev-build) that causes the drift.

## 3. The fix

### 3.1 Dev builds compile the canonical file in place
For every affected build script:
- **delete** the `cp "$SHARED"/*.c|*.h  <local>` lines;
- add `-I "$SHARED"` (and `-I "$SHARED/lib"` where `stb_image_write.h`
  is used) to the compile flags;
- on the compile line, name the **canonical** path for any file that is
  its own translation unit:
  `cc $CFLAGS -I "$SHARED" main.c "$SHARED/khtpm_css_parser.c" $LIBS -o +x/<bin>.+x`
- files that are **text-included** (`khtpm_render_core.c`,
  `khtpm_draw_core.c`) need no compile-line entry — `-I "$SHARED"` is
  enough for `#include "khtpm_render_core.c"` to find them.
- the `.o` and the final `.+x` still land in the consumer's local
  `+x/` — **only the binary is local, never the source.**

`SHARED` is already resolved in every script
(`cd .../&.widgits/_shared-lib && pwd`); no path logic changes.

### 3.2 Remove the stale copies
- `git rm` the tracked copy
  (`*.monads/*.livedesk-taskbar/ops/khtpm_css_parser.c` and `.h`, plus
  any other tracked ones the audit finds);
- delete the untracked copies from the working tree;
- add to the repo `.gitignore` (belt-and-braces so a stray future `cp`
  can't re-commit one):
  ```
  **/ops/khtpm_css_parser.c
  **/ops/khtpm_css_parser.h
  **/ops/khtpm_render_core.c
  **/ops/khtpm_draw_core.c
  **/ops/lib/stb_image_write.h
  ```
  (scoped to `ops/` so the canonical `_shared-lib/` copies are
  unaffected).

### 3.3 Packaging keeps a self-contained subtree
Add `&.widgits/_shared-lib/vendor-into.sh <target_ops_dir>` — the
single blessed place that copies the shared `.c`/`.h` into a consumer
`ops/` dir. It is called **only** by the install/zip path
(`rezip-house.sh` / the installer's tree-assembly step), never by a
`build_*.sh`. Document it in
`04.harnecient-fresh-install-design.md` §5.1 as the mechanism that
makes shipped subtrees self-contained. A shipped tree can then still
build with no `-I` (the file is sitting next to `main.c`), so nothing
downstream of packaging changes.

### 3.4 `prisc+x.c` — related, bigger, NOT in this pass
Separately, there are **~26 full copies of `prisc+x.c`** across the
house (`101.mutaclsym…/system/`, `041.pal-chain⛓️/system/`, every
pal-using project, plus session copies), with
`&.widgits/_shared-lib/system/prisc+x.c` as the intended canonical.
These are **vendored forks**, not build-copies — some have drifted,
some may carry project-specific patches (the pal VM is load-bearing
for many games). Consolidating them needs a per-fork `diff` +
classification (identical / merely-behind / genuinely-patched) +
reconciliation before any project's build can switch to
`-I "$SHARED/system"`. That is its own design doc and its own risk
budget — flagged here, deferred.

## 4. Migration steps (one commit each, rebuild + smoke-test each)

1. `build_core_render.sh` → compile-in-place; rebuild
   `khtpm_core_render.+x`; KPI set §5 A/B/C/D.
2. `build_khtpm_strip.sh` → same; `run_khtpm_strip.sh new`; strip
   renders; KPI E.
3. `build_db_hq.sh` → same; open db-hq; KPI E.
4. events-hq / chat-hai / tile-picker / livedesk-clock / open-hai build
   scripts → same, one per commit, open each window, KPI E.
5. `git rm` tracked copies + `.gitignore` block + delete untracked
   copies (after 1–4 all green).
6. `vendor-into.sh` + wire it into `rezip-house.sh` / installer; KPI G.
7. Update `_shared-lib/README.md` (the "each consumer's build.sh copies
   these" paragraph is now wrong), `X11-HQ-APP-DESIGN-WISDOMS.md` §8,
   `OPERATIONAL-LANDMINES.md` #1, and the compact doc's build section.

## 5. Testable KPIs — prove nothing broke

Run these per consumer as each build script is converted.

**A — binary parity.** Before the change: `sh build_X.sh`,
`md5sum +x/<bin>.+x` → record. After: clean `+x/`, `sh build_X.sh`,
`md5sum` again.
- For consumers whose current copy already `cmp`-equals canonical
  (the strip), the checksum **must be identical** — same bytes in,
  same bytes out.
- For the 6 already-drifted copies, expect a change; **`diff` the old
  copy vs canonical first**, confirm the delta is intended (canonical
  is newer/correct), and record the reason.

**B — zero source churn on build.** After `sh build_X.sh`:
```
git status --porcelain | grep -E 'ops/(khtpm_(css_parser|render_core|draw_core)\.|lib/stb_image_write\.h)'
```
must print **nothing**. (Today it prints several lines.)

**C — the canonical file is genuinely the one compiled.** Append a
harmless marker to `&.widgits/_shared-lib/khtpm_css_parser.c` (e.g. a
`#warning KHTPM_CANON_LIVE` or a one-off `fprintf(stderr, …)` in an
init path), rebuild consumer X, confirm the marker appears in the
build output / at runtime, then revert the marker. Proves the `-I`
path resolves to `_shared-lib/`, not a stale local file.

**D — A/B frame diff (shared-render changes).** `git stash` the
build-script change; `sh build_X.sh` (old); launch the window; capture
a frame (`dump_frame_png_op.+x <hex_win> old.png`, or read the
published `_ui.txt`). `git stash pop`; clean `+x/`; `sh build_X.sh`
(new); relaunch; capture `new.png`. `cmp old.png new.png` (or diff the
`_ui.txt`) → **identical**.

**E — every consumer builds clean and its window works.** Each of
`build_core_render.sh`, `build_khtpm_strip.sh`, `build_db_hq.sh`,
`build_events_hq_manager.sh`, chat-hai, tile-picker, livedesk-clock,
open-hai exits `0`. Then: `run_khtpm_strip.sh new` → strip renders;
open db-hq, events-hq, chat-hai, a tile picker, the clock → each maps
and shows a real frame (verify via its `_ui.txt` / one PNG dump, per
`06-testing/`). No stray manager processes after
(`ps aux | grep _manager`).

**F — drift cannot recur.**
- `git check-ignore` returns success for each formerly-copied
  `ops/…` path.
- `grep -rn "cp .*_shared-lib.*\.\(c\|h\)" **/build_*.sh **/button.sh`
  returns nothing.
- A deliberate `touch ops/khtpm_css_parser.c` then `sh build_X.sh`
  leaves no tracked change (file is git-ignored) and the build ignores
  it (canonical is what compiles).

**G — the installer/packaging path still yields a self-contained
subtree.** Run `vendor-into.sh` against a scratch copy of a consumer
`ops/` dir; confirm the shared `.c`/`.h` are now present in it; build
that scratch dir **without** `-I` → still compiles and links. Then
`rezip-house.sh` (or the installer tree step) produces a tree where
each shipped `ops/` is self-contained (spot-check with the same
no-`-I` build).

**H — house smoke test unchanged.** `button.sh reset` (or
`run_khtpm_strip.sh new`); desktop comes up; open + close 3 HQ windows
and 1 toy; no errors in the strip log; `git status` shows only
expected runtime noise.

## 6. Related

- `&.widgits/_shared-lib/README.md` (the rationale being revised).
- `*.monads/*.livedesk-taskbar/ops/build_core_render.sh`,
  `build_khtpm_strip.sh`, `build_db_hq.sh`.
- `02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md` §8 (the workaround
  note), `CENTROID_GOLD_STD.md` §2 Stage 1 (the "no in-house `.h`,
  text-include a `.c`" convention this preserves).
- `03-pitfalls/OPERATIONAL-LANDMINES.md` #1 (cp-on-build clobber).
- `xyz-installer-dev/dev-doc/04.harnecient-fresh-install-design.md`
  §5.1 (self-contained `ops/` subtrees — the goal we keep).
- `08-roadmap/OPEN-ITEMS.md` #6 (audit `khtpm_core_render.c` for inline
  loaders — same "shared file hygiene" theme).
- Follow-on: a `PRISC-X-FORK-CONSOLIDATION.md` for the ~26 `prisc+x.c`
  copies.
