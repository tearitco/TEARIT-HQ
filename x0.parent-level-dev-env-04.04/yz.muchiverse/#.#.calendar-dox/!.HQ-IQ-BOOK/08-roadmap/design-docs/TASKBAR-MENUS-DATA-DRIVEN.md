# Taskbar / HQ menus should be data-driven (not 81 branches of C)

Status: **DESIGN — not started.** Written 2026-09-08 after the "save-as
gets stuck on 3 like load did" report and the follow-up:

> "we shouldn't be hardcoding tb behavior either. it should all be
>  layout (new layout?) pdl / manager driven. get it?"

Yes. This doc is the plan to get there. Decide scope after reading.

---

## 1. Where we are now

The strip / HQ menu system in
`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c`
is almost entirely hardcoded C:

- **~20 `livedesk_build_*_menu()` functions** — each one hand-writes its
  rows: `snprintf(menu[n].label, ..., "save-as"); snprintf(menu[n].command,
  ..., "livedesk:save-as"); n++;` repeated for every row of every cell.
- **`ktb_hq_open(s, which)`** — a `switch (which)` (1..15 real cells,
  100/101/102 internal sub-lists reached by *replacing the open menu in
  place* — the fragile shape behind both the "load" and "save-as"
  stuck-on-3 bugs).
- **`ktb_hq_activate()` / the dispatch** — **81** `else if
  (strcmp(m->command, "livedesk:xyz") == 0)` branches, each with bespoke
  C: some call an internal function, some `ktb_system_recorded()` a
  shell string, some `ktb_hq_open(s, 100+)` to swap the menu.

What already IS data-driven (precedent to extend, not invent):
`#.desktop/livedesk_launchers.pdl` (cell → launcher script),
`livedesk_theme.pdl`, `livedesk_shortcuts.pdl`, `livedesk_taskbar.pdl`
(SECTION-row `.pdl` convention, read by `read_key_value()`).

### Why this keeps biting

Every new menu action = a new C branch + a rebuild + a relaunch, and
the "sub-list by replacing the menu in place" trick has no clean exit,
so nav latches on the parent cell. "load" was fixed by special-casing
it to launch a real window; "save-as" then hit the identical bug. We
are fixing symptoms one `strcmp` at a time.

---

## 2. Target shape

### 2.0 ALIGN with the existing convention — do NOT invent a new file

**Correction 2026-09-08 (after reading `TASKBAR-MENU-ARCHITECTURE.md`
in full).** This house already has the data-driven menu convention and
a standing instruction to finish rolling it out:

- Menu rows live in **`#.desktop/livedesk_taskbar.pdl`** as
  `SECTION | <cell>_menu_<N>_label | <text>` +
  `SECTION | <cell>_menu_<N>_cmd | <command>` rows, read at cell-open
  time via `read_key_value()` — no recompile to add/edit/reorder.
- Already converted: `hq_menu_*`, `file_menu_*`, `desk_menu_action_*`,
  `palettes_menu_*`, `network_menu_*` (their `livedesk_build_*_menu()`
  keeps the hardcoded rows only as a cache-miss fallback).
- Still C-hardcoded (the debt list in `TASKBAR-MENU-ARCHITECTURE.md`
  §"Standing refactor debt", 2026-08-24): `user`, `player`, `db`,
  `pals`, `toys`, `clock`, `ai`.

So this refactor is **not** a new `livedesk_menus.pdl`. It is:
1. Finish converting the debt-list builders to the `<cell>_menu_N_*`
   read loop (`livedesk_build_hq_menu()` is the reference shape).
2. **Flatten** — delete the `which==100/101/102` replace-in-place
   sub-lists (`session` picker, `db_ez_sections`, `db_common_events`).
3. Add **one** new `_cmd` prefix — `widget:` — so a menu row can open
   a helper window and act on its result without a bespoke `strcmp`
   branch. Everything else already works: `livedesk:<name>` = internal
   dispatch (existing), a bare string = shell (existing fallback via
   `ktb_action_portable`).

