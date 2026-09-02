# State & PDL conventions

*Condensed from `HOUSE_FAQ.md` "FILES / COMPLIANCE" + "ASSETS"
sections and `!.HOUSE_STDS.md` §A.7, 2026-09-02.*

## "If it's not in a file, it's a lie"

Compliance/audit reasoning: an in-memory flag dies with the process
and leaves zero trace to prove after the fact what happened or when —
even if technically sufficient for correctness, it's not auditable.
The house standard (wraith-alpha/TPMOS) always uses a real file for
anything that needs to be provable later, e.g. `frame_changed.txt`
(one-byte append, size-only, content never read) gates every real
redraw. An early in-memory-flag proposal for db-hq's Phase 4 was
directly, correctly rejected in favor of a file
(`db_hq_frame_changed.txt`) for exactly this reason.

## What a "manager" is, and why logic can't just live in the renderer

A manager is a real, SEPARATE compiled binary (`<name>_manager.c`)
that owns a feature's own logic/state and publishes a real state file
the shared renderer's generic injection code reads — never
bash-`printf`-generated `.chtpm`, never logic baked into the shared
renderer beyond generic injection. `khtpm_hq_manager.c` (Common
Events) and `palettes_manager.c` are the two proven examples. See
`TPMOS-COMPLIANCE-DEBT.md` (folded into `08-roadmap/`) for the
standing rule and its violations.

## Runtime-configurable values always go in PDL

Any value a user may want to tweak without recompiling — position,
color, size, label, toggle — belongs in a `.pdl` file, not C source.
Working precedents: `#.desktop/livedesk_theme.pdl`
(`COLOR | bg | #000000`), `#.desktop/livedesk_shortcuts.pdl`
(`SHORTCUT | $ | command`), `package_dir/meta.pdl`
(`STATE | menu_stay_open | 1`). Extend the nearest existing `.pdl`
rather than inventing a new config file; missing keys must fall back
to safe defaults.

**Known live violation (found 2026-08-15, unresolved):**
`livedesk_taskbar.pdl` has `strip_btn_14_menu_0/1_label`/`_cmd` rows
for cell 14 (h-ai)'s submenu, but `khtpm_taskbar_manager.c`'s
`livedesk_build_ai_menu()` does not read them — the menu is fully
hardcoded in C and the PDL rows are dead. Treat as a bug to fix (make
cell 14 read the PDL like `ktb_hq_open()`'s HQ branch already
correctly does), not as precedent.

## Asset paths belong in PDL too

RPG Maker asset paths live in a `.pdl` pointer key (`img_root` in
`RMMV-ASSET-SOURCE-LOCATION.pdl`), never hardcoded in C, so the path
can change (drive letter, OS, physical move) with zero C rewrite.
2026-08-28: all RMMV `img` assets moved out of `&.widgits/palettes/`
(which gets zipped/shared as "the house" and was bloating it with real
PNGs) to `#.NNEST_ASSETS/rmmv-www-img/`, a general asset container
above the house meant to hold future non-RMMV asset types as siblings.
Only the PDL's `img_root` key needed to change.
