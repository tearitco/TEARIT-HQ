# PROGRESS — network_browser → static xhtpm + manager UI projection

**Branch:** `chtpm-var-substitution`  **Date:** 2026-09-03  **Status:** done (first cut), parallel launch

## What changed

network_browser was already on the shared renderer via `<module>`, but
`network_browser_manager.c` regenerated the **entire** `.chtpm` markup
tree every ~300ms tick (`write_chtpm_projection()`) — the layout-updates
pattern the pass kills.

- **`network-browser-hq.xhtpm`** (new, STATIC) — sidebar (bookmarks
  `<repeat bind="bm">` + history `<repeat bind="h">`), panel (fixed
  toolbar row, tab strip `<repeat bind="t">`, `<cli_io>` address, status
  `<text>`, content `<scrolllist>`). `class="network-browser-pal
  database-window"` — does NOT trip `g_is_*`.
- Page content = ONE `<repeat count="${content_count}" bind="c">` whose
  body carries four `show=`-gated candidates (title / text / link /
  media). The projector sets exactly one `c_<n>_is_*` per row.
  **`<repeat>` v2 verified working with zero renderer changes** — a
  fetched google.com page rendered title + link `<item>`s + text
  `<text>`s correctly, no overlap.
- **`network_browser_manager.c`**: added `write_ui_projection()` +
  `g_mode_ui` + `g_ui_output_path`. `write_chtpm_projection()` now
  early-dispatches to it when `g_mode_ui`. `g_mode_ui` is set when any
  argv is `"ui"` — the `.xhtpm`'s `<module src="..." id="ui"/>` passes
  `"ui"` as argv[3] via the renderer's generic `launch_module` extra
  arg. Writes `#.desktop/network-browser-hq_ui.txt` (key=value),
  content-gated (`strcmp` vs last).
- **`button.sh`**: default → the `.xhtpm`; `NB_ROLLBACK=1` → the old
  `.chtpm` + bootstrap self-heal (kept intact, guarded). pgrep pattern
  widened to `network-browser-hq\.[cx]htpm`.

## `ui.txt` schema

```
act_back/act_fwd/act_stop/act_reload/act_home/act_bm/act_close/act_newtab/act_go   fixed action strings
addr_label=   status=Status: <s>
n_bm=  no_bm=   bm_<i>_label=  bm_<i>_action=
n_hist= no_hist=  h_<i>_label=  h_<i>_action=
n_tabs=  t_<i>_label=  t_<i>_action=
content_count=  content_empty=  empty_msg=
c_<i>_kind=title|text|link|img|video
c_<i>_is_title= / _is_text= / _is_link= / _is_media=   (exactly one = 1)
c_<i>_text=   (title/text/link)
c_<i>_label=  c_<i>_sprite=   (media)
c_<i>_action= (link/media)
```
Repeat binds: bookmarks `bm`, history `h`, tabs `t`, content `c` — the
key prefix MUST match the `bind=` name.

## Verified headless
`greet_player`-style dump: sidebar + toolbar + tab strip + address +
status + content all lay out, nav order 1..26, class did not trip
`g_is_*` (generic chrome present), a real google.com fetch rendered as
heterogeneous rows with no `|`-corruption (projector `uisan()` strips
CR/LF and maps `|`→`/` in every label).

## Follow-up: `js: script error TypeError: ...` rows in page content

**What it is (intentional, pre-port):** the built-in JS interpreter
`ops/nb_js_eval.c` (`+x/nb_js_eval.+x`) evaluates a fetched page's
`<script>` blocks. Real sites call DOM/browser APIs it does not
implement (`document.getElementById(...).addEventListener`,
`window.onload = ...`, `el.onclick = ...`), so it emits TypeErrors.
`network_browser_manager.c` deliberately turns each one into a content
row: `do_fetch()` / the JS-effects merge path does
`fprintf(out, "TEXT|js: %s\n", payload)` (~line 791-796, the
`strncmp(line, "LOG|", 4) == 0 || strncmp(line, "TEXT|", 5) == 0`
branch, prefix `js:`). `write_ui_projection()` (and the old
`write_chtpm_projection()`) just pass every `TEXT|` row through — the
noise is upstream of the projector, not caused by it.

For the deeper question - making the JS engine capable enough that
sites actually render - see `08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md`.

**Meanwhile, to just quiet the rows (manager-side, pick one):**
1. **Drop them by default.** In the branch that writes `TEXT|js: ...`,
   skip the write unless a debug flag is set
   (`#.desktop/network_browser_jsdebug.txt` exists, or an env var).
   One `if` around the `fprintf`. Cleanest; errors still reachable when
   debugging the JS engine.
2. **Collapse to one row.** Count `js:` errors during the fetch, emit a
   single `TEXT|js: N script error(s) - open JS log` row plus write the
   full list to `#.desktop/nb_js_errors.txt`; a toolbar item opens it.
3. **Separate kind.** Give them their own state kind (`JSERR|<msg>`)
   instead of `TEXT|js: `, then in `write_ui_projection()` emit them as
   `c_<n>_is_jserr` and gate the whole group in `events-hq.xhtpm`-style
   `show="${js_errors_on}"` (default off). Most work, best UX.

**Recommendation:** option 1 for now (tiny, reversible), option 3 when
the JS engine gets real attention. Do it in the manager — the projector
and template stay generic.

## Left / known gaps (first-cut, documented)
- **sprite-grid-row wrap**: consecutive IMG/VIDEO no longer group into
  one `<row class="sprite-grid-row">` — each media is its own row. The
  old projector wrapped runs of ≥2. Re-add via a `<repeat>` that can
  emit a wrapping row, or a media-run pre-pass in the projector.
- HTML entities in text (`&copy;`) still show literally — pre-existing,
  not a regression.
- Not wired into any launcher/menu — parallel only. Retarget
  `open_network_app.sh` / `livedesk_launchers.pdl` after sign-off, then
  delete `write_chtpm_projection()` + the `.chtpm`/`.bootstrap`.
