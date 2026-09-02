# CHTPM nav mock (media suite)

Visual-only bridge toward house CHTPM nav language, inspired by  
`&.widgits/event-editor/gl_mock/ee_gl_mock.c`.

## What you see
**Top methods bar** (`CHTPM_NAV_BAR_H` = 28px) is reserved. App chrome is **`glTranslate`’d down** so File/tools are not covered. Mouse Y uses `chtpm_nav_mouse_y(my)`.

| Badge | Meaning |
|-------|---------|
| `[>] n Label` | focused mock nav chip (top bar) |
| `[ ] n Label` | other chips |

Yellow outline on the focused content region.

## Keys (do not steal app tools)
| Key | Action |
|-----|--------|
| **Tab** / **Shift+Tab** | cycle mock focus |
| **\`** (backtick) | toggle **digit-jump** mode |
| **0–9** | only in digit-jump mode (house digit_accum style) |
| **Enter** | mock ACTIVATE status only (when digit mode) |

Space, B/E/G, G/R/S, play keys, etc. stay with the real app.

## Slot map (Phase-1)

| App | Slots |
|-----|--------|
| **DAW** | Methods/File · Transport · ChannelStrip · Arrangement · PianoRoll · Mixer? · Status |
| **Video** | Methods/File · Transport · PreviewCanvas · Inspector · TimelineTracks · Status |
| **Image** | Methods/File · ToolStrip · Canvas · Layers · Status |
| **Blend** | Methods/File · ToolStrip · Viewport3D · Outliner · Status |

## Code
- `chtpm_nav_mock.h` / `chtpm_nav_mock.c` — shared
- Linked from each `button.sh` alongside the app `*_main.c`

## Later bridge
Map slot index → real CHTPM `KEY:n` / `piece_methods` / `[>]` in `current_frame.txt`.  
Do **not** reimplement `chtpm_parser_pal` inside freeglut forever — this mock is the target layout only.
