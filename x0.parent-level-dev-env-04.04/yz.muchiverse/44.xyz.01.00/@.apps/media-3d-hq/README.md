# media-3d-hq — 3D viewport (house-spec)

Split from the short-lived `media-img3d-hq` hybrid. Source:
`103.media-studio/103.3d=blender-clone/` (`HOW2_BLEND.md`).

2D paint lives in `@.apps/media-img-hq/`. Camera (orbit/zoom/frame)
belongs only here; reuse piececraft Interact-Mode later, do not invent
a second camera stack in the renderer.

## Works now
Demo Cube/Sphere/Ground wireframe, Sel/Grab/Rot/Scl, orbit/zoom/frame,
outliner pick/delete, +cube/+sphere. `canvas.raw` blit. No renderer C.

## Still vs HOW2
MMB orbit, Assimp import, real mesh pick, wireframe toggle Z.
