# HQ window map + agent input — findings for later agents (2026-08-28)

Companion to `_.0.aigent-testing-k9.txt` (relay-over-xdotool order) and
`GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`. These were **not** already in
k9 / F-19 / archive/a11.focus-troubleshooting.md as a single checklist.
a11 is a *different* bug (pointer-grab on tile-picker context menus).
F-19 is "bare XSetInputFocus on override_redirect does not deliver keys
under Mutter" — the opposite direction of "do not steal the human."

Target binary: `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c`
(+ `+x/khtpm_entity_menu_render.+x`).

---

## 1. Two different "focus" problems (do not conflate)

| Symptom | Cause | What actually fixes it |
|---|---|---|
| Human loses **browser / other programs** when an agent **opens a window** | WM-managed `XMapRaised` — Mutter **activates** the new window | `XMapWindow` (open-hai `khtpm_open_hai_render.c`, egg_window.c). Chat-hai was the live proof: Settings + entity-menu (override_redirect) did **not** steal; chat-hai (`XMapRaised`) did. |

**Caveat (re-prove 2026-08-28):** HQ `XMapWindow` is the pattern, not a
hard "Mutter will never activate." One chat-hai launch still assigned
`getwindowfocus` after the MapWindow change. Popups (Settings/entity)
were the cases that did not steal. Document the ask (no MapRaised / no
SetInputFocus on map), not an absolute WM guarantee.

| Human loses focus when an agent **clicks/types** | `xdotool` / XTest injects into the global pointer/keyboard | Drive `#.desktop/<mode>_history/<pid>.txt` (`MOUSE_EVENT:` / `KEY_PRESSED:`) — per-PID as of 2026-08-29, see §3. The human can keep the same display. k9 already said this; see §3. |
| Override-redirect popup **keyboard** never arrives | F-19: bare `XSetInputFocus` on a fresh popup under XWayland | raise-then-focus **only when the human needs keys in that popup**. Do not call it on map just to make agent tests work. |
| Agent file lines do nothing | `poll_agent_history()` used to **return without reading** unless `hq_window_has_x_focus()` | Removed. This process's history file is a mailbox: consume + dispatch even if the human is in the browser. Dual-consume is "two processes, one file," not "unfocused skip." |

Live (user, 2026-08-28): "the windows u opened didn't interfere till u opened hai-chat." Other agent's tests do not interfere; ours must match that.

---

## 2. Map call by window family (current)

- **WM-managed HQ** (db-hq / palettes / bookmarks / stats-hq, events-hq, chat-hai): `XMapWindow`. `focus_grab` hail-mary stays **off** (`g_dbhq_focus_grab_enabled=0`, `chai_focus_grab_enabled=0`).
- **Override-redirect popups** (entity-menu, taskbar-settings/swatch): `XMapRaised` so the menu is on top of the thing that spawned it, **no** `XSetInputFocus` on map (egg_window / chai default). ButtonPress still arrives. Human keys in that popup may wait until they click it — that is accepted vs stealing the browser.
- **Do not** `XSetInputFocus` / `XRaiseWindow` a **foreign** window to make a test land. Tab-cycle uses `nav_tab_active.txt` self-focus only.

---

## 3. How to drive tests (k9 order, this binary's files)

Prefer this over xdotool. User also said agents may drive however they need **if** windows do not steal; file relay is the no-steal default.

