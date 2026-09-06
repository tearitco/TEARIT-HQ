# Terminal-mirror parity — every khtpm window, not just the strip

**Status:** steps 1-3 BUILT + verified 2026-09-06 · step 4 (true no-X)
still design-only · **Momentum doc** — deliberately not exhaustive.

## BUILT (2026-09-06) — steps 1, 2, 3

- `khtpm_core_render.c` `kh_write_ascii_frame()` — every non-dock window
  drops `#.desktop/ascii_frames/<pid>.frame.txt` + `<pid>.pulse.txt`
  (DIAMOND size-growth marker, rotates >64KB) on every `redraw()`.
  Cleaned up via `atexit(kh_ascii_frame_unregister)`.
- `history_init_empty()` — the per-PID input relay is now created empty
  at startup so a generic keyboard's FIRST keystroke isn't eaten by
  `poll_agent_history()`'s first-sight cursor seed.
- `khtpm_render_ascii.+x <house> <pid>` — generic presenter (60Hz pulse
  poll, explicit `\r\n`, exits ~2s after the frame file disappears).
- `khtpm_kbd_ascii.+x <house> <pid>` — generic raw-termios keyboard
  relay → `KEY_PRESSED:` lines into `entity_menu_history/<pid>.txt`.
- `ops/open_window_cli.sh <pid>` — attach presenter+keyboard terminal
  to a running window.
- `$.crypts/scrypts/headless/window_headless.sh` — the headless entry
  point (see "Running headless" at the bottom).

Verified: taskbar-settings window launched, `cat <pid>.frame.txt`
showed its live tree, `khtpm_kbd_ascii.+x` Down×4/Up×2 moved `[>]`
focus exactly, strip `cli` unaffected.

---

## 0. What we have vs. what we want

**Have (shipped, 2026-09-06):** the taskbar strip has a live, bidirectional
terminal mirror — `cli` in the HQ menu. `khtpm_core_render.c` dock mode
walks its laid-out `Elem` tree to an indented text frame
(`dock_ascii_walk()` → `strip_ascii_current_frame.txt`), appends a byte
to `strip_ascii_pulse.txt` (DIAMOND marker), and a tiny presenter
(`khtpm_strip_render_ascii.+x`) tails that marker at 60Hz and prints
the frame with explicit `\r\n`. Input comes back through
`strip_history.txt` → the manager. X11 and terminal drive one shared
focus cursor.

**Want:** the same for **any** khtpm window —
- entity **context menus** (right-click an entity)
- taskbar **sub-menus** (the dropdowns off a strip cell)
- taskbar-opened **HQ windows** (events-hq, db-hq, chat-hai, irc-chat,
  chain, music-player, pdl-read, …)
- windows opened **from** an entity menu (e.g. an entity's "events"
  row opening events-hq)

…so an agent (or a human on a headless box) can start any window, read
its state as text, and drive it — for testing IRC/chain from many
ports, for CI, for "show me what that window looks like right now"
without a screenshot.

## 1. Why this is mostly plumbing, not new architecture

The hard parts already exist and are generic:

| need | already there |
|---|---|
| one binary renders every window from `.chtpm`/`.xhtpm` + a manager's projected vars | `khtpm_core_render.+x` (strip, entity menus, db-hq, events-hq, chat-hai, taskbar-settings, pdl-read, …) |
| per-window input relay, X input and agent writes through the same path | `history_path()` → `#.desktop/entity_menu_history/<pid>.txt`, polled every tick by `poll_agent_history()`. Codes: printable ASCII literal, `13`/`27`/`8`/`9`, `200-203` = arrows, `204/205` = PgUp/Dn, `MOUSE_EVENT: <btn> <x> <y> <press>` |
| Elem-tree → indented text walk | `dock_ascii_walk()` — currently gated to dock, but the logic (labels, nav numbers, `[>]` focus, `*` active, container indent) is not dock-specific |
| DIAMOND marker discipline | `mark_frame_changed()` / `consume_frame_changed()` helpers + `strip_ascii_pulse.txt` precedent |
| one-shot snapshot | `--dump-and-exit` (writes PNG + wire-format `.frame.txt`) — needs a readable-text sibling |

What's genuinely missing: (a) a **headless** path (no `XOpenDisplay`),
(b) making the text walk + frame file **per-PID and mode-agnostic**,
(c) a **generic presenter** instead of the strip-specific one.

## 2. The plan (4 steps, each shippable alone)

### Step 1 — mode-agnostic text frame + marker (no X changes)
Promote `dock_write_ascii_frame()` → `kh_write_ascii_frame()`:
- runs for **every** window at the end of `redraw()`, not just dock.
- writes `#.desktop/ascii_frames/<pid>.frame.txt` (create dir like
  `history_path()` does) + appends to `<pid>.pulse.txt`, rotating past
  64KB (`fopen "w"`), exactly as the strip pulse now does.
- `dock_ascii_walk()` stays the walker; drop the `window_is_dock()`
  gate, keep the `e->y < -1000` skip (closed/parked rows).
- strip keeps its existing `strip_ascii_*` filenames (the `cli`
  launcher + docs already point at them) — treat the strip as the
  already-done special case, everything else uses the `<pid>` files.

Deliverable: launch events-hq normally, `cat
#.desktop/ascii_frames/<pid>.frame.txt` → its live tree as text. No
presenter yet, no headless yet.

