# LIVEDESK SESSIONS + DESKS — WORKLOG (2026-08-10)

Working through the sessions/desks feature (K9 + K10, then K11, now C2b
full File sessions UI — all DONE + verified). Full design:
`livedesk-editor-design.md`. Verbose feature-work report:
`livedesk-report-2026-08-10.md`. This file is the resume-point checklist —
read it first if the session was interrupted.

## DONE before this pass
- C1 "USER: jb" fix (double-encode bug) — taskbar rebuilt, verified.
- Schema cleanup EXECUTED (§9.3): user homes now ONLY at
  `<house>/xyzfs/users/<uuid>/home/`; login/avatar ops + tsc_elo/yahoo
  fallbacks re-pointed; stale guest identity deleted; jb's avatar recreated
  (brown skin) and shows in the taskbar USER cell as an MC-style sprite.
- Backup of the pre-migration schema: `/tmp/opencode/schema-backup-1786340314/`.

## DONE + RUNTIME-VERIFIED this pass (K9/K10 scaffold)

### Code landed in `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
- New `livedesk:*` reserved commands, dispatched IN-PROCESS by
  `livedesk_dispatch()` (no shell-out). Wired into `run_popup_row()`
  (File submenu) + `open_cell_popup()` (desks button).
- Pure-logic block (after `active_avatar_dir()`): storage at
  `<house>/xyzfs/users/<uuid>/home/livedesk/sessions/` — root
  `session.pdl` (`active_session`/`last_session`), per-session
  `session.pdl` (`name`/`active_desk`), desk pdl rows
  `DESK | entity | path | x | y | grid_x | grid_y | glyph | index`.
- Functions: user_uuid (emoji-free `0.user-pal*` walk), sessions_root,
  root pdl read/write, ensure_session, next_id, set_name, session_name,
  active_desk read/write, desk_list, next_desk, read_open, glyph/pos
  readers, snapshot_desk, close_all (CLOSE relay + 450ms nanosleep),
  spawn_desk, default_session (auto-create `s1` "pre-design" from the
  CURRENT live desktop), switch_desk, load_session, new_session,
  new_desk (empty desk_0N + switch), save, save_as (cp -r clone), plus
  dynamic session-list / desk-list popups.
- `livedesk_mkdir_p()` — plain `mkdir()` is one level only; the per-user
  `home/livedesk/sessions` chain rarely exists, so every session mkdir
  goes through the recursive helper.
- `g_livedesk_popup_x` anchor — dynamic popups open under the triggering
  strip cell (like the file submenu), not the strip's left edge.
- `livedesk_switch_desk()` guard — snapshot the OUTGOING desk only;
  snapshotting the incoming desk from a dead/empty live registry wiped
  desk_01.pdl (live-caught), then spawn read an empty file.
- **Desk pdl paths are HOUSE-RELATIVE** (`livedesk_rel_path` on snapshot,
  `livedesk_join_path` on spawn). `*.monads/*.muchi-pet/...` are LITERAL
  dirnames (real asterisks in the fs), so `access()` on them works.
- **File button = current session**: `file:<name>` (e.g. `file:pre-design`),
  refreshed on the 1s poll tick, `mark_strip_frame_changed` only on change.
- **Strip extended to 11 cells** (STRIP_BTN_MAX 8→10, pdl + defaults):
  HQ, USER, file, desks, edit, palettes, player, db, plugins, store,
  network — nav 1..11, tabs from 12.
- **sscanf full-match fix** in `load_strip_config` — `sscanf("1_cmd",
  "%d_label")` returns 1 (partial), which wrongly landed `strip_btn_1_cmd`
  in the `_label` branch (desks button showed `livedesk:desks`). Now
  `%n`-verified full consumption.
- Entities still close via `interact_relay.txt` (CLOSE) + ~450ms nanosleep,
  respawn via `tp_desktop_window.+x` after their `desktop_pos.txt` is
  rewritten to the saved grid cell.

### Verified on screen (receipt = `#.desktop/livedesk_open.txt`)
Clicking desk_01 spawned all 6 entities at their saved positions:
`tile:m1_ninjadragon:🥷 160x160@+240+80`, `tile:book-stack:📚 64x64@+480+160`,
`tile:self:🤖 64x64@+640+720`, `tile:m8_redhorned:👺 160x160@+720+0`,
`tile:asa:👨 64x64@+560+1440`, `tile:ava:👩 64x64@+800+1440`.
Registry rows (PID/INDEX/ENTITY/PATH) are the audit trail, all PIDs alive.

