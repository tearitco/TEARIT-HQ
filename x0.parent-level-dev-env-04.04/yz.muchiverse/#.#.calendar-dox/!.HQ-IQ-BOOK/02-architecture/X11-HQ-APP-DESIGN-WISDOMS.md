# X11-HQ app design — wisdoms

Distilled from building File Explorer, pdl-read, text-edit-hq, csv-hq,
the `<grid>` element, and clipboard/selection (2026-09-05 session).
Read alongside `CENTROID_GOLD_STD.md` and the `khtpm-house-standards`
skill - this is the "how it actually feels to build one" layer.

## 0. Start from the runnable skeleton

`44.xyz.01.00/&.hq-apps/_template-hq/` is a **working** minimal HQ
window — full chrome, bottom-taskbar entry, a live-refreshing sidebar,
two `action=` buttons, a `<repeat>` list. `cp -r` it, rename three
things (its `README.md` lists them), and you have a registered window
on screen before writing any logic. It also serves as a smoke test for
the renderer + `<module>` + `vars=` + `<repeat>` + `action=` path.

**The five things that bite everyone (all learned live, proc-mon
2026-09-10):**

1. **`<sidebar>` + `<panel>` is mandatory for chrome + the taskbar
   entry.** `khtpm_core_render` only runs `layout_sidebar_panel()` —
   which synthesises the `X` / `_` chrome and writes
   `#.desktop/livedesk_hq_windows_<pid>.txt` (the strip entry) — when
   the page has **both**. A `<panel>`-only page renders with neither and
   is un-closable except by `kill`.
2. **`class="… database-window"` (or `palettes-pal`) makes it
   persistent** — otherwise the renderer closes the window after any
   `action=` fires.
3. **`<repeat bind="X">` → `${X.text}` → `${X_<n>_text}`.** The bind
   name must equal the key *prefix* the publisher writes. Mismatch =
   the list renders the right number of **blank** rows.
4. **`<module src="a b c"/>` is `execv`'d directly** (shebang honoured
   for a script), with `<house_root> <package_dir>` appended, and
   `SIGTERM`ed on window close. Omit the tag for a static window.
5. **HQ-menu `cmd` runs through `sh -c`** — a leading `&` in a path is
   job-control. An app under `&.hq-apps/` needs a glob-safe
   `_.monads/_.livedesk-taskbar/ops/open_<app>.sh` shim in its `.pdl`
   row (pattern: `open_mon.sh`). `vars=` and `${PKG}` resolve against
   the `.xhtpm`'s own directory.

## 1. The shape never changes

Every real X11-HQ app is the SAME three parts:

1. A **static `<name>-pal.xhtpm` + CSS** - layout only, no logic.
   Rendered by the shared `khtpm_core_render.+x`. You write zero C for
   the frontend.
2. A **real compiled manager** (`ops/<name>_manager.c`, a `<module>`)
   - owns ALL business logic/state, publishes it as `<name>_ui.txt`
   (plain `key=value` lines), polls `<name>_action.txt`
   (`seq=<n>\ncmd=<verb>`).
3. A **`button.sh`** launcher following the toy.pdl convention
   (`sh button.sh run`, derives `house_root` itself) + a `toy.pdl`
   (`SECTION | title | <name>` / `SECTION | launch | button.sh`).

The renderer draws whatever the manager published, via `vars=`. The
manager never touches X11, never draws. If you're about to write a
`parse_chtpm()` loop or a draw call in app code, stop - you're doing
it wrong.

## 2. Delegating the manager to a fresh agent WORKS

`file_explorer_manager.c`, `pdl_read_manager.c`, `text_edit_manager.c`,
`csv_hq_manager.c` were all written by a fresh Haiku agent against a
complete, self-contained spec (exact argv contract, every file format,
every command, escaping rules, the gcc line to self-check with). All
compiled clean on the first try. The manager is a pure, well-bounded
problem - it's the ideal thing to delegate. Then make the small
argv-order / integration corrections yourself.

**The spec must include:** the real `launch_module()` argv contract
(`<house_root> <package_dir> <extra_arg-if-id-set>` - NOT a made-up
`<pkg> <start> <mode>`), the exact `_ui.txt` keys, the exact
`_action.txt` `seq/cmd` shape, the content-escaping scheme if any
(`\n`->`\\n`), and "compile-check with THIS gcc line before you're
done".

## 3. Cross-app communication = shared published state, never direct IPC

File Explorer doesn't call pdl-read. It writes `result=` /
`result_action=LOAD` into its own `file_explorer_ui.txt`; any consumer
polls that file (`poll_file_explorer_pick()` pattern) and reacts. Same
as the drag-and-drop pid registry, the nav-master ledger, etc. A
launched standalone window + a polled result file is the whole
mechanism - no sockets, no callbacks, no coordination protocol.

## 4. `dispatch()` verbs are the app's action vocabulary