### Step 2 — generic presenter
`khtpm_render_ascii.+x <house> <pid>` = `khtpm_strip_render_ascii.c`
with the path templated on `<pid>` instead of hardcoded `strip_ascii_*`.
Same 60Hz pulse-size poll, same explicit `\r\n`, same screen-clear.
One binary for all windows.

### Step 3 — generic keyboard relay
`khtpm_kbd_ascii.+x <house> <pid>` = `khtpm_strip_keyboard_ascii.c`
retargeted to append `KEY_PRESSED: <code>` lines to
`entity_menu_history/<pid>.txt` (the relay `poll_agent_history()`
already consumes) instead of `strip_history.txt`. Arrow → `200-203`,
Enter → `13`, etc. Raw termios, never prints (same split as the strip,
same staircase reason).

Then a generic `open_window_cli.sh <pid>` (or an HQ-menu row on every
window, like `cli` is on the strip) launches presenter + keyboard in
one gnome-terminal.

### Step 4 — headless (`--headless`)
Add `--headless` (argv, any position, like `--dump-and-exit`). When
set:
- skip `XOpenDisplay` and every X call; `window_is_*` still works off
  the `.chtpm` class.
- run the normal tick loop (parse, `${var}` sub, `reparse_*_if_changed`,
  `poll_agent_history`, layout, `kh_write_ascii_frame`) with no
  `select(xfd)` — just `usleep(16667)` and poll the relay + marker
  files.
- guard the X calls behind `if (dpy)` / a `g_headless` check. This is
  the real work of the whole effort; most X calls are already funnelled
  through a few helpers (`redraw()`, `set_window_opacity()`,
  `dock_grab_keyboard()`), so it's bounded, not scattered.

Deliverable: `khtpm_core_render.+x --headless <house> <events-hq.xhtpm>`
on a box with no display → writes a live text frame, driven by the
relay. This is the IRC/chain-from-many-ports test surface.

## 3. Menus & windows-from-menus — already covered by the above

- An **entity context menu** and a **strip sub-menu** are just windows
  rendered by `khtpm_core_render.+x` with their own PID → Step 1 gives
  them a `<pid>.frame.txt` for free. (The strip's *dropdown* is drawn
  into the strip's own tree via `<repeat>`, so it already shows up in
  the strip mirror under `--- HQ menu (open) ---`.)
- An entity menu row that **opens events-hq** spawns a new
  `khtpm_core_render.+x` with a new PID → that window gets its own
  frame file and presenter. Nothing special: the parent menu and the
  child window are two independent mirrors, discoverable via
  `#.desktop/ascii_frames/*.frame.txt` (or the existing
  `livedesk_hq_windows_<pid>.txt` registry).

## 4. Non-goals / deferred

- **Entity manipulation from the text view** (place/move/delete an
  entity by typing) — real feature, own design later. This doc is
  read + nav + activate only.
- Replacing the strip's bespoke `strip_ascii_*` names — leave them;
  the strip is the working reference.
- A TUI with panes/scrollback — the presenter stays a dumb full-screen
  reprint (DIAMOND: the marker is the clock).
- `khtpm_draw_core.c`'s sprite-cache `st_mtime` — unrelated (asset
  cache coherency, not a render trigger); leave as-is.

## 5. Order of attack

Steps 1-3 DONE. Step 4 (`--headless`, true no-X) is next and on its own
branch — it touches the X-call surface and wants careful live-
verification that the windowed path is byte-identical after.

## 6. Running headless (today, before step 4)

`khtpm_core_render.+x` still needs an X connection to build its window,
so "headless" right now is one of:

| you have | how |
|---|---|
| a real `DISPLAY` (`:0`) | the window draws there; ignore it, drive via files. `window_headless.sh` uses it automatically. |
| no display, `xvfb-run` installed | `window_headless.sh` wraps it in a throwaway virtual X server — genuinely screenless (SSH/CI). `sudo apt-get install -y xvfb` once. |
| no display, no xvfb | script stops and tells you to do one of the above. |

```
$.crypts/scrypts/headless/window_headless.sh <house> <window.xhtpm> [x] [y] [--attach]
```
Prints `PID=<n>`, the `frame:` path (`cat` it), and the `relay:` path
(`printf 'KEY_PRESSED: 201\n' >> …` to drive). `--attach` also runs the
presenter+keyboard in the current terminal. PID is also written to
`#.desktop/ascii_frames/last_headless.pid`.

For **many windows at once** (IRC + chain from different ports): call
`window_headless.sh` once per window; each gets its own PID-keyed
frame/pulse/relay triple, fully independent.

Step 4 will add `khtpm_core_render.+x --headless …` so the DISPLAY /
xvfb requirement goes away entirely.

## See also
- `reference/TPMOS-DIAMOND-render-chain.md` — the marker discipline all
  of this follows.
- `taskbar-tpmos-parallel-refactor.md` — the strip mirror that's the
  working template (built + verified).
- `HQ-WINDOW-MAP-AND-AGENT-INPUT.md` + `_.0.aigent-testing-k9.txt` —
  the per-PID relay and the "don't steal the human's focus" rules the
  headless path sidesteps entirely.
- `10-user-docs/FEATURE-CATALOG.md` "Missing" — the one-paragraph
  version of this.
- `DB-EVENTS-HQ-PORT-DESIGN.md` — parallel effort (per-app C → template
  + projector); a window ported there is automatically easier to
  mirror here.