### Build status
- `gcc -std=c11 -Wall -O2 ops/tp_taskbar.c -o ops/+x/tp_taskbar.+x -lX11`
  EXIT=0. Warnings are all pre-existing `-Wformat-truncation` noise.

## DONE + RUNTIME-VERIFIED this pass (K11 — desk properties / rename)

### Code landed in `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
- **cli-io modal block** (`cliio_*`): standalone centered edit window
  (`300x56`, class "MuchiverseLivedesk", override_redirect, Exposure |
  KeyPress | ButtonPress), TWO numbered rows: `1. rename desk: [<buf>]` and
  `2. cancel`. NAV-ENTRY shape per direct instruction — `[>]` pref when
  focused; Enter turns it `[^]` and captures keys till Escape deactivates.
  Row numbers always shown (`1.`/`2.`) and digits `1`/`2` selectable in nav
  mode; Up/Down cycle focus; Escape in typing deactivates only, Escape in
  nav closes.
- `cliio_key_allowed()` sanitizer `[A-Za-z0-9_-]` — applied BEFORE the char
  reaches the buffer (house cli-io standard, §10.4). Backspace trims, Enter
  commits (buffer non-empty) → `livedesk_rename_desk()` → `cliio_close`.
- `livedesk_rename_desk()`: validates non-empty/different/sanitized, guards
  `access()` both sides, `rename(2)` the pdl, then if the renamed desk is
  the ACTIVE one updates `STATE|active_desk` via `livedesk_write_active_desk`.
- **Focus-steal fix**: `cliio_open()` maps+focuses the editor, and
  `close_popups()` skips its `XSetInputFocus` restore whenever
  `g_cliio_active` — so the caller's post-open `close_popups()` can't yank
  focus back (was dropping every keystroke).
- `livedesk_open_rename_modal()` wraps `cliio_open`; dispatch `rename-desk:`
  opens the modal (in-process, no shell-out).

### Verified end-to-end (fs receipts — all renames went through the app)
- `desk_01.pdl` → `office.pdl` (first pass, focus bug fixed) → strip label
  polled `desks:office`, `STATE|active_desk | office`.
- Keyboard path re-verified on clean build: `office` → `office22222222`
  (8×`2`), → `officeoffice1finaldesk` (sanitizer + focus-capture proof:
  user keystrokes landed in the FOCUSED editor buffer — window truly holds
  focus; `!` never appeared in any name → sanitizer works), Escape
  deactivate → `x` ignored → re-activate → commit.
- Final state restored to `office.pdl` / `active_desk=office`.
- Mouse click path (desks btn 1068,66 → right-click row 1068,96 → rename
  1068,124) opened selector/props/edit from mouse on the clean binary;
  Escape closed the editor, leaving strip + bottom bar only.
- Taskbar rebuilt (`gcc -std=c11 -Wall -O2 … tp_taskbar.c -o …/+x/… -lX11`,
  EXIT=0, only pre-existing `-Wformat-truncation` noise) + restarted via
  `kill $(cat …pid)` + `setsid nohup … & disown` (PID 319840 live).
- All temporary debug logging removed; `rename_debug.log` deleted.

## NEXT (resume here)
0. **§4.8 + §4.9 implementation — pals-canonical runtime + pals popup**
   (design docs locked; model: 1 pal = 1 hash = 1 canonical copy in
   `xyzfs/users/<uuid>/home/livedesk/pals/`, Pokédex/party, NFT future;
   approved 2026-08-10):
   - (A) Make the pals registry the RUNTIME home — entities run FROM the
     canonical pals copy, not dev folders; drop the `access(dev-path)` skip
     in spawn; snapshot self-copy guard; save-as clones layout only, never
     pals; migrate existing s1..s4 `entities/` into `pals/` with content
     hashes and rewrite desk pdl `path=` to reference the pal.
   - (B) **pals popup** (design §3.1 + §4.9): `livedesk:pals` lists the
     pokedex — every owned pal (glyph + name + hash), available in all
     sessions; selecting places it onto the current desk (row referencing
     the pal + spawn). Pals are never removed from the list by placing.
   - Re-verify the C2b round-trip with entities loading from the pals
     registry.