### 2.1 `_cmd` value grammar (three forms, two already exist)

| `_cmd` value                          | handler | status |
|---------------------------------------|---------|--------|
| `livedesk:<name>[:<arg>]`             | the existing `ktb_hq_activate()` dispatch chain — internal ops that touch live desk/session state (`save`, `new-desk`, `rename-desk`, `quit`, `reset-entities`, `open-settings`, …) | **exists** |
| *(anything else)*                     | shell, via `ktb_action_portable()` + `ktb_system_recorded()` — `setsid nohup` from house_root. This is where `notes` and `dir` land (`sh '<house>/#.desktop/scripts/notes.sh' '<cell>'`, `xdg-open .`) | **exists** |
| `widget:<name> <MODE> <start-token>`  | **NEW** — launch `#.desktop/scripts/menu-widget.sh <name> <MODE> <start> <result-verb>`; it runs the widget (File Explorer today), resolves the pick, writes `#.desktop/livedesk_widget_result.txt` (`verb=…\nvalue=…`); `ktb_poll_widget_result()` (main loop) consumes it and routes on `verb`. `<start-token>`: `@sessions` / `@package` / `@house` / a literal path. | **build in this refactor** |

`widget:` replaces the two ad-hoc launchers added 2026-09-08
(`pick-session.sh` + `ktb_poll_pending_session_open`, `save-as-session.sh`
+ `ktb_poll_pending_save_as`) with one generic path.

### 2.3 Manager changes

- **Convert each debt-list builder** (`user`, `player`, `db`, `pals`,
  `toys`, `clock`, `ai`) to the `livedesk_build_hq_menu()` shape: a
  loop reading `<cell>_menu_<N>_label` / `_cmd` from
  `#.desktop/livedesk_taskbar.pdl` via `read_key_value()`, keeping the
  current hardcoded rows only as the `count == 0` fallback.
  Directory-scanning builders (`toys`, `session`) keep the scan but
  emit rows into the same `HQMenuItem[]` shape.
- **FLAT — no nested sub-menus** (decided 2026-09-08). Delete the
  `which==100/101/102` branches in `ktb_hq_open()` and
  `livedesk_build_session_menu` / `_db_ez_sections_menu` /
  `_db_common_events_menu`, plus the `ktb_hq_open(s, 100/101/102)`
  calls in `ktb_hq_activate()`. Their contents become either:
  - a `widget:` row on the parent cell (the `session` picker → the
    File Explorer, which "load" already does as of 2026-09-08), or
  - a flat `livedesk:<name>` row, or
  - dropped if obsolete (db-ez sub-lists — confirm with the user /
    grok first; db-hq is still a placeholder per DB-HQ-HANDOFF.md).
  This removes the whole "open menu replaced in place → nav latches on
  the parent cell" bug class (load, save-as, future) by construction.
- **`ktb_hq_activate()`** — add ONE branch, checked before the existing
  `livedesk:` chain: `if (strncmp(cmd, "widget:", 7) == 0)` → build and
  `ktb_system_recorded()` the `menu-widget.sh` call, `ktb_hq_close(s)`.
  The existing `livedesk:` chain and the bare-string shell fallback are
  untouched. No 4-way verb switch, no `builtin:`/`script:`/`shell:`
  prefixes — those were superseded by aligning to the existing grammar
  (§2.1).
- **`ktb_poll_widget_result(s)`** (main loop) — replaces
  `ktb_poll_pending_session_open` + `ktb_poll_pending_save_as` with one
  reader of `#.desktop/livedesk_widget_result.txt`
  (`verb=` / `value=`), routing on `verb` (`open-session` →
  `livedesk_load_session`, `save-as` → `livedesk_save_as_with_name`,
  `load-map` → pc-hq, …). The two interim pollers + their scripts
  (`pick-session.sh`, `save-as-session.sh`) are then deleted.

---

## 3. Fold in the File Explorer round-trip (already built 2026-09-08)

The picker contract shipped for "load" is already the right generic
primitive — keep it, just stop wrapping it per-feature:

