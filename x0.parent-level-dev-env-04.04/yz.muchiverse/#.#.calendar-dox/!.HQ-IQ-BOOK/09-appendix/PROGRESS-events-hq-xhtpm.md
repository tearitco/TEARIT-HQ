# PROGRESS — events-hq → static xhtpm + projector

**Branch:** `chtpm-var-substitution`
**Design:** `08-roadmap/design-docs/EVENTS-HQ-XHTPM-PORT.md` (read it first)
**Started:** 2026-09-03

## Contract recap

- New window: `class="events-hq-pal"` (NOT `events-hq-window` → that trips `g_is_events_hq`).
- Static `events-hq.xhtpm`; data via `$PKG_DIR/.hq_manager/ui.txt` (per-entity, multi-instance safe).
- Compile chain (`compile_page`, IR→pal→cmd_N.sh) stays in `khtpm_events_hq_manager.+x`. Untouched.
- Projector = `.pal` (`pal/evhq_projector.pal`), never bash.
- Old `button.sh` + `pieces/dashboard.chtpm` stay as rollback until dumps match.

## Manager published schema (what the projector reads)

`$PKG_DIR/.hq_manager/pages.state.txt` — one page-dir name per line (`page_1`…).
`$PKG_DIR/.hq_manager/selected_page.txt` — one line, selected page-dir name (action.sh writes).
`$PKG_DIR/.hq_manager/page.state.txt` — for the selected page:
```
TRIGGER|<trigger>
SWITCH|<name>              (optional)
CMD|<id>|<type>|<params>   (params pipe-separated, key=val)
SCRATCHBLOCK|<key>|<val>   (scratch view only)
```
`$PKG_DIR/.hq_manager/action.txt` — pending request the manager polls (`append:<type>|…`).

## The ONE generic renderer hook added (not events-hq-specific)

`main()` argv[3]: if it is an existing directory, treat it as an
"instance dir" —
- builtin `${ARG3}` resolves to it (like `${PID}`),
- `g_extra_vars_path = "<argv3>/.hq_manager/ui.txt"` is appended to the
  template's own `vars=` list (content-hash reparse already covers it),
- every `<module>` fork gets `KHTPM_ARG3=<argv3>` in its env.

Guarded so the existing `argc>=5 → g_win_x=atoi(argv[3])` popup path
only runs when argv[3] is NOT a directory.

## Launch shape

`button-pal.sh <package_dir> [house_root]`:
1. `PKG_DIR=<package_dir>/event_pkg`, `LABEL=basename`.
2. start `khtpm_events_hq_manager.+x  <house>  <PKG_DIR>  <LABEL>` (background, per-entity guard by exact PKG_DIR match — reuse `same_entity_pids`).
3. `setsid khtpm_core_render.+x  <house>  events-hq.xhtpm  <PKG_DIR>  <LABEL>`.
   - renderer forks the `<module>` projector `prisc+x.+x pal/evhq_projector.pal`, which reads `KHTPM_ARG3` = `<PKG_DIR>`.

## Status

- [x] renderer hook (`g_arg3_dir` / `${ARG3}` / `g_extra_vars_path` / `KHTPM_ARG3`)
- [x] generic: `layout_sidebar_panel` lays out **every** `<tabbar>` child of the
      page, stacked; per-group active-tab resolution (a click in one tabbar
      no longer un-highlights the other)
- [x] `events-hq.xhtpm` + `events-hq.css` — trigger LEFT (sidebar),
      command list RIGHT (panel), view-tabs + page-tabs stacked on top
      (mirrors old `pieces/dashboard.chtpm`)
- [x] `pal/evhq_projector.pal` — pages + trigger + commands, read-only,
      params truncated at first `|` (frame-dump separator; registry
      pretty-print is phase 2)
- [x] `ops/evhq_action.sh` — `view N`, `page <name>` (edit/picker/play = phase 2 stub)
- [x] `button-pal.sh` parallel launcher (starts manager + renderer + module projector)
- [x] headless: `greet_player` renders clean, no overlap (PNG verified 2026-09-03)
- [x] **projector is now C** (`ops/evhq_projector.c` + `build_evhq_projector.sh`) -
      the `.pal` version is deleted. Registry (`#.ref/menu/event_commands.registry.pdl`)
      is parsed for `type -> LABEL + PARAMS`, so command rows read
      `Change Gold (amount: 1016)` etc. - mirrors old `evhq_describe_command`.
      Content-gated write (only when ui.txt changes).
- [x] **picker**: `+ Add Command` -> `evhq_action.sh picker open` -> projector
      lists all 44 registry types in the right panel (`show="${picker_open}"`
      swaps the command list for the type list; Cancel row on top). Click a
      type -> `evhq_action.sh pick <type>` -> writes `append:<type>|` to
      `action.txt` + closes picker. Verified: manager appended `CMD|4|change_gold|`,
      recompiled, projector showed the new row.
      NOT a floating centered overlay (full-panel swap) - polish later.
      NOT a per-field editor - appends with empty params (manager defaults).
