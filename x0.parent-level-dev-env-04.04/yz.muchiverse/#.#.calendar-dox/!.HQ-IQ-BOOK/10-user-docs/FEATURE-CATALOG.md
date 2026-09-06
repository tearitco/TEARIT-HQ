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

The bottom strip. Click a cell → its submenu. `livedesk_build_<cell>_menu()`
in `khtpm_taskbar_manager.c` builds each; rows come from
`livedesk_taskbar.pdl` (`<cell>_menu_N_*`) and dispatch via
`livedesk:open-X` strings resolved through `livedesk_launchers.pdl`.

| cell | what it opens | status |
|---|---|---|
| **file** | new/open/save/save-as (desk file ops) | ✅ |
| **desks** | switch / new / save desktop layouts | ✅ |
| **pals** | place a pal entity on the desk | ✅ |
| **palettes** | emoji / elements / rmmv / paint pickers | 🟡 emojis+elements ported (`palettes_manager.c`); rmmv/paint on the old path (`PROGRESS-palettes-xhtpm.md`) |
| **edit** | text-edit-hq (below) | ✅ |
| **player** | entity player: play/pause/reset entities | ✅ |
| **db** | db-hq-pal record browser (below) | 🟡 read-only fields |
| **plugins** | — | 🔶 inert placeholder |
| **store** | — | 📋 (`07-install-and-ship/`) |
| **network** | IRC Chat / Forum / Chain / Browser | see "13.network" below |
| **menus** | context-menu editor | 🟡 |
| **datetime** | livedesk-clock widget | ✅ |
| **tools** | proc-monitor, misc | 🟡 |
| **h-ai** | chat-hai / open-hai / co-lab-hai | ✅ chat-hai, open-hai; co-lab-hai 🟡 |

The **HQ button** (top-left of a window) submenu: always-on-top,
restart, hide/show, dir, quit, settings, stats, cli, cursword, kill,
debug — all ✅ (window-chrome actions).

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
| **piececraft-hq** | piece-craft board (`<canvas>` + toolbar); `pchq_board_projector` | 🟡 (canvas blank bug notes in `09-appendix/pc-hq-bugs.md`) |
| **piececraft-xyz** | world manager | 🟡 |
| **media-canvas / media-daw / media-vid** | media-studio family | 🔶 templates present, `PROGRESS-media-studio.md` / `MEDIA-STUDIO-XHTPM-PORT.md` |
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
- **Missing / would help:** a headless *text* render of a window's
  view-state (only `--dump-and-exit` PNG + the `*_ui.txt` files exist
  today; a `strip_render_ascii` exists but for the taskbar strip only).