- `&.widgits/file-explorer/ops/file_explorer_manager.c` reads
  `fe_request.txt` (`mode` / `start_dir` / `result_file`), writes the
  picked absolute path to `result_file`.
- `&.widgits/file-explorer/fe-pick.sh <LOAD|SAVE> <start_dir>` — modal
  wrapper, prints the pick.

There is **one** launcher shim (not `pick-session.sh` +
`save-as-session.sh` + pc-hq's inline copy):
`#.desktop/scripts/menu-widget.sh <widget> <mode> <start> <result-verb>`
→ runs `fe-pick.sh`, resolves the pick, writes
`livedesk_widget_result.txt` with the given `verb`. `widget:` rows just
name the verb they want back (`open-session`, `save-as`, `load-map`…).

pc-hq's `file-hq` verb keeps its own tiny consumer but calls the shared
`fe-pick.sh` (it already does as of 2026-09-08) — no change needed there.

---

## 4. Migration (safe, incremental) — each step builds + ships on its own

1. **`widget:` prefix + `menu-widget.sh` + `ktb_poll_widget_result()`**
   alongside the two interim pollers. Point `file_menu_*` `save-as`/
   `load` `_cmd` rows at `widget:file-explorer SAVE @sessions` /
   `widget:file-explorer LOAD @sessions`. Verify live. Then delete
   `pick-session.sh`, `save-as-session.sh`,
   `ktb_poll_pending_session_open`, `ktb_poll_pending_save_as`.
2. **Flatten**: remove `which==100/101/102` (`session` picker folds
   into step 1's `widget:`; `db_ez_sections` / `db_common_events`
   pending a keep/drop call from the user + grok).
3. **Convert the debt-list builders** one at a time to the
   `<cell>_menu_N_*` read loop: `user`, then `player`, `db`, `pals`,
   `toys`, `clock`, `ai`. Each = add PDL rows + swap the builder body,
   hardcoded rows kept as `count==0` fallback. Build + click-test each.
4. **`03-pitfalls/HOUSE_CODE_PITFALLS.md`**: new entry — "strip menu
   behavior is `livedesk_taskbar.pdl` data; a fresh
   `strcmp(m->command, "livedesk:…")` branch for a *launch* is the
   smell (internal desk-state ops still dispatch that way)."

## 5. Open questions for review

- ~~Nested sub-menus: keep or flatten?~~ **DECIDED: FLATTEN** (§2.3).
- ~~`builtin`/`script`/`shell` verb set?~~ **DROPPED** — aligned to the
  existing `livedesk_taskbar.pdl` `<cell>_menu_N_*` grammar instead
  (§2.0/§2.1); only new prefix is `widget:`.
- **db-ez sub-lists** (`which==101/102`): keep as `widget:`/flat rows,
  or drop while db-hq is still a placeholder? — needs user/grok.
- **Cell identity**: `ktb_cell_id()` returns a name only for cells
  declared in `#.desktop/livedesk_header_cell_ids.txt`; others fall
  back to a `switch (which)`. The PDL rows are keyed by the hardcoded
  `<cell>` name (`user_menu_*`, `player_menu_*`, …), so this doesn't
  block — but finishing `livedesk_header_cell_ids.txt` for all 15 is
  worth doing alongside.
- **ASCII strip renderer** (`khtpm_strip_render_ascii.c`): shares the
  manager's published frame, so menu rows flow through unchanged —
  confirm no separate menu read there.

---

## 6. Status

- **2026-09-08**: design aligned to `TASKBAR-MENU-ARCHITECTURE.md`.
  Flatten confirmed by user. Verb set dropped in favour of the
  existing grammar + one `widget:` prefix. Interim `save-as`/`load`
  fixes (`pick-session.sh`, `save-as-session.sh`, the two pollers) are
  committed on `claude` (`ea164d99`) and are **step-1 fodder** — the
  `widget:` prefix replaces them.
- Starting: **step 1** (`widget:` prefix).
