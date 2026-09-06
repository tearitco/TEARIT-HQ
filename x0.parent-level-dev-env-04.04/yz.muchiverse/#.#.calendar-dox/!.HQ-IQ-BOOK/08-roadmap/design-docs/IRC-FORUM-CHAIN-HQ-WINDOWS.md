# IRC / Forum / Chain → X11-HQ windows — Phase 2 implementation plan

**Status: PLAN, 2026-09-06.** Companion to
`NETWORK-CELL-HQ-WINDOWS-DESIGN.md` (that doc's §6 recipe + §12 per-app
UI shapes are the source of truth for *what* each window is; this doc
is the concrete *how*, grounded in a fresh read of each app's real
files). Browser is a separate effort (opencode). Phase 1 (network
taskbar cell wired, live-verified 2026-08-31) is DONE — this is Phase 2.

## Hard rules carried in from TPMOS-COMPLIANCE-DEBT.md §5–6 + CENTROID_GOLD_STD §3

1. **Zero new C in `khtpm_core_render.c` / `khtpm_entity_menu_render.c`.**
   No `g_is_irc_*`, no per-mode dispatch branches, no hand-built `Elem`
   tree. Both were tried by earlier agents and fully reverted. The
   renderer already draws an unknown `<window class="...">` correctly
   via the generic sidebar/panel/scrolllist/cli_io path.
2. **New class names** `irc-chat-window` / `forum-window` /
   `chain-window` — deliberately NOT `chat-window` (would trip
   chat-hai's persona-loop `<module>` assumptions in older docs) and
   not any existing `g_is_*` trigger token.
3. **Reuse each CLI app's ops verbatim** (`chat_post_message.+x`,
   `chat_replay_ledger.+x`, `forum_*`, `chain_*`). The window manager
   `popen()`/`system()`s them with their real argv contracts; it never
   reimplements their logic. No new sockets (PAL-NET-STANDARD).
