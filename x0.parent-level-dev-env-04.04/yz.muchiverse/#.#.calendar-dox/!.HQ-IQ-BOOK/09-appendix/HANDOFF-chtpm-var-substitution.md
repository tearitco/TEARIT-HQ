# HANDOFF — branch `chtpm-var-substitution`

**Last updated:** 2026-09-03 (mid-session)
**Branch:** `chtpm-var-substitution` (off `origin/main` @ `bbf9caf2`), pushed
**Goal:** restore the tpmos layout/data separation for khtpm windows
(`CHTPM-ARCHITECTURE-FIX.md`) — static template + a projector that writes
`state/ui.txt`, instead of a manager that regenerates markup or C that
builds the tree.

Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN

---

## State: DONE and pushed

### Renderer primitives — `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`

| primitive | what |
|---|---|
| `${var}` substitution | `parse_chtpm()`: if the template has `${` or `<repeat`, load the `vars="<path>"` state file and substitute `${key}` before parsing. `\$ \{ \\` escapes, `\n`→newline, unknown→empty. Relative `vars=` resolves against the `.chtpm`'s own dir (`g_package_dir`). |
| `${HOUSE}` / `${PKG}` | built-ins in `kh_get_var()` for action paths |
| `show="${x}"` | in `parse_element()` — drop the element when the value is `""`/`0`/`false` |
| `<repeat count="${n}" bind="x">…${x.field}…${x.#}…</repeat>` | in `parse_chtpm()` before `${var}`. `count` = bare int or one `${var}`. No nesting (v1). `KH_REPEAT_MAX` 4096. |
| `<!-- -->` skipped in both passes | a `${…}`/`<repeat>` inside a doc comment was being processed |
| `--dump-and-exit` (any argv) | every mode now: `dump_frame_png()` writes `/tmp/entity-menu-frame.png` + `.receipt.txt` + `.frame.txt` (ASCII Elem tree). Calls `reparse_chtpm_if_changed()` first so a just-launched projector's write lands. |
| `launch_module()` splits `src` on whitespace | `<module src="<interp> <script>"/>` (tpmos style). Each relative token resolved vs `package_dir` then `house_root`. Exports `KHTPM_HOUSE` / `KHTPM_PKG` / `PRISC_PROJECT_ROOT` to the child. One-token `src` (compiled manager) unchanged. |

Wrapper: `44.xyz.01.00/&.widgits/_shared-lib/ops/khtpm_png_dump.sh <chtpm> [house] [outdir]`.

### PAL interpreter string ops — `44.xyz.01.00/&.widgits/_shared-lib/system/prisc+x.c`

Backwards-compatible (additive: new `strcmp` branches, new executor cases,
`sregs[16][4096]` separate zero-init bank, enum values appended). A `.pal`
with no `s*` mnemonic runs byte-for-byte as before. The ~20 project-local
`prisc+x.c` copies are untouched.

Ops: `slit scpy sappend sgetenv sfmt sread ssplit sfind slen sfopen
sfappend swrite sfclose sbeq sbne strim satoi` — full spec in
`_shared-lib/system/string-ops.md`. Also: trailing `#` comments on
instruction lines (quote-aware). Build: `_shared-lib/ops/build_prisc.sh`
→ `_shared-lib/system/+x/prisc+x.+x`.

### Apps converted (manager/projector writes `state/ui.txt`, static template does layout)

| app | projector | commit |
|---|---|---|
| `&.hq-apps/signup-hq` | C (`signup_hq_manager.c` `write_state()`) | `1e088661` |
| `&.widgits/open-hai` | C (`khtpm_open_hai_manager.c` `write_state()`) | `a255846f` |
| `&.hq-apps/co-lab-hai` | C (`colab_hai_manager.c` `write_chtpm_projection` → key=value) | `f4ace065` |
| `&.widgits/db-hq-actors-pal` | **PAL** (`pal/actors_projector.pal`) — the reference | `5438043b` |

All verified headless with `khtpm_png_dump.sh`.

