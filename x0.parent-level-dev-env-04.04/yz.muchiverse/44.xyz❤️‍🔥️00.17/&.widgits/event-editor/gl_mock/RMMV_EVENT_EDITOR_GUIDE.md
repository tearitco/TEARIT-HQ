# RMMV Event Editor (pure GLUT) — guide & build status

**House:** `44.xyz…`  
**Path:** `&.widgits/event-editor/gl_mock/`  
**Date:** 2026-07-28  
**Status:** UI chrome **built** (look/nav). Product logic **mostly not wired**.  

This is the **good one** — the RPG Maker MV–style freeglut layout (not the flat “one long list”, not the headless CHTPM→rgb mirror).

---

## 1. Is it already built?

| Layer | Built? | Notes |
|-------|--------|--------|
| RMMV **layout chrome** (left/right/footer/methods) | **YES** | `ee_gl_mock.c` paints it in freeglut |
| Continuous nav 1..N + multi-digit | **YES** | same model as CHTPM digit_accum |
| Commands \| Scratch toggle | **YES** | Tab or method #5; rewrites content list |
| Page tabs select | **YES** | page_sel 1–4 (visual only) |
| GLUT fonts / readable text | **YES** | built-in GLUT bitmaps (no registry fonts needed) |
| Save / Load / Import / Export | **STUB** | status line only |
| Edit event.pal / IR / switches for real | **NO** | |
| Desktop package I/O | **ops exist**, not called from GLUT yet | `../ops/ee_*.c` |
| Mutaclysm Space → open this window | **NO** | open request path exists on muta side |
| event_run (runtime of scripts) | **NO** | |

**Summary:** You can open and navigate a **presentable RMMV-like shell**.  
You cannot yet author real events end-to-end. That is what we build next.

---

## 2. How to run

```bash
cd '&.widgits/event-editor/gl_mock'
sh button.sh r          # or: sh button.sh run
```

Optional A/B (product pipeline, different look):

```bash
cd '&.widgits/event-editor'
sh button.sh r          # chtpm → current_frame → rgb → gl_mirror
```

| Key | Action |
|-----|--------|
| ↑ ↓ | Move focus (global list) |
| ← → | Jump zone (methods / pages / fields / contents / footer) |
| PgUp/PgDn | Jump ±8 items |
| `0`–`9` | Multi-digit jump (e.g. `1` then `7` → #17) |
| Enter | Activate focused item |
| Tab | Commands ↔ Scratch |
| Esc | Clear digit accum, or quit |
| q | Quit |

---

## 3. Visual reference (saved)

| File | What |
|------|------|
| `VISUAL_REF_rmmv_target.jpg` | Target look (RMMV-style chrome reference) |
| `VISUAL_REF_rmmv_early.jpg` | Earlier generated mockup (same family) |
| `event_editor_rmmv_look.jpg` | Original session mockup (if present) |
| **Live code** | `ee_gl_mock.c` — source of truth for layout |

Open refs:

```bash
xdg-open '&.widgits/event-editor/gl_mock/VISUAL_REF_rmmv_target.jpg'
```

Layout map (must match when coding):

```text
+======================================================================+
| Event Editor                                                         |
| status: Nav > _                                                      |
+======================================================================+
| [1 Save][2 Load][3 Import][4 Export][5 Toggle][6 Edit.pal][7][8 Help]|
+---------------------------+------------------------------------------+
| Event                     | Contents  [ COMMANDS* | Scratch ]        |
| Name: [door_guard    ]    | #  Command                               |
| Pages: [1][2][3][4]       | 17 Show Text ...                         |
| Conditions box            | 18 Show Choices ...                      |
| Image [@]   Fields 13-16  | ...                                      |
| map / package paths       | 28 (empty)                               |
+---------------------------+------------------------------------------+
| [29 OK][30 Cancel][31 Apply][32 Add Cmd][33 Palette][34 Desktop]     |
+======================================================================+
```

**Continuous nav map (do not restart per section):**

| # | Region |
|---|--------|
| 1–8 | Methods bar |
| 9–12 | Page tabs |
| 13–16 | Trigger / Priority / Options / Walk |
| 17–28 | Contents (Commands or Scratch rows) |
| 29–34 | Footer |

---

## 4. Source files

| Path | Role |
|------|------|
| `ee_gl_mock.c` | RMMV freeglut UI (this guide’s target) |
| `button.sh` | `sh button.sh r` compile+run (POSIX sh) |
| `README.txt` | Short pointer |
| `../ops/ee_*.c` | Real file ops (desktop, package, import) — **wire next** |
| `../pieces/chtpm/layouts/event_editor.chtpm` | Alternate product (ASCII→rgb) path |

---

## 5. What “building it” means next

Recommended order (functionality on **this** GLUT shell):

1. **Wire method bar to ops**  
   - Save → `ee_package_init` / write package under `#.desktop/events/`  
   - Export → `ee_export_entity`  
   - Import → `ee_import_to_world` (needs muta world path / focus)  
   - Load → pick package from `#.desktop/events/`

2. **Editable fields**  
   - Name, trigger, page conditions → `state.txt` in package dir  
   - Persist on OK/Apply

3. **Contents list**  
   - Add Cmd / insert RMMV-ish command  
   - Dual write `event.ir.pdl` + `event.pal`  
   - Scratch view stays alternate render of same graph

4. **Mutaclysm spawn**  
   - Space → Event already writes open request; launch `gl_mock` or later product path with package_dir

5. **Keep A/B**  
   - GLUT = pretty primary for makers (your preference to test)  
   - `../button.sh r` = house-standard mirror path (optional)

---

## 6. Honest one-liner

**Built:** the good RMMV-looking GLUT window + nav.  
**Not built:** real save/load/event scripting behind the buttons.  
**Guide + visual refs:** this file + `VISUAL_REF_*.jpg`.

*End RMMV_EVENT_EDITOR_GUIDE.md*
