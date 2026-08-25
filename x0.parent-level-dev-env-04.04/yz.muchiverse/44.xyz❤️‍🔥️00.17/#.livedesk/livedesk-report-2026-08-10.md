# LIVEDESK SESSIONS + DESKS — FEATURE-WORK REPORT (2026-08-10)

Companion to `livedesk-sessions-worklog.md` (resume checklist) and
`livedesk-editor-design.md` (full design). This report is the verbose
feature-work summary covering the completed work arc and the exact,
runtime-verified receipts behind it.

---

## 1. Scope of this report

The "sessions + desks" feature arc on the legacy livedesk taskbar
(`&.widgits/livedesk-taskbar/ops/tp_taskbar.c`) — everything from the
storage-schema migration through the full File-menu sessions UI:

| Step | Status | Where verified |
|---|---|---|
| C1 — "USER: jb" double-encode fix | DONE, rebuilt | worklog |
| Schema cleanup (§9.3) — user homes only at `<house>/xyzfs/users/<uuid>/home/` | DONE, executed | worklog |
| K9/K10 scaffold — sessions + desks data layer, popups, dispatch | DONE + runtime-verified | worklog |
| K11 — desk properties / rename (`cli-io` standard) | DONE + runtime-verified | design §10.7, worklog |
| C5 — per-session entity FULL copies (K5) | DONE + runtime-verified | design §4.5b, worklog |
| **C2b — File→new/save/save-as/load full sessions UI (K6)** | **DONE + runtime-verified** | design §4.6b, §2 below |
| pals strip button (reserved, pdl-only) | BUILT (behavior FUTURE) | design §3.1, worklog |
| Nav harness `/tmp/opencode/nav.sh` | BUILT + verified | §7 below |
| khtpm port | DEFERRED until after this report + strip support lands in khtpm core | §9 below |

The two open follow-ups (documented, not blocking): snapshot-before-close
ordering (§6) and the pals popup behavior (FUTURE).

---

## 2. C2b — File menu = full sessions UI (this pass)

### 2.1 What the user asked for (design §4.6)

- **File→save** writes session entity data; only position writes live on drag.
- **`+new-desk`** creates an EMPTY desk in the current session and switches
  to it; `cancel` closes the popup.
- **Load list** scans `sessions/` dirs; names come from each `session.pdl`
  `STATE|name` — no master index file to keep consistent.
- **File menu verbs:** `new` (blank template), `save`, `save-as` (clone to a
  new id), `load`.
- Session switching = quit-then-launch teardown.

### 2.2 Direct instruction (locked in)

- File menu verb `new` = **blank template** (`livedesk:new` → new session),
  replacing the old `new-desk` row on the File button. `+new-desk` stays in
  the **desks** popup, so desk creation remains reachable.
- `livedesk:*` commands dispatch **in-process** via `livedesk_dispatch()`;
  non-livedesk rows keep `setsid nohup` shell-out.
- Stay in legacy `tp_taskbar.c`; never run `build_khtpm.sh` (it overwrites
  the running binary with a strip-less build).

### 2.3 Implemented

- **File menu pdl**: `strip_btn_0_menu_0_label = new`,
  `strip_btn_0_menu_0_cmd = livedesk:new`. Rows:
  `1. new | 2. save | 3. save-as | 4. load`. PDL-driven — no recompile for
  label/cmd changes.
- **`livedesk_new_session()`** — blank template: `livedesk_next_id()` scans
  for `s\d+` dirs and returns `s<best+1>`; `livedesk_ensure_session()` writes
  `session.pdl` (`STATE|name`) + `desks/` + empty `desk_01.pdl`; then the
  new session is set active (root `session.pdl` `STATE|active_session`),
  outgoing desk snapshotted, entities closed, empty desk spawned.
  `ensure_session` never overwrites an existing session dir.
- **`run_popup_row()` keep-open fix** (§3.3) — without it the load picker
  popup died the moment it opened.

### 2.4 End-to-end receipts (all nav-index driven, no coordinate clicks)

Driven entirely through `/tmp/opencode/nav.sh` (see §7): `nav 3` opens the
File menu, `row N` activates a row.

1. **`new`** from s1 (office, 6 live entities) →
   - s2 `session2` created; `active_session=s2`.
   - Strip polled `file:session2` / `desks:desk_01`.
   - Empty `desks/desk_01.pdl`, **0** entities in registry (`livedesk_open.txt`
     emptied), s2 session copy had no `entities/`.
2. **`save`** →
   - s2 gained `entities/`; `desks/desk_01.pdl` preserved.
3. **`save-as`** (on s2) →
   - s3 `session3` = independent clone: own `session.pdl name=session3`, own
     `desks/desk_01.pdl`, own `entities/`. Strip `file:session3`.
4. **`save-as`** (back on s1) →
   - s4 `session4` = FULL clone of s1: 6 entity packages under `entities/`,
     `office.pdl` + `desk_01.pdl`, m8_redhorned hp=42.
