# Feature catalog — what exists, how to open it, is it working

**Living doc, first cut 2026-09-06.** The one place to see *what the
house can actually do right now*. Before this, that answer was spread
across the live `toy.pdl` scan, `08-roadmap/design-docs/`,
`09-appendix/PROGRESS-*-xhtpm.md`, and per-app `.md` files. Correct any
status that's wrong — this is meant to be edited.

**Status key**

| | meaning |
|---|---|
| ✅ | real static `.xhtpm` + real compiled manager, opens + functions (verified this era) |
| 🟡 | first-cut / partial — opens and does real work, known gaps documented |
| 🔶 | placeholder shell only (a `.chtpm` with static text, no wiring) |
| 📋 | design doc / brainstorm only, not built |
| 🗂️ | legacy CLI app — works, but not yet an HQ window |

---

## Taskbar strip cells (`#.desktop/livedesk_taskbar.pdl`)

The bottom strip. Click a cell → its submenu. Rows come from
`livedesk_taskbar.pdl` as `<cell>_menu_N_label` / `<cell>_menu_N_cmd`
pairs, read at open time (`livedesk_pdl_menu_rows()`); the hardcoded
`livedesk_build_<cell>_menu()` bodies in `khtpm_taskbar_manager.c` are
now just the `count==0` fallback (player/ai/db converted; the four
directory-scanning builders — user/pals/toys/clock — still hardcoded).
A `_cmd` is a `livedesk:open-X` string (→ `livedesk_launchers.pdl`), a
bare shell command, or `widget:<name> <MODE> <start> <verb>`. See
`08-roadmap/design-docs/TASKBAR-MENUS-DATA-DRIVEN.md`.

| cell | what it opens | status |
|---|---|---|
| **file** | new/open/save/save-as (desk file ops) | ✅ |
| **desks** | switch / new / save desktop layouts | ✅ |
| **pals** | place a pal entity on the desk | ✅ |
| **palettes** | emoji / elements / rmmv / piececraft (Mineclonia) / cdda pickers | 🟡 chooser-grid live for rmmv+piececraft+cdda+emojis (`08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md`); paint/df/kenney still stub |
| **edit** | text-edit-hq (below) | ✅ |
| **player** | entity player: play/pause/reset entities | ✅ |
| **db** | db-ez · db-hq(-pal) record browser · sql-hq (SQL over csv/pdl) | 🟡 db-hq-pal read-only fields; sql-hq window live |
| **plugins** | — | 🔶 inert placeholder |
| **store** | — | 📋 (`07-install-and-ship/`) |
| **network** | IRC Chat / Forum / Chain / Browser | see "13.network" below |
| **menus** | context-menu editor | 🟡 |
| **datetime** | livedesk-clock widget | ✅ |
| **tools** | proc-monitor, misc | 🟡 |
| **h-ai** | chat-hai / open-hai / co-lab-hai | ✅ chat-hai, open-hai; co-lab-hai 🟡 |

The **HQ button** (top-left of a window) submenu: always-on-top,
restart, hide/show, dir, quit, settings, stats, **cli**, cursword,
kill, debug — mostly ✅ window-chrome actions.

**`cli` — the terminal mirror of the taskbar strip (real, bidirectional).**
`open_cli.sh` opens a gnome-terminal running TWO binaries (TPMOS's
`renderer.c` / `keyboard_input.c` split — never combined, because raw
termios breaks `\n`→`\r\n`):
- `khtpm_strip_render_ascii.+x` — no termios, reads the parser's
  composed frame (`#.desktop/strip_frame.cells.pdl` + `strip_state.txt`
  for `strip_focus_cell`), writes `strip_ascii_current_frame.txt`
  (readable text: `[>] 1. HQ`, `[ ] 2. jb`, …) + a timestamped
  `strip_ascii_frame_history.txt`.
- `khtpm_strip_keyboard_ascii.+x` — raw termios, never prints; relays
  terminal keys (arrows as `ESC[A/B/C/D` → `KSC_FOCUS_LEFT/RIGHT`,
  digits, Enter) through `khtpm_strip_parser.c`'s `dispatch_key_code()`
  into the SAME manager the X11 strip uses.

So it is genuinely **bidirectional**: X11 clicks and terminal keys
both drive `khtpm_taskbar_manager`, both renderers read its published
state. Live-verified 2026-08-18: relay-driven HQ-open → digit-select →
Enter-activate end to end; arrows move the `[>]` focus. Runs headless
(the render binary alone produces the text frame with no terminal).

