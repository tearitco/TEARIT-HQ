# House Runtime Ledger — Architecture

## Concept

A single pipe-delimited file, inside the **current user's xyzfs**, where every program in the user's session writes an ONLINE entry on start and an OFFLINE entry on stop. Peers read the file to discover who's running.

Think `~/.bash_logout` for file-mediated IPC — the user owns the processes, the user's xyzfs owns the ledger.

## Why xyzfs, not the house root

Per user-pal signup (`0.user-pal👤️/00.login-signup/`), each signed-in human has a UUID-based tree:

```
xyzfs/users/<uuid>/
  meta.txt          — uuid, user_id, display_name, created_at
  home/
    avatars/
    wallet.txt
    exchange/
    net/
  projects/
```

The ledger belongs in `home/runtime/` because:
- The user **owns** their processes (like `pkill` by user)
- Multiple users on the same house each have their own ledger
- xyzfs is durable (survives /tmp session cleanup)
- The directory already exists at signup (provisioned once), the `runtime/` subdir is created on first write

## Resolution Chain

```
house_root.txt          — "where is the house?" (written by app launcher)
  → current_login.txt   — "who am i?" (<house>/0.user-pal👤️/00.login-signup/current_login.txt)
    → ledger.txt         — "what's running?" (<house>/<current_xyzfs>/home/runtime/ledger.txt)
```

Each file is a known path relative to the previous one. No config, no env vars beyond `PRISC_PROJECT_ROOT` (house root).

## Ledger Schema

Pipe-delimited, same shape as `master_ledger.txt` in `101.ledger-player-npc-simple+3`:

```
timestamp|event|type|project_id|session_root|pid|display_name|inbox_path
```

| Field | Example | Meaning |
|-------|---------|---------|
| timestamp | `2026-07-29T07:00:00` | ISO 8601 |
| event | `ONLINE` / `OFFLINE` | Lifecycle event |
| type | `editor` / `widget` / `app` | Program category |
| project_id | `agy-editor` / `file-menu` | Which project |
| session_root | `/tmp/.text-editor-xyz-editor-1234` | Session dir (for IPC) |
| pid | `3877162` | Process ID (for aliveness checks) |
| display_name | `text-editor-xyz` / `FILE MENU` | Human-readable |
| inbox_path | `pieces/system/widget_cmds/inbox.txt` | Command inbox (relative to session_root) |

## Ops

### `ledger_append <event> <type> <project_id> <session_root> <pid> <display_name> <inbox_path>`
Appends one line to the ledger. Resolves the ledger path from the current user identity. Shape identical to `ledger_append.c` in `101.ledger-player-npc-simple+3`.

### `ledger_peers <type>`
Scans the ledger for the latest `ONLINE` entry of each `project_id` matching `<type>`, checks that the PID is alive (`/proc/<pid>`). Returns a list of active peers with their `session_root` and `inbox_path`.

Replaces `find_widget_session()` in `editor_menu_input.c`.

## Lifecycle

### Program start (button.sh `run` / `run-widget`)
```
ledger_append ONLINE editor agy-editor /tmp/session-1234 $$ "text-editor-xyz" pieces/system/widget_cmds/inbox.txt
```

### Program stop (cleanup trap)
```
ledger_append OFFLINE editor agy-editor /tmp/session-1234 $$ "text-editor-xyz" pieces/system/widget_cmds/inbox.txt
```

## Migration from current scan pattern

**Current** (`editor_menu_input.c` lines 204-239):
```
read house_root.txt
scan &.widgits/file-menu/pieces/sessions/*/focus.txt for session_root=<editor>
→ found: write to widget's interact_relay.txt
```

**New**:
```
read house_root.txt
read current_login.txt → resolve xyzfs_path
read ledger.txt
ledger_peers widget → find all active widgets
→ pick a widget, write to its inbox_path or interact_relay.txt
```

Both editor and widget write to the same ledger on start/stop. Neither scans the other's session dir — they read the shared file.

## Crashed processes

If a program crashes without writing OFFLINE:
- `ledger_peers` checks `/proc/<pid>` — stale entries are skipped
- No heartbeat needed, no sweep daemon

## Relationship to existing `widget+plan.txt`

The widget plan (`widget+plan.txt`) describes the widget cmd bus (inbox/status). The ledger replaces the discovery side: instead of the editor scanning widget session dirs for `focus.txt`, both sides write to the ledger and the editor queries it.

The cmd bus itself (inbox.txt → status.txt → widget_bridge.txt) stays unchanged.
