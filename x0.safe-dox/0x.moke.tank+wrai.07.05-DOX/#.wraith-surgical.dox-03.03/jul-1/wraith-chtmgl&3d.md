# Wraith CHTMGL & 3D Plan

Date: 2026-06-27
Scope: how Wraith should move from rudimentary text project probes to real RGB-rendered 3D/map/widget presentations while keeping ASCII auditability.

## Problem

The current Wraith validation projects are useful for launcher/body/receipt plumbing, but they are not yet visually ambitious.

Expected direction:
- `wraith-3d-cube` should look like a real GL cube primitive, even though Wraith GL is only presenting an RGB buffer.
- `piececraft-wraith` should move toward the Piececraft reference screenshot: green 3D block tiles, depth, z-order, camera controls, POV switching, and selected object/wireframe style audit affordance.
- `chtmgl-wraith` should look like the `chtmgl-alpha` widget page: real panels, buttons, checkbox, sliders, media placeholders, menu, and a canvas-like preview surface.
- `emoji-studio-wraith` should validate 2D RGBA/image extrusion into a 3D `${game_map}` view using Emoji Studio's CSV convention.

Current reality:
- Wraith exports text/body rows.
- `wraith_rgb_daemon` rasterizes semantic rectangles/text into `current_frame.rgba32`.
- `wraith_gl` only displays that RGB texture.

Therefore, real 3D must be done before GL, inside the RGB conversion path.

## Core Principle

Wraith GL remains a thin presenter.

The 3D/view/widget work belongs in:
- semantic scene export
- RGB converter primitives
- receipts that prove the conversion

Do not make `wraith_gl.c` own scene logic, camera logic, widget logic, or 3D mesh logic.

Do not make `wraith-alpha_manager.c` own project-specific cube/map/widget/emoji logic either.

Project truth must live in auditable project files:
- `project.pdl`
- `layouts/*.chtpm`
- `session/state.txt`
- `session/wraith_body.txt`
- `session/scene.objects.pdl`
- `pieces/**`
- `maps/**`
- `assets/**`

Wraith Alpha may host, discover, and compose project windows. It must not hardcode the internals of `wraith-3d-cube`, `piececraft-wraith`, `chtmgl-wraith`, or `emoji-studio-wraith`.

Rule:
- if it is not in a project file or receipt, it is not project truth.
- if it is hardcoded in the host manager, it is not reusable enough for Wraith.

## What "XYZ Pixel Map To 3D RGB View" Means

The needed pipeline is:

1. Source state
   - cube vertices or tile map cells
   - x/y/z coordinates
   - size, color, material, selection/focus state
   - camera settings

2. Semantic object export
   - object role such as `mesh_cube`, `voxel_tile`, `map_surface`, `widget_button`
   - world bounds and screen bounds
   - z/depth metadata
   - action/hit regions

3. Software projection
   - transform 3D world points into 2D screen points
   - use the same camera-state contract as Piececraft/GL-OS: camera mode, camera position, pitch, yaw, and roll
   - support perspective/POV-style projection, not only isometric projection

4. Software rasterization into RGB
   - draw filled quads/triangles into `current_frame.rgba32`
   - depth-sort faces/tiles before drawing
   - draw outlines/wireframes for audit/focus overlays

5. GL presentation
   - `wraith_gl` uploads/displays the RGB texture only

This is still "GL-looking" because the RGB buffer can contain projected 3D shapes. It is not OpenGL mesh rendering yet.

## Stage 1: Real `wraith-3d-cube`

Goal:
- render a rotating or stepped cube into the RGB buffer.
- store the cube using the existing Piececraft/fuzz-op-gl piece artifact z-slice convention, not a new mesh-only format.
- use the same camera command model as Piececraft so cube controls become the small proof of the later Piececraft path.

Current live validation status:
- `wraith-3d-cube` opens inside Wraith.
- ASCII audit body and GL/RGB preview both appear.
- the project-owned `${game_map}` surface can enter interact mode and show `[^]`.
- `session/history.txt` records `COMMAND: INTERACT`.
- project-owned input op exists at `ops/+x/wraith_project_input.+x`.
- input op consumes `session/history.txt`, updates project-owned state/body/scene files, and leaves Wraith Alpha generic.
- `zslice_piece` can render as a projected wireframe cube when scene metadata includes `rot=` and `camera=`.
- final filled/depth-sorted face rendering is still pending.
- project windows now default to shell/window nav mode on open (`is_map_control=0`).
- `INTERACT` controls target `${game_map}` with `target_surface=game_map`; the `${game_map}` object itself should remain payload-only (`nav=0`).