Doc with full status + `<repeat>` v2 sketch:
`08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md` §8.

---

## State: IN FLIGHT (this session, may be uncommitted)

### `&.hq-apps/db-hq-pal/` — the full 15-tab db-hq as static template + PAL projector

- `dashboard.chtpm(.bootstrap)` — sidebar = 15 static tab `<item>`s, each
  `action="'${PKG}/ops/dbhq_action.sh' 'tab' '<file>' '<TAG>' '<title>'"`.
  Panel = `${tab_title}` + `<repeat>` of records + `${detail_title}` +
  `<repeat>` of the selected record's kv. `class="db-hq-pal"` (NOT
  `db-hq`) so the renderer's `g_is_db_hq` C path stays dormant.
- `ops/dbhq_action.sh` — maintains `state/active.pdl` (`file|tag|title|sel`).
  Verbs: `tab` / `sel` / `open-ce` (launches the real db-hq for CE).
- `pal/dbhq_projector.pal` — **NOT YET WRITTEN** (or written & untested).
  Reads `state/active.pdl`, defaults to Actors, parses
  `<house>/#.desktop/db_hq_<x>.state.txt` (uniform `TAG | key | value`
  records, first field `id`, second `name`), writes `state/ui.txt`:
  `tab_title`, `rows_count`+`row_i_text`, `kv_count`+`kv_i_text`,
  `detail_title`, `is_ce`. Two passes over the file (pass 1 rows, pass 2
  the `sel`-th record's kv). Model it on `db-hq-actors-pal/pal/actors_projector.pal`.
- Common Events tab (11) — projector sets `is_ce=1`; the panel then shows
  a note + "Open the Common Events editor" button. CE keeps its existing
  C editor; not converted.

### Pending cleanup the owner asked for (2026-09-03)

1. **Rename `*.chtpm` → `*.xhtpm`** for the converted static-template apps
   (x11-hq style, distinct from tpmos `.chtpm`). Parser doesn't care —
   the renderer takes an explicit path arg. Touch each app's `button.sh`
   (`CHTPM=` path + the `pgrep -f '…\.chtpm'` patterns). `khtpm_png_dump.sh`
   already strips any extension for its stem. Check the renderer's sibling
   `<stem>.css` lookup still resolves.
2. **Drop the `*.chtpm.bootstrap` duplication.** It existed so `button.sh`
   could restore the template after a manager clobbered it — but projectors
   now write `state/ui.txt`, never the template, so the `.xhtpm` is just a
   normal checked-in source file. Delete the `.bootstrap`, delete the stale
   live `.chtpm` copy, remove the restore block from each `button.sh`.

---

## State: NOT DONE

- **network_browser** — page-content area is heterogeneous (TITLE/TEXT/
  LINK/IMG/VIDEO + sprite-grid wrapping). Needs `<repeat>` v2 (per-kind
  element or nested). Sketch in `CHTPM-ARCHITECTURE-FIX.md` §8.
- **chat-hai** — projector is `chat_hai_projector.sh` (bash). Same split,
  in shell.
- **db-hq (real)** — `db-hq-pal` is the parallel proof; folding it back
  into the `class="db-hq"` window (retiring the `dbhq_*` C in the
  renderer) is the large follow-on. CE is its own subsystem.
- `CENTROID_GOLD_STD.md` — update once the above land.

---

## How to test any converted window

```sh
HOUSE=…/44.xyz.01.00
sh "$HOUSE/&.widgits/_shared-lib/ops/khtpm_png_dump.sh" \
   "$HOUSE/&.hq-apps/<app>/<name>.xhtpm" "$HOUSE" /tmp/out
# prints + cats /tmp/out/<name>.receipt.txt and .frame.txt ; PNG at .png
```

`frame.txt` is the ground truth (tag | id | class | label | … | onclick | …).

## Rules

- Never `git add -A` — stage explicit paths. `khtpm_core_render.c` and
  `khtpm_taskbar_manager.c` have concurrent editors.
- Commit footer: `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`
  / `Claude-Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN`
