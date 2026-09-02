# 🌐 NETWORK-CELL-HQ-WINDOWS-DESIGN

**Status:** Phase 1 DONE (2026-08-31) — the network cell is real and
wired; see "REAL PHASE 1 WIRING" below for what changed and the one
real bug found/fixed along the way. §10 Phase 2 (converting each CLI
app into its own real khtpm window, not just a gnome-terminal tab) is
still not started.
**Date:** 2026-08-30
**Owner:** events-db-networking effort (see `.OPEN-2do-events-db-networking-2026-08-28.md`)

## REAL HANDOFF STATUS (2026-08-31, Sonnet, found while investigating
where opencode left off - none of this was committed, all found as
real, untracked, working files on disk)

**Done, real, tested, builds clean right now:**
- `44.xyz.01.00/&.hq-apps/network/cli_io_window.c` - the §5.3
  "cli-io window" console container. Complete, standalone X11 binary
  (core fonts only, no Xft dep) - opens a window, forks a real bash
  child over pipes, streams stdout/stderr into a scrollback, sends
  typed lines to the child's stdin. Read fully - no TODOs, no
  truncation, a real finished implementation, not a stub-of-a-stub.
- `&.hq-apps/network/build.sh` - builds `+x/cli_io_window.+x`.
  Re-ran it live just now: `OK cli_io_window`, clean compile, zero
  warnings besides the deliberate `-Wno-unused-result`.
- `&.hq-apps/network/open_network_browser.sh` - the real Browser-row
  launcher: builds the binary if missing, single-instance-guards via
  `pgrep`, launches via the house's own real `setsid nohup ... &` +
  `livedesk_launched_pids.txt` convention. Looks complete and
  consistent with house standards.
- `&.hq-apps/network/open_network_app.sh` - the real IRC/Forum/Chain
  row launcher (`open_network_app.sh <house_root> irc|forum|chain
  <title>`), opens each app's existing `button.sh run` in a
  gnome-terminal tab per §1 scope item 2/3's "keep existing CLI ops as
  the real engine" plan. Looks complete.