Do not treat the current projected wireframe as complete. It proves file bridge, interact bridge, project-owned input, and metadata-driven RGB projection. It does not yet prove Piececraft-grade filled face/depth rendering.

## Existing 2D-To-3D Standards

There are two existing standards to preserve:

1. World/game-map storage:
   - `maps/map_01_z0.txt`, `maps/map_01_z1.txt`, etc.
   - each file is a 2D tile slice.
   - `assets/tiles/registry.txt` maps symbols to tile definitions.
   - `assets/tiles/*.tile.txt` supplies color, solidity, walkability, and `extrude`.
   - this is the right standard for `piececraft-wraith`.

2. Piece/entity storage:
   - `pieces/<piece_id>/artifact.txt`
   - each artifact is a stacked z-bitmask shape.
   - this is already used in `piececraft-3d/pieces/adam/artifact.txt` and `fuzz-op-gl/pieces/legend_block/artifact.txt`.
   - this is the right standard for `wraith-3d-cube`.

Emoji Studio is related but should not be the first Wraith standard:
- it turns a 2D RGBA atlas/image into `voxels.csv`.
- it is useful later for image/icon extrusion inside a `${game_map}` surface.
- it is not the best first standard for cube or Piececraft map work because Piececraft already has TPMOS map/tile/piece conventions.

Therefore, for Wraith:
- `${game_map}` is the only surface that may invoke these 2D-to-3D conversions.
- normal windows, buttons, debug rows, and widget chrome must not use this path.
- `piececraft-wraith` should use map z-slices plus tile extrusion.
- `wraith-3d-cube` should use a piece artifact z-bitmask.
- `emoji-studio-wraith` should use RGBA CSV extrusion.

## Cube Storage As A Piece Artifact

The cube source should follow the existing `pieces/<id>/artifact.txt` z-slice convention: a 3D piece is stored as stacked 2D slices.

Suggested Wraith path:

```txt
1.TPMOS_c_+rmmp.0102.0027/projects/wraith/wraith-projects/wraith-3d-cube/pieces/cube_probe/artifact.txt
```

Solid 8x8x8 cube example:

```txt
id=cube_8
size=8,8,8
palette.1=#24C94A
z0=FF,FF,FF,FF,FF,FF,FF,FF
z1=FF,FF,FF,FF,FF,FF,FF,FF
z2=FF,FF,FF,FF,FF,FF,FF,FF
z3=FF,FF,FF,FF,FF,FF,FF,FF
z4=FF,FF,FF,FF,FF,FF,FF,FF
z5=FF,FF,FF,FF,FF,FF,FF,FF
z6=FF,FF,FF,FF,FF,FF,FF,FF
z7=FF,FF,FF,FF,FF,FF,FF,FF
```

Wire/hollow audit cube example:

```txt
id=cube_wire_8
size=8,8,8
palette.1=#24C94A
z0=FF,81,81,81,81,81,81,FF
z1=81,00,00,00,00,00,00,81
z2=81,00,00,00,00,00,00,81
z3=81,00,00,00,00,00,00,81
z4=81,00,00,00,00,00,00,81
z5=81,00,00,00,00,00,00,81
z6=81,00,00,00,00,00,00,81
z7=FF,81,81,81,81,81,81,FF
```

Interpretation:
- each `zN` line is one z-level.
- each comma-separated hex byte is one y row.
- each bit in the byte is one x voxel.
- occupied bits become world voxels `(x,y,z)`.
- palette/material data maps occupied voxels to RGB colors.

This keeps the source auditable as 2D slices while still being a real 3D source.

Important:
- ASCII must not be treated as the only source of 3D truth.
- ASCII is an audit projection: current z slice, top/front/side summary, object id, voxel count, rotation, camera state.
- the semantic frame/metadata must carry the 3D source path, voxel bounds, camera state, rotation, selected object, and target surface bounds.
- if only a plain ASCII frame is available, full 3D reconstruction is lossy and should be considered impossible.

