#.desktop — House "desktop tray" for portable pieces
====================================================
House: 44.xyz…  |  Date: 2026-07-28

WHAT
----
A **file desktop** (not the OS wallpaper): a shared folder where pickers,
event-editor, zoo/pets, and mutaclysm exchange **portable piece packages**.

Mental model:
  tile-picker / map assets  → place onto #.desktop/
  event-editor              → open/edit packages on #.desktop/ (or muta live)
  pets / charas             → can live as desktop windows + folders here
  mutaclysm                 → DROP / import from #.desktop/ into world_01

This is the same idea as `exchange/` for pet envelopes, but **maker-wide**:
events, tiles, entities, not only pets.

LAYOUT
------
  #.desktop/
    events/      # event packages (event.ir + event.pal + state)
    entities/    # pets, NPCs, charas (piece.pdl + state + optional events)
    tiles/       # tile / emoji brush stamps (glyph + meta)
    inbox/       # optional drop zone watched by focused mutaclysm
    README.txt   # this file

Override root: env XYZ_DESKTOP_ROOT=/abs/path

RULES
-----
  - Packages are directories (or single .pdl + sidecar), never opaque blobs only.
  - SAVE/LOAD of mutaclysm copies world_01; desktop is **outside** the live
    world until imported (so you can edit offline, then drop in).
  - Drag-drop (X11) can target desktop windows later; file ops are source of truth.

See: 101.mutaclsym…/dox/xelector-context.md § desktop + event-editor widget
     &.widgits/event-editor/

LIVEDESK CONFIG (.pdl) — canonical set
--------------------------------------
#.desktop also hosts the livedesk taskbar (khtpm) state/config. This is the
canonical map — every file here is read at runtime by the khtpm binaries from
house_root (khtpm_taskbar_manager.c / khtpm_strip_parser.c / khtpm_hq_render.c
/ crypt_autostart.c), so moving/renaming one here means recompiling C.

  livedesk_taskbar.pdl    strip cell labels/cmds, hq menu rows, strip offsets,
                          datetime lang. Editable, NO recompile (defaults live
                          here and the manager publishes them via ${vars}).
  livedesk_theme.pdl      theme colors (parser: khtpm_css_parser.c).
  livedesk_launchers.pdl  hq app launcher paths (ktb_hq_launcher_path()).
  livedesk_shortcuts.pdl  shortcut/tab entries.
  hq_ui.pdl               -hq app UI config (font_scale, window x/y).

RUNTIME (C-written, do NOT hand-edit):
  livedesk_taskbar.pid       taskbar pid (written by manager, read by crypt_autostart)
  livedesk_open.txt          entity/taskbar registry (PATH=/PID= rows)
  livedesk_registry.lock     cross-process lock around the registry
  livedesk-nav-claims/       per-window nav claim registry (livedesk_nav_claims.txt)
  khtpm_strip_parser.log     taskbar stdout/stderr log (shell runners redirect here)

NOTE: the house-wide LAUNCH list (which processes autostart at login) is
$.crypts/autostart.pdl, deliberately OUT of this dir — it is owned by the
autostart daemon (crypt_autostart.c). Its LAUNCH rows point at these binaries
and at the entity pals paths; it is the single source of truth for what runs.

