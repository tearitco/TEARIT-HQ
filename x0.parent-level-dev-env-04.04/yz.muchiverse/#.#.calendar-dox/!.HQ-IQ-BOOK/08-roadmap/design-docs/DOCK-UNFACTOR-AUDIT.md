# Dock unfactor — audit and staged plan (2026-09-20)

**Scope:** the dock/strip mode (`window_is_dock()`: header strip + bottom "pals" row + toys dropdown) inside `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`. Parent plan: `INMEM-DB-STATE-LAYER-PLAN.md` §6 (files stay default; nothing here depends on any DB). Style goal: **manager + template data + generic renderer**, standalone ops for process management, **no new cross-binary `#include`** and no new binary that includes the engine (text-includes are transitional — see `INMEM-DB-STATE-LAYER-PLAN.md` §0/§5).

**Baseline (commit `03404a08`):** `khtpm_core_render.c` = 12,550 lines (5,010 comment, 344 blank, 7,196 code). 1,347 function lines are dock-related. `window_is_dock()` is referenced at **55** sites, so the dock is woven through the generic event loop, layout, paint and key handling, not a clean module. -O2 baseline: 160 warnings.

## 1. Function inventory and buckets

A = generic renderer capability (belongs in the shared engine as an ordinary element/CSS feature, not a `window_is_dock()` special case) · B = manager/op/data · C = irreducible X11 glue (stay, small, commented) · D = dead.

| Function(s) | Lines | Bucket | Why / dependency |
|---|---:|---|---|
| `ktb_zorder_apply_tree`, `ktb_toggle_zorder_apply`, `ktb_toggle_zorder_respawn` | 244 | **B → standalone op** | Process management: scans `/proc`, SIGTERM, staggered `execve` re-launch of entity windows so `override_redirect` (create-time only) changes; raises/lowers windows. Not rendering. Precedent: `swatch_picker_manager`, `apply_theme_op` are standalone ops the renderer spawns. **Stage 2.** |
| `load_zorder_mode`, `save_zorder_mode` | 19 | B/C | Tiny state files (`khtpm_zorder_mode.state.txt`, `livedesk_override_redirect.pdl`); the renderer still needs the creation-time value. Stay. |
| `load_theme_opacity`, `write_theme_opacity` | 114 | B (out of dock scope) | Generic theme/opacity persistence used by settings + all windows. Candidate for the `apply_theme_op` family; not dock-specific. Left. |
| `load_dock_strip_offset` | 31 | B | Reads `strip_x_offset/strip_y_offset` from `livedesk_taskbar.pdl`. Same PDL-loader family as `khtpm_ui_common.c`; the manager could publish it. Left (needs a published-value consumer). |
| `dock_text_px`, `dock_cell_natural_w`, `layout_dock_toolbar_row`, `dock_item_cw`, `dock_place_pager`, `dock_draw_separators`, `layout_dock_bar` | 423 | **A** | Row packing with shrink-to-fit, pager, separators. The comments already say the CSS flex-wrap engine (`css_layout_pass`) owns packing; what's dock-only is shrink-to-fit, the `no-nav` cell shape, and the pager. **Blocked**: needs generic flex "shrink to container" + a pager element first. |
| `dock_paint_peer`, `dock_paint_menu` | 234 | **A/C** | Paint for the *second and third X windows owned by one process* (bottom row, toys dropdown). **Blocked**: needs a generic "secondary surface" notion in the engine. |
| `dock_poll_strip_state`, `dock_ascii_walk`, `dock_ascii_append_state`, `dock_write_ascii_frame` | 171 | A/B | Terminal-mirror producer + manager focus-cursor sync (marker-size driven, DIAMOND standard). The producer walks the laid-out Elem tree — a generic "ASCII frame for any window" (the frame serializer `kh_serialize_frame_elem` already exists) could replace it. Medium risk; later. |
| `apply_dock_window_hints` | 30 | C | `_NET_WM_WINDOW_TYPE_DOCK`, `WM_HINTS input=True`, sticky/above, `WM_NORMAL_HINTS`. |
| `dock_relay_focus_code` | 7 | C | Appends resolved focus codes to `strip_history.txt` (already file IPC to the manager). |
| `window_is_dock`, `dock_is_our_win`, `dock_grab_keyboard`, `dock_release_keyboard_if_left` | 28 | C | Keyboard grab/focus semantics — **do not change** (pitfalls below). |
| `kh_ensure_dock_peer_window` | 46 | C | Creates the peer X window (managed dock, opacity). |
| **Total** | **1,347** | | A 657 · B 408 (of which 114 out-of-scope theme) · C 111 · D 0 in scope (see §2) |

## 2. Dead code (D)

The compiler (`-O2 -Wall`) reports these `static` items unused in `khtpm_core_render.c` (all real; none dock-named): `hq_window_has_x_focus`, `consume_frame_changed`, `mark_frame_changed`, `nav_ledger_publish`, `nav_tab_cycle`, `nav_tab_unregister`, `nav_tab_register`, `history_unregister`, `apply_theme`, `input_disarm`, `hq_run_detached`, `reusable_slot`, `g_is_events_hq`, `g_pal_rmmv_button1_was_down`. Also dead by consequence: `ktb_toggle_zorder_apply` reads `#.desktop/nav_tab/*` but `nav_tab_register` (its only writer in this binary) is unused and the directory is empty on the live desktop. Removing them is **Stage 1**. Their history comments are summarised in §6 so nothing is lost.

## 3. Coupling and pitfalls each stage must preserve