Minimum semantic scene:

```txt
OBJECT | id=cube_surface | tag=surface | role=game_map | x=3 y=4 w=86 h=16
PIECE_MODEL | id=cube_probe_01 | role=zslice_piece | source=pieces/cube_probe/artifact.txt | center=0,0,0 | unit_size=1 | rot=15,35,0 | selected=true
CAMERA | mode=4 | x=0.00 | y=0.00 | z=0.00 | pitch=15.00 | yaw=0.00 | roll=0.00
```

For the current Wraith bridge, the project-owned file is:

```txt
projects/wraith/wraith-projects/wraith-3d-cube/session/scene.objects.pdl
```

The host manager imports this file generically. It must not know what `cube_probe` means.

Minimum RGB converter work:
- extend the current first-pass `zslice_piece` primitive path in `wraith_rgb_daemon.c`.
- parse the z-slice artifact into occupied voxels.
- optionally collapse the occupied voxels into exterior faces for efficient drawing.
- rotate voxels/faces around X/Y/Z.
- transform vertices through camera position/rotation.
- project vertices to 2D surface pixels with the Piececraft camera contract.
- sort faces/voxels by camera-space depth.
- fill faces with simple shaded colors.
- draw a contrasting outline.
- draw optional yellow wireframe if selected/focused.

Manager/op responsibilities:
- Wraith Alpha owns only the generic host bridge: project window hosting, `INTERACT`, `ESC`, and appending map-control keys to project `session/history.txt`.
- the `wraith-3d-cube` project owns `cube_rotation_x`, `cube_rotation_y`, `cube_rotation_z`, `camera_mode`, `cam_x/y/z`, `cam_pitch/yaw/roll`, and any cube-specific response text.
- project-owned ops/actions should reuse Piececraft commands where possible:
  - `CAMERA_MODE:n`
  - `CAMERA_SET:x,y,z,pitch,yaw,roll`
  - `CAMERA_MOVE:dx,dy,dz`
  - `ROTATE_CUBE:axis,delta`
- the project-owned manager/op reads `session/history.txt`, writes updated state into `session/state.txt` and `pieces/cube_probe/state.txt`, and emits the semantic `PIECE_MODEL` scene record.
- `wraith_rgb_daemon` converts the semantic scene record to RGB.
- `wraith_gl` only uploads/displays the resulting RGB texture.

Acceptance:
- ASCII can show a simple cube audit plus `rot=x,y,z`.
- GL shows a recognizable cube, not just text.
- receipts include piece artifact source path, slice count, occupied cell count, world bounds, camera state, rotation, projected 2D face bounds, face draw order, and checksum.

Suggested first implementation:
- no real animation loop yet.
- use `camera_mode`, `cam_x/y/z`, `cam_pitch/yaw/roll`, and cube rotation values from session state.
- project-owned input op already consumes `session/history.txt` and translates interact-mode keys into `CAMERA_MODE`, `CAMERA_SET`, `CAMERA_MOVE`, and `ROTATE_CUBE`; next step is live-test/harden.
- regenerate `session/wraith_body.txt` and `session/scene.objects.pdl` from project state after each command.
- verify static cube first.

## Piececraft Camera Contract

Correction:
- Piececraft is not merely an isometric tile renderer.
- The GL-OS view is a true 3D/POV-style host with camera controls and camera state.
- Wraith should reuse this control model for both `wraith-3d-cube` and `piececraft-wraith`.

Observed in `projects/piececraft-3d/layouts/main.gltpm`:

```txt
<camera mode="${camera_mode}" x="${cam_x}" y="${cam_y}" z="${cam_z}" pitch="${cam_pitch}" yaw="${cam_yaw}" roll="${cam_roll}" />
```

Observed in `projects/piececraft-3d/session/state.txt`:

```txt
camera_mode=4
cam_x=0.00
cam_y=0.00
cam_z=0.00
cam_pitch=15.00
cam_yaw=0.00
cam_roll=0.00
```

