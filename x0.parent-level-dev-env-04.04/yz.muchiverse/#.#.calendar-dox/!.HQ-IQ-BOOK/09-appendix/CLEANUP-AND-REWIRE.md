# chtpm port — cleanup (delete per-app C) + launcher rewire

**Branch:** `chtpm-var-substitution`  **Date:** 2026-09-03
**Do NOT start any of this until the -pal windows are click-through
signed off by the owner.** Retargeting a launcher to an unvalidated
window breaks the live desktop; deleting the C removes the rollback.

---

## 0. Precondition — owner sign-off checklist

Open each new window via its `button-pal.sh` (all are parallel today,
nothing is wired into a menu) and confirm it behaves like the old one:

| window | launcher | check |
|---|---|---|
| db-hq-pal | `&.hq-apps/db-hq-pal/button.sh <house>` | 15 tabs switch, record list, panel fields. (Field editing / CE editor still TODO — see PROGRESS-events-hq for the shared picker to reuse.) |
| events-hq | `&.widgits/events-hq/button.sh <entity_dir>` (already retargeted) | view+page tabs, command list, Add Command → pick → fields → Save, Play, delete |
| bookmarks | `&.widgits/bookmarks/button-pal.sh <house>` | list + New+ cli_io |
| stats-hq | `&.hq-apps/stats-hq/button-pal.sh <house>` | session list + aggregate panel |
| taskbar-settings | `&.widgits/taskbar-settings/button-pal.sh <house>` | 12 swatches, ring on chosen, opacity ±, apply |
| network_browser | `&.hq-apps/network/button.sh <house>` (already retargeted) | fetch, links, back/fwd, tabs, address |
| chat-hai | `&.hq-apps/chat-hai/button-pal.sh <house>` | sessions, transcript, Stop/Speed |
| palettes | `&.widgits/palettes/button-pal.sh emojis <house>` / `... elements <house>` | 6-wide sprite grid, click places a tile |

Each PROGRESS-*-xhtpm.md lists that window's known gaps — accept or fix
before sign-off.

---

## 1. Launcher rewire

**DONE 2026-09-03** (verification run passed first). Each keeps an
`X_ROLLBACK=1` env fallback to the pre-port window:

| launcher | env | routes to |
|---|---|---|
| `&.hq-apps/chat-hai/button.sh` | `CHAT_HAI_ROLLBACK` | `chat-hai/button-pal.sh` |
| `&.hq-apps/stats-hq/open_stats_hq.sh` | `STATS_ROLLBACK` | `stats-hq/button-pal.sh` |
| `*.monads/*.livedesk-taskbar/ops/button_taskbar_settings.sh` | `TBSET_ROLLBACK` | `&.widgits/taskbar-settings/button-pal.sh` |
| `&.widgits/bookmarks/bm_menu.sh` (launch path only; verbs unaffected) | `BM_ROLLBACK` | `bookmarks/button-pal.sh` |
| `&.widgits/events-hq/button.sh` | `EZ_PKG_DIR` set | already retargeted (earlier) |
| `&.hq-apps/network/button.sh` | `NB_ROLLBACK` | already retargeted (earlier) |

**Still on the old path (deliberate):**
- `#.desktop/livedesk_taskbar.pdl` `livedesk:open-palette:<cat>` — handled
  by the taskbar manager verb, which is in `khtpm_taskbar_manager.c`
  (concurrently edited). Fold the emojis/elements retarget into the
  cleanup PR when that file is being touched. rmmv stays old.
- `#.desktop/livedesk_launchers.pdl` `launcher_db` — waits on db-hq-pal
  DB-record field editing.

### (original notes) — old sub-launcher change plan

Point the real launch paths at the new windows, keeping the old as a
one-env-var rollback. NONE of this touches `khtpm_core_render.c`.

