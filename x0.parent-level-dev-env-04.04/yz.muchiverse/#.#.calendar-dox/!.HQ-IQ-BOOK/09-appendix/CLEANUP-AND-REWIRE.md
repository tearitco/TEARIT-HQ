# chtpm port — final cleanup: delete the per-app C

**Branch:** `chtpm-var-substitution` (merged to `main` @ `2f644c52`, keep merging)
**Updated:** 2026-09-03

The port is done and on `main`. This doc is now **only** the remaining
task: delete the `g_is_*` / `evhq_*` / `dbhq_*` layout-as-C from
`khtpm_core_render.c` and drop the now-dead rollback files.

---

## DONE (do not redo)

- **All windows ported** to static `<name>.xhtpm` + projector, each with
  an old-path rollback. On `main`.
- **Verification run** — 9/9 `-pal` windows PASS headless (db-hq-pal,
  events-hq, bookmarks, stats-hq, taskbar-settings, network_browser,
  chat-hai, palettes-emojis, palettes-elements). No new breakage.
- **Launchers retargeted** with `X_ROLLBACK=1` env fallbacks:
  `chat-hai/button.sh` (CHAT_HAI_ROLLBACK), `stats-hq/open_stats_hq.sh`
  (STATS_ROLLBACK), `button_taskbar_settings.sh` (TBSET_ROLLBACK),
  `bookmarks/bm_menu.sh` launch path (BM_ROLLBACK); events-hq
  (`EZ_PKG_DIR`) and network (`NB_ROLLBACK`) done earlier.
- **Renderer generic capabilities** the ports rely on — all committed,
  all generic, **KEEP**: `${var}` subst, `<repeat>` (+ heterogeneous
  `show=` bodies), `show=` gating, multi-row `<tabbar>` in
  `layout_sidebar_panel`, `kh_elem_in_scope` scoped nav, content-hash
  reparse, `<cli_io>` in `<scrolllist>`, `${HOUSE}/${PKG}/${PID}/${ARG3}`
  builtins, the argv[3] instance-dir hook (`g_arg3_dir` /
  `g_extra_vars_path` / `KHTPM_ARG3`).

### Still NOT rewired (do these AS PART OF this cleanup, they touch the same files)

- `#.desktop/livedesk_taskbar.pdl` `livedesk:open-palette:<cat>` — the
  verb handler is in `khtpm_taskbar_manager.c`. Point `emojis`/`elements`
  at `&.widgits/palettes/button-pal.sh <cat> <house>`. `rmmv` /
  `piececraft` / `user-pallet` stay on `palettes_menu.sh` (not ported;
  plan in `PROGRESS-palettes-xhtpm.md`).
- `#.desktop/livedesk_launchers.pdl` `launcher_db`
  (`*.monads/*.muchi-pet/ops/open_db_hq.sh`) → `&.hq-apps/db-hq-pal/button.sh`.
  db-hq-pal DB-record field editing is still read-only (see below) — if
  that's a blocker for the owner, keep `launcher_db` old and ship the
  rest; the C can still be deleted (the read-only panel is generic).
- taskbar `db` cell menu already has a `db-hq (PAL)` row
  (`livedesk:open-db-hq-pal`). Once db-hq-pal is the default `db`
  target, drop or relabel the old row.

---

## THE TASK — delete the per-app C from `khtpm_core_render.c`

`khtpm_core_render.c` is **~18.77k lines**. Live symbol counts
(2026-09-03): **`evhq_` 233, `dbhq_` 428**, `g_is_events_hq` 30,
`g_is_db_hq` 48, `g_is_stats_hq` 23, `g_is_palettes` 35,
`g_is_bookmarks` 20, `g_is_swatch_picker` 18. Expect **−1500…−2200
lines** net.

Grok's concurrent edits to this file are done — it should be quiet now.
Still: **compile after every few deletions**, and this is its own PR.

### 1. events-hq and db-hq must go TOGETHER (they are cross-tangled)

The "db-hq Common Events editor" bridges the two: `dbhq_ce_inject_panel`
calls `evhq_load_command_registry`; `dbhq_ce_draw_overlay_if_needed` /
`dbhq_ce_handle_key_if_needed` drive `evhq_draw_picker_overlay` /
`evhq_*` picker state from inside db-hq; the guard
`g_dbhq_ce_editing && g_evhq_picker_open` appears in
`dbhq_assign_nav_indices` (~3656), `dbhq_handle_click` (~4574),
`evhq_redraw_content` (~5475), and the mouse loop (~10929). The
tentative decls `g_evhq_picker_open` / `g_evhq_picker_type` at
lines 1931-1932 exist **only** so `dbhq_handle_click` can see them.

