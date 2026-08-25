# Muchi Blend — Phase 1 try card (Blender-shaped)

## Run
```bash
cd 103.media-studio/13.3d=blender-clone
sh button.sh r
```
Stop: **Esc**, title-bar ✕, or `sh button.sh kill`

## Layout
- **File** menu — New / Demo / Add Cube / Add Sphere / Quit  
- **Tool strip** — Select · **G**rab · **R**otate · **S**cale  
- **3D viewport** — grid, axes, solid/wire meshes  
- **Outliner** (right) — object list + transform readout  
- **Status** — tool + fps  

## Try
1. Demo scene: Cube, Sphere, Ground.  
2. **MMB** drag — orbit · **Shift+MMB** — pan · **wheel** — zoom  
3. Click object (or outliner / **1–9**) to select  
4. **G** grab · **R** rotate · **S** scale — drag mouse · **X/Y/Z** constrain · **LMB** confirm · **RMB/Esc** cancel  
5. **Z** wireframe toggle · **.** frame selected · **C** cube · **U** sphere  
6. Drop **`.obj`** or **`.fbx`** (house paths OK) onto viewport  

## Keys
| Key | Action |
|---|---|
| G R S | grab / rotate / scale |
| X Y Z | axis constrain (while transforming) or X=delete when idle |
| Z | wireframe |
| . | frame selected |
| 1 3 7 | front / side / top-ish views |
| C U | add cube / sphere |
| d | demo scene |
| n | new scene |
| arrows / PgUp/Dn | nudge selected |
| Esc | cancel xform or quit |

## Import
Via **Assimp**: `.obj`, `.fbx`, and often `.gltf`/`.glb`/`.dae`/`.stl`/`.ply`.  
Meshes are auto-centered and scaled into view.

## CPU budget
- ≤**20fps** while orbiting/transforming, ≤**8fps** idle  
- Main loop always sleeps  
- No per-frame disk I/O  

## Honest MVP limits
No edit mode mesh verts, no materials/textures, no animation, no true Blender keymap parity, freeglut not CHTPM shell. Selection is screen-space center pick (not full mesh raycast).
