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
- [ ] side-by-side parity pass vs the OLD window on a real entity (owner click-through)
- [ ] per-field command editor (new + edit), delete-command, view-mode content swap
- [ ] **delete `evhq_*` / `g_is_events_hq` (~676 refs in khtpm_core_render.c)** -
      deliberately NOT done here. Big surgery on a concurrently-edited file;
      needs owner sign-off (§8 step 8) and is safest as its own PR (design
      doc §8 step 10 says follow-up commit). Old `pieces/dashboard.chtpm` +
      the `evhq_*` C stay as the working rollback until then.

## Log

### 2026-09-03 kickoff
- Read design doc, old `button.sh`, `dashboard.chtpm`, `picker.chtpm`,
  manager schema, `evhq_describe_command`. Test entity:
  `44.xyz.01.00/common_events/greet_player` (3 cmds, TRIGGER|Autorun).