1. khtpm port (deferred — legacy `tp_taskbar.c` stays). C5+C2b must land
   BEFORE the port: it's a refactor of settled logic, not a feature pass —
   full rationale in `livedesk-editor-design.md` §5.1 (resolution:
   C5 → C2b → report → port). PORT END-STATE NOTE: the port is intended to
   be chtpm-close — layouts as `.chtpm` markup + logic as `<module>`-tagged
   `.pal` files ("htpm js"), NOT just a C core/plat split. See §5.1 callout;
   scope decision (a) vs (b) still pending before we start the port.
2. FUTURE (design §3.1): **`pals`** popup/list behavior — the strip button
   itself is BUILT (2026-08-10, pdl-only, reserved `livedesk:pals` no-op).
   Owned default-tile library; entities launch from files/desks/pals.
   Final strip order (12 cells; nav 1..12): HQ | USER | file | desks | pals
   | palettes | edit | player | db | plugins | store | network.
3. Report (feature-work summary) — **DELIVERED 2026-08-10**:
   `livedesk-report-2026-08-10.md` (verbose, all receipts). Last step
   before the khtpm port is done.

## DONE this pass (C2b — File→new/save/save-as/load full sessions UI)

### Code landed in `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
- File menu verb `new` (was `new-desk`): `strip_btn_0_menu_0_label = new`,
  `cmd = livedesk:new` → `livedesk_new_session()` (blank template: new
  `s\d+` id, `session.pdl`, empty `desks/desk_01.pdl`, switch to it). PDL
  row change only — no recompile. `+new-desk` stays in the desks popup.
- **`run_popup_row()` keep-open bug fixed**: a `livedesk:*` row command that
  opens a REPLACEMENT popup (load → sessions picker, desk-props) was killed
  the instant it opened — row handler always returned keep_open=0, so the
  caller's `close_popups()` destroyed the fresh popup. Now returns 1 (keep
  open) ONLY when `g_strip_popup_open && g_strip_popup_menu != menu` (the
  popup now showing is a different menu than the one the row ran in).
  Commands that leave the source popup alone (new/save/save-as/rename→
  cli-io) still return 0 → caller closes the source popup normally.
  Precedent: `livedesk_edit_focused_desk` already returned 1.

### Verified end-to-end (full RPG-maker cycle, nav-index driven via nav.sh)
- `new` from s1 (office, 6 entities) → s2 `session2`, empty `desks/desk_01.pdl`,
  all entities closed; strip `file:session2` / `desks:desk_01`.
- `save` → s2 gains `entities/`, desks pdl preserved.
- `save-as` → s3 `session3` = independent clone (own desks/ + entities/).
- `save-as` from s1 → s4 `session4` = FULL clone (6 entity packages +
  office.pdl + desk_01.pdl, m8_redhorned hp=42).
- **K6 independence**: edited s4 copy hp → 7; s1 copy stayed **42**; s2/s3
  empty; each session has its own desks/ set. save-as is `cp -r` — K6 is
  structural, no extra code (see §4.5b).
- `load`: sessions picker (keep-open popup) lists
  pre-design/session2/session3/session4; loading pre-design restores s1 =
  office + 6 entities relaunched, live hp=42, `active_session=s1`.
- Load popup survives the 3s poll + row activation (keep-open fix receipt).
- Final state: canonical s1 active (`file:pre-design`/`desks:office`).
  Test sessions s2/s3/s4 stay — they prove the File menu works; s4 carries
  the K6 probe (m8 hp=7) evidencing copy independence.
- Harness hardening: `nav.sh row` retries popup discovery 6× (popup can lag
  its claim in xwininfo) and aborts if no popup is actually open — the
  earlier failure typed digits into whatever held focus (landed a desk
  switch on the desks popup). Recovery = reload s1 (done).
- Taskbar PID 332068 live; build EXIT=0 (pre-existing `-Wformat-truncation`
  noise only).

## DONE this pass (pals strip button + nav harness + strip order fix)

### pals button — built (pdl-only, no recompile)
- `#.desktop/livedesk_taskbar.pdl`: `pals` = `strip_btn_2` with reserved cmd
  `livedesk:pals` (dispatch ignores it → silent no-op until implemented).
  Inserted between `desks` and `palettes`; `edit` moved AFTER `palettes`
  (user correction 2026-08-10). Shifted palettes→3, edit→4, player→5,
  db→6, plugins→7, store→8, network→9. 10 buttons fit `STRIP_BTN_MAX=10`.
