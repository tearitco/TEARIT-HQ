event-editor widget — &.widgits/event-editor/
=============================================

SAME SHAPE AS file-menu FOR THE EDITOR
--------------------------------------
  Editor  → file-menu widget   (SAVE/LOAD docs, no second TTY)
  Mutaclysm / desktop packages → event-editor widget

  DEFAULT = widget profile (§35):
    gl_window=1          PRIMARY UI
    ascii_renderer=0     do not steal parent terminal
    still writes: current_frame.txt, rgb_frame.raw (headless ASCII exists)

  OPTIONAL ASCII secondary:
    SHOW_ASCII=1  or  ./button.sh run --ascii
    starts system/renderer so you can watch the same frame in a terminal

PIPELINE (mutaclysm menu law — never freeglut custom UI)
--------------------------------------------------------
  chtpm_parser_pal  →  current_frame.txt   ← nav source ([>] n. labels)
  chtpm_rgb_render  →  rgb_frame.raw       ← needs pieces/registry/fonts/
  gl_mirror         →  GL window            ← blit only

Session always symlinks mutaclysm pieces/registry/ (glyph bitmaps).
Without that, GL shows color slabs and no letters.

Headless does NOT mean “no ASCII frame”. It means “no terminal printer”.
GL always mirrors the CHTPM ASCII frame. Use pure ASCII in layouts
(no unicode box-drawing) so glyph.txt 32-126 covers every char.

RUN
---
  ./button.sh compile
  ./button.sh run-widget          # product default (headless GL)
  ./button.sh run                 # same
  ./button.sh run --ascii         # + optional terminal surface
  ./button.sh kill                # emergency cleanup

SPAWN (later, like editor FILE → file-menu)
------------------------------------------
  mutaclysm Space → Event  →  export package + open request
  →  &.widgits/event-editor/button.sh run-widget
  not a second human terminal

Commands | Scratch: KEY:5  (compose rewrites content rows)
Desktop tray: #.desktop/   drop into muta via ee_import_to_world

FUNCTIONALITY TODAY
-------------------
  Product (sh button.sh r):
    [x] CHTPM frame + continuous nav 1..N + multi-digit
    [x] KEY:5 Commands|Scratch toggle
    [x] Page cycle / help messages
    [x] GL text (registry fonts)
    [~] Save/Load/Import/Export/Edit.pal = status stubs only
    [ ] real package edit, event_run, muta auto-spawn

  A/B look (sh gl_mock/button.sh r):
    pure freeglut RMMV-ish UI + continuous nav (same model)
    most actions still stub — compare feel vs product mirror