Observed in `piececraft-3d_manager.c`:
- `CAMERA_SET:x,y,z,pitch,yaw,roll` updates camera position and rotation.
- `CAMERA_MODE:n` switches camera mode / POV.
- `CAMERA_MOVE:dx,dy,dz` moves the camera.
- `INTERACT` enters map control.
- `ESC` exits map control.
- while `is_map_control=1`, arrow keys move the selected entity; WASD/ZX are reserved by the host for camera flight.

Observed in `new-handoff-m1+1.txt`:
- camera/input sovereignty was a core fix.
- GL-OS host handles flight.
- manager handles pieces.
- 1st person camera is raised to eye level and pitched down.
- 3rd person is positioned high and angled down.
- mouse-look / drag-to-rotate is a future goal.

Wraith requirement:
- `wraith-3d-cube` must use this same camera command vocabulary.
- `piececraft-wraith` must preserve this camera-state model.
- receipts must record camera mode/position/rotation per frame.

## Stage 2: Piececraft-Like 3D Map

Reference:
- `1.TPMOS_c_+rmmp.0102.0027/projects/piececraft-3d`
- screenshot shows GL-OS rendering a tile world as green 3D blocks through a camera/POV host.

Existing reference facts:
- `piececraft-3d/layouts/main.gltpm` already uses `${game_map}`.
- it already exposes `INTERACT`.
- it already uses `is_map_control`.
- it already uses camera fields: `camera_mode`, `cam_x`, `cam_y`, `cam_z`, `cam_pitch`, `cam_yaw`, `cam_roll`.
- it already uses command forms: `CAMERA_SET`, `CAMERA_MODE`, and `CAMERA_MOVE`.
- `piececraft-3d/maps/map_01_z0.txt` provides a tile grid.
- `piececraft-3d/assets/tiles/registry.txt` maps symbols to tile names:
  - `.` = grass
  - `#` = wall
  - `T` = tree
  - `R` = stone
  - `A` = adam
  - `E` = eve

Goal for `piececraft-wraith`:
- convert a tile/z map into 3D block primitives and render it to RGB using the Piececraft camera contract.
- keep map source and scene declarations in project files, not in the Wraith host manager.

Minimum semantic scene:

```txt
SURFACE | id=piececraft_map | role=game_map | x=3 y=4 w=86 h=16 | camera=camera_main
CAMERA  | id=camera_main | mode=4 | x=0.00 | y=0.00 | z=0.00 | pitch=15.00 | yaw=0.00 | roll=0.00
TILE    | id=tile_04_02_00 | x=4 y=2 z=0 | symbol=T | tile=tree | h=1 | selected=true
TILE    | id=tile_00_00_00 | x=0 y=0 z=0 | symbol=# | tile=wall | h=1
```

Current Wraith bridge file:

```txt
projects/wraith/wraith-projects/piececraft-wraith/session/scene.objects.pdl
```

Current implementation status:
- `piececraft-wraith` has a project-owned `Control_Map` UI control with `target_surface=game_map`.
- its `${game_map}` surface is payload-only with `nav=0`.
- it has local `maps/map_01_z0.txt`, `assets/tiles/registry.txt`, and starter tile definition files.
- it has a `tile_zmap` semantic model record.
- RGB now draws a pseudo-height tile preview so trees/rocks/walls are visually distinct from grass, but it is still not Piececraft-grade 3D camera rendering.

Minimum RGB converter work:
- parse a map slice into tile objects.
- map each tile to a block height and color.
- transform tile/cube vertices through the active camera.
- project visible faces using perspective/POV camera math.
- draw top/side faces according to camera visibility.
- depth-sort by camera-space depth, not `x + y + z`.
- draw selected tile as outline/wireframe overlay.

Initial camera projection model:

```txt
world -> subtract camera position
      -> rotate by inverse camera pitch/yaw/roll
      -> perspective divide using focal length / camera_z
      -> map into surface pixel bounds
```

Fallback:
- an orthographic/isometric-like projection may be kept only as `camera_mode=debug_ortho`.
- it should not be documented as the main Piececraft target.

Initial tile colors:
- grass: top `#1F9B24`, side `#14751B`
- wall: top `#6D7F9E`, side `#526580`
- tree: top `#16821D`, side `#0F5D15`
- stone: top `#6C83AA`, side `#50627F`