So: do NOT try "events-hq first". Remove `g_is_events_hq` + `evhq_*`
+ `g_is_db_hq` + `dbhq_*` + `dbhq_ce_*` in one PR (separate commits per
step is fine, but they land together).

### 2. KEEP these — they are generic despite the `dbhq_`/`g_dbhq_` name

Rename to `kh_*` if you want, but do NOT delete:

| symbol | ~line | why keep |
|---|---|---|
| `dbhq_serialize_frame_elem` / `_subtree` | 3975 / 4000 | the frame-file round trip **every** default/popup window uses (`redraw()` → serialize → `dbhq_paint_frame_line`) |
| `dbhq_paint_frame_line` | 4054 | ditto (paints the read-back frame) |
| `dbhq_append_frame_history` | 3895 | frame-history marker file, generic |
| the swatch-grid layout branch (`class="swatch"`) | ~8722 | palettes + taskbar-settings depend on it; NOT gated on `g_is_swatch_picker` |
| `g_dbhq_active_scope_root` (the var) | — | referenced by `khtpm_draw_core.c`'s `[^]` badge `is_scope` test. Either keep the var as an always-NULL stub, or delete its one use in `_shared-lib/khtpm_draw_core.c` too (that file is authoritative; the ops copy is rebuilt from it). |
| `g_dbhq_chrome_h` / `dbhq_draw_chrome_bar` | 3802 | check: if the generic chrome (`chrome-minimize`/`-close`/`-fullscreen` items) fully replaces it, delete; else keep the bar draw for popup mode |

Also generic, keep: `zero_nav_subtree` (101 / used by evhq but also
generic tree util — check callers before deleting).

### 3. Delete list (`khtpm_core_render.c`)

**Flags + class detection** (~line 17900-17940): the six
`if (strcmp(g_window->classes[i], "<x>") == 0) { g_is_* = 1; ... }`
lines for `db-hq`, `events-hq-window`, `stats-hq`, `palettes`,
`bookmarks`, `swatch-picker`, and their `static int g_is_* = 0;`
declarations. `g_is_stats_hq` / `g_is_palettes` / `g_is_bookmarks`
also set `g_is_db_hq = 1` — all covered by removing the db-hq path.

**argv reinterpret** (~17960): drop `if (g_is_events_hq) { ... argv[3]
= g_evhq_pkg_dir ... }`. Keep the `else if (argc >= 5 && !g_arg3_dir[0])`
popup-xy branch (my ${ARG3} hook already guards it).

**evhq_ block** — forward decls 3863-3886, then the bulk. Functions:
`evhq_measure_text_px`, `evhq_build_scratch_view`,
`evhq_handle_block_onclick`, `evhq_dispatch_picker_onclick`,
`evhq_redraw_content`, `evhq_open_edit_picker`,
`evhq_load_command_registry`, `evhq_draw_picker_overlay`,
`evhq_handle_key`, `evhq_cleanup_module`, `evhq_handle_term_signal`,
`evhq_launch_module`, `evhq_load_entity_sprite`, `evhq_describe_command`,
`evhq_palette_cls_for_type`, `evhq_init_manager_paths`,
`evhq_load_pages`, `evhq_write_selected_page`, `evhq_load_page_state`,
`evhq_request_append_node` / `_edit_node` / `_delete_node` /
`_trigger_update`, `evhq_layout_pass`, `evhq_assign_nav_indices`,
`evhq_activate_elem`, `evhq_handle_click`, plus all `g_evhq_*` state
(pages, cmds, blocks, picker, registry `g_evhq_cmd_defs`, drag,
close_elem, digit_accum, sprite, mtimes, cksums).
Files: `git rm &.widgits/events-hq/pieces/dashboard.chtpm`,
`pieces/picker.chtpm`; drop the `EZ_PKG_DIR` rollback guard in
`&.widgits/events-hq/button.sh` (it already execs `button-pal.sh`);
delete `evhq_launch_module` call site.