- Verified live (PID 327528): strip 1720px, `cell[4]="pals" nav=5` between
  desks(4) and palettes(6). `nav 5` = no-op (no popup, no crash).

### Nav harness — `/tmp/opencode/nav.sh` (index-driven, reusable)
- User request: stop using exact click coords; drive via nav INDEX input +
  a reusable harness. All geometry DERIVED from X at runtime (xwininfo),
  no hardcoded coordinates.
- Commands: `strip` (derived geom), `arm` (Esc + right-click strip center),
  `nav <n>` (arm → type digits → Enter → activate cell by its nav#),
  `row <n>` (type digits + Enter on an open popup row), `key <sym>`,
  `esc`, `cells`, `popups`, `wait`.
- Verified: `nav 5` → pals no-op; `nav 6` → palettes popup opens
  (`new-palette` row); `nav 4` → desks popup opens (desk_01/office/edit/
  +new-desk/cancel). Replaces coordinate-click probing for C2b testing.


## DONE this pass (C5 — per-session entity FULL copies)

### Code landed in `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
- `livedesk_base_name()` + `livedesk_copy_full()` (rm + `cp -r`, quoted
  paths) — design §4.5/§4.5b.
- `livedesk_snapshot_desk()` clones every live entity package →
  `<session>/entities/<basename>/` (full package, every file — entity
  OWNERSHIP intent: user storage owns the whole entity, store flow later
  reuses the same copy).
- `livedesk_spawn_desk()` restores `<session>/entities/<basename>/` → live
  package before launch. save-as's existing `cp -r` clones entities for free.

### Verified (KPI K5)
- Save captured full packages (6 entities incl. m8_redhorned's hp/mp/level/
  xp/gold/inventory/event_pkg).
- `+new-desk` → empty `desk_01`, all 6 closed (registry 0).
- Session copy hp set to 42; switch back to `office` → 6 respawned, live
  hp.txt = **42** (was 100). Session copy is authoritative.
- Build EXIT=0 (pre-existing warnings only); taskbar restarted (PID 324539);
  test artifact left: s1 m8 hp=42 in session copy + live (consistent).

### Follow-up (documented §4.5b)
- Snapshot-before-close ordering: revisit when entities write state on CLOSE.

## KNOWN RESERVED COMMAND SET
```
livedesk:new        livedesk:new-desk    livedesk:save
livedesk:save-as    livedesk:load        livedesk:desks
livedesk:open-session:<id>
livedesk:switch-desk:<id>/<desk>
livedesk:desk-props:<desk>
livedesk:rename-desk:<id>/<desk>
livedesk:delete-desk:<id>/<desk>
```

## CRITICAL ENV FACTS
- jb uuid = `0a9558a7-7c74-4358-833c-2d5b21edc421`.
- Session storage: `<house>/xyzfs/users/<uuid>/home/livedesk/sessions/`.
- GRID_CELL_PX = 80 (`LIVEDESK_GRID_PX` in taskbar mirrors it).
- Entity spawn binary: `<house>/&.widgits/tile-picker/ops/+x/
  tp_desktop_window.+x <package_dir>`.
- Entity positions live in `<package_dir>/desktop_pos.txt` (x= / y=),
  written live on drag-end (Q9).
- Registry: `<house>/#.desktop/livedesk_open.txt` rows
  `PID=|INDEX=|ENTITY=|PATH=` — the spawn receipt to audit.
- Taskbar pidfile: `<house>/#.desktop/livedesk_taskbar.pid`. Kill via
  `kill $(cat …pid)`; NEVER `pkill -f tp_taskbar.…` in the tool shell (it
  self-matches and hangs) — use `[t]p_taskbar` bracket trick if pgrep.
- Relaunch rule: `setsid nohup CMD </dev/null >/dev/null 2>&1 & disown`
  in a minimal command — never combine kill+sleep+launch+inspect in one
  bash call (hangs the tool).
- Click injection: `/tmp/opencode/tk_click <x> <y> [button]` (XTest-built
  from tk_click.c). No xdotool on this system.
- Kill entities cleanly: `./EMERGENCY_CLOSE.sh` from house root.