- [x] **Play**: `evhq_action.sh play` -> `action.txt=play` -> manager runs
      `play_event.sh`. Verified via master_ledger.txt.
- [x] **phase 3 retarget**: `button.sh` execs `button-pal.sh` for normal
      positional invocation (one guard line; `EZ_PKG_DIR=... sh button.sh run`
      still hits the old `pieces/dashboard.chtpm` for A/B). Rollback = delete
      the 3-line guard block.
- [x] per-field command editor (new + edit), delete-command, view-mode
      content swap — DONE (see the phase-3 Log entry below). `evhq_action.sh`
      verbs pick/edit/field/commit/cancel-fields/del/play; `evhq_projector.c`
      emits `picker_open`/`fields_open`/`list_open`, `pk_<i>_*`/`n_picker`,
      `f_<i>_name/_prompt/_value`/`n_fields`/`editor_type`/`editor_title`.
      Registry-driven from `#.ref/menu/event_commands.registry.pdl`.
- [x] **`evhq_*` / `g_is_events_hq` / `dbhq_ce_*` DELETED from
      `khtpm_core_render.c`** — 2026-09-03, commit `81cedb8f` on
      `chtpm-delete-per-app-c` (−2465 lines). `g_is_events_hq` kept as
      `static const int = 0` so pure-flag guards constant-fold. The old
      embedded Common Events editor (`dbhq_ce_*`) is gone too; db-hq-pal
      opens `events-hq.xhtpm` for CE editing (two compliant windows).
      Old `pieces/dashboard.chtpm` + `button.sh` `EZ_PKG_DIR` guard stay
      as rollback but the `class="events-hq-window"` C path is now dead.
- [ ] side-by-side parity pass vs the OLD window on a real entity
      (owner click-through) — the one remaining verify: add a command,
      edit a field, save, delete, in a live events-hq.xhtpm window.

## Log

### 2026-09-03 kickoff
- Read design doc, old `button.sh`, `dashboard.chtpm`, `picker.chtpm`,
  manager schema, `evhq_describe_command`. Test entity:
  `44.xyz.01.00/common_events/greet_player` (3 cmds, TRIGGER|Autorun).

### 2026-09-03 phase 3 — field editor + delete + view swap
- **Per-field command editor.** `pick <type>` (or clicking a command
  row = `edit <id>`) now writes `.hq_manager/editor.txt`
  (`mode=fields` / `type=` / `edit_id=`), not a bare `append:`. The
  projector reads the registry `PARAMS` + `FIELD1/FIELD2` prompts and
  emits `fields_open=1`, `n_fields`, `f_<i>_name/_prompt/_value`
  (value pre-filled from `pending_fields.txt`, else from the existing
  CMD when editing). The template's view C is one `<cli_io>` per field;
  Enter on a field runs `evhq_action.sh field <type> <name> ... <value>`
  → upsert into `pending_fields.txt`. **Save** → `evhq_action.sh commit`
  assembles `append:<type>|k=v|…` or `edit:<id>|<type>|k=v|…` into
  `action.txt`. **Cancel** clears `editor.txt` + `pending_fields.txt`.
- **Delete.** Each command row has a `- delete #<id>` sub-row →
  `evhq_action.sh del <id>` → `delete:<id>` in `action.txt`.
- **View swap.** `list_open = !picker_open && !fields_open`; the three
  right-panel views are `show=`-gated on those flags.
- Verified end-to-end on `greet_player`: pick Change Gold → type 250 →
  Save → manager appended `CMD|5|change_gold|amount=250`; edit CMD 1 →
  `edit:1|change_gold|…`; del CMD 1 → count 5→4.
- Still open: view-mode (Scratch/Blueprints) content swap; floating
  centered overlay; the `evhq_*` / `g_is_events_hq` C deletion.

### 2026-09-03 view-mode content swap
- `evhq_action.sh view N` -> `.hq_manager/view.txt`. Projector reads it:
  `view=0|1|2`, `is_scratch`, `is_blueprints`; when not Scripting it
  forces `picker_open=fields_open=0` so Scratch/Blueprints own the panel.
- Scratch (view 1): the manager's `SCRATCHBLOCK|<key>|<val>` rows for
  the page, listed read-only (`scratch_count` / `scratch_<n>_text`, `|`
  mapped to `:`). Blueprints (view 2): a "not built" stub.
- Template `<panel>` now has 5 show=-gated views: command list /
  picker / field editor / scratch / blueprints.
- Verified: view 0 -> `list_open=1`; view 1 -> `is_scratch=1`,
  `list_open=0`, panel shows the 4 SCRATCHBLOCK rows.
- events-hq is now at feature parity with the old window minus a real
  visual block editor (Scratch/Blueprints stay read-only stubs, as they
  were pre-port). Remaining: the `evhq_*` / `g_is_events_hq` C deletion.