**Docs** (these are the ones a `grep cli`/`terminal`/`headless` misses
— they're under `08-roadmap/design-docs/` and named `taskbar-*`):
`taskbar-tpmos-parallel-refactor.md` (the built + verified mirror,
current status + still-open items), `taskbar-keyboard-relay-and-
terminal-render.md` (superseded origin), `taskbar-history-txt-
migration-investigation.md` (the X11-capture→relay cutover).

**Not built:** the same for an arbitrary **HQ window** (the "open a
terminal render of any window" vision). See "Missing" below.

---

## 13.network cell — IRC / Forum / Chain / Browser

Design: `08-roadmap/design-docs/NETWORK-CELL-HQ-WINDOWS-DESIGN.md` +
`IRC-FORUM-CHAIN-HQ-WINDOWS.md`. Dispatch is fully data-driven
(`launcher_network_<key>` rows — add an app with zero C).

| row | opens | status |
|---|---|---|
| **IRC Chat** | `&.hq-apps/irc-chat-hq/` — real HQ window: room-list sidebar, message feed, composer. Reuses `044.pal-chat-irc👥️+2` ops verbatim. Real identity (house login `jb`), per-window `palnet_peer` port, P2P delivery. Junk test-rooms filtered. | ✅ v1 (local + P2P verified 2-instance) |
| **Chain** | `&.hq-apps/chain-hq/` — wallet dashboard: Wallet / Send / Mine / History tabs. **One global shared ledger** (`041.pal-chain⛓️/data/blockchain.txt`, seeded to 14k blocks); one shared `palnet_peer` per node. Reuses `chain_*` ops. Mining, send, balance, full-chain history all live. | ✅ v1 |
| **Forum** | legacy CLI app (`041.pal-forum👥️`) in a terminal tab via `open_forum_cli.sh` | 🗂️ → 🔶 (`&.hq-apps/forum-hq/` placeholder exists; HQ window not built — next up) |
| **Browser** | `&.hq-apps/network/` — network-browser-hq, static xhtpm + `network_browser_manager.c`, own JS engine (`nb_js_worker`). Renders fetched pages as heterogeneous rows. | 🟡 (opencode owns this; JS engine WIP — `NB-JS-ENGINE-ROADMAP.md`) |

Admin/perms (owner = `jb`, make/modify chains, grant permissions,
driven from the GUI): 📋 hooks planned, not built.

---

## `@.apps/` toys (taskbar "toys" menu — auto-scanned for `toy.pdl`)

| toy | what it does | status |
|---|---|---|
| **pdl-read** | paginated doc reader; scans `#.DOX`; File-Explorer "open other file" | ✅ |
| **text-edit-hq** | multi-line editor: line-number gutter, selection, real clipboard, Save-As via File Explorer | ✅ |
| **csv-hq** | spreadsheet: `<grid>` in-place cell edit, 5 named functions (SUM/AVG/MIN/MAX/COUNT), 26 cols, plain-comma | ✅ v1 |
| **music-player-hq** | iTunes-style player: `.pal`-dir library, `mpg123 -R` playback, transport, ASCII visualizer placeholder | ✅ v1 (`MUSIC-PLAYER-HQ-DESIGN.md`) |
| **piececraft-hq** | studio board (`<canvas>` + Interact); HQ menu / Toys → Piececraft-HQ; File → samples. Desktop is also an RM-like map (z, transfers, save). Play = Interact; **do not hide chrome**. | 🟡 viewer + map load live; stamp/events/transfer-shop-battle still kv — vision `PIECECRAFT-HQ-GAME-EDITOR-AND-PLAY.md` |
| **piececraft-xyz** | world manager | 🟡 |
| **media-img-hq / media-3d-hq / media-daw-hq / media-vid-hq** | house-spec media toys | 🟡 vid demo clip is `NNEST-12.00/#.NNEST_ASSETS/video/sample-10s-vp9.mp4` (`VIDEO-ASSET-SOURCE-LOCATION.pdl`); see `PROGRESS-media-studio.md` |
| **my-chara-txt / my-lawyer** | `toy.pdl` only, no template yet | 📋 |
| the rest of `@.apps/*` (civ-*, genesis-*, tactics-txt, pets, …) | no `toy.pdl` | 📋 / concept dirs |

---

## `&.hq-apps/` (HQ windows, launched by their own `button.sh`)

| app | status | notes |
|---|---|---|
| **chat-hai** | ✅ | persona round-robin chat; the template all these windows model on |
| **open-hai** | ✅ | prompt composer |
| **co-lab-hai** | 🟡 | agent-collaboration surface (has `USER-FAQ.md`, `onboard-co-lab.txt`) |
| **db-hq-pal** | 🟡 | record browser; field editing still read-only |
| **events-hq** (in `&.widgits/`) | ✅ | event/command list |
| **stats-hq** | ✅ | dashboard |
| **signup-hq** | 🟡 | login/signup flow |
| **irc-chat-hq / chain-hq** | ✅ | see 13.network |
| **sql-hq** | 🟡 | SQL over `.csv` / `.pdl` (vendored sqlite3). Window: macro sidebar + editor + results grid; also a `sql_hq repl` CLI. Open from `[ ]db → sql-hq`. Staged Commit/Rollback + Export/History still WIP. `SQL-HQ-DESIGN.md`. |
| **forum-hq / db-hq / js** | 🔶 | placeholder / legacy `.chtpm` |

---

## `&.widgits/` (helper widgets)

file-explorer ✅ (LOAD mode; SAVE = v2) · bookmarks ✅ · palettes 🟡 ·
open-hai ✅ · events-hq ✅ · taskbar-settings ✅ · livedesk-clock ✅ ·
context-menu ✅ · file-menu ✅ · proc-monitor 🟡 · board-viewer 🟡 ·
map-picker / tile-picker 🟡 · event-editor / event-ez 🟡 ·
db-hq-actors-pal 🟡 · yahoo-broker / yahoo-chart / vvarware-report 🔶 ·
_shared-lib = the shared renderer core (not a widget)

---

## Shared building blocks (for anyone building a new HQ app)

- Static `<name>.xhtpm` + CSS, rendered by the ONE shared
  `khtpm_core_render.+x` — `CENTROID_GOLD_STD.md`.
- Generic elements: `<sidebar>/<panel>/<scrolllist>/<repeat>/<tab>/<tabbar>/
  <cli_io>/<text_area>/<grid>/<canvas>/<item>/<text>`, `${var}` +
  `show=` gating.
- Real compiled `<name>_manager.c` `<module>` — publishes `<name>_ui.txt`,
  polls `<name>_action.txt`.
- Clipboard (window↔window↔OS) ✅ · text selection ✅ · multi-digit
  nav-jump ✅.
- **Terminal mirror for arbitrary HQ windows** — ✅ steps 1-4 BUILT
  (2026-09-06). Every non-dock window writes
  `#.desktop/ascii_frames/<pid>.frame.txt` + `<pid>.pulse.txt` (DIAMOND
  marker) on every repaint; `khtpm_render_ascii.+x <house> <pid>`
  presents it and `khtpm_kbd_ascii.+x <house> <pid>` relays keys into
  the per-PID `entity_menu_history/<pid>.txt` the renderer already
  polls. `ops/open_window_cli.sh <pid>` attaches a terminal to a
  running window. **`khtpm_core_render.+x --headless <house>
  <window.xhtpm>` runs with NO X connection at all** (real
  `assign_nav_and_layout()` with estimated text metrics), so
  `$.crypts/scrypts/headless/window_headless.sh` works over plain SSH /
  in CI with no `DISPLAY` and no Xvfb — verified on taskbar-settings,
  events-hq, irc-chat-hq, and strip mode. No layout logic duplicated —
  same `dock_ascii_walk()` the strip uses. Design + how-to:
  `08-roadmap/design-docs/TERMINAL-MIRROR-PARITY-all-windows.md`.
  Deferred: entity manipulation from the text view (its own design).

### Verified: chain P2P sync across ports (2026-09-06)

Two isolated chain nodes (own `data/`+`net/`, shared presence dir):
`palnet_peer` bound **:9950** (node A) and **:9951** (node B); node A
mined 16 blocks; **node B's `blockchain.txt` grew by all 16** via the
mesh + `chain_inbox_watcher` merge. So: distinct ports work, and the
"one global ledger reconciled over P2P" model holds. On a single
machine, all `chain-hq` windows share the one real
`041.pal-chain⛓️/data/blockchain.txt` directly (one node); cross-node
sync is the P2P path just verified. Harness:
scratch `chain_2node_sync.sh` (not committed - regenerate from this
description).
