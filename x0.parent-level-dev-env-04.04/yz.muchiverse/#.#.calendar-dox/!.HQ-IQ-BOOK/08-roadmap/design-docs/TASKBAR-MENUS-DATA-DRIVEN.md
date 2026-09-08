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

### 2.1 `#.desktop/livedesk_menus.pdl` — the menu definition

One SECTION per cell (by stable cell id, not `which` number), each row
= `label` + `action`. Example (syntax matches the house SECTION-row
`.pdl` the theme/taskbar readers already parse):

```
SECTION file
  row  new-desk   builtin:new-desk
  row  save       builtin:save
  row  save-as    widget:file-explorer SAVE  @sessions
  row  load       widget:file-explorer LOAD  @sessions
  row  notes      builtin:notes
  row  dir        builtin:open-dir @package
  row  cancel     builtin:close

SECTION desk
  row  rename     builtin:rename-desk
  ...
```

### 2.2 Action verbs (the whole vocabulary — small and generic)

| verb form                              | meaning |
|----------------------------------------|---------|
| `builtin:<name>`                       | one of a **fixed, small** set the manager implements in C (close, save, new-desk, rename-desk, notes, open-dir, quit, reset-entities). These are the genuinely-internal ops that touch live desk state. |
| `widget:<widget-name> <MODE> <start>`  | spawn a `&.widgits/<widget-name>` via its `button.sh`, in `<MODE>`, rooted at `<start>` (a token like `@sessions`, `@package`, `@house`, or a literal path). The widget returns a pick through the **generic** `fe_request.txt → result_file` round-trip (see §3). |
| `script:<rel-path> [args…]`            | `setsid nohup sh <house>/<rel-path> …` — the escape hatch for genuinely one-off shell glue, but now it's *one* code path, not 40. |
| `shell:<cmd>`                          | raw command (today's `m->command[0]` fallback), unchanged. |

Everything currently spelled `livedesk:save-as`, `livedesk:load`,
`livedesk:open-settings`, `livedesk:open-session:<id>` … collapses into
those four forms.

### 2.3 Manager changes

- `ktb_load_menus(s)` — parse `livedesk_menus.pdl` once at startup +
  on change (reuse the existing marker/`reparse` pattern) into
  `s->menus[cell][row]`.
- `ktb_hq_open(s, which)` — look up `cell = ktb_cell_id(s, which)`,
  copy that section's rows into `s->hq_menu[]`. **Delete** the ~20
  `livedesk_build_*_menu()` builders and the `100/101/102`
  replace-in-place sub-list mechanism.
- **FLAT — no nested sub-menus** (decided 2026-09-08). Every cell is a
  single one-level row list ending in `cancel`. Whatever the old
  `100/101/102` sub-lists exposed (the file-cell session picker, the
  new-desk/save/save-as/load split) is now either a flat row on the
  parent list or a `widget:`/`script:` row that opens a real window.
  This removes the entire class of "open menu replaced in place → nav
  latches on the parent cell" bugs (load, save-as, and any future one)
  by construction — there is no replace-in-place code left to latch.
- `ktb_hq_activate()` — one `switch` on the **verb prefix** (`builtin` /
  `widget` / `script` / `shell`), ~4 cases. The `builtin` case has the
  only remaining per-name `strcmp` chain, and it's short by
  construction.
- **Generic pending-pick poll** — `ktb_poll_widget_result(s)` replaces
  the per-feature `ktb_poll_pending_session_open` /
  `ktb_poll_pending_save_as`: the launcher script writes
  `#.desktop/livedesk_widget_result.txt` as
  `verb=<what-to-do>\nvalue=<pick>`; the manager routes on `verb`.

---

## 3. Fold in the File Explorer round-trip (already built 2026-09-08)

The picker contract shipped for "load" is already the right generic
primitive — keep it, just stop wrapping it per-feature:

- `&.widgits/file-explorer/ops/file_explorer_manager.c` reads
  `fe_request.txt` (`mode` / `start_dir` / `result_file`), writes the
  picked absolute path to `result_file`.
- `&.widgits/file-explorer/fe-pick.sh <LOAD|SAVE> <start_dir>` — modal
  wrapper, prints the pick.

Under the target design there is **one** launcher shim (not
`pick-session.sh` + `save-as-session.sh` + pc-hq's inline copy):
`#.desktop/scripts/menu-widget.sh <widget> <mode> <start> <result-verb>`
→ runs `fe-pick.sh`, resolves the pick, writes
`livedesk_widget_result.txt` with the given `verb`. `widget:` rows just
name the verb they want back (`open-session`, `save-as`, `load-map`…).

pc-hq's `file-hq` verb collapses to the same `menu-widget.sh` call with
`result-verb=load-map` and a pc-hq-side poll — or pc-hq keeps its own
tiny consumer, but calls the shared `fe-pick.sh` (it already does as of
2026-09-08).

---

## 4. Migration (safe, incremental)

1. Land `livedesk_menus.pdl` + `ktb_load_menus` + the 4-way verb
   dispatch **alongside** the existing builders. New `pdl` wins when a
   section exists; fall back to the C builder when it doesn't.
2. Move cells over one at a time (`file` first — it's the buggy one),
   deleting each `livedesk_build_*_menu()` as its section lands.
3. When the last builder is gone, delete the `which==100/101/102`
   replace-in-place path and the fallback.
4. `03-pitfalls/HOUSE_CODE_PITFALLS.md`: new entry — "menu behavior is
   data; adding a `strcmp(m->command…)` branch is the smell."

## 5. Open questions for review

- ~~Nested sub-menus: keep or flatten?~~ **DECIDED 2026-09-08: FLATTEN.**
  No nesting, no `back` rows, no replace-in-place. See §2.3.
- **`builtin` set boundary**: is `notes` / `open-dir` really internal,
  or should those also be `script:`? Leaning: `open-dir` and `notes`
  become `script:` (they already are `sh …notes.sh`), leaving
  `builtin` = only ops that mutate live desk/session state.
- **Cell identity**: `ktb_cell_id()` currently returns a name for real
  cells but falls back to a `switch (which)` for some — need every cell
  to have a stable string id for the `pdl` to key on.
- Do the **ASCII strip renderer** (`khtpm_strip_render_ascii.c`) and
  the X11 path share the menu model, or does ASCII need its own read?

---

## 6. Scope decision (fill in)

- [ ] Design approved as-is
- [ ] Approved with changes: …
- [ ] Do the interim C `save-as` fix now too (unblocks today), refactor after
- [ ] Hold all save-as work until the refactor lands

The interim `save-as` fix (a `script:`-style branch + `save-as-session.sh`
+ `ktb_poll_pending_save_as`) is **written but uncommitted** on `claude`
as of 2026-09-08 — it builds and works, it's just more of what this doc
wants to delete. Keep or drop per the boxes above.
