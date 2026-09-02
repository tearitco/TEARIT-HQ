# Glossary

*A fuller glossary appendix, if needed, lives at
`09-appendix/GLOSSARY-APPENDIX.md`. This is the core, orientation-level
list.*

- **house / TEARIT-HQ** — this whole repo; a file-backed desktop OS.
- **livedesk** — the taskbar + desktop shell that spawns/manages entities.
- **entity** — one running app/game/tool spawned onto the desktop, its own process.
- **pal (PAL)** — the tiny scripting language/VM (`prisc+x`) gluing ops together in a loop; also "muchi-pals" the pet/companion app family — context disambiguates.
- **PDL** — the house's plain-text config format, `SECTION | KEY | VALUE` rows.
- **chtpm** — the layout/markup file format (`.chtpm`) describing a screen: buttons, text, substitution variables. Two different engines parse it (see below).
- **chtpm_parser_pal family** — the original ASCII/text-grid rendering engine and its PAL-VM ecosystem; no box model.
- **khtpm family** — the newer Elem/CSS rendering engine (`khtpm_core_render.c`, formerly `khtpm_entity_menu_render.c`), real x/y/w/h layout + CSS styling.
- **Elem** — the positioned, styled tree node type in the khtpm family; the "centroid" every renderer walks.
- **manager** — a separate compiled process (`<name>_manager.c`) owning a feature's business logic/state, publishing a plain-text state file for the shared renderer to read generically. Never inline business logic in the shared renderer.
- **relay** — a plain file (e.g. `livedesk_agent_relay.txt`, `interact_relay.txt`) used to inject input, consumed same-tick by a poll loop. The house's standing alternative to synthetic X11 events.
- **receipt** — a small adjacent file recording a renderer's own output dimensions/checksum, for verification.
- **session** — `pieces/sessions/<timestamp>-<pid>/`, an isolated per-instance working copy under the copy-in/persist-out model.
- **HQ window** — one of the merged khtpm-family app modes (db-hq, events-hq, chat-hai, palettes, bookmarks, stats-hq, taskbar-settings).
- **nav_index / digit-jump** — the house-wide keyboard navigation convention: numbered rows jump-focusable by digit key.
- **xyzfs** — the house's per-user runtime filesystem tree (accounts, home dirs, harness results) — live runtime state, not source.
- **#.desktop/** — house-root runtime state directory (relay files, history, PDL config) — live runtime state, not source.
- **Harnecient** — the house's harness-authoring/testing-delegation subsystem and its own guide/lesson series.
