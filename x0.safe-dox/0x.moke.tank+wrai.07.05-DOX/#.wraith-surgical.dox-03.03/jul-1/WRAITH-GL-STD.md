# WRAITH GL Standard

Date: 2026-06-26
Status: Draft working standard
Scope: Wraith GL arrangement, auditability, semantic export, and renderer verification

## 1. Purpose

This standard exists so Wraith GL can be audited against intention instead of judged only by eye.

The problem we are solving:

- ASCII currently has stronger arrangement parity than GL
- GL may be semantically correct but still feel visually wrong
- agents need a stable way to compare:
  - intended layout
  - exported semantic scene
  - rendered result

This document defines the minimum contracts needed to make that comparison practical.

Reference standards for judging the GL feel:

- `chtmgl-alpha` for richer markup/window arrangement
- `gl-os` for desktop-like graphical presentation

These are reference targets for behavior and layout quality, not required code reuse targets.

## 2. Core Principle

Wraith GL should be audited in three layers:

1. Intent layer
2. Semantic scene layer
3. Render receipt layer

If those three layers are present, a human or agent can answer:

- what should be on screen
- what the exporter thinks is on screen
- what the renderer actually drew

## 3. Required Audit Files

Wraith GL should converge on these files as the standard audit bundle:

1. `intent`
   - `projects/wraith-alpha/session/gl_intent.pdl`

2. `semantic scene`
   - `pieces/display/current_frame.meta.pdl`
   - `pieces/display/current_frame.objects.pdl`
   - `pieces/display/current_frame.desktop_state.pdl`

3. `render receipt`
   - `projects/wraith-alpha/session/rgb/current_frame.receipt.pdl`

4. `optional geometry grid`
   - `projects/wraith-alpha/session/rgb/current_frame.layout_grid.pdl`

These do not replace each other.
They answer different questions.

## 4. Intent Layer

The intent layer is the declared arrangement plan.

It should answer:

- which rows exist
- what order they appear in
- what each row contains
- expected bounds
- expected spacing
- expected padding
- expected alignment

This is the file that lets an agent compare implementation against declared UX intent.

### Recommended format

`gl_intent.pdl` should include rows like:

```txt
ROW | id=taskbar_row | kind=taskbar_row | x=0 y=28 w=96 h=1 | padding_left=0 | gap=1 | align=left
ITEM | row=taskbar_row | id=taskbar_prefix | slot=1 | text=| [ TASKBAR ]
ITEM | row=taskbar_row | id=taskbar_terminal | slot=2 | nav=1 | selector=[ ] | text=[Terminal]

ROW | id=debug_row | kind=debug_row | x=0 y=31 w=44 h=1 | padding_left=0 | gap=1 | align=left
ITEM | row=debug_row | id=view_prefix | slot=1 | text=View:
ITEM | row=debug_row | id=view_ascii | slot=2 | nav=2 | selector=[ ] | text=[ASCII]
ITEM | row=debug_row | id=view_gl | slot=3 | nav=3 | selector=[>] | text=[GL*]
```

This is not just for debugging.
It is the arrangement contract.

## 5. Semantic Scene Layer

The semantic scene layer is what Wraith believes exists right now.

This is already partly present.

The missing rule is that the scene must not only export isolated objects.
It must also export row/container structure.

### Required object fields

Each object should carry:

- `id`
- `tag`
- `role`
- `x y w h`
- `z`
- `focused`
- `nav`
- `nav_selected`
- `nav_selector_glyph`
- `label_core`
- `action`

### Required row/container fields

Each row/container should carry:

- `row_id`
- `row_kind`
- `row_index`
- `row_x row_y row_w row_h`
- `padding_left`
- `padding_right`
- `gap`
- `align`
- `baseline`

### Required relationship fields

Each child item should carry:

- `parent_row_id`
- `slot_index`
- `slot_kind`
- `ancestor_chain`
- `clip_chain`

Chain fields should agree with the exported parent/container ids. If an object says it belongs to `taskbar_row`, its chains should not skip that row or name a different container.

Without these, agents are forced to infer arrangement from loose coordinates.

## 5A. Project-Local Interact Controls

