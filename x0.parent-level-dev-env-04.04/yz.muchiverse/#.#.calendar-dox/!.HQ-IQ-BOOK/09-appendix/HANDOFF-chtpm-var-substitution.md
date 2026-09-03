# HANDOFF — branch `chtpm-var-substitution`

**Last updated:** 2026-09-03 (mid-session, rev 2)
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
| `&.hq-apps/db-hq-pal` | **PAL** (`pal/dbhq_projector.pal`) — the real 15-tab db-hq | `4837684a` |

All verified headless with `khtpm_png_dump.sh`.

Doc with full status + `<repeat>` v2 sketch:
`08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md` §8.

---

## Done — this rev

- **`*.chtpm` → `*.xhtpm`** for all converted apps (`4837684a`). Parser
  doesn't care; sibling `.css` found by extension-swap. `.bootstrap`
  duplication removed (projectors write `state/ui.txt`, never the
  template), restore blocks stripped from every `button.sh`.
- **`&.hq-apps/db-hq-pal/`** — the full 15-tab db-hq as ONE static
  `dashboard.xhtpm` + `pal/dbhq_projector.pal`. Tabs are toolbar
  `<item>`s writing `state/active.pdl` via `ops/dbhq_action.sh`; the
  projector reads that + the tab's `#.desktop/db_hq_<x>.state.txt`
  (uniform `TAG | key | value`) and writes rows + selected-record
  fields. `class="db-hq-pal"` keeps the renderer's `g_is_db_hq` C
  path dormant. Common Events tab → `is_ce=1` → links to the existing
  editor. Verified headless: tab switch + record select.
- `prisc+x` `NUM_SREGS` 16 → 32.

## State: IN FLIGHT — nothing right now

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

---

## Rev 3 (2026-09-03) — generic `<tabbar>` + remaining-work handoff

### Done this rev

- **Generic `<tabbar>`/`<tab>`** ported into `khtpm_core_render.c` so a
  plain `class=""` template gets the old db-hq shape (`1e628f49`):
  - `layout_sidebar_panel()` lays a `<tabbar>` child of `<page>` as a
    horizontal strip, grows the window to fit, tabs nav-numbered first,
    active tab gets an `"active"` class (CSS `.tab`/`.tab.active`).
  - `activate_focused()` `<tab>` branch: run `action=`, mark active
    (`g_default_active_tab_id`, survives reparse), scope nav into the
    page `<sidebar>` — `[^]` on the tab, `Esc` back to the tab row.
- **Interact-mode scoped nav** for generic `ACTIVATE` (`0eae03b1`):
  `onclick="ACTIVATE" target_id="X"` confines nav to container `X`'s
  subtree; `Esc` pops; `[^]` on the trigger. Gated by
  `g_default_scope_confine` — a plain dropdown `ACTIVATE` is unchanged.
- **db-hq-pal** rewritten to the faithful shape (`<tabbar>` + 15
  `<tab>` + `<sidebar>` + `<panel>`, `dashboard.css` ported from the
  original). Renders ~1285px like the old db-hq.

### Left to do — safe for another agent / Haiku subagents

Each is the same recipe: static `<name>.xhtpm` template + a projector
(`.pal` OR compiled `.+x` C — whichever is easier) that writes
`state/ui.txt`; `button.sh` launches `khtpm_core_render.+x <house>
<name>.xhtpm`; verify with `khtpm_png_dump.sh`.

| # | target | notes / difficulty |
|---|---|---|
| 1 | **live-test db-hq-pal** tab→scope→`Esc`, then add its `toy.pdl` to a launcher / the toys menu | needs a real desk click; toy.pdl already exists |
| 2 | **chat-hai** | replace `ops/chat_hai_projector.sh` (bash) with a `.pal` or `.c` projector writing `state/ui.txt`; author `chat-hai.xhtpm`. Message list = one `<repeat>`. Straightforward. |
| 3 | **tile-picker** (`&.widgits/tile-picker/ops/khtpm_show_choices.c`) | small one-shot chooser; low priority |
| 4 | **events-hq** (`&.widgits/events-hq`, `g_is_events_hq`) | now unblocked — same `<tabbar>` port as db-hq-pal. Has view sub-tabs (Scripting/Scratch/Blueprints) + a command list. Its `evhq_*` C retires. Medium. |
| 5 | **palettes / bookmarks / stats-hq** (`g_is_palettes` / `g_is_bookmarks` / `g_is_stats_hq`, all ride `g_is_db_hq`) | each: static template + projector reading its existing state files. Palettes needs the `sprite=` grid (already a generic `draw_elem` capability). Medium each. |
| 6 | **swatch-picker / taskbar-settings** (`g_is_swatch_picker`) | flat `<item>` list + opacity ± ; small |

### Left to do — needs care (do NOT hand off)

| target | why |
|---|---|
| **network_browser** | page content is heterogeneous (TITLE/TEXT/LINK/IMG/VIDEO + sprite-grid wrapping). Needs **`<repeat>` v2** (per-kind element via `show=` on each candidate, or nested repeats). Sketch in `CHTPM-ARCHITECTURE-FIX.md` §8. |
| **retire `dbhq_*` C** | once db-hq-pal is signed off, point the real `open_db_hq.sh` at `dashboard.xhtpm`, drop `class="db-hq"`, delete the `dbhq_*` renderer C (~1350 lines). Big, do deliberately, its own branch. |
| **`CENTROID_GOLD_STD.md`** | document static-template + projector (`.pal`/`.c`) as the standard once the above lands. |

### `<repeat>` v2 sketch (for network_browser / any heterogeneous list)

```xml
<repeat count="${content_count}" bind="c">
  <text class="page-title" label="${c.text}"   show="${c.is_title}"/>
  <text                    label="${c.text}"   show="${c.is_text}"/>
  <item label="${c.text}" action="${c.action}" show="${c.is_link}"/>
  <item label="${c.label}" sprite="${c.sprite}" action="${c.action}" show="${c.is_media}"/>
</repeat>
```
The projector sets exactly one `c_<i>_is_*` to 1 per row. Sprite-grid
wrapping (consecutive media in one `<row class="sprite-grid-row">`) is
lost in this first cut — acceptable, or add a `<repeat wrap-class=...
wrap-when="${c.is_media}">` later.
