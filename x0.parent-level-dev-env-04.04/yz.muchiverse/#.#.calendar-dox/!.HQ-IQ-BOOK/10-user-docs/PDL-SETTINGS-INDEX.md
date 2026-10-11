# Where the .pdl customizations live (developer index)

2026-10-09, claude. There was **no** index of these. `@.apps/piececraft-hq/pieces/system/maps.pdl` indexes only the pc-hq levels. This page lists the tuning
`.pdl` files found by reading them; paths are relative to the house `44.xyz.01.00`. A `.pdl` is either pipe rows (`SECTION | KEY | VALUE`, `OPT | name | n`) or plain
`key=value`; open the file and copy the style of the line next to the one you change. Most are live-reloaded; windows with a cached binary or template need a reopen.

| File | Format | What you can tune |
|---|---|---|
| `#.desktop/hq_ui.pdl` | key=value | per-desktop UI options read by every khtpm window (event object visibility, incremental reparse, dropdown row cap, ...). **Owner's file: add lines only** |
| `#.desktop/livedesk_theme.pdl` | key=value | desktop colours/theme |
| `#.desktop/livedesk_taskbar.pdl` | rows (158) | taskbar cells, menus, always-on-top toggle |
| `#.desktop/ai_backend.pdl` | key=value | the shared Ollama/Gemma endpoint (the Mac, never local) |
| `#.desktop/proc_mon_cpu_watch.pdl` | key=value | process-monitor CPU watch |
| `@.apps/piececraft-hq/pieces/system/keybinds.pdl` | rows | every pc-hq key -> verb, camera/POV keys, `OPT multipress_compensator`, `minimize_pauses_game`, and (new) the dispatcher key-drop tuning `dispatch_stale_factor`, `dispatch_stale_floor_ms`, `dispatch_stale_ceil_ms`, `dispatch_arrow_run_cap` |
| `@.apps/piececraft-hq/pieces/system/view.pdl` | key=value | `see_through` (sky/parallax), fog start/end, view options |
| `@.apps/piececraft-hq/pieces/system/hud.pdl` | rows | board HUD master switch, per-element switches, layout |
| `@.apps/piececraft-hq/pieces/system/move_range_style.pdl` | rows | Move range look |
| `@.apps/piececraft-hq/pieces/system/maps.pdl` | rows | index of the levels (derived; nothing reads it) |
| `@.apps/piececraft-hq/pieces/system/maps/<book>/game.pdl` | rows | one book: icon, label, desks. The File menu is generated from these |
| `.../maps/<book>/<desk>/{extrusion,events}.pdl`, `state.pdl`, `tunables.pdl` | rows | wall heights, event declarations, game counters, rules numbers |
| `#.ref/menu/event_commands.registry.pdl` | rows (700+) | every event command the events editor offers; add a block, no rebuild |
| `&.widgits/*/` per-widget `*.pdl` | mixed | each widget owns its own (eden: `game.pdl`, ledgers) |

Gaps this index does not fill (do next): a generated index (script walks `*.pdl`, prints file + header comment + keys + defaults, run on commit so it cannot rot),
and a Settings window that lists and edits them (see layout studio strategy). Until then, grep for the key name: `grep -rn "dispatch_stale" --include=*.c --include=*.pdl`.