- **Dock windows are WM-managed unconditionally** (`dock_managed = window_is_dock()` in window creation; comment "never remove" — `X11-AND-SESSION-PITFALLS.md` 2026-09-05 "arrows control nav broke again"). Any change that lets `livedesk_override_redirect.pdl` reach the dock re-breaks keyboard nav.
- **Keyboard grab/focus** (`dock_grab_keyboard`, `dock_release_keyboard_if_left`, click path `XRaiseWindow`+grab+`XSetInputFocus`): pitfall **#24** (`HOUSE_CODE_PITFALLS.md`) — ungrab must use the same `Display` that grabbed; a stale display-wide grab starves every window. Semantics unchanged in all stages.
- **Nav numbering** across header / bottom row / dropdown (`g_dock_header_nav_hi`, `g_dock_drop_lo/hi`, `dock_relay_focus_code(6000+n)`); pager occupies nav slots.
- **`assign_nav_and_layout` idempotency** (called many times per frame; never accumulate translation/scale — `khtpm-shared-layout-caution`, `HOUSE_CODE_PITFALLS.md` #14).
- **Toys dropdown** `g_dock_menu_win` is a deliberate short-lived `override_redirect` popup; header cell numbers: pals 6, palettes 7, player 9, db 10, network 14, ai 15, clock 16 (`ktb_cell_id()` refactor, `bug_bounty.md` 2026-09-17).
- **Golden geometry at 2496x1664:** header dock `2073x45+200+50` (top strip `2082x45+200+50` on the live desktop), bottom `2096x45+200+1619`; one row, no wrapped labels; also fits at 1366x768 / 1920x1080 / 3840x2160 (UI-scale work, `BUG-LOG.md`).
- **Respawn must be house-scoped**: the current `ktb_toggle_zorder_respawn` matches process names with `strstr(argv0, needle)` across **all** of `/proc`, so a private/test house — or a second house — would SIGTERM the user's live desktop. Any extracted version filters on the house root.

## 4. A real bug found by the audit

Since the pal unfactor (`da57ae00`) desktop pals run as **`khtpm_entity.+x`**, but `ktb_toggle_zorder_respawn` still looks for `tp_desktop_window_rgb`, `khtpm_core_render` and `network_browser_render`. The `@` always-on-top toggle therefore **no longer respawns pals** (override_redirect is creation-time-only, so pals keep their old stacking mode). The manager (`khtpm_taskbar_manager.c` lines ~1411, ~2419, ~2507, ~2773, ~3667) was updated for `khtpm_entity`; this function was missed. The Stage 2 op fixes it.

## 5. Stage table

| Stage | What | Status |
|---|---|---|
| 0 | This audit | done |
| 1 | Delete the dead statics in §2 (+ the helpers/vars that became dead, + two stale comment blocks) | **done** - 12,550 → 12,172 lines, warnings 160 → 146, dock frames pixel-identical to the pre-change binary at 4 sizes (only the clock digits differ, same as run-to-run noise) |
| 2 | Z-order toggle → standalone op `ktb_zorder_op` (renderer keeps only the mode flip + own dock raise/lower + detached spawn); house-scoped; `khtpm_entity` fixed | **done** - renderer 12,172 → 11,971 lines; dock frames pixel-identical at 4 sizes; e2e relay click verified in private Xephyr (BUG-LOG 2026-09-20) |
| 3 | Terminal-mirror producer: reuse the generic frame serializer for the dock | not started (medium risk) |
| 4 | Layout A-bucket (shrink-to-fit, pager) as generic flex/CSS features | **blocked**: needs generic flex "shrink to container" + pager element in the engine |
| 5 | Peer/menu paint A/C-bucket | **blocked**: needs a generic "secondary surface" (multi-window-per-process) concept |
| — | `load_theme_opacity`/`write_theme_opacity` → `apply_theme_op` family | out of dock scope; noted |

Realistic outcome of this pass: Stages 1–2 (≈260–500 lines out of the renderer, one real bug fixed). Stages 3–5 are design work on the *generic engine*, not dock moves; they should be planned with the user before code.

## 6. Comment/history mirrored from removed code (so docs-only readers keep it)

- `ktb_toggle_zorder_respawn` history (2026-09-13 live reports): the strip windows are WM-managed regardless of the always-on-top setting, so the toggle must **not** respawn the strip (only entities); death is polled with `kill(pid,0)` (≤6×30 ms) instead of a flat 300 ms sleep because SIGTERM interrupts `select()` immediately (sigaction without `SA_RESTART`); the strip respawn came first historically and is now moot; entities are staggered (30 ms) and `nice(8)` so a burst of GUI launches doesn't saturate the CPU (project note: weak-CPU machine). Carried into the op's header comment.
- Dead-code notes: `nav_tab_*` was the old per-window registry (`#.desktop/nav_tab/<pid>`) read by `tp_find_window_by_navtab.c` and the z-order raise/lower loop; the registration is no longer called from this binary and the live dir is empty. `tp_find_window_by_navtab.c` remains a test-suite consumer and should be revisited if that suite is revived.

## 7. Grounding

`INMEM-DB-STATE-LAYER-PLAN.md`, `UNFACTOR-PAL-X.md`, `XHTPM-RE.md`, `CENTROID_GOLD_STD.md`, `TASKBAR-MENU-ARCHITECTURE.md` (2026-09-18 corrections), `X11-AND-SESSION-PITFALLS.md`, `HOUSE_CODE_PITFALLS.md` #14/#20–#24, `CHTPM-INCREMENTAL-REPARSE-DESIGN.md` (dock-first rollout order).