Acceptance:
- ASCII audit shows map slice and selected tile metadata.
- GL/RGB shows a camera-projected 3D block map.
- camera mode/POV switching changes the rendered RGB view.
- selected cell/focus is visible as a wireframe/outline.
- receipts report tile count, camera mode, camera position/rotation, projected bounds, selected tile, and draw order.

## Stage 3: CHTMGL Wraith Widgets

Reference:
- `1.TPMOS_c_+rmmp.0102.0027/projects/chtmgl-alpha/layouts/index.chtmgl`

Reference widgets:
- `window`
- `header`
- `panel`
- `text`
- `button`
- `checkbox`
- `slider`
- `img`
- `video`
- `canvas`
- `menu`
- `menuitem`

Wraith naming rule:
- keep implementation surface name as `${game_map}` for now.
- docs may call it canvas-like, but do not make `canvas` the primary runtime contract until Wraith intentionally changes standards.

Goal for `chtmgl-wraith`:
- parse/represent widget roles as semantic primitives.
- render them as RGB rectangles/widgets, not just text lines.
- keep widget declarations in project-owned layout/scene files, not in the Wraith host manager.

Minimum semantic scene:

```txt
OBJECT | tag=window   role=window        id=chtmgl_window x=2 y=2 w=90 h=22
OBJECT | tag=header   role=header        id=chtmgl_header x=2 y=2 w=90 h=2
OBJECT | tag=panel    role=panel         id=controls_panel x=4 y=5 w=22 h=16
OBJECT | tag=button   role=button        id=btn1 nav=5 label=Test Button
OBJECT | tag=checkbox role=checkbox      id=chk1 checked=true
OBJECT | tag=slider   role=slider        id=sld1 value=50 min=0 max=100
OBJECT | tag=surface  role=game_map      id=main_canvas view_mode=3d
OBJECT | tag=menu     role=menu          id=options_menu label=Options
```

Minimum RGB converter work:
- draw panel fills and borders.
- draw button rectangles with label and nav marker.
- draw checkbox square and checked mark.
- draw slider track and thumb.
- draw image/media placeholders as framed boxes with labels.
- draw menu as compact button/dropdown primitive.
- draw `${game_map}`/canvas-like area as a surface placeholder, later using cube/map rendering.

Current implementation status:
- `chtmgl-wraith` has static project-owned semantic scene records for CHTMGL-like `panel`, `button`, `checkbox`, `slider`, `menu`, `window_toolbar_item`, `${game_map}`, and `widget_surface_probe`.
- this is not yet a parser for `chtmgl-alpha/layouts/index.chtmgl`.
- RGB now draws first-pass graphical button/checkbox/slider/menu shapes and a `widget_surface_probe`.
- the next implementation need is a project-owned parser/manager that emits these records from a layout file.

## Stage 4: Emoji Studio Wraith RGBA Extrusion

Reference:
- `#.emoji-studio-501.02.05t/&.emoji-studio-solo.02.01`

Goal for `emoji-studio-wraith`:
- validate a third `${game_map}` source type: 2D RGBA CSV -> extruded 3D colored cells.
- keep CSV source and scene declarations in project-owned files, not in the Wraith host manager.

Existing reference facts:
- Emoji Studio extracts pixels from an emoji/image atlas.
- output is stored as `pieces/<emoji_name>/voxels_<resolution>.csv`.
- rows are RGBA pixels.
- alpha determines visible/occupied pixels.
- RGB determines material/color.
- the reference renderer extrudes visible pixels into 3D columns/cubes.
- current Wraith sample source is `projects/wraith/wraith-projects/emoji-studio-wraith/pieces/sample_emoji/voxels_8.csv`.

Minimum semantic scene:

```txt
SURFACE | id=emoji_surface | role=game_map | x=3 y=4 w=86 h=16 | camera=camera_main
IMAGE_EXTRUSION | id=emoji_sample | role=rgba_extrusion | source=pieces/sample_emoji/voxels_8.csv | resolution=8 | selected=true
CAMERA | id=camera_main | mode=4 | x=0.00 | y=0.00 | z=0.00 | pitch=15.00 | yaw=0.00 | roll=0.00
```

Current Wraith bridge file:

```txt
projects/wraith/wraith-projects/emoji-studio-wraith/session/scene.objects.pdl
```

Current implementation status:
- `emoji-studio-wraith` has `Control_Extrusion` and `Res_8` controls targeting a payload-only `${game_map}`.
- RGB currently draws a flat extruded preview using visible alpha pixels and CSV RGB material color.
- depth-sorted camera projection is still pending.

Minimum RGB converter work:
- parse the CSV header and RGBA rows.
- infer resolution from metadata or row count.
- create one extruded cell/column for each alpha-visible pixel.
- use RGB as the material color.
- use alpha as occupancy, and later as optional height/opacity policy.
- project cells through the Piececraft camera contract.
- depth-sort by camera-space depth.
- draw projected faces into `current_frame.rgba32`.

Acceptance:
- ASCII audit shows CSV path, resolution, visible pixel count, and camera state.
- GL/RGB shows a recognizable extruded image sample inside `${game_map}`.
- receipts report CSV path, resolution, visible pixel count, camera, projected bounds, draw order, and checksum.

CHTMGL acceptance:
- `chtmgl-wraith` visually resembles a widget page.
- controls are still indexed/auditable.
- receipts expose widget roles, bounds, values, focus/hit rectangles, and actions.

## Receipts Needed For 3D And Widgets

Current receipts are good for 2D object/text bounds.

Current primitive receipt support:
- `SECTION | PRIMITIVES | SOURCE_AND_CONVERSION_AUDIT` exists in `current_frame.receipt.pdl`.
- `zslice_piece` reports source existence, mtime, byte size, declared size, parsed slices/rows, occupied bits, surface bounds, projection mode, camera/rotation consumed flags, `rot`, `camera`, and `final_projection`.
- `tile_zmap` reports source existence, mtime, byte size, row/column counts, tile counts by class, surface bounds, and `final_projection=0`.
- `rgba_extrusion` reports source existence, mtime, byte size, resolution, total pixels, visible pixels, surface bounds, and `final_projection=0`.
- for `zslice_piece`, `final_projection=1` currently means projected wireframe, not filled/depth-sorted faces.

Add for 3D:
- `primitive_type=mesh_cube|voxel_tile|widget_button|widget_slider`
- `world_x/world_y/world_z`
- `world_w/world_h/world_d`
- `camera_id`
- `camera_mode`
- `cam_x/cam_y/cam_z`
- `cam_pitch/cam_yaw/cam_roll`
- `projected_px_x0/y0/x1/y1`
- `face_count`
- `face_draw_order`
- `depth_min/depth_max`
- `selected=true/false`
- `wireframe_px_bounds`

Add for tile maps:
- `tile_count`
- `map_id`
- `active_z`
- `selected_cell`
- `tile_registry_source`
- one receipt line per visible tile or per clipped tile batch

Add for widgets:
- `widget_state`
- `value`
- `checked`
- `min/max`
- `hit_rect`
- `focus_rect`
- `label_rect`

## Best Next Implementation Order

1. Add a software cube rasterizer to `wraith_rgb_daemon.c`.

This gives immediate visible payoff and proves RGB can look 3D without changing `wraith_gl.c`.

2. Add map tile block rasterizer for `piececraft-wraith`.

This has the most momentum because Piececraft already owns the right model vocabulary and camera-control command seam.

3. Add CHTMGL widget primitives.

This is the broadest task. It should be done after cube/map rendering proves the primitive pipeline.

## What Not To Do

- Do not make `wraith_gl.c` render real OpenGL meshes yet.
- Do not bypass receipts.
- Do not rename `${game_map}` to `canvas` yet.
- Do not make `piececraft-wraith` a top-level TPMOS project.
- Do not call `piececraft-wraith` by the same name as `piececraft-3d` unless it becomes the same project/codepath.
- Do not call `chtmgl-wraith` by the same name as `chtmgl-alpha`.

## Summary

To get the visual result shown in the Piececraft screenshot, Wraith needs a software shape/mesh rasterization layer inside the RGB converter.

The shortest path is:
- cube mesh primitive first
- camera-projected tile/block primitive second
- CHTMGL widget primitives third

That keeps Wraith auditable while making GL look like real graphical UI instead of text painted into a window.