5. **K6 independence probe** —
   - Edited s4's `m8_redhorned/hp.txt` → **7**.
   - s1's copy still **42**. s2/s3 still empty. Every session owns its own
     `desks/` set and its own entity data. → save-as is a `cp -r` of the
     whole session dir, so K6 independence is **structural**, no extra code.
6. **`load`** —
   - Sessions picker (the keep-open popup) listed
     `1. pre-design 2. session2 3. session3 4. session4`.
   - Loading **pre-design** restored s1: `active_session=s1`, strip
     `file:pre-design` / `desks:office`, **6 entities relaunched**, live
     m8_redhorned hp = **42** (matches s1 session copy).
   - The picker popup survived the 3s poll and row activation (keep-open
     fix receipt).
7. **Final canonical state** — s1 active (`file:pre-design`/`desks:office`,
   6 live entities, hp=42). Test sessions s2/s3/s4 retained as working
   evidence of the File menu.

---

## 3. Bugs found + fixed this pass

### 3.1 The `row 4` focus flake (harness-side)

`nav.sh row` failed to find the load popup once (`no popup window found`),
then typed its digits into whatever held focus — which turned out to be the
desks popup from an earlier probe, landing a desk switch to the empty
desk_01. Root cause: the popup window can lag its claim in `xwininfo`
(claim written before the window is fully mapped/visible to the query).

**Fix:** `focus_popup()` now retries discovery 6× at 0.3s and ABORTS if no
popup is actually open, instead of typing digits blind. Recovery was
re-running `nav 3` → `row 4` → `row 1` to reload s1 (canonical state
restored, verified above).

### 3.2 Load popup killed on open (code-side, the real bug)

A `livedesk:*` popup-row command that opens a **replacement** popup (load →
sessions picker, desk-props) was destroyed the instant it opened: the row
handler always returned keep_open=0, so the caller's `close_popups()` closed
the fresh popup it had just opened.

**Fix:** `run_popup_row()` returns 1 (keep open) only when
`g_strip_popup_open && g_strip_popup_menu != menu` — i.e. the popup now
showing is a DIFFERENT menu than the row just ran in. Commands that leave
the source popup alone (new/save/save-as/rename→cli-io) still return 0 and
the source popup closes normally. Precedent already existed:
`livedesk_edit_focused_desk` returned 1.

### 3.3 Earlier fixes carried in (context)

- `sscanf` full-match fix in `load_strip_config` (§K9/K10).
- `cliio_open` focus-steal fix (§K11).
- Outgoing-desk-only snapshot guard (§K9/K10).

---

## 4. Architecture as built

### 4.1 Code home

Everything lives in `&.widgits/livedesk-taskbar/ops/tp_taskbar.c` (legacy
taskbar, per §5.1 the port is deliberately deferred). Build from house root:

```
gcc -std=c11 -Wall -O2 "&.widgits/livedesk-taskbar/ops/tp_taskbar.c" \
  -o "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x" -lX11
```

EXIT 0; the only warnings are pre-existing `-Wformat-truncation` noise.

### 4.2 Storage (single source of truth)

```
<house>/xyzfs/users/<uuid>/home/livedesk/sessions/
  session.pdl                 # STATE | active_session | s1
                              # STATE | last_session  | s4
  s1/  session.pdl            # STATE | name | pre-design
       desks/  desk_01.pdl    # DESK | entity | path | x | y | grid_x | grid_y | glyph | index
                office.pdl
       entities/<entity-basename>/   # FULL package clone (C5)
```

- **House-relative** entity paths in desk pdl files (`livedesk_rel_path` on
  snapshot, `livedesk_join_path` on spawn) — `*.monads/*.muchi-pet/...` are
  literal dirnames (real asterisks), so `access()` works on them.
- `uuid` = jb `0a9558a7-7c74-4358-833c-2d5b21edc421`, resolved by walking
  `0.user-pal*` (emoji-free), no hardcoded id.

### 4.3 The C5 copy layer

- `livedesk_base_name()` — rel-path basename → entity-dir key.
- `livedesk_copy_full()` — `rm` + `cp -r` with single-quoted paths (literal
  `*`/emoji/space dirnames stay literal).
- **Snapshot** (`livedesk_snapshot_desk`): after writing the desk pdl,
  clone every live entity's whole package → `<session>/entities/<basename>/`.
  One hook covers save / new-desk / new-session / load / switch(outgoing) /
  save-as.
- **Restore** (`livedesk_spawn_desk`): per desk row, replace the live package
  with an exact clone of the session copy, then write the saved position and
  launch `tp_desktop_window.+x`.
- **save-as** = `cp -r` of the whole session dir → clones desks + entities in
  one move; K6 is structural (§2.4.5).

### 4.4 Dispatch + popups

- `livedesk_dispatch()` handles in-process: `new`, `new-desk`, `save`,
  `save-as`, `load`, `desks`, `open-session:`, `switch-desk:`,
  `desk-props:`, `rename-desk:` (→ cli-io modal), `delete-desk:`,
  `pals` (reserved no-op).
- Dynamic session-list and desk-list popups anchored under the triggering
  strip cell (`g_livedesk_popup_x`), like the File submenu.