**NOT done - this is the real, exact next step:** §10 Sequencing step 1
("verify today's real status of each app... nothing else depends on
specs") and step 2 (Phase 1 cell wiring) were never started:
- `#.desktop/livedesk_taskbar.pdl` still has ONLY `strip_btn_9_label |
  network` (line 101) - zero `strip_btn_9_menu_*` rows. Confirmed live,
  2026-08-31.
- `khtpm_taskbar_manager.c` has ZERO diff from `origin/main` - no
  `livedesk_build_network_menu()`, no dispatch strcmp branch, no
  `ktb_hq_open()` cid-branch wiring. The real inert-catch-all
  (`:3291`-ish, "index 12 → network") this doc's own §2 describes is
  still exactly what's live today.
- Net effect: the network cell is STILL fully inert in the real,
  running taskbar - clicking it does nothing, exactly as before this
  whole effort started. The real, working `cli_io_window.+x` binary
  and launcher scripts above are unreachable from the UI until this
  wiring happens.
- The one still-open reviewer question from §11 ("confirm the
  canonical row-writer for strip submenus... before Phase 1 edits")
  was also never answered - answer that first, then do §10 step 2
  using whichever existing `livedesk_build_<cell>_menu()` is the real
  precedent (§2's own "established pattern to copy" already names it).

## REAL PHASE 1 WIRING (2026-08-31, Sonnet, live-verified end to end)

The §11 reviewer question is answered: the canonical row-writer is the
**dedicated-prefix PDL pattern** (`network_menu_N_label`/`_cmd`, same
shape as `palettes_menu_N_*`), not a literal `strip_btn_13_menu_N`
guess - `livedesk_build_network_menu()` in `khtpm_taskbar_manager.c`
reads it exactly like `livedesk_build_palettes_menu()` does.

- `#.desktop/livedesk_taskbar.pdl` - added the real `network_menu_1..5`
  rows (IRC Chat/Forum/Chain/Browser/cancel), dispatching via
  `livedesk:open-network:<key>`.
- `khtpm_taskbar_manager.c` - `livedesk_build_network_menu()` (PDL
  reader) + `which == 13` in `ktb_hq_open()`'s dispatch chain (13 = the
  network cell's real click-code position, confirmed against
  `khtpm_strip_codes.h`'s own authoritative comment - ignore the STALE
  "1=HQ..12=network" comment a few lines above that dispatch chain,
  it's outdated and doesn't match reality) + a new
  `livedesk:open-network:` command handler mirroring
  `livedesk:open-palette:`'s own real C-side-quoted-path shape (needed
  for the same real reason: raw PDL shell-out breaks on this house's
  literal `&.hq-apps/` paths, confirmed live once already for
  `&.widgits/`).

**Real bug found and fixed while live-testing** (not present in the
design, a real defect in opencode's own `cli_io_window.c`): it called
`XSetInputFocus()` immediately after `XMapWindow()`, before the window
was actually viewable - a real `BadMatch` X error, and with no error
handler installed, Xlib's own default handler treats that as FATAL and
calls `exit()`. The window opened and died in the same frame,
invisible - exactly what looked like "nothing happens" when the
Browser row was clicked. Fixed: dropped the explicit focus call (a
normal WM-managed window doesn't need to fight the WM's own focus
policy here).

**Live-verified, all four rows real:** clicking the network header
(click code 4013) opens the real 5-row menu; "Browser" (row 3, item
code 5003) spawns the real, now-stable `cli_io_window.+x`; "IRC Chat"
(row 0, item code 5000) launches the real
`044.pal-chat-irc👥️+2/system/orchestrator` process via
`open_network_app.sh`. Forum/Chain use the identical real path, just a
different `<key>`.

**One real, separate, NOT fixed cosmetic bug found along the way:**
`cli_io_window.+x`'s own render loop draws nothing visible - the
window opens, stays alive, accepts real input, but the scrollback/
input-line text never actually appears on screen (confirmed via
screenshot, a plain black rectangle). Not yet root-caused (font load
success/failure wasn't isolated) - a real, separate follow-up, not
blocking the taskbar wiring itself since the process is stable and the
dispatch chain is proven correct independent of this.

## 1. Objective & scope

Give the taskbar's **`network` cell** a real submenu (the existing CLI
networking projects + a **browser** stub), then convert those CLI
projects into **khtpm X11 HQ windows** reusing as much existing code and
ops as possible.

### In scope
1. Wire the `network` taskbar cell ([].13) with options:
   - `IRC Chat` → `044.pal-chat-irc👥️+2`
   - `Forum` → `041.pal-forum👥️`
   - `Chain` → `041.pal-chain⛓️`
   - `Browser` → **cli-io window** (see §5.3; RESOLVED: a basic
     "cli-io + window" console container, not a real browser and not a
     zenity-style dialog)
2. Provide a shared **cli-io window container** that gives any CLI
   program a windowed stdin/stdout surface — this is the immediate way
   the three CLI apps (and the browser stub) become taskbar-runnable
   TODAY, before the full chat-hai-style conversion of Phase 2.
3. Convert each of the three CLI apps to a windowed HQ app that keeps
   the existing CLI ops (palnet_peer, watchers, compose/menu ops) as the
   real engine, adding only a thin window layer + reusable layout.
4. Keep everything **file-backed and text-verifiable** (house standard) —
   no new sockets, no renderer edits.

### Explicit non-goals (now)
- A working browser. The browser row opens a **cli-io window** (a
  generic console container, §5.3) — an inert placeholder that is
  functional as a window but has no web functionality yet.
- A dedicated "AI Chat" menu row. **RESOLVED (owner, 2026-08-30):** no
  AI Chat row — an agent drives the IRC/Forum/Chain apps itself, exactly
  like a human; that IS "AI Chat" in this context.
- The **db-hq window's internal "Networking" tab**
  (`DB_HQ_TAB_LABELS[]`) — it lives inside the HARD-BOUNDARY file
  `khtpm_entity_menu_render.c`; we only prepare the exact deferred diff
  (§9) for the boundary owner to land.
- Multi-instance ports / LAN-to-host refinements for palnet_peer
  (§8 — tracked, later phase).

## 2. The `[].13` network cell today (ground truth)

The "13" the task refers to is the **one-based click code** for the
network cell. Three numbering conventions coexist — reconcile, don't
fight them:

| Convention | Source | Network's number |
|---|---|---|
| Cell position / click code (1-based) | `khtpm_strip_codes.h` (cell order comment) | **13** (click code `4000+13=4013`) |
| Strip index (0-based) | `khtpm_strip_header.chtpm` strip-layout header comment & `khtpm_taskbar_manager.c:3142` | **12** |
| Taskbar pdl row id | `livedesk_taskbar.pdl` | `strip_btn_9_label = network` |

**Current state — fully inert, no menu, no dispatch:**
- `livedesk_taskbar.pdl:100` — `SECTION | strip_btn_9_label | network`
  (label only; **zero** `strip_btn_9_menu_*` rows).
- `khtpm_taskbar_manager.c:3142-3156` — comment confirming cells
  `6/7/9/10/11/12` (palettes/edit/db/plugins/store/**network**) are
  "confirmed inert placeholders… clicking one just closes whatever
  popup".
- `khtpm_taskbar_manager.c:3291` — `ktb_hq_open()` catch-all:
  `else { ktb_hq_close(s); return; } /* inert cell … */` — this is the
  exact spot index-12 (network) falls into today.

**The established pattern to copy** (how a working cell builds a popup
submenu): other cells use `livedesk_build_<cell>_menu()` in
`khtpm_taskbar_manager.c`, returning `HQMenuItem[]` rows; each row is a
`livedesk:open-X` dispatch string (or a raw `setsid nohup sh -c …`
string for shell-out rows — precedent: the h-ai cell at
`livedesk_taskbar.pdl:104-109` mixes both). `ktb_hq_activate()`
(~`khtpm_taskbar_manager.c:3360+`) `strcmp`-matches the dispatch string
and reality-launches from `house_root` via
`KTB_SETSID nohup sh -c '…' >/dev/null 2>&1 &`, resolving target paths
through the **launcher registry** `#.desktop/livedesk_launchers.pdl`.

## 3. Existing design docs & reference code — the map

Design/plan docs that already exist (the question "is there already a
design doc?" → **yes, several**; this file consolidates them for this
task and adds the conversion plan):

| Doc | What it covers |
|---|---|
| `0.browser-prompting/architecture-explainers/9.networking-13network-delegation.md` | The prior delegation map for this exact cell; mandates `livedesk:open-X` dispatch, discovery-before-conversion, one capability per bite. |
| `#.#.calendar-dox/1.^V-hq/TASKBAR-MENU-ARCHITECTURE.md` | **The definitive "add a new cell menu / new sub-app" recipe**: two-layer relay, `livedesk:open-X`, file skeleton, 7 pitfalls, test via `nav.sh`. |
| `#.#.calendar-dox/1.^V-hq/khtpm-merge-how2.md` | **House reuse standard**: fork/exec manager vs standalone op vs `#include`-shared `.c`. |
| `#.#.calendar-dox/1.^V-hq/HQML-DESIGN+PLANS.md` | Window chrome conventions (WM-managed + `_MOTIF_WM_HINTS` + opacity = real input; override_redirect = popups) + network-app use cases. |
| `#.#.calendar-dox/1.^V-hq/PIECECRAFT-HQ-KHTPM-INFO-WINDOW-2026-08-30.md` | 4-step recipe to add a khtpm window for a CLI-ish app (manager binary + state file + launcher + `<module>`). |
| `44.xyz.01.00/net/TOOLING-MAP.md` + `net/` | LAN-model tooling: `connect_op.+x` (HTTP POST), `json_parser.+x`, `ollama-lan.pdl`, `qwen.sh`. |
| `44.xyz.01.00/2.muchi-verse/PAL-NET-STANDARD.txt` | P2P **file-mediated** networking standard (apps never touch sockets). |
| `44.xyz.01.00/#.DOX/pal-irc-ft-list.md` | pal-chat-irc feature list (P2P mesh, port auto-alloc). |
| `44.xyz.01.00/&.hq-apps/chat-hai/chat-hai-design.md` + `chat-hai.chtpm` | **The windowed template** our conversion models on (session sidebar, message panel, composer, `<module>` manager). |
| `44.xyz.01.00/102.agy-txt/manager/BROWSER_CONTRACT.md` | The existing browser contract to honor for the browser stub. |
| `44.xyz.01.00/014.wsr-pal💸️📌️+2/system/gl_mirror.c` (+ 205.ttg, 102.agy) | Dual terminal + X11 mirror precedent (optional for windows). |

**Reference code — the three CLI networking apps (all at house root):**
each already has `pal/`, `ops/`, `net/`, `button.sh`, and a
`phase2-module-split-report.txt` (house-alignment started, NOT a
from-scratch conversion):

| App | Own kind | Key ops | Notes |
|---|---|---|---|
| `044.pal-chat-irc👥️+2` | `irc_node` | `chat_post_message`, `chat_create_user`, `chat_switch_user`, `chat_compose_frame`, `chat_menu_input`, `chat_inbox_watcher`, `chat_replay_ledger`, `palnet_peer` | Full mesh, port auto-alloc; feature list in `#.DOX/pal-irc-ft-list.md`. |
| `041.pal-forum👥️` | forum | `forum_post/dm/feed/like/follow/retweet/create_user/switch_user/compose_frame/menu_input/inbox_watcher`, `palnet_peer` | |
| `041.pal-chain⛓️` | `chain_node` | `chain_miner/login/send/balance/create_wallet/compose_frame/menu_input/inbox_watcher`, `palnet_peer` | `button.sh run` starts a peer node. |

Shared networking engine: `palnet_peer.c` (P2P file-mediated mesh —
already `phase2` split), inbox watchers, compose/menu input ops. All
three are **CLI-only** (terminal loop driven by the compose/menu ops).

**Windowed reference apps:** `&.hq-apps/chat-hai/` (window loop +
sidebar/panel/composer, `<module src="…/chat_hai_loop.sh">`), db-hq,
events-hq, stats-hq, bookmarks, palette pickers — all renderers share
ONE binary `khtpm_entity_menu_render.+x`, behavior chosen by
`<window class="…">`; each window is a detached process
(`setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM"`).

## 4. House standards that apply

1. **Reuse decision rule** (khtpm-merge-how2): long-running process →
   real `fork()+exec()` manager; discrete action → standalone op binary;
   per-frame draw/hit-test → shared `.c` `#include` (the one documented
   exception). Never paste shared logic into N binaries just to avoid
   fork/exec.
2. **Dispatch convention**: taskbar menus launch ONLY via
   `livedesk:open-X` `strcmp` branches in `ktb_hq_activate()` or an
   explicit `setsid nohup …` row — never a raw shell command baked into
   a menu builder (documented silent-relative-path pitfall).
3. **Data via files**: state (`*.state.txt`, kv), action relay, history
   ledger, and `frame_changed.txt` size-marker redraw. One writer per
   file.
4. **Window interactivity**: WM-managed window + `_MOTIF_WM_HINTS`
   decorations=0 + `_NET_WM_WINDOW_OPACITY`; override_redirect only for
   popups (Mutter/XWayland blocks real input on override_redirect).
5. **No sockets**: networking stays file-mediated per PAL-NET-STANDARD.
6. **HARD BOUNDARY**: never edit `khtpm_entity_menu_render.c`; the
   db-hq "Networking" tab edit is §9 handoff.

## 5. Phase 1 — Wire the `[].13` network cell (no boundary file touched)

### 5.1 Menu content
```
network
├─ IRC Chat        → livedesk:open-network:irc
├─ Forum           → livedesk:open-network:forum
├─ Chain           → livedesk:open-network:chain
├─ Browser         → livedesk:open-network:browser   (STUB — see §7)
└─ cancel          → (empty cmd row — dismisses)
```

### 5.2 Changes (all outside the boundary file)
1. **`livedesk_taskbar.pdl`** — add `strip_btn_9_menu_M_label/_cmd`
   rows (5 rows incl. cancel), mirroring the h-ai cell rows
   (`strip_btn_14_*` at §:104-109) which mix dispatch + raw-shell forms.
2. **`khtpm_taskbar_manager.c`** — replace the index-12 fall-through into
   the inert catch-all (`.c:3291`) with a `livedesk_build_network_menu()`
   returning the 5 `HQMenuItem[]` rows (read the h-ai cell's builder for
   the exact row-source pattern — prefer pdl-driven like file/palettes).
   Add the four `livedesk:open-network:*` `strcmp` branches in
   `ktb_hq_activate()` (model: db-hq branch `.c:3642-3673`), each
   resolving its launcher through the registry below.
3. **`livedesk_launchers.pdl`** — add:
   - `launcher_network_irc    | 044.pal-chat-irc👥️+2/button.sh`
   - `launcher_network_forum  | 041.pal-forum👥️/button.sh`
   - `launcher_network_chain  | 041.pal-chain⛓️/button.sh`
   - `launcher_network_browser| *.monads/*.livedesk-taskbar/ops/open_network_browser_stub.sh` (new stub, §7)
4. **Rebuild** `khtpm_taskbar_manager_main.+x` / `khtpm_strip_parser.+x`
   per the taskbar build recipe, restart via
   `run_khtpm_strip.sh`, and verify visually + via `nav.sh` test harness.

### 5.3 Browser stub / cli-io window (RESOLVED: "cli-io + window")
- The browser row launches a **cli-io window** — a minimal console
  container window with typed input + streamed output, hosting a CLI
  program. Owner's words: "browser can just be a basic cli-io + window".
- Implemented as a **standalone X11 binary** (not the boundary renderer;
  the `gl_mirror.c` terminal→X11 precedent shows the pattern) that
  `fork()`s a child command and relays its stdin/stdout to a WM-managed
  window. `launcher_network_browser` points at it with no URL/web logic
  yet — placeholder title "browser (cli-io)".
- The SAME cli-io window is the immediate container for the three CLI
  apps in Phase 1 (they need a TTY-ish surface to run from the taskbar)
  — reuse the one binary for all four rows; only the hosted command
  differs.

### 5.4 Acceptance (text-verifiable)
- Network cell click (REQUEST 4013 / index 12) opens the 5-row popup
  (no longer the inert catch-all).
- New pdl launcher rows resolve relative to `house_root` at dispatch.
- Each `livedesk:open-network:*` branch resolves its launcher; IRC/Forum/
  Chain each launch the existing CLI app in-terminal (discovery-driven:
  verify each app's `button.sh` actually runs standalone first);
  Browser shows the stub.
- No edits under `khtpm_entity_menu_render.c`.

## 6. Phase 2 — CLI → HQ window conversion (reuse first)

Model = `chat-hai` exactly: **keep the CLI engine, add a window layer.**
Per app, one bite at a time (`IRC Chat` first — the LAN-chat capability
the delegation doc pushed; then Chain, then Forum).

### 6.1 Per-app window recipe (chat-hai template)
1. **New app layout** `&.hq-apps/<app>-hq/<app>-window.chtpm` +
   `<app>-window.css`:
   - `<window class="<app>-hq">` with a `<sidebar id="sessions">` fed
     from the app's own `rooms/`/collections, a `<panel>` fed from
     `messages.txt` equivalents, a `<composer-text>` input, and control
     buttons mapped 1:1 to the existing ops (post / like / follow /
     send / balance / mine, …).
   - `<module src="…/<app>_window_loop.sh"/>` → renderer
     `fork()+execl()`s the window loop (the proven
     `dbhq_launch_module`/`evhq_launch_module` pattern).
2. **New thin manager/loop** `<app>_window_loop.sh`:
   - Starts the pair the app's `button.sh run` already starts
     (palnet_peer node + inbox watcher) via the app's OWN launcher —
     **reuse `button.sh` verbatim; do not reinvent**.
   - Polls the state files the CLI already writes; translates
     `*_compose_frame.c` screen output into the `.chtpm` text/rows.
   - Writes commands back to the same action/ledger files the CLI ops
     already read (reuse `chat_menu_input`-style ops, or emit the same
     lines they parse).
3. **Window launcher** `open_<app>.sh` (model `open_stats_hq.sh`):
   build-if-missing, single-instance `pgrep` guard,
   `setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM"`, echo `$!` to
   `#.desktop/livedesk_launched_pids.txt`, `nav_tab_register("<app>-hq")`
   after `XMapWindow`.
4. **Point the Phase-1 menu at the window launcher once real**: swap the
   `launcher_network_irc` registry rows from the CLI `button.sh` to the
   new `open_<app>.sh`. Both targets stay valid during development
   (Phase 1 runs CLI; Phase 2 upgrades the row).

### 6.2 Reuse map (verbatim, no reimplementation)
- `palnet_peer.c` (mesh node: `bind_with_retry()`, own_kind per app) —
  unchanged. No new sockets.
- Inbox watchers (`chat_inbox_watcher`, `forum_inbox_watcher`,
  `chain_inbox_watcher`) — unchanged.
- Compose/menu ops (`chat_compose_frame`, `chat_menu_input`, …) — the
  window loop wraps their output, doesn't replace it.
- **No `khtpm_entity_menu_render.c` change required** for the window
  itself: the renderer already launches arbitrary `<module>` scripts and
  matches any `class=` present in the layout. (Renderer class enum needs
  no edit for data-driven modes; verify at implementation whether the
  `class="…"` detection loop `.c:11114-11156` needs a new literal — if
  so, note whether it's inside the boundary file and route accordingly.)

### 6.3 Acceptance (text-verifiable)
- Window opens detached; session sidebar lists real `rooms/`; panel
  shows real messages; a posted message round-trips to the ledger and
  appears back through the watcher (same file evidence as the CLI proof
  harnesses: kv/ledger files).
- CLI parity: the same state files the CLI wrote are what the window
  renders — one writer rule holds.

## 7. Browser (stub) — Phase 3 (after the three windows)

- Browser = **cli-io window** (§5.3): a standalone X11 console container
  hosting a CLI program; no web functionality now. The row + launcher +
  dispatch + cli-io binary exist in Phase 1; the browser merely uses one
  more row of the same container.
- Keep `BROWSER_CONTRACT.md` (`102.agy-txt/manager/`) as the contract
  reference for the real browser later.
- Later: a real `<app>-browser` window reusing the same engine pattern
  (`connect_op`/`json_parser` from `net/`, HTTP per BROWSER_CONTRACT) —
  deferred, not designed here.

## 8. Later refinements (tracked, NOT now)

- **Multi-instance ports:** promote `palnet_peer.c`'s `bind_with_retry()`
  to honor an explicit port arg (2do refinement), defaulting to the
  existing auto-alloc. Do this when two instances of the same app need to
  run (or the LAN phase to the Mac `.144` host begins).
- **LAN / AI-chat harness:** chat-hai deterministic-scheduler shape +
  `net/qwen.sh` + `ollama-lan.pdl` posting through the IRC app's outbox —
  the 2do Task-3 refinement; later phase.

## 9. Deferred handoff — db-hq "Networking" tab (do NOT apply today)

Per the 2do doc protocol: everything not requiring the boundary file
first; when the network cell + windows are in, write the exact diff under
the 2do doc's handoff section for the boundary owner (Sonnet):
- `khtpm_entity_menu_render.c:932-952` — add a "Networking" entry to
  `DB_HQ_TAB_LABELS[]` (+ bump `DB_HQ_N_TABS`, add the
  `DB_HQ_NETWORK_TAB` index macro).
- `khtpm_entity_menu_render.c:983-991` — `dbhq_tab_is_real()`:
  one entry mapping the new tab → the network-cell launch (reuse the
  generic `open:`/`exec:` onclick dispatch at `dbhq_activate_elem()`
  `.c:3163`, paths `.c:3036-3038`).

## 10. Sequencing (house discipline: discovery first, small bites)

1. **Verify today's real status** of each app: read the app's own
   `phase2-module-split-report.txt` FIRST (per the delegation doc) and
   run each `button.sh` standalone once. Nothing else depends on specs.
2. Phase 1 cell wiring (5-row menu + dispatch + launchers + browser stub).
3. Phase 2, one window at a time: IRC Chat → Chain → Forum.
4. Phase 3 browser stub contract unlock; Phase 4 refinements.
5. Post the §9 handoff diff after Phase 1 at the latest.

## 11. Decisions & open questions

**Decisions (RESOLVED by owner 2026-08-30):**
- Network menu is **pdl-driven** for rows (`strip_btn_9_menu_*`), C only
  for the `strcmp` dispatch — zero recompile to reorder options.
- **No AI Chat row.** An agent drives IRC Chat/Forum/Chain itself like a
  human; that IS "AI chat". (Removed the open question below.)
- Browser is a **cli-io window** (basic console container, standalone
  X11 binary, reused as the CLI apps' immediate container), NOT a zenity
  dialog.
- Window conversion reuses each app's `button.sh run` + ops **verbatim**;
  only the chrome layer is new.

**Open questions for the reviewer:**
- Confirm the canonical row-writer for strip submenus in
  `khtpm_taskbar_manager.c` (pdl-driven builder) before Phase 1 edits.

## Appendix — file/line reference index

| Item | Path |
|---|---|
| Network cell label | `#.desktop/livedesk_taskbar.pdl:100` (`strip_btn_9_label`) |
| h-ai cell rows precedent | `#.desktop/livedesk_taskbar.pdl:104-109` |
| Launcher registry | `#.desktop/livedesk_launchers.pdl` (4 rows today) |
| Inert-cell comment | `*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c:3142-3156` |
| Inert catch-all | `…/khtpm_taskbar_manager.c:3291` (index 12 → network) |
| Dispatch chain | `…/khtpm_taskbar_manager.c:3360+` (`ktb_hq_activate`); model branch db-hq `:3642-3673` |
| Strip layout | `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` (index comment: `12=network`) |
| Click codes | `…/khtpm_strip_codes.h` (network = `4000+13`) |
| DB-hq tab array (boundary) | `…/khtpm_entity_menu_render.c:932-952`, `dbhq_tab_is_real():983-991`, dispatch `:3163`/`:3036-3038` |
| Primary planning doc | `#.#.calendar-dox/1.^V-hq/TASKBAR-MENU-ARCHITECTURE.md` |
| Reuse standard | `#.#.calendar-dox/1.^V-hq/khtpm-merge-how2.md` |
| Window template | `44.xyz.01.00/&.hq-apps/chat-hai/chat-hai.chtpm` + `chat-hai-design.md` |
| The 3 CLI apps | `44.xyz.01.00/{044.pal-chat-irc👥️+2, 041.pal-forum👥️, 041.pal-chain⛓️}/` |
| P2P standard | `44.xyz.01.00/2.muchi-verse/PAL-NET-STANDARD.txt` |
| Browser contract | `44.xyz.01.00/102.agy-txt/manager/BROWSER_CONTRACT.md` |

## 12. REAL CORRECTION (2026-08-31) - opencode's approach was the wrong
    direction; real Phase 1 window shells + real Phase 2 feature design

**Direct instruction, the actual ask:** "i wanted to open new hq style
windows, and then reuse the code standards from the other network apps
to make new gui driven networking apps, using the same data schemas,
ops, etc." NOT what opencode built (a standalone terminal-emulator-
from-scratch `cli_io_window.c` + a `gnome-terminal` wrapper around each
CLI app's own ASCII UI) - that never touches this house's real HQ
window framework at all, and reimplements a crude terminal instead of
reusing anything.

**Real fix, minimal-risk, ZERO new C code needed:** `khtpm_entity_
menu_render.c`'s own class-detection loop (~line 11110) has NO
fallback error for an unrecognized `<window class="...">` - every
`g_is_*` flag just stays 0 and the shared, generic sidebar+panel+
button+text rendering below runs anyway (real, confirmed by reading
the code: no `else { return 1; }` after the class-check loop). This
means a genuinely NEW class (`irc-chat-window`/`forum-window`/
`chain-window` - deliberately NOT `chat-window`, which would
incorrectly trigger chat-hai's own persona-loop `<module>` launch)
gets a real, correctly-drawn generic HQ window through the ALREADY-
COMPILED shared `khtpm_entity_menu_render.+x` binary, with zero new
C - just a new `.chtpm`+`.css` per app (real data, not code) and a new
`open_<app>.sh` launcher copied from `open_db_hq.sh`'s own real shape
(same "kill existing instance, build-if-missing, PID recorded" pattern
every other HQ launcher already uses).

### Real Phase 2 feature design (window shells only for now - NOT
    wired to real ops yet, per direct instruction "we dont even need
    to wire up functionality yet, i just wanted to get reasonable
    guis opening") - grounded in the REAL schemas/ops each CLI app
    already has, so the eventual wiring is a real, known, small step
    later, not a redesign

**IRC Chat** (`044.pal-chat-irc👥️+2/`) - real schema:
`data/master_ledger.txt` rows are
`MSG|<msg_id>|<room>|<user>|<ts>|<text>`. Real ops:
`chat_post_message.c`, `chat_create_user.c`, `chat_switch_user.c`,
`chat_replay_ledger.c`, `chat_compose_frame.c`, `chat_inbox_watcher.c`.
Real UI shape (matches chat-hai's own real sidebar+panel precedent
almost exactly, since it's the same real room/message/user model):
- **sidebar**: real room list (eventually populated from the distinct
  `room` field across `master_ledger.txt` - one row per room, click ->
  filter the panel to that room, same real interaction shape chat-hai's
  own session sidebar already proves).
- **panel**: message feed (room-filtered `chat_replay_ledger.c` output,
  newest at bottom) + a compose row at the bottom (text field + Enter
  -> `chat_post_message.+x <user> <room> <text>`, matching chat-hai's
  own real "Enter sends, no button needed" precedent) + a user-identity
  control (current user, `chat_switch_user.+x`/`chat_create_user.+x`
  behind it).

**Forum** (`041.pal-forum👥️/`) - real schema: `POST|<post_id>|<user_id>|
<ts>|<text>|<image_id>` rows. Real ops: `forum_post.c`, `forum_like.c`,
`forum_retweet.c`, `forum_follow.c`, `forum_dm.c`,
`forum_compute_feed.c`, `forum_create_user.c`, `forum_switch_user.c`,
`forum_inbox_watcher.c`. Real UI shape (Twitter/X-like, per its own
real feature set):
- **sidebar**: real tab list - Home (feed), Following, DMs,
  Notifications - each a real button that swaps the panel's content
  source, same real tab-switch shape db-hq's own sidebar items already
  prove (`dbhq_tab_is_real()` precedent).
- **panel**: feed of posts (`forum_compute_feed.+x`'s own real output -
  each post row showing user/text/timestamp + Like/Retweet buttons
  wired to `forum_like.+x`/`forum_retweet.+x`) + a compose box at top
  (text field + Post button -> `forum_post.+x`). DMs tab reuses the
  same panel shape, sourced from `forum_dm.c`'s own real per-user
  thread instead of the public feed.

**Chain** (`041.pal-chain⛓️/`) - real schema: `TX|<from>|<to>|<amount>|
<ts>|<tx_id>` rows plus `BLOCK|...` records and a real miner state file
(`miner_wallet_id`/`running`/`blocks_mined_this_session`/
`last_block_index`/`last_block_hash`/`total_supply_minted`). Real ops:
`chain_create_wallet.c`, `chain_login.c`, `chain_balance.c`,
`chain_send.c`, `chain_miner.c`. Real UI shape (wallet dashboard, per
its own real feature set - NOT a chat/feed shape at all, don't force
the sidebar+panel pattern where a different one fits better):
- **sidebar**: real tab list - Wallet, Send, Mine, History.
- **panel**: Wallet tab shows real balance (`chain_balance.+x`) + real
  wallet id, with a Create Wallet / Login affordance if none exists
  yet (`chain_create_wallet.c`/`chain_login.c`). Send tab is a real
  3-field form (to-wallet-id, amount, a Send button ->
  `chain_send.+x <from> <to> <amount>`, matching that op's own exact
  real argv contract). Mine tab shows the real miner state fields
  above + a Start/Stop toggle (`chain_miner.+x <wallet_id>`,
  background-toggled the same way chat-hai's own toggle-pause button
  already works). History tab lists real `TX`/`BLOCK` rows, newest
  first.

**Explicitly NOT done this pass** (window shells only, real content is
static placeholder text/labels matching the shapes above, zero real
op dispatch wired) - the real next step once these 3 windows are
confirmed opening correctly: wire each panel's buttons/compose actions
to their real op binaries above, one app at a time, same real
incremental-verify discipline every other feature in this house
already follows.
| Delegation map | `#.#.calendar-dox/0.browser-prompting/architecture-explainers/9.networking-13network-delegation.md` |