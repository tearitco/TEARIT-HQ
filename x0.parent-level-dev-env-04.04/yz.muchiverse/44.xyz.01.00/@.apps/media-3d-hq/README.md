# media-3d-hq — 3D viewport (house-spec)

Split from the short-lived `media-img3d-hq` hybrid. Source:
`103.media-studio/103.3d=blender-clone/` (`HOW2_BLEND.md`).

2D paint lives in `@.apps/media-img-hq/`. Camera (orbit/zoom/frame)
belongs only here; reuse piececraft Interact-Mode later, do not invent
a second camera stack in the renderer.

## Works now
Loads **cursword 3D voxels** at start (`media/cursword.voxels.csv`,
house `x,y,z,r,g,b` phymoji format). Also `LOAD:` **`.obj`** (parsed
here) and **`.fbx`** (via `assimp export` → obj). Orbit/tools/gutter.
No renderer C.

## Still vs HOW2
MMB orbit, Assimp import, real mesh pick, wireframe toggle Z.