4. **Discovery already done** (this doc's §2). Re-run each app's
   `button.sh` standalone once before wiring the P2P daemons in v2.

## 1. Shape — same three-part house pattern as chat-hai / csv-hq / music-player-hq

Per app, a new sibling dir `&.hq-apps/<app>-hq/`:

- **`<app>-hq.xhtpm` + `.css`** — static template. `<window
  class="<app>-window" vars="<app>_ui.txt">`, one `<module
  src=".../ops/+x/<app>_manager.+x"/>`. Sidebar + panel + `<cli_io>`,
  all generic tags. Modeled on `&.hq-apps/chat-hai/chat-hai.xhtpm`
  (3 KB, the cleanest existing example).
- **`ops/<app>_manager.c`** (compiled `<module>`, music-player-hq
  style) — argv `<house_root> <package_dir> [arg3]`. Publishes
  `<app>_ui.txt` (key=value), polls `<app>_action.txt` (`seq=`/`cmd=`).
  Drives the real CLI ops. `build_<app>_manager.sh` beside it.
- **`ops/<app>_item.sh` + `<app>_send.sh`** — thin action shims, exact
  chat-hai convention:
  - `<item action="'${PKG}/ops/<app>_item.sh' 'VERB ARG'">` → shim is
    called `<shim> 'VERB ARG' '<pkg>' '<house>'` → writes one
    `seq/cmd` line to `<app>_action.txt`.
  - `<cli_io action="'${PKG}/ops/<app>_send.sh'">` → shim is called
    `<shim> '<pkg>' '<house>' '<live typed text>'` (renderer's
    `default_cli_io_run_action()` contract — verified in
    `chat-hai/ops/ch_send.sh`'s header) → writes `cmd=SEND:<text>`.
- **`open_<app>_hq.sh`** — launcher copied from
  `&.hq-apps/chat-hai/button-pal.sh` shape (build-if-missing,
  single-instance `pgrep` guard, `setsid nohup "$RENDER_BIN"
  "$HOUSE" "$XHTPM"`, record pid).
- **Wire-in:** point the Phase-1 `livedesk:open-network:<key>` handler
  (or `livedesk_launchers.pdl` row) at `open_<app>_hq.sh` instead of
  the CLI `button.sh`. Both stay valid during dev.

### Why a compiled manager, not chat-hai's bash `loop.sh` + `projector.+x` split

chat-hai splits engine (bash persona loop) from projection (compiled
projector) because its engine is genuinely its own long-running thing.
Here the "engine" is a set of one-shot ops (`chat_replay_ledger`,
`chat_post_message`) plus, in v2, two daemons. One compiled manager
that polls the action file, runs the ops, and writes the UI file is
simpler and matches the current house pattern (pdl-read, csv-hq,
music-player-hq all do exactly this). No bash projector.

## 2. Discovery — real schemas & ops (read 2026-09-06)

### IRC — `044.pal-chat-irc👥️+2/`
- **Ledger:** `data/master_ledger.txt`, one row per line:
  `MSG|<msg_id>|<room>|<user>|<ts>|<text>`
  (`msg_id` = `<epoch>-0-<seq>`; `ts` = epoch seconds).
- **Rooms** are just the distinct `<room>` values (also dirs under
  `rooms/<room>/`). **Users** = dirs under `users/<user>/`.
- **Ops** (`ops/+x/`, all built): `chat_post_message.+x <room> <user>
  <text>` (appends a MSG row); `chat_replay_ledger.+x` (no args, reads
  `$PRISC_PROJECT_ROOT/data/master_ledger.txt`, rebuilds every
  `rooms/<room>/messages.txt`); `chat_create_user.+x`,
  `chat_switch_user.+x`, `chat_inbox_watcher.+x`, `palnet_peer.+x`
  (own_kind `irc_node`), `chat_compose_frame.+x` (legacy TUI — NOT
  used).
- **Env:** ops resolve the project via `PRISC_PROJECT_ROOT` (falls
  back to cwd). The manager exports it = the real
  `044.pal-chat-irc👥️+2/` abs path.

### Forum — `041.pal-forum👥️/`
- **Schema (per NETWORK-CELL §12, verify at build):** `POST|<post_id>|
  <user_id>|<ts>|<text>|<image_id>`. Per-user `users/<u>/wall.txt`,
  `users/<u>/feed_cache.txt`.
- **Ops:** `forum_post.+x`, `forum_like.+x`, `forum_retweet.+x`,
  `forum_follow.+x`, `forum_dm.+x`, `forum_compute_feed.+x` (feed
  output), `forum_create_user.+x`, `forum_switch_user.+x`,
  `forum_inbox_watcher.+x`, `palnet_peer.+x`.

### Chain — `041.pal-chain⛓️/`
- **Schema:** `data/blockchain.txt` rows
  `BLOCK|<idx>|<prev_hash>|<nonce?>|<hash>|<ts>|<miner_wallet>|` and
  (per §12) `TX|<from>|<to>|<amount>|<ts>|<tx_id>`; `data/pending_tx.txt`;
  a miner state file (`net/miner_status.txt`).
- **Ops:** `chain_create_wallet.+x`, `chain_login.+x`,
  `chain_balance.+x`, `chain_send.+x <from> <to> <amount>`,
  `chain_miner.+x <wallet_id>` (background toggle),
  `chain_inbox_watcher.+x`, `palnet_peer.+x`.
- **Wallets** = dirs under `wallets/<id>/`.

## 3. Per-window layout (from NETWORK-CELL §12)

### IRC (`irc-chat-window`)
- sidebar: `${n_rooms}` `<repeat bind="r">` of `<item action="'…/irc_item.sh'
  'ROOM ${r.name}'">`; header `+ New Room` / current-user line
  (`<item> USER` / `NEWUSER`).
- panel: `<text>` `${cur_room} — ${cur_user}`; `<scrolllist>` of
  `${n_msgs}` `<repeat bind="m">` `<text class="${m.cls}"
  label="${m.text}"/>` (cls = `msg-self` / `msg-other`); `<cli_io
  id="composer" action="'…/irc_send.sh'">` → `SEND:<text>` →
  `chat_post_message.+x ${cur_room} ${cur_user} <text>`.

### Chain (`chain-window`) — wallet dashboard, NOT a feed
- sidebar tabs: Wallet / Send / Mine / History (`<item action="'…'
  'TAB wallet'">` …).
- panel content swapped by `${cur_tab}` via `show=`-gated blocks:
  Wallet = balance (`chain_balance.+x`) + wallet id + Create/Login;
  Send = 3 `<cli_io>` (to / amount) + Send `<item>` →
  `chain_send.+x`; Mine = miner state fields + Start/Stop toggle
  (`chain_miner.+x`); History = `${n_rows}` `<repeat>` of TX/BLOCK
  rows, newest first.

### Forum (`forum-window`)
- sidebar tabs: Home / Following / DMs / Notifications.
- panel: compose box at top (`<cli_io>` → `forum_post.+x`); feed of
  `${n_posts}` `<repeat bind="p">` rows (user / text / ts + Like /
  Retweet `<item>`s → `forum_like.+x` / `forum_retweet.+x`). DMs tab
  reuses the row shape sourced from `forum_dm.+x`.

## 4. `<app>_ui.txt` schema (IRC shown; forum/chain analogous)

```
n_rooms=<N>
r_0_name=lobby        r_0_cls=          (r_<i>_cls="active" for cur_room)
...
cur_room=lobby
cur_user=guest
n_msgs=<M>            (cap ~200, newest-last, room-filtered)
m_0_text=<user>: <text>      m_0_cls=msg-other|msg-self
...
n_users=<U>  u_0_name=...    (for the switch-user control)
status=<short status line>
```

Repeat binds: rooms `r`, msgs `m`, users `u` — key prefix MUST match
`bind=`. Manager escapes `|`, CR, LF out of every published value
(same `uisan()` discipline network-browser's projector uses).

## 5. `<app>_action.txt` verbs (IRC)

`ROOM:<name>` (switch current room; create the dir if new),
`SEND:<text>` (→ `chat_post_message.+x`), `USER:<name>` (switch),
`NEWUSER:<name>` (→ `chat_create_user.+x`), `RESCAN` (re-run
`chat_replay_ledger.+x`, re-read ledger). Chain: `TAB:<name>`,
`SEND:<to>|<amt>`, `MINE_TOGGLE`, `WALLET_NEW`, `LOGIN:<id>`. Forum:
`TAB:<name>`, `POST:<text>`, `LIKE:<post_id>`, `RETWEET:<post_id>`,
`FOLLOW:<user>`.

## 6. Scope — honest small v1, then v2

**v1 (build + verify first): local ledger, no mesh.**
The manager reads/writes the app's own local ledger via its real ops
and publishes the UI. Messages/txns/posts made in the window
round-trip through the real ledger file and reappear. NO
`palnet_peer` / `*_inbox_watcher` started yet. This is exactly the
"wire each panel's action to its real op binary" step NETWORK-CELL §12
ends on.

**v2: P2P.** Manager also starts `palnet_peer.+x` (correct own_kind) +
`<app>_inbox_watcher.+x` as children (SIGTERM'd on window close), so a
second instance / a real peer sees the messages. Run each app's
`button.sh` standalone first to confirm exactly which daemons/env it
needs; add only those.

## 7. Order

1. **IRC first** (simplest: one ledger, one message shape, chat-hai
   layout is a near-exact precedent).
2. **Chain** (different shape — tabbed wallet dashboard; proves the
   `show=`-gated multi-tab panel).
3. **Forum** (most ops; reuses Chain's tab pattern).

Each: build headless-verify (frame dump, `<app>_ui.txt` inspection) →
live launch from `open_<app>_hq.sh` → retarget the network-cell row →
live-verify from the taskbar. One app per commit, own small steps.

## 8. Files touched (IRC, v1) — nothing shared, nothing in the boundary

New: `&.hq-apps/irc-chat-hq/{irc-chat-hq.xhtpm,.css}`,
`ops/{irc_chat_manager.c,build_irc_chat_manager.sh,irc_item.sh,irc_send.sh}`,
`open_irc_chat_hq.sh`. Edited: `#.desktop/livedesk_launchers.pdl` OR the
`livedesk:open-network:irc` target (one line). `khtpm_core_render.c`,
`khtpm_entity_menu_render.c`, `khtpm_taskbar_manager.c` — **untouched**.