| launcher | change |
|---|---|
| `#.desktop/livedesk_taskbar.pdl` `palettes_menu_*_cmd` (`livedesk:open-palette:<cat>`) | the taskbar manager verb handler that runs `palettes_menu.sh <cat>` → run `&.widgits/palettes/button-pal.sh <cat> <house>` for `emojis`/`elements`; leave `rmmv`/`piececraft` on the old path (not ported) |
| `#.desktop/livedesk_launchers.pdl` `launcher_db` (`*.monads/*.muchi-pet/ops/open_db_hq.sh`) | → `&.hq-apps/db-hq-pal/button.sh` **only after** db-hq-pal field editing lands |
| `&.hq-apps/stats-hq/open_stats_hq.sh` | exec `button-pal.sh` (guard with `STATS_ROLLBACK=1` → old path), same shape as events-hq/network `button.sh` |
| bookmarks / swatch-picker / chat-hai launchers (`bm_menu.sh` / `button_taskbar_settings.sh` / `chat-hai/button.sh`) | prepend `[ -z "${*_ROLLBACK:-}" ] && exec sh button-pal.sh "$@"` — the exact pattern already in `&.widgits/events-hq/button.sh` and `&.hq-apps/network/button.sh` |
| taskbar `db` menu | already has a `db-hq (PAL)` row (`livedesk:open-db-hq-pal`); once db-hq-pal is the default, drop the old `db` row or relabel |

Rule: every rewired launcher keeps `X_ROLLBACK=1 sh <launcher>` working
against the pre-port window until §2 deletes it.

---

## 2. Delete the per-app C  *(HIGH RISK — one careful PR, NOT a blind subagent)*

`khtpm_core_render.c` is ~18.6k lines and concurrently edited. The
`g_is_*` families:

| flag | ~refs | files/functions to remove |
|---|---|---|
| `g_is_events_hq` + `evhq_*` | ~676 | `evhq_layout_pass`, `evhq_assign_nav_indices`, `evhq_activate_elem`, `evhq_handle_key`, `evhq_describe_command`, `evhq_*` picker/registry state, `evhq_launch_module`, the `class=="events-hq-window"` detection, argv[3]/[4] reinterpret block, `pieces/dashboard.chtpm` + `pieces/picker.chtpm` + `EZ_PKG_DIR` guard in `button.sh` |
| `g_is_db_hq` + `dbhq_*` | ~1350 | `dbhq_layout_pass`, `dbhq_assign_nav_indices`, `dbhq_activate_elem`, `dbhq_inject_palette_tiles` (palettes), tab arrays, `g_is_stats_hq`/`g_is_palettes`/`g_is_bookmarks`/`g_is_swatch_picker` (all ride `g_is_db_hq`), `open_db_hq.sh`, `&.hq-apps/db-hq/dashboard.chtpm` |
| `g_is_stats_hq` / `g_is_palettes` / `g_is_bookmarks` / `g_is_swatch_picker` | folded into the above | their class-detection lines (~17748-17780), per-flag layout/nav/draw branches, `SWATCH_COLS` etc. only if nothing generic still needs them |

**BUT:** several "generic" capabilities the ports rely on are physically
in `dbhq_*`-named code:
- `dbhq_serialize_frame_elem` / `dbhq_serialize_frame_subtree` /
  `dbhq_paint_frame_line` — the frame-file round trip EVERY default-mode
  window uses. **Keep** (rename to `kh_*` if you like, don't delete).
- the swatch-grid layout (`class="swatch"`, ~8722) — palettes +
  taskbar-settings depend on it. **Keep.**
- `<tabbar>` multi-row layout, `kh_elem_in_scope`, `${ARG3}` hook,
  `<cli_io>` in `<scrolllist>` — all generic, keep.

**Procedure:**
1. One `g_is_*` at a time, its own commit. Start with `g_is_events_hq`
   (self-contained, own binary/manager).
2. `grep -n 'g_is_events_hq\|evhq_' khtpm_core_render.c` → delete each
   branch, compile after every few deletions.
3. Delete the forward decls + the functions once no refs remain.
4. Build the strip (`build_khtpm_strip.sh`), relaunch, open db-hq-pal +
   the strip + one entity menu — confirm nothing regressed.
5. `git rm` the dead `.chtpm` + `button.sh` rollback bits for that app.
6. Repeat for `g_is_db_hq` (bigger; do `g_is_palettes` /
   `g_is_bookmarks` / `g_is_stats_hq` / `g_is_swatch_picker` first since
   they ride it and are smaller).

Expected: −1500 to −2000 lines net from `khtpm_core_render.c`.

---

## 3. After

- Rename any surviving `dbhq_*` shared helpers to `kh_*`.
- Update `CENTROID_GOLD_STD.md`: static template + projector is THE
  pattern; `g_is_<app>` is banned (it already was — now it's true).
- Fold the per-app `PROGRESS-*-xhtpm.md` notes into one closed record.
