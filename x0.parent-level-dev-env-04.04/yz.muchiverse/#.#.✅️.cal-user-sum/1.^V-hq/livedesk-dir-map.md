# livedesk-dir-map.md — directory map of everything meaningful to the livedesk toolbar

Purpose: a real, current inventory of where the pieces actually live, so an xyzfs-migration
decision can be made with the full scope in view, not guessed at. Written 2026-08-17. Everything
below is under the house root:
`.../x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.17/` (shown as `<H>/` below).

**Real, confirmed finding driving this doc**: `<H>/xyzfs/` currently holds only 2 real apps
(`muchi-pet`, `livedesk-clock`), and their own shape is narrower than a full standalone project —
just an `ops/` dir of shared, callable command binaries, not a full game engine + window binary +
launcher tree. Everything else described below (taskbar, all 5 merged window apps, mutaclysm,
piececraft, etc.) still lives OUTSIDE `xyzfs/`, directly under `<H>/`.

---

## 1. The taskbar itself (root process, everything else launches from here)

`<H>/*.monads/*.livedesk-taskbar/`

| File | Role |
|---|---|
| `khtpm_strip_header.chtpm` | Layout for the persistent top header strip (HQ/USER/file/desks/.../toys/store/network/h-ai/date-time cells + popup submenu content) |
| `khtpm_strip_bottom.chtpm` | Layout for the separate bottom tab-bar window |
| `khtpm-strip-parser-design.md`, `khtpm-strip-parser-SCOPE.md` | Design/scope docs for the strip parser's own tag vocabulary |
| `README.md`, `walk-off-au5.md` | Taskbar-level notes |
| `ops/khtpm_strip_parser.c` | The real Xlib renderer for both taskbar windows (hq_win header + win bottom bar) — hit-testing, popup mapping, drag |
| `ops/khtpm_strip_layout.c` / `.h` | `LayDoc`/`LayElement` — the taskbar's OWN separate parser (flat-array, `${var}` substitution, `ACTIVATE`-scope), confirmed this session to be a DIFFERENT, more complete lineage than `khtpm_render_core.c`'s `Elem` tree. Deliberately NOT merged with the window-app family. |
| `ops/khtpm_strip_codes.h` | Cell index ↔ code constants |
| `ops/khtpm_taskbar_manager.c` / `.h` | The real, separate manager PROCESS (no Xlib) — owns `KtbState`, builds popup menu content, dispatches cell activation (`ktb_hq_open`/`ktb_hq_activate`), toys-cell scanning (`livedesk_build_toys_menu`) |
| `ops/khtpm_taskbar_manager_main.c` | Manager's own `main()` entry point |
| `ops/khtpm_render_core.c` | Shared `Elem`/`CssStyle` tree + hit-test/find helpers — used by the 5-app merged window family, NOT the taskbar's own `LayDoc` |
| `ops/khtpm_css_parser.c` / `.h` | Shared CSS parser for the `Elem` family (flexbox + descendant-selector support added this session for chat-hai) — **another agent may still be touching this, confirm before editing** |
| `ops/khtpm_draw_core.c` | Shared `draw_elem()`/`render_tree()`/`font_for()` etc. for the `Elem` family |
| `ops/khtpm_entity_menu_render.c` | **THE merged binary** — now serves ALL 5 window apps (entity-menu, taskbar-settings, db-hq, events-hq, chat-hai), mode-selected via `class=` on each app's own `<window>` tag. See §2. |
| `ops/khtpm_entity_menu_render.c.bak-*` | Pre-merge snapshots, kept as rollback points |
| `ops/khtpm_hq_render.c` | db-hq's ORIGINAL standalone renderer — **kept live, not archived**, because `stats-hq` independently still launches this exact file |
| `ops/khtpm_hq_render.c.bak-*` | Pre-Stage-3/pre-merge snapshots |
| `ops/khtpm_hq_manager.c` | db-hq's own manager process (separate from the taskbar's manager) |
| `ops/khtpm_taskbar_settings_render.c.bak-*` | Old standalone taskbar-settings renderer — superseded by the merged binary's swatch-picker mode, kept as `.bak` only (no longer a live `.c`) |
| `ops/apply_theme_op.c` | Real op binary — applies a bg/fg theme pair (fork/exec'd, not `#include`d) |
| `ops/tp_desktop_window_rgb.c` | Legacy entity-context-menu popup engine (the "old" ava right-click menu path; entity-menu now proven and taking over per-entity) |
| `ops/tp_range_grid.c`, `ops/tp_asset_to_sprite.c` | Misc real op binaries |
| `ops/build_*.sh` | Build scripts: `build_entity_menu.sh` (the merged binary), `build_db_hq.sh`/`build_db_hq_manager.sh`, `build_khtpm_strip.sh` |
| `ops/button_taskbar_settings.sh`, `ops/button_taskbar_stats.sh` | Launcher scripts, pgrep-disambiguated so they can't confuse instances |
| `ops/run_khtpm_strip.sh`, `ops/khtpm_vars.sh` | Taskbar launch/env plumbing |
| `ops/entity_menu_default.css`, `ops/taskbar_settings.chtpm`, `ops/taskbar_settings.css` | Layout/style for 2 of the 5 merged modes |
| `ops/stb_image.h` | Vendored PNG writer (shared by every khtpm app's own `dump_frame_png`-style debug capture) |

---

## 2. The 5 apps now living inside the ONE merged binary (`khtpm_entity_menu_render.c`)

Each still has its own `.chtpm`/`.css` and (for 3 of them) a separate MANAGER process — only the
RENDERER got merged into one binary. Old standalone renderer sources are either archived or kept
live as noted.

| App | Own dir | `.chtpm`/`.css` | Old standalone renderer | Status |
|---|---|---|---|---|
| entity-menu | (no own dir — invoked per-entity) | n/a, package-local `menu.chtpm` per entity | never had one (built merged from day 1) | live in merged binary only |
| taskbar-settings | `*.livedesk-taskbar/ops/` | `taskbar_settings.chtpm`/`.css` | `khtpm_taskbar_settings_render.c` | **archived** (`.zip`, see §4) |
| db-hq | `<H>/&.hq-apps/db-hq/` | `dashboard.chtpm`/`.css` | `khtpm_hq_render.c` | **kept live** — `stats-hq` still uses it directly |
| events-hq | `<H>/&.widgits/events-hq/` | `pieces/dashboard.chtpm`/`.css`, `button.sh`, own `ops/` (manager + shared parser copies) | `khtpm_events_hq_render.c` | **archived** |
| chat-hai | `<H>/&.hq-apps/chat-hai/` | `chat-hai.chtpm`/`.css`, `chat_hai_config.pdl`, `button.sh`, `pieces/`, `state/` | `chat_hai_hq_render.c` | **archived** |

`stats-hq` (`<H>/&.hq-apps/stats-hq/`) is a related-but-separate app: its own `dashboard.chtpm`/
`.template.chtpm`/`.css` + `open_stats_hq.sh`, but it launches the **original, unmodified**
`khtpm_hq_render.c` binary — this is real, current, in-use code, explicitly why that file was not
archived alongside the other 3.

---

## 3. `#.desktop/` — the taskbar's real, shared runtime state directory

`<H>/#.desktop/` — every file here is real, live, cross-process file-IPC, not a design artifact.
Grouped by what writes/reads it:

- **Taskbar strip itself**: `strip_state.txt`, `strip_history.txt`, `strip_frame_changed.txt`,
  `khtpm_strip_frame_history.txt`, `khtpm_strip_parser.log`, `livedesk_popup.lock`,
  `livedesk_registry.lock`, `livedesk_next_index.txt`, `livedesk_nav_claims.txt` +
  `livedesk-nav-claims/` dir, `livedesk_header_cell_ids.txt` (the id=→position map added this
  session for data-driven cell dispatch), `livedesk_master_ledger.txt`, `livedesk_open.txt`
- **Published header vars** (`${var}` substitution targets in `khtpm_strip_header.chtpm`):
  `strip_var_hqitems.txt`, `strip_var_username.txt`, `strip_var_datetime.txt`,
  `strip_var_avatar_dir.txt`, `strip_var_desks_label.txt`, `strip_var_file_label.txt`,
  `strip_var_shortcuts.txt`, `strip_var_tabs.txt`
- **PDL config** (flat `key=value`, real convention now shared/documented): `hq_ui.pdl`
  (`window_x`/`window_y`/`font_scale`/`focus_grab` for the merged -hq binary),
  `livedesk_taskbar.pdl`, `livedesk_launchers.pdl`, `livedesk_shortcuts.pdl`, `livedesk_theme.pdl`
- **Per-app agent-relay test files** (one per app, the real, ONLY sanctioned testing input path —
  never direct CLI calls): `livedesk_agent_relay.txt` (taskbar), `entity_menu_agent_relay.txt`,
  `taskbar_settings_agent_relay.txt`, `db_hq_agent_relay.txt`, `events_hq_agent_relay.txt`,
  `chat_hai_agent_relay.txt`, `open_hai_agent_relay.txt`, `lc_reminder_relay.txt`
- **Per-app frame history / debug**: `chat_hai_frame_history.txt`, `tp_taskbar_debug`,
  `tp_taskbar_restore.log`
- **db-hq specific**: `db_hq_action.txt`, `db_hq_agent_relay.txt`, `db_hq_common_events.state.txt`
- **Misc real dirs**: `entities/`, `events/`, `harnesses/`, `inbox/`, `clocks/`, `tiles/`, `x`,
  `build_uid_sprite/` (this session's PID+clock-emoji taskbar marker), `taskbar-settings-audit/`,
  `#.dox`, `README.txt`

**Real, load-bearing point for any xyzfs migration**: this whole directory is a flat, shared,
house-root-relative namespace. Every merged-binary mode's `relay_path()`/`dump_frame_png()` and
every manager's own state file resolve relative to `g_house_root` (the argv[1] passed to every
khtpm binary). Moving an app into `xyzfs/` without also deciding what happens to its `#.desktop/`
file(s) is the real crux of the migration question, not just "move the source directory."

---

## 4. Archived/legacy, already out of the live tree

- `<H>/_.ARCHIVED-pre-merge-legacy.zip` (223KB) — the 3 archived standalone renderers +
  build scripts + old compiled binaries (events-hq, chat-hai, taskbar-settings), with a
  `MANIFEST.txt` inside recording original paths + why each is dead. Unzipped copy deleted after
  zipping (dereferenced, not destroyed).
- 16 real `gl_mirror.c` legacy-GL copies house-wide (§5c.1 of `khtpm-merge-how2.md`) — separate
  from the taskbar proper, these are per-project display shims. First one converted this session:
  `101.mutaclsym🧟‍♂️️+18.01/system/x11_mirror.c` (new, plain Xlib), `gl_mirror.c` kept as automatic
  fallback. 15 more still legacy-GL, not yet converted.

---

## 5. Toys-cell (taskbar cell 11) discovery targets

The toys cell scans `<H>` (house root) top-level + `<H>/@.apps/` one level deep for a `toy.pdl`
file (opt-in by presence). Currently 3 real toys registered:

- `<H>/101.mutaclsym🧟‍♂️️+18.01/toy.pdl` → Mutaclysm (own top-level project, NOT under `@.apps/`)
- `<H>/@.apps/piececraft-xyz/toy.pdl` → Piececraft
- `<H>/@.apps/my-lawyer/toy.pdl` → My Lawyer
- `<H>/@.apps/my-chara-txt/toy.pdl` → My Chara

(A 4th, mutaclysm, is listed above under its own top-level entry, not `@.apps/` — the scan covers
both roots.)

---

## 6. Testing/dev tooling used against all of the above

`<H>/&.widgits/tile-picker/` — NOT taskbar-specific, but the real toolbox every khtpm-family test
this session ran through:

- `ops/tp_test_send_key.c`, `ops/tp_test_send_click.c` — real XTest-direct input injection (no
  `xdotool` dependency, built this session after finding `xdotool` isn't installed)
- `ops/khtpm_*` (render_core.c, css_parser.h, main.c, show_text.c, show_choices.c, choice_picker.c,
  plat.h) — this dir carries its OWN copies of the shared khtpm core files (real duplication,
  not yet reconciled with the taskbar's own `ops/khtpm_render_core.c` — worth knowing about if a
  future consolidation pass looks at "how many copies of `khtpm_render_core.c` exist")
- `ops/tp_*` — real desktop-entity placement/drag/arm-placer tooling (separate subsystem: desktop
  icon placement, not the taskbar strip)
- `system/` — its own copies of `gl_mirror`, `chtpm_parser_pal`, `keyboard_input`, `renderer`,
  `prisc+x` (same real binary family as mutaclysm's own `system/`, a SEPARATE copy for
  tile-picker's own test scenarios)
- `scenarios/test_tile_desktop_place.sh` — a real test harness script
- `KHTPM-ARCH.txt`, `chtpm-vs-khtpm.md`, `TILE_PICKER_DESIGN.md` — design/architecture docs

---

## 7. `xyzfs/` — the real, current per-user filesystem (for comparison)

`<H>/xyzfs/`

- `xyzfs/bin/muchi-pet/ops/` — shared RPG-Maker-style event-command ops (`mr_change_gold.c`,
  `mr_show_text.c`, `mr_show_choices.c`, `mr_monster_extract.c`, various launcher `.sh`/`.ps1`
  scripts) — NOT a full standalone project, a command-library shape
- `xyzfs/bin/livedesk-clock/` — similarly narrow: `reminder.css` + `ops/` (own `lc_clock.c`,
  `lc_reminder_popup.c`, a private copy of `khtpm_css_parser.c`/`.h`, `build_lc_clock.sh`)
- `xyzfs/users/<uuid>/` — real per-user real estate: `meta.txt`, `home/` (with `net`/`exchange`
  subdirs), `harnesses/` (test harnesses + results), and at least one user has a `projects/` dir

**The real open question this map exists to inform**: none of §1–§6 above (taskbar, the 5 merged
apps, mutaclysm, piececraft, the other toys, tile-picker's own test tooling) currently live inside
`xyzfs/` at all. Only 2 small, ops-only apps have made that move so far. Deciding whether/how the
much bigger tree in §1–§6 should migrate — and whether it should take the `xyzfs/bin/<app>`
ops-library shape or something closer to `xyzfs/users/<uuid>/projects/`, given the `#.desktop/`
shared-state coupling noted in §3 — is real, undecided platform architecture, not yet started.