Add a `<PREFIX>_*` block in `khtpm_core_render.c`'s `dispatch()`
(alongside `FE_*`, `PDL_*`, `CSVH_*`, `TXT_*`, `MUS_*`). It writes the
app's `_action.txt`. For a verb that needs a LIVE `<cli_io>`/
`<text_area>`/`<grid>` value (the manager's last-published value is
stale the instant a human types), read it off the live tree with
`find_by_id(g_window, "<id>")` and either embed it in the `cmd=` line
(short, safe tokens) or dump it to a scratch file the manager reads
(arbitrary text). `FE_SAVEAS` and `CSVH_SETCELL` are the templates.
One `dispatch()` call runs exactly ONE verb - if two things must
happen (notify a manager AND switch page), the C does both directly.

## 5. Layout gotchas that cost real time

- **`<scrolllist>` is ONLY laid out by `layout_sidebar_panel()`**,
  which needs BOTH a `<sidebar>` AND a `<panel>` child of `<page>`. A
  bare top-level `<scrolllist>` never gets sized (x/y stuck, w/h=0).
  `<panel>` supports a nested `<scrolllist>` too (same
  `layout_fixed_rows_and_scrolllist()` helper).
- **`g_current_page` defaults to `"main"`**, not a template's first
  `<page>`. Name your first page anything else and the window is
  0-height.
- **`vars=` path doubling**: a relative token resolves against the
  loaded `.xhtpm`'s own dir FIRST. Writing
  `vars="&.widgits/x/file.txt"` while already inside `&.widgits/x/`
  doubles the path and silently substitutes zero vars. Use the bare
  filename.
- **`<tab>` scope-confine**: a `<tab>` only traps nav into a container
  when it has an explicit `target_id=` (db-hq/events-hq content tabs).
  A plain menu-bar action `<tab>` (New/Open/Save, no target_id) must
  NOT confine, or the app's main content (a `<text_area>`/`<grid>` in
  `<panel>`) becomes unreachable.
- **A tall element (`<text_area>`, `<grid>`) needs its layout height
  to match EXACTLY what its own draw code consumes** - the generic
  nav-focus box is sized from the laid-out height; a mismatch leaves
  it floating past the real content. Two hard-coded copies of the
  cell/status pixel constants kept in sync deliberately.

## 6. `label=` is not newline-safe; `content=` is

Multi-line text in `label=` corrupts the frame serialize/reparse round
trip past that element. For a `<text_area>`, set its buffer from a var
via `content="${var}"` (which IS newline-escaped). Any per-element
runtime field you add MUST go into BOTH `kh_serialize_frame_elem()`
(write) and `kh_paint_frame_line()` (parse) or it never reaches
`draw_elem()`. Label-shaped fields that can hold a literal `|` need
`frame_field_escape_pipe` (this bit `label` itself - fixed).

## 7. Testing: relay, not xdotool

Drive a khtpm_core_render.c window via
`#.desktop/entity_menu_history/<RENDER-pid>.txt` - one line per event
(`KEY_PRESSED: <decimal>`, `MOUSE_EVENT: <btn> <x> <y> <press>`, bare
decimals, `#`-comments). NOT xdotool/XTest (the house-standards skill
has the full section; the k9 testing doc has the history). Real
hardware CLICKS aren't delivered as `ButtonPress` to override-redirect
windows on this Mutter/XWayland desktop - mouse needs `XQueryPointer`
polling. Real KEYS do reach `handle_key()`.

**On this shared desktop specifically**: relay tests frequently
collide with a live interactive session. Kill every stray instance
first (`ps aux | grep <binary>`), confirm exactly one, then drive it.
The frame file is `entity_menu_frame_<RENDER-pid>.txt` (render pid,
not manager/shell pid). Verify state changes from the frame file /
the published `_ui.txt`, not just a PNG (PNG dumps can look stale).

## 8. Rebuild + restart flow for shared render changes