A `${game_map}` is a canvas-like payload inside a Wraith window. It should not be treated as the visible toolbar for its own mode controls.

Standard:

- A project window that contains `${game_map}` must expose map/interact through a project-local UI control with `action=INTERACT` and `target_surface=game_map`.
- That project-local UI control may live in a headerbar, toolbar, side panel, footer, menu, overlay, or other declared project layout region.
- The visible selector for map control is `[>]` before entry and `[^]` while `is_map_control=1`.
- The `${game_map}` surface remains the rendering payload for tiles, cube projections, z-slices, RGBA extrusions, and other canvas-like content.
- GL presenters should draw project-local controls above projected map/model content, so primitives cannot hide the active selector.
- Normal Wraith window chrome remains host-owned and separate from project-local controls.
- Receipts should make this relationship explicit through `ancestor_chain`, `container_id`, `action=INTERACT`, and the target/payload surface id.

Recommended scene split:

```txt
OBJECT | tag=control role=window_toolbar_item nav=5 action=INTERACT label=Control_Map target_surface=game_map
OBJECT | tag=surface role=game_map id=game_map nav=0 action=NONE
OBJECT | tag=model role=zslice_piece surface=game_map source=pieces/cube_probe/artifact.txt
```

Temporary compatibility:

- Existing projects may still attach `nav=<n> action=INTERACT` directly to `role=game_map`.
- New work should migrate that selector into project-local control records and keep the map surface as payload only.

## 5B. Window Move/Resize Standard

Wraith window movement and resizing are host window-management behavior, not project behavior.

Standard:

- Window geometry is session truth and must be file-owned/auditable.
- Projects may declare their own inner panels, widgets, and map controls, but they do not own host window drag or resize logic.
- A visible titlebar or drag region should export `role=window_titlebar action=WINDOW_DRAG window_id=<id>`.
- Resize handles should export `role=window_resize_handle action=WINDOW_RESIZE edge=<n|s|e|w|ne|nw|se|sw> window_id=<id>`.
- The GL presenter should emit semantic input events for drag/resize. It must not silently mutate project layout or window geometry by itself.
- The Wraith manager/session host consumes those events, clamps geometry, writes the updated window registry, and exports the next frame.
- Receipts must include enough data to audit old rect, requested delta, clamp result, final rect, and the source hit object.

Recommended geometry records:

```txt
WINDOW_GEOMETRY | window_id=3 x=2 y=3 w=94 h=28 min_w=24 min_h=8 mode=free
OBJECT | tag=control role=window_titlebar nav=1 action=WINDOW_DRAG window_id=3
OBJECT | tag=control role=window_resize_handle nav=0 action=WINDOW_RESIZE edge=se window_id=3
```

Required receipt facts:

- `window_id`
- `old_x old_y old_w old_h`
- `raw_mouse_px`
- `mapped_cell_x mapped_cell_y`
- `drag_dx drag_dy` or `resize_dw resize_dh`
- `edge`
- `clamp_reason`
- `final_x final_y final_w final_h`
- `hit_object_id`
- `hit_action`

## 6. Render Receipt Layer

The render receipt should describe what the RGB stage actually used.

Current receipt is useful but incomplete.

It should eventually include:

- presenter mode
- accepted source project id
- accepted source layout
- object count
- row count
- raw mouse position and derived visual cursor position
- generation time in epoch seconds and ISO-8601 UTC
- source file mtime markers for semantic meta and object bundles
- previous receipt freshness marker or equivalent sequence token
- for each row:
  - actual bounds used
  - actual child order
  - actual spacing used
- for each drawn item:
  - final text
  - final x/y
  - final width clipping result
- for each source-backed primitive:
  - source path
  - source exists/missing
  - source mtime and byte size
  - parsed source counts
  - preview/final projection mode
  - whether camera and rotation metadata were consumed

Current primitive audit section:

```txt
SECTION | PRIMITIVES | SOURCE_AND_CONVERSION_AUDIT
PRIMITIVE | ... role=zslice_piece ... parsed_slices=... occupied_bits=... projection_mode=stacked_face_preview final_projection=0
PRIMITIVE | ... role=tile_zmap ... rows=... cols=... tile_count=... projection_mode=flat_tile_preview final_projection=0
PRIMITIVE | ... role=rgba_extrusion ... resolution=... visible_pixels=... projection_mode=flat_extrusion_preview final_projection=0
```