**dbhq_ block** — everything `dbhq_*` at lines 458-4629 EXCEPT the
keep-list in §2. Big ones: `dbhq_layout_pass` (3427),
`dbhq_assign_nav_indices` (3648), `dbhq_activate_elem` (4428),
`dbhq_handle_click` (4566), `dbhq_handle_key` (4629),
`dbhq_redraw_content` (4177), `dbhq_inject_*` (bookmark/palette/actor/
list/sidebar), `dbhq_load_*` (common_events/bookmark/palette/actors/
list_tab/font_scale/palette_options), `dbhq_show_actors` /
`dbhq_show_list_tab`, `dbhq_apply_css*`, `dbhq_measure_text_px`,
`dbhq_pal_*` (shift/scroll/write_frame/paint), `dbhq_rmmv_*`,
`dbhq_ce_*` (open/inject_panel/handle_onclick/draw_overlay/handle_key/
restore_tab_content), `dbhq_activate_scope` / `dbhq_back_scope` /
`dbhq_nav_take` / `dbhq_elem_is_navigable` / `dbhq_cli_io_navigable`,
`dbhq_render_placeholder_tab`, `dbhq_soft_focus`,
`dbhq_grab_keyboard_retry` (check: generic `<cli_io>` arm uses its own
`XGrabKeyboard` — if so delete; else keep + rename), `dbhq_tab_is_real`,
`dbhq_sidebar_label_for`, `dbhq_list_idx_for_tab`, `dbhq_file_checksum`
(generic-ish — check callers), `dbhq_launch_module` (delegates to
generic `launch_module`; db-hq-pal uses `kh_launch_window_modules`, so
delete), `dbhq_cleanup_module` / `dbhq_handle_term_signal`, all
`g_dbhq_*` / `DbhqActor` / `DbhqListRec` state.

**Dispatch / loop call sites** — every `if (g_is_db_hq) dbhq_...` /
`if (g_is_events_hq) evhq_...` in `handle_key`, `hq_run_event_loop`,
the mouse handlers, `assign_nav_and_layout`, `redraw`, `main`.
Notable: `assign_nav_and_layout` head has
`if (g_is_db_hq) { dbhq_layout_pass; dbhq_assign_nav_indices; return; }`
and `if (g_is_events_hq) { evhq_layout_pass; ...; return; }` — delete
both so every window takes the generic `layout_sidebar_panel` path.
`nav_tab_register("evhq", ...)` (~18475); the
`g_is_palettes ? "palettes" : ...` string (~18379).

### 4. Rollback files to `git rm` once the C is gone

- `&.hq-apps/db-hq/dashboard.chtpm` (+ its `.css`), `open_db_hq.sh`
  (or keep `open_db_hq.sh` as a thin `exec button.sh` shim)
- `&.widgits/events-hq/pieces/dashboard.chtpm`, `pieces/picker.chtpm`
- keep `palettes-<cat>.chtpm` for `rmmv`/`piececraft`/`user-pallet`
  (still on the old path) — only remove `palettes-emojis.chtpm` /
  `palettes-elements.chtpm` if `dbhq_inject_palette_tiles` is truly gone
- `&.hq-apps/stats-hq/dashboard.chtpm`, `&.widgits/bookmarks/bookmarks.template.chtpm`,
  `*.monads/*.livedesk-taskbar/ops/taskbar_settings.chtpm` — remove after
  confirming nothing else provisions/reads them

### 5. Procedure

1. `grep -n 'g_is_events_hq\|g_is_db_hq\|g_is_stats_hq\|g_is_palettes\|g_is_bookmarks\|g_is_swatch_picker\|evhq_\|dbhq_' khtpm_core_render.c > /tmp/hits.txt` — work top-down.
2. Delete a function or a call-site cluster; `cd "$(ls -d …/ops)" && sh build_core_render.sh` (or `build_khtpm_strip.sh`). Fix the compile. Repeat.
3. When a `dbhq_*` you're about to delete is in the §2 keep-list — **stop**, rename it `kh_*` across the file instead, keep going.
4. After the C is out: `build_khtpm_strip.sh`, `run_khtpm_strip.sh new`, then open the strip + one entity right-click menu + `db-hq-pal/button.sh` + `events-hq/button.sh <entity>` + `chat-hai/button-pal.sh` — confirm none regressed. Use the headless method in `HANDOFF-scope-nav-and-chtpm-port.md §4`.
5. `git rm` the §4 files. Rewire the two launchers in "Still NOT rewired" above.
6. Commit per step; own PR; merge to `main` when the strip + windows are confirmed.

### Do NOT touch

- `pchq_*` / `g_is_pchq_board` (137 refs) — that's the piececraft
  board-viewer, a separate feature, not this port.
- `g_is_cursword` (30) — cursword entity, separate.
- `g_is_chat_hai` (3) — vestigial; safe to remove with the rest but not
  load-bearing.

---

## After the C is deleted

- Rename surviving `dbhq_*` shared helpers to `kh_*`.
- `CENTROID_GOLD_STD.md`: static template + projector is THE pattern;
  `g_is_<app>` is now actually banned (no branches left to add to).
- Fold the per-app `PROGRESS-*-xhtpm.md` notes into one closed record.
- Open follow-ups (own tickets): palettes `rmmv`/`debug`/`piececraft`/
  `user-pallet` ports; db-hq-pal RPG-Maker record field editing;
  events-hq real visual block editor (Scratch/Blueprints); the
  stats-hq panel-left stray `[>]N` badge over the first kv row
  (cosmetic — kv rows are indented 3 spaces as a stopgap).
