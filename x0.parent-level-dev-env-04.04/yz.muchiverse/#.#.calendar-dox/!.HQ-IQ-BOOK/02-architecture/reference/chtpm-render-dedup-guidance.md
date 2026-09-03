# chtpm_rgb_render.c / chtpm_parser_pal.c dedup — guidance for later

**Status:** NOT started. Deliberately deferred 2026-08-12 ("the chtpm
render has a few versions that may have not been up to date but may
have one or 2 unique features. however we will wait on worrying about
those"). This doc exists so a future pass doesn't have to re-derive
the investigation from scratch.

---

## What was found (2026-08-12 duplication-inventory pass)

Two files show up copy-pasted across many app/widget directories, but
**unlike** `khtpm_css_parser.{c,h}`/`stb_image_write.h` (which WERE
byte-identical and got consolidated into `&.widgits/_shared-lib/` the
same day — see that dir's own README), these two have **already
diverged per-app**:

| File | Total copies found | Distinct md5s | Largest identical cluster |
|---|---|---|---|
| `chtpm_rgb_render.c` | 22 | 16 | 16 files |
| `chtpm_parser_pal.c` | 28 | 15 | 8 files |

So roughly 6 of the 22 `chtpm_rgb_render.c` copies, and ~20 of the 28
`chtpm_parser_pal.c` copies, are **not** the same file as the majority
cluster anymore — real, intentional per-app edits, not just drift to
clean up. Naively overwriting them with one canonical version would
silently delete real features.

## Why this is a bigger job than the 3 files already fixed

The safe dedup done this session (`khtpm_css_parser.*`,
`stb_image_write.h`) worked because verification was cheap: `md5sum`
across every copy showed 100% identical, zero risk. This pair needs
actual investigation before any consolidation:

1. **Group every copy by md5**, not by filename/directory guess.
2. **For each non-majority-cluster file**, diff it against the
   majority version and characterize what actually changed — is it a
   real per-app feature (keep, don't touch) or accumulated drift/bugfix
   that should have been synced back (candidate to fold into a shared
   canonical version)?
3. Only after that triage does "shared file" vs "per-app fork" become
   a real decision instead of a guess.

## Where these files live (apps touched, as of the 2026-08-12 scan)

Re-run this to get the current list (the file tree changes over time,
don't trust a stale list):

```sh
HOUSE="<house_root>"
find "$HOUSE" -iname "chtpm_rgb_render.c" -exec md5sum {} \; | sort
find "$HOUSE" -iname "chtpm_parser_pal.c" -exec md5sum {} \; | sort
```

Known 2026-08-12 footprint (for orientation, not authoritative):
`002.zoo.../`, several `@.apps/*` pieces, `300.*-xyz` family,
`&.widgits/*` widgets, `014.wsr-pal💸️📌️+2/`, `101.mutaclsym🧟‍♂️️+18.01/`
— i.e. this spans well beyond the khtpm/livedesk/HARNECIENT scope
(`04.harnecient-fresh-install-design.md`'s own install payload is only
taskbar + tile-picker + login-signup) into other product lines
entirely. That's the other reason this is a bigger, separate
project — it's not scoped to one install target the way the 3 already-
fixed files were.

## When to actually do this

Per direct instruction: **not now — "when its more relevant."** No
trigger condition was specified; use judgment (e.g. if a real bug
shows up that's traceable to two copies drifting apart, or if a
dedicated cleanup pass gets scheduled). Don't start this speculatively.

## Pattern to follow once it's time (matches this session's approach)

1. md5-group every copy (script above).
2. Diff-triage every non-majority file — real feature vs. driftable.
3. For files that ARE safe to consolidate: same shape as
   `&.widgits/_shared-lib/` — single canonical source, each consumer's
   own `build.sh` copies it in as a build step before compiling (NOT a
   shared runtime include path — see that dir's README for why:
   independent top-level install/product units reaching across each
   other via relative paths is the `!.HOUSE_STDS.md` #20 fragility
   class, already been burned by this more than once).
4. Files with real per-app divergence: leave them alone, maybe just
   note in a comment which "family" they belong to so it's not
   mistaken for accidental drift next time someone greps for
   duplicates.