- `livedesk_build_session_menu()` / `livedesk_build_desk_menu()` render the
  pickers from the on-disk truth (no cached index).
- File button label polls `file:<active-session-name>` on the 1s tick and
  only marks the frame changed when it differs.

### 4.5 Strip layout (as built, pdl-driven)

12 cells, nav 1..12, `STRIP_BTN_MAX=10`:

```
HQ | USER: jb | file | desks | pals | palettes | edit | player | db | plugins | store | network
 1      2        3      4      5       6         7      8       9     10        11       12
```

`pals` = `strip_btn_2` (reserved `livedesk:pals`); `edit` moved AFTER
`palettes` (user correction). All 10 data buttons fit `STRIP_BTN_MAX=10`.

---

## 5. KPI acceptance mapping

| KPI | Criterion | Receipt |
|---|---|---|
| K5 | Session save captures FULL entity packages; switch restores from session copy | `m8_redhorned` hp 100→(save)→42 in session copy→(switch)→live hp=42; 6 packages captured (hp/mp/level/xp/gold/inventory/event_pkg) |
| K6 | save-as → session is fully independent (own desks AND own entity data) | s4 clone hp=7 vs s1 copy 42; s2/s3 empty; own `desks/` set each |
| C2b | new/save/save-as/load all work from the File menu, in-process | Full round-trip §2.4, canonical s1 restored |
| K11 | rename/desk-props via `cli-io` (mouse + keyboard paths) | design §10.7 (sanitizer proof: no `!` ever reached a name) |
| Nav | index-driven, reusable harness, runtime-derived geometry | §7 below |

---

## 6. Known issues + follow-ups (non-blocking)

1. **Snapshot-before-close ordering** (design §4.5b): snapshot runs BEFORE
   `close_all()`, so an entity writing state only on CLOSE could lose its
   final write to the outgoing snapshot. Harmless today (no entity writes
   on close); revisit when entities gain `state.txt` write semantics.
2. **pals popup behavior** (design §3.1): button built + reserved; listing
   owned tiles + launching onto the current desk is FUTURE.
3. **No stale-PID cleanup** in `livedesk_open.txt` for SIGKILLed entities
   (README, carried).
4. **Test sessions s2/s3/s4** remain in `sessions/` as real saved sessions.
   s4 carries the K6 probe (m8 hp=7) as copy-independence evidence. Remove
   them any time with `rm -r …/sessions/s{2,3,4}`.

---

## 7. Tooling — the nav harness

`/tmp/opencode/nav.sh` (index-driven, reusable; user asked to stop using
coordinate clicks). All geometry DERIVED from X at runtime via `xwininfo` —
no hardcoded coordinates.

- `strip` — print derived strip geometry.
- `arm` — Esc (exit stale nav) + right-click strip center.
- `nav <n>` — arm, force strip focus (`/tmp/opencode/setfocus`), type the
  digits, Enter → activates the strip cell with global nav number `<n>`.
- `row <n>` — focus the open popup (retry 6×, abort if none) → type digits +
  Enter → activates popup row `<n>`.
- `key <sym>` / `esc` — raw key / Escape.
- `cells` / `popups` — dump current strip-cell or popup-row frames from the
  debug logs; `wait [sec]`.

Helpers: `/tmp/opencode/tk_key`, `/tmp/opencode/tk_click`,
`/tmp/opencode/setfocus` (XTest-based; keys reach only the window holding
focus, hence the explicit setfocus before typing).

---

## 8. Runtime state (at report time)

- Taskbar PID **332068** (pidfile `#.desktop/livedesk_taskbar.pid`).
- `active_session=s1` (pre-design), `last_session=s4`; strip
  `file:pre-design` / `desks:office`; 6 entities live; m8 hp=42.
- Restart recipe (never plain `pkill`):
  ```
  kill "$(cat "#.desktop/livedesk_taskbar.pid")"; sleep 1;
  setsid nohup "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x" "$PWD" \
    </dev/null >/dev/null 2>&1 & disown
  ```
- Debug frames: `#.desktop/tp_taskbar_debug/strip_frame_log.txt` and
  `popup_frame_log.txt`.

---

## 9. Next steps

1. **khtpm port** (the big deferred item): requires (a) the §5.1 scope
   decision (a) vs (b) still pending, and (b) strip support landing in the
   khtpm core FIRST — `grep -n strip khtpm_taskbar_core.c` is currently
   empty, and `build_khtpm.sh` silently overwrites the running taskbar with
   a strip-less binary. Port end-state is chtpm-close: layouts as `.chtpm`
   markup + logic as `<module>`-tagged `.pal` files. Rationale: the port is
   a refactor of SETTLED logic (C5+C2b now landed) — not a feature pass.
2. **pals popup behavior** (design §3.1) — owned default-tile library;
   entities launch from files/desks/pals.
3. **Snapshot/close ordering** (§6.1) when entities gain close-write state.

---

*Generated 2026-08-10. Companion: `livedesk-sessions-worklog.md`,
`livedesk-editor-design.md`.*