**BREAKING CHANGE 2026-08-29** — real live incident: an agent's own test
relay input to the flat `db_hq_history.txt` was delivered to the
user's separately-open, real db-hq window too ("why isn't arrow/index
nav working in db-hq anymore?") — the file was keyed by MODE NAME
ONLY, so every window of the same mode (real user window, a test
window, a second agent's window) read the identical stream. Fixed:
`history_path()` now mirrors `nav_tab`'s own existing per-PID
convention exactly — one real file per PROCESS, not per mode. **The
flat mode-named files below no longer exist as of this binary
version** — do not write to them, nothing reads them anymore.

Per-PID history files under `<house>/#.desktop/<mode>_history/`:

- db-hq / palettes / bookmarks / stats-hq: `db_hq_history/<pid>.txt` (stats has its own dir name if `g_is_stats_hq`)
- events-hq: `events_hq_history/<pid>.txt`
- chat-hai: `chat_hai_history/<pid>.txt`
- swatch: `taskbar_settings_history/<pid>.txt`
- entity-menu: `entity_menu_history/<pid>.txt`

**Finding the right `<pid>` for a specific window** (no new registry -
this already exists): `nav_master_current.txt` publishes live
`<pid> <tab_ordinal> <nav_index> <id>` rows for every open window's
current nav tree (see `nav_ledger_publish()`), and `nav_tab/<pid>`
holds that same pid's real registered window title
(`nav_tab_register()`). Cross-reference the two to identify which pid
is "the events-hq window showing asa" vs "the db-hq window on Common
Events" before writing any input, rather than guessing/broadcasting.

Format (mutaclysm / pieces/keyboard, no timestamp prefix on these files):

```
MOUSE_EVENT: <button> <x> <y> <is_press>
KEY_PRESSED: <decimal>
```

Useful codes: 13 Enter, 27 Escape, 8 Backspace, 9 Tab, 200–205 arrows/page, 112 dump PNG, 210 text dump (`/tmp/db-hq-state.txt`).

Swatch 2-phase is **not** in the renderer. Append `MOUSE_EVENT` on a swatch; renderer writes `taskbar_settings_action.txt` (`seq=` + `PICK:n`); `swatch_picker_manager.+x` advances `taskbar_settings_state.txt` (`phase/bg/fg/apply`).

**Leftover PICK trap:** if `taskbar_settings_action.txt` still has `PICK:2` from last session, the manager used to treat it as pick 1 and the human's first click became pick 2 → window closed, no secondary. Launch now wipes action/state to `seq=0` / `phase=0`. Manager ignores non-increasing `seq`. Renderer only quits when `apply && phase>=2 && fg>=0`.

**button.sh gotcha:** `button_taskbar_settings.sh` / `chat-hai/button.sh` **kill existing instances**. Do not relaunch chat-hai "to test" if the human already has it open.

---

## 4. What is still a mode `if` (not a map/focus bug)

Renderer still has `g_is_swatch_picker` layout/chrome/status rings, `is_popup` phantom 150ms + stale drain (X11, keep), generic `dispatch()` with `PICK:` / `CLOSE` / `void`. Entity-menu hit/`void` still quits (legacy). Palettes frame-file paint is the Phase 2 receipt proof; other modes not yet. Sonnet owns LayDoc / `_shared-lib/`.

---

## 5. Do not "fix" these the wrong way

- Do not re-add `if (!hq_window_has_x_focus()) return;` on poll to copy wraith's unfocused skip. Wraith has **one** history file and **two** parsers. Here each process has **its own** history file.
- Do not steal with `XMapRaised` on HQ "so the test window is visible." `XMapWindow` still maps it.
- Do not use popup `XSetInputFocus` on map "so Down works in tests." Append `KEY_PRESSED: 201`.


## 6. Frame files (2026-08-28 D.4)

Popup paint is file-derived too:
- swatch: `#.desktop/taskbar_settings_frame.txt`
- entity-menu: `#.desktop/entity_menu_frame.txt`
Same pipe fields as palettes_frame.txt. Chrome/status/chosen-ring
still drawn in-process (not Elem tree).


## 7. Two `"BACK"` onclick meanings (Sonnet review, not a bug)

Same string, two dispatch functions, two processes/modes:

- Popup / entity-menu `dispatch()`: `BACK` → `switch_page()` (chtpm
  page stack; real `action="BACK"` in pals/*/menu.chtpm).
- db-hq ACTIVATE scope pop: `DEACTIVATE` → `dbhq_back_scope()`.
  Do **not** reuse `BACK` here — that was the collision. LayDoc's
  own `lay_back()` stays on the strip parser, not this onclick.
