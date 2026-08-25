# Muchi Image — Phase 1 try card (Photoshop-shaped)

## Run
```bash
cd 103.media-studio/103.img-editor
sh button.sh r
```
Stop: **Esc** or `sh button.sh kill`

## Layout
- **File** menu (top)
- **Tool strip** (left): B brush · E eraser · G fill · R rect · I eyedrop · H hand
- **Canvas** (center) with checkerboard under transparency
- **Layers** (right): 1–6, visibility, active highlight
- **Status** bar: tool, brush size, zoom, UI fps

## Try
1. Demo doc loads with sample shapes.
2. **B** brush — drag paint · **[ / ]** size · **X** swap FG/BG  
3. **E** erase · **G** click fill · **R** drag rectangle  
4. **I** sample color · **H** pan canvas  
5. **1–6** select layer · **V** toggle visibility · **N** new layer  
6. **Ctrl+Z** undo (per stroke on active layer)  
7. **Ctrl+S** / File → Export PNG → `pieces/apps/player_app/export.png`  
8. Drop **PNG/JPG** (house paths OK) onto window → new layer  

## Keys
| Key | Action |
|---|---|
| B E G R I H | tools |
| [ ] | brush size |
| X | swap FG/BG |
| + - / wheel | zoom |
| arrows | pan |
| 1–6 | active layer |
| V | layer visibility |
| Del | clear active layer |
| Ctrl+Z | undo |
| Ctrl+S | export PNG |
| d | reload demo |
| Esc | quit |

## CPU budget
- UI ≤**20fps** while painting, ≤**8fps** idle  
- Main loop always sleeps (no busy-spin)  
- Composite + GL upload only when dirty  
- **ffmpeg only** on import/export (never on paint path)  

## Honest MVP limits
Fixed 800×600 canvas, max 6 layers, no masks/adjustments/text/filters, freeglut not CHTPM shell. Export is flattened PNG via ffmpeg.

## Files
- `ops/ie_main.c` — app  
- `../shared/media_drop_path.c` — house-safe drop paths  
- `pieces/apps/player_app/canvas.raw` — for future canvas-widgit (`mode=canvas`)  
- `pieces/apps/player_app/export.png` — export  