This makes it possible to answer:

- was the export wrong
- or did the renderer arrange the export incorrectly

## 7. Geometry Grid Standard

Agents need a stable unit system.

For Wraith GL, the recommended audit unit is:

- primary geometry unit: text-cell grid
- secondary geometry unit: pixel grid

Rules:

- all semantic layout should be auditable first in cell coordinates
- pixel coordinates are a derived output
- row/container alignment should be specified in cells
- renderer may convert cells to pixels, but cells remain the audit truth

### Standard cell assumptions

Unless overridden by the frame metadata:

- `cols = 96`
- `rows = 30`
- `cell_width_px = 10`
- `cell_height_px = 18`

## 8. Row Primitive Standard

Wraith GL should use explicit row primitives.

Minimum recommended row kinds:

- `taskbar_row`
- `debug_row`
- `summary_row`
- `launcher_row`
- `window_chrome_row`

Each row kind should define:

- default left padding
- default right padding
- inter-item gap
- preferred truncation behavior
- prefix behavior
- selector slot width
- nav number width

Example:

```txt
ROW_KIND | debug_row | selector_slot_w=3 | nav_num_w=3 | label_wrap=bracketed | gap=1
ROW_KIND | taskbar_row | prefix_w=14 | item_gap=1 | truncation=clip_right
ROW_KIND | summary_row | field_gap=3 | truncation=field_preserve
```

## 8.1 Footer Band Standard

The desktop shell and the GL footer are not the same region.

Rules:

- the desktop body owns the upper shell area
- the footer band is a separate reserved region below the desktop body
- `taskbar_row`, `summary_row`, and `debug_row` belong to the footer band
- the renderer may reproject footer-band rows to the bottom of the GL viewport when semantic rows are authored in shell-relative coordinates
- the renderer should apply that footer-band projection to the entire footer subtree, not just the row panel itself
- the render receipt must include both semantic `y` and renderer `render_y`
- the footer band should keep its own background fill so the desktop box does not visually absorb it

## 9. Functional UX Parity Definition

Wraith GL has functional UX parity when:

1. source parity
   - GL and ASCII describe the same windows, focus, and controls

2. nav parity
   - the same indices and selector states are visible in both

3. action parity
   - keyboard and mouse activate the same control seams

4. arrangement parity
   - rows read in the same order with similar spacing and grouping

5. audit parity
   - an agent can explain any mismatch using the audit files without guessing

## 10. Required Comparison Workflow For Agents

When an agent evaluates a GL mismatch, it should compare in this order:

1. `intent`
   - what row/item arrangement was intended

2. `semantic scene`
   - what rows/items were actually exported

3. `render receipt`
   - what rows/items were actually drawn

4. final screenshot / frame
   - how that drawing looked

This avoids the bad pattern of only staring at the frame and guessing.

## 11. Recommended Failure Labels

Agents should classify GL issues using these labels:

- `truth_mismatch`
  - exported scene does not match Wraith session truth

- `row_structure_missing`
  - exporter emits loose objects but not row containers

- `spacing_mismatch`
  - row exists but gaps/padding/alignment are off

- `selector_format_mismatch`
  - `[ ]` / `[>]` / nav-number formatting differs from intent

- `truncation_mismatch`
  - widths or clipping differ from intent

- `hit_target_mismatch`
  - visible control and actual action seam do not align

- `render_receipt_gap`
  - not enough downstream evidence to explain the frame

## 12. Immediate Adoption Plan

The shortest path to make this useful:

1. create `gl_intent.pdl`
   - only for `taskbar_row`, `debug_row`, and `summary_row` first

2. add row/container fields to `current_frame.objects.pdl`

3. extend `current_frame.receipt.pdl`
   - include final row/item placement

4. only then tighten renderer spacing

This keeps the audit system ahead of the visual polish.

## 13. Rule For Future Work

Do not add more visual complexity to Wraith GL unless the new element can be audited through:

- intent
- semantic scene
- render receipt

If a feature cannot be explained through those three layers, it is too implicit for Wraith.