Editing `&.widgits/_shared-lib/khtpm_render_core.c` /
`khtpm_draw_core.c` / `_.monads/_.livedesk-taskbar/ops/
khtpm_core_render.c` needs `sh build_core_render.sh` AND `sh
build_khtpm_strip.sh`, then restart the taskbar (`sh
run_khtpm_strip.sh new`) for the live desktop to pick it up. Running
app windows need a full relaunch (a rebuild doesn't hot-swap). Only
the `&.widgits/_shared-lib/` copies are committed; the `ops/` copies
are build-generated and show up dirty in `git status` forever - don't
stage them.

## 9. Scope: ship the honest small version

Every feature this session shipped a deliberately-bounded v1 with the
bigger version flagged, not attempted: File Explorer is LOAD-only
(SAVE mode = v2), csv-hq is 26 cols / plain-comma / no formulas beyond
5 named functions, text-edit-hq clipboard is whole-buffer-or-
selection (no rich text), the `<grid>` has no native scroll yet.
Document the limit IN the file's own header as a real, deliberate
choice - not an apology, not a TODO that rots.

## 10. Runtime state files are noise

`*_ui.txt`, `*_action.txt`, `*_buffer.txt`, `module_parent.pid`,
`text_area_*.txt`, `history.txt`, `entity_menu_frame_*` - all
generated, none tracked. A `git status` full of them is normal. When
archiving the house, exclude them (or use `git archive`, which only
sees tracked files and can't race a live writer - the safe snapshot).

## 11. Translating a GUI mockup → the generic vocabulary (the "playbook")

When the task is *convert an existing GUI app* (or build a new one from
a visual mockup) and you're unsure which tags to use, whether a "new
HTML category" is needed, or how to order the nav `[ ]` items — there
is a concrete recipe:

**`08-roadmap/browser-prompting/platform-passes/13.grok-media-studio-continuation-delegation.md`
→ section "🎨 DESIGN / LAYOUT PLAYBOOK".**

It has: the 4 window skeletons (pick one, don't hybridise), a
mockup-widget → generic-tag table (menu bar, transport, LCD, track
lanes, piano roll, canvas, inspector, mixer drawer, sliders/checkboxes
/dropdowns — none of which exist as elements), the nav-ordering rules
(document order = run order; `<text>` never numbered; chrome `[X]`
auto-last; `target_id` scoping; scroll regions number only visible
rows), the CSS-subset reality (explicit px widths or it renders 0),
and a fully worked DAW example with annotated nav numbers. Written for
Grok, useful for any conversion.

## 12b. One layout, several windows: the instance-dir hook (plugz-hq, 2026-11-14)
Use this when several windows are the SAME screen over different data (plugz-hq: plugs, harnz and tests). Do not copy the `.xhtpm` per kind and do not add a per-kind branch to `khtpm_core_render.c`.
Worked example: `&.hq-apps/plugz-hq/` (read it, it is small). Read events-hq first for the same pattern at larger scale (`&.widgits/events-hq/events-hq.xhtpm` + `ops/evhq_projector.c`).

1. **The hook.** `khtpm_core_render <house> <xhtpm> <instance_dir>`: argv[3], if it is an existing directory, is the instance. `${ARG3}` = that dir, `<instance>/.hq_manager/ui.txt` is added to the template's vars automatically (no `vars=` needed), and every `<module>` gets `KHTPM_ARG3=<instance>` in its environment. One instance dir per window kind (`instances/<kind>/`, gitignored runtime state).
2. **Three small files, no logic in the template.** (a) the static `.xhtpm`; (b) a C **projector** `<module>` that reads the instance's state files and writes `ui.txt` ONLY when it changed; (c) a C **action op** that every `action=` runs (`'${PKG}/ops/+x/x_act.+x' 'verb' 'arg' '${ARG3}'`) and that is the only writer. The rules about the DATA (what rows exist, how to toggle or run) stay in ONE existing op that other consumers also call (here `harness_menu_op rows|describe|pick`): the projector formats, it does not decide.
3. **Wake-up by marker, not by timer alone.** Every writer appends one byte to `<instance>/.hq_manager/marker.txt` (cap it, 4 KB, restart it); the projector polls the marker's SIZE every 400 ms (two stats) and republishes on growth, plus a slow full refresh (5 s) so a change made elsewhere (the editor) shows up. Idle cost: two stats per 400 ms.
4. **Long actions never block the window.** An action that can take minutes starts a detached, registered worker through the spawn funnel (`kh_spawn.sh <house> 0 <name> -- <worker command>`; the funnel prints the pid) and returns at once; the worker writes the result line to a file and bumps the marker. The window shows "running ..." and then the result.
5. **Opening the window from an op.** `fork`+`execv` through the funnel with the instance dir as the last argument; keep a `window.pid` in the instance and, when that pid is alive and is a `khtpm_core_render`, only refresh the instance files and bump the marker (one window per kind).
6. **Make the projector testable.** Give it a `--once <instance>` mode (publish once, exit) so a harness case can run the REAL projector and the REAL action op in a scratch house and assert on `ui.txt`, which is what the window shows. Then open the real window once and look at the PNG.

Traps hit while building it (each cost a failed run):
- `execv` needs an absolute path: `execv("sh", ...)` silently fails (the funnel never ran, "spawn-failed"). Use `/bin/sh`.
- A rebuilt op is not an installed op: I compiled the new verbs to a scratch binary and the case ran the old `+x/` one. Build with the app's own `build_*.sh` into `+x/`, and check the verb answers from THAT path.
- Compare kinds explicitly: an inverted `!strcmp(...) != !flag` check rejected exactly the rows it should accept and the detail panel came up empty (the list was fine). Test the detail path for EVERY kind.
- `show=` flags are `1`/`0`; a selected list row is `class="data-item active"` (the CSS rule `.data-item.active` is in the sibling css); `<repeat bind="row">` needs keys `row_<n>_text`, and an index you need in an action (`row_<n>_idx`) must be written by the projector.
- The renderer reparses an edited `.xhtpm` live (about 5 s), so layout changes show without a relaunch; a `<scrolllist>` that starts at the end needs `class="from-top"`; a long line wraps in a narrow sidebar.
- Windows opened by two instances of one layout open at the SAME screen position (stacked); there is no per-instance position yet.
- Action strings get `'<package_dir>' '<house_root>'` appended by the renderer: derive the house from the instance path, do not depend on those arguments.
