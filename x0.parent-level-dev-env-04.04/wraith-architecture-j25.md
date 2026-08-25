# Wraith Architecture J25

## Purpose

This document captures the intended Wraith model for TPMOS development in J25:

- Wraith is a virtual OS, not a single game.
- Session state is sovereign and file-backed.
- Save/load applies to the entire Wraith environment.
- `terminal`, `blank-project`, and the desktop shell are all real contexts.
- Reusable Ops are preferred wherever a lifecycle action already exists or can be shared.

This is a reference document, not an implementation commit log.

## Design Precedent

`op-ed` is the closest existing precedent:

- It treats each game as a sovereign folder-context.
- Save/load copies whole folders, not isolated UI widgets.
- The browser and the working project are distinct views over a real project tree.

That is the right mental model for Wraith too, except the folder-context is the whole session environment.

## Core Model

Wraith is split into three layers:

### 1. Piece

Pieces own their own files:

- `piece.pdl`
- `state.txt`
- any project-owned history or memory files

### 2. Module

Modules orchestrate pieces:

- launch work
- route input
- publish state projections
- invoke Ops for real work

Modules should stay thin. They should not become one-off business logic dumps.

### 3. OS / Theater

The OS layer renders the current truth:

- current layout
- active focus
- current frame
- session-visible debug/projection data

The theater should compose and route, not invent hidden state.

## Wraith Session Scope

The Wraith session is the unit that should be saved and loaded.

That means the restore boundary includes:

- desktop shell state
- terminal windows
- focused project
- active layout
- registry and window stack
- session history
- any project-owned session files needed to recreate the same environment

This is larger than a terminal buffer and larger than a single project.

## Real Contexts

### Terminal

The terminal is a real context.

- It should have persistent state.
- It should be launchable more than once.
- It should be restorable as part of the session.
- It should participate in the session model, not sit outside it.
- It should resolve to a real project directory under `projects/wraith-alpha/wraith-projects/terminal`.
- This applies to every terminal entry point, including the default bootstrap/taskbar terminal and any later terminal windows opened from inside Wraith.
- The toolbar terminal is not a fake shell shortcut; it is a real project-backed launcher entry that should resolve through the same project contract as other Wraith contexts.

### Blank Project

`blank-project` is also a real context.

- "Blank" means empty or minimal, not fake.
- It should have its own project directory under `projects/wraith-alpha/wraith-projects/blank-project`, modeled like a template-style project.
- The launcher alias can still be `blank-proj`, but it should resolve to that project directory and open a `Blank Project` window in the current Wraith toolbar/taskbar.
- It must not open eagerly or automatically during bootstrap. It should open only when the user explicitly selects or activates the `blank-proj` launcher row.
- Every project opened inside Wraith uses the same standard desktop window chrome and controls: half-size, minimize, close, and taskbar participation.
- The project ID must preserve the full nested path, not collapse to the first path segment.
- The project should live inside the same Wraith desktop session, not as a separate OS/session.
- The GL presenter is an IO mirror for the single Wraith desktop session. `blank-project` must not spawn a second GL window.
- Launcher dispatch should treat the launcher row order consistently with the manifest. The current fix path is to use a one-based walk over the launcher PDL so `blank-proj` can resolve to `DESKTOP_ACTION:launch_blank_project` instead of falling off the end of the list.

### Desktop Shell

The desktop shell is the top-level Wraith frame.

- It is part of the session.
- It owns shell-level context and window projection.
- It should be restorable exactly like the other contexts.
- Its bottom toolbar should list every non-closed window in the current session, not only minimized windows.
- Shell launch controls should be authored as real CHTPM buttons in the layout, not injected as markup-like text through manager state.
- Manager output should carry session truth and projection values, not fake UI fragments that require a second parse pass.
- Launcher Enter dispatch should resolve the selected launcher method from `projects/wraith-alpha/wraith-projects/launcher/piece.pdl`, so `blank-proj` and `terminal` share the same reusable launch contract.
- The parser-to-manager handoff for launcher focus should use `pieces/display/active_gui_index.txt` as the live selection seam, so Enter dispatch reads the same row the layout just exported.
- Launcher selection is not the same thing as window-chrome count; the manager must preserve launcher-range indices instead of clamping them to `1` when the desktop shell is focused.
- The shell should render its buttons from runtime state: launcher rows come from the launcher PDL and taskbar rows come from the live non-closed window registry.
- Static menu slot math is a bug, not a feature. The desktop shell should derive interactive indices from the current manifest and registry size every frame.
- When the shell needs to inject a dynamic button region, that region must be expanded before tokenization so the parser sees real tags instead of literal markup text.
- The desktop shell must still emit the full chrome frame even when no active window is focused; a missing active window should degrade to an empty desktop body, not a one-line taskbar.
- When no desktop window is focused, the shell should degrade to the minimal taskbar-only bootstrap view shown in the reference frames. The full chrome appears only after a real window is active.

## Save / Load Contract

Save/load is for the entire Wraith environment.

The contract should be:

- Save writes the whole session to a folder or session slot.
- Load restores the whole session from that folder or slot.
- Restore should be deterministic and round-trip safe.
- The restored session should match the saved session as closely as practical.
- Window actions such as `terminal` and `blank-proj` remain inside the same session and should update the shared window registry rather than creating a new OS instance.
- `blank-proj` should resolve to a real project directory, not a hardcoded placeholder window.
- The bottom toolbar is a live session list. Open windows should appear there as well, so `terminal` and `blank-project` remain visible while they are open.
- Opening `blank-project` must stay inside the existing Wraith desktop shell and window registry. It is not a shell-to-project layout swap and it is not a separate GL session.
- The launcher row should reuse existing parser button semantics and `DESKTOP_ACTION:` routing rather than inventing a custom recursive-markup parser path.
- The shell should build its own index order from the current launcher PDL plus the live taskbar registry so the visible row numbers are never hardcoded.

This is the same folder-level idea as `op-ed`, applied to an OS session instead of a game world.

## Login / Auth Extension

The save/load boundary can later become login/auth.

The path is straightforward:

- Save slot selection becomes session selection.
- Restore can require a password before rehydration.
- A saved session can become a user identity + session bundle.

That should be an extension of the same save/load model, not a separate system invented from scratch.

## Reusable Ops

Reusable Ops are the default pattern.

Rules:

- Managers orchestrate.
- Ops do the reusable work.
- If a lifecycle action already exists as an Op, reuse it.
- If a new action belongs to more than one flow, make it an Op before embedding logic in the manager.

Good candidates for reuse:

- save session
- load session
- restore session
- project launch
- terminal launch
- auth-gated restore

This keeps the Wraith architecture modular and prevents manager bloat.

## File-Owned Project Truth

Wraith Alpha is the host OS/session manager. It may discover projects, open Wraith desktop windows, maintain the live window registry, route input, and compose the shared desktop frame.

Wraith Alpha must not own project-specific logic for validation projects.

Rules:

- Do not hardcode cube, Piececraft, CHTMGL, Emoji Studio, or other project-specific bodies in `wraith-alpha_manager.c`.
- Do not hardcode project-specific scene records, source paths, ops, or 2D-to-3D conversion declarations in `wraith-alpha_manager.c`.
- Project-specific truth belongs in project-owned auditable files.
- If it is not in a file or receipt, it is not project truth.
- If it is hardcoded in the host manager, it is not reusable enough for Wraith.

Current Wraith project bridge:

- `projects/wraith-alpha/wraith-projects/<project>/project.pdl`
- `projects/wraith-alpha/wraith-projects/<project>/layouts/*.chtpm`
- `projects/wraith-alpha/wraith-projects/<project>/session/state.txt`
- `projects/wraith-alpha/wraith-projects/<project>/session/wraith_body.txt`
- `projects/wraith-alpha/wraith-projects/<project>/session/scene.objects.pdl`
- `projects/wraith-alpha/wraith-projects/<project>/pieces/**`
- `projects/wraith-alpha/wraith-projects/<project>/maps/**`
- `projects/wraith-alpha/wraith-projects/<project>/assets/**`

`session/wraith_body.txt` is the plain audit/body text that Wraith Alpha may place inside the project window.

`session/scene.objects.pdl` is the project-owned semantic scene bridge that Wraith Alpha may import into the current frame object stream. Wraith Alpha should treat those records as data, not know what a cube, tile map, widget surface, or emoji extrusion means internally.

Project-owned managers/ops are the right place to update these files frame-by-frame. Wraith should prefer fork/file/receipt seams over hidden in-memory coupling.

For dynamic project surfaces, project-owned ops should also own the redraw seam. Current Wraith manager behavior already watches `session/fs_watch.marker` for the active project; dynamic projects should append there when they need the host shell/GL frame rerendered.

For live media surfaces such as webcam and video, file-backed redraw is only one proof strategy, not the required runtime model. A stricter lane may run fully memory-backed at runtime and only dump or mirror to files for audit, fallback, or debug. The important contract is that project-owned data remains inspectable and keyed by stable file-like paths. It is not necessary for disk files to carry the hot runtime path.

Current webcam proof note:

- the webcam lane now uses a TPMOS shared decoded-frame cache for hot frame truth
- both the webcam ASCII preview and the webcam GL image preview now consume that live frame cache first
- sampled `session/current_frame.png` remains as audit/fallback snapshot rather than mandatory per-frame hot truth
- this is materially closer to the intended media transport model, though manager wake and redraw policy can still be improved further

Preferred future optimization path:

- keep logical file-path identities such as `session/current_frame.png`
- let those become:
  - dump names
  - audit keys
  - fallback snapshot names
- while the true hot path uses:
  - decoded-frame cache
  - shared-memory or later ring-buffer frame handoff
  - shared surface consumption by both ASCII projection and GL upload

Future preferred shape:

- keep file-like path contracts as the sovereignty and debug seam
- allow runtime to resolve those paths either to memory-backed keys or to real files
- allow optional file dump or file mirror modes for audit/debug
- prefer dump/mirror execution in forked background workers so audit output does not stall runtime
- keep project-owned runtime ops as the control seam
- move hot frame transport toward decoded-frame caching, then shared memory or ring-buffer handoff
- let GL upload and ASCII projection consume the same in-memory decoded frame truth

This should be treated as a backend choice, not as a reason to force file-backed runtime everywhere.

If a lane declares zero runtime file dependence, then frame identities such as `session/current_frame.png` should remain only as logical path keys or snapshot export names, not as the live transport itself.

If a project consumes `session/history.txt` for button or command actions, it should persist a command-cursor seam in project state so redraw-triggered reruns do not replay stale commands.

## Project Interact Contract

`${game_map}` is the current TPMOS/Wraith name for canvas-like interactive surfaces. This naming follows `fuzz-op` / `fuzz-op-gl` for now; do not rename it to `canvas` until the platform standard changes.

Direct map control is a project-owned mode exposed through a generic host bridge:

- project scene files declare a `${game_map}` payload and a selectable project-local control with `nav=<n>`, `action=INTERACT`, and `target_surface=game_map`
- the project-local control may be in a headerbar, toolbar, side panel, footer, menu, overlay, or other declared project layout region
- Wraith shell nav shows `[>]` on that project-local control when the map is selected but not active
- pressing Enter on that nav sets the active project's `session/state.txt` key `is_map_control=1`
- the active project-local control then shows `[^]`, meaning keyboard ownership is inside the `${game_map}` surface
- pressing `ESC` sets `is_map_control=0` and returns keyboard ownership to Wraith shell nav
- while `is_map_control=1`, keypresses are written to the active project's `session/history.txt`
- project-owned managers/ops consume that history and update project state/body/scene files

The `${game_map}` surface is the payload, not the preferred place to draw the interact selector. From this point forward, Wraith windows that contain a `${game_map}` should expose map controls as project-local UI controls that target the payload surface. Normal Wraith window chrome remains host-owned and separate. This allows CHTMGL-style sidebars, footer controls, overlays, menus, and local toolbars without forcing every project into a titlebar-only pattern. It also prevents projected GL primitives from obscuring the nav selector.

This is host routing, not project business logic. Wraith Alpha may implement the generic `INTERACT`/`ESC` bridge, but it must not hardcode project-specific camera controls, cube rotation, Piececraft map ops, CHTMGL widget behavior, or Emoji Studio extrusion behavior.

The `[>]` to `[^]` transition is required for `wraith-3d-cube`, `piececraft-wraith`, and any later `${game_map}` project before that project claims keyboard hotkey support.

## Window Move/Resize Contract

Move and resize are Wraith host window-management features. They must not be implemented separately inside `wraith-3d-cube`, `piececraft-wraith`, `chtmgl-wraith`, `emoji-studio-wraith`, or `wraith-ed`.

Required model:

- each open window has file-owned geometry in the shared Wraith session/window registry
- the manager/session host owns `x`, `y`, `w`, `h`, `min_w`, `min_h`, focus, z-order, and window mode
- the exported scene includes semantic host controls such as `role=window_titlebar action=WINDOW_DRAG window_id=<id>`
- resize affordances export `role=window_resize_handle action=WINDOW_RESIZE edge=<edge> window_id=<id>`
- GL and ASCII presenters expose the same action seams and selectors where practical
- GL input emits semantic drag/resize events; it does not mutate geometry directly
- the manager consumes those events, clamps to screen bounds/minimum size, writes the registry, then exports the next frame

Receipts for drag/resize must show:

- source `window_id`
- hit object and `action`
- old geometry
- raw mouse and mapped cell coordinates
- drag delta or resize edge/delta
- clamp reason if any
- final geometry

This keeps subwindow movement/resizing reusable across CHTMGL panels, Piececraft maps, Emoji Studio surfaces, and future Wraith projects.

## State Ownership

The source of truth should be explicit.

Suggested ownership split:

- Manager-owned: registry, focus, current session selection, launch/restore bookkeeping
- Parser-owned: frame composition, layout projection, visible session output
- Renderer-owned: final display only
- Project-owned: project-specific state, history, body/audit projection, scene records, maps, pieces, assets, and ops

If a file is the truth, only one subsystem should own it.

## Input and History

Input provenance should be durable.

Current expectation:

- keyboard and mouse events should be written into the appropriate session history files
- session history should survive enough to support replay, audit, and debugging
- the presenter/renderer path should not silently swallow input if that input is supposed to matter later

If the GL presenter is display-only, then the manager or session host must be responsible for event logging.

## Debug Output

Debug output must not dominate the actual frame contract.

Rules:

- debug text can exist, but it should be intentional
- frame truth should not be buried under redundant diagnostics
- if the shell becomes noisy, the layout contract should be tightened rather than adding more logging

## Auditability Model

Wraith should be audited at two levels:

### 1. Frame-Level Audit

This is the simplified human/agent view of the current frame.

- It should be readable in a plain ASCII terminal.
- It should show semantic UI truth, not raw renderer internals.
- It does not need to mirror every pixel or every GL primitive 1:1.
- It must stay close enough to the GL scene that a mismatch is obvious and actionable.

Examples of useful audit truth:

- window title and focus
- window bounds and mode
- visible buttons and labels
- visible control indices / nav ids
- expanded/collapsed sections
- taskbar order
- active layout id
- selected control id / index

### 2. Scene-Level Audit

This is the canonical seam between logic and presentation.

- The runtime scene should be representable as stable nodes with ids, bounds, z-order, label, style intent, and interaction state.
- Both the ASCII audit presenter and the GL presenter should consume this same scene truth.
- If the two presenters disagree, the scene contract is wrong or one presenter is stale.

This is a better standard than trying to make ASCII visually imitate GL pixel-for-pixel.

## Dual Presenter Contract

Wraith should explicitly support two presentation modes over one runtime scene:

### Audit ASCII View

Purpose:

- debugging
- replay review
- low-cost inspection
- agent/human verification

Rules:

- default to semantic boxes, rows, labels, focus, and hierarchy
- avoid flooding the main frame with raw debug text
- treat ASCII as an audit projection, not as the product UI skin

### Graphical Blocks View

Purpose:

- show the same UI in a more GL-native way
- render windows, buttons, panels, and layout regions as solid forms
- act as the bridge between pure terminal audit and richer `chtmgl` presentation

Rules:

- start with rectangles, titlebars, fills, outlines, and focus states
- use existing color/asset metadata where present
- do not fork the state model or invent a separate session representation
- retain indexed/nav affordances in the GL view so a human or agent can still audit and target controls deterministically
- graphical rendering is allowed to look better than ASCII, but not to hide which control is index `N` or which target currently has nav focus

The important rule is one scene, two presenters.

## RGB Conversion Contract

Wraith should also be explicit about the middle fork between audit text and GL presentation.

There are two very different implementations:

### 1. Glyph Raster Mirror

This is the current transitional pattern.

- read `current_frame.txt`
- rasterize ASCII glyphs into an RGBA buffer
- let GL present that raw RGB surface

This is useful as a bootstrap mirror, but it is not the final Wraith vision.

### 2. Semantic RGB Shape Converter

This is the intended Wraith direction.

- read frame object/meta truth, not just flat glyph rows
- convert semantic objects into RGB primitives
- emit rectangles, titlebars, panels, buttons, borders, focus fills, and map-region surfaces into the RGB buffer
- let GL remain a thin presenter of that RGB output
- for project-specific `${game_map}` sources, consume project-owned scene records and project-owned files rather than host-manager hardcoded declarations

The pure vision is not "draw terminal glyphs inside GL forever."

The pure vision is:

- ASCII remains the audit/debug theater
- frame objects and metadata remain the scene truth
- a converter turns that truth into presentable RGB shapes
- GL presents the RGB result without owning application logic

## Dead Branch Cleanup

`wraith-pm` should be treated as an erroneous prior branch of `wraith-alpha`, not as a valid live Wraith source.

Rules:

- Wraith semantic GL mode must not accept `wraith-pm` scene receipts as current truth.
- If semantic artifacts still identify as `wraith-pm`, the converter should reject them and fall back to the ASCII-derived path.
- Any remaining `wraith-pm` frame/meta/audit files are cleanup debt, not architecture precedent.
- The correct long-term path is for `wraith-alpha` to emit its own semantic receipt and object stream from current session truth.

## Current Gap

At the moment, Wraith appears to have the bootstrap RGB mirror path, but not the full semantic converter path.

That means the architecture should treat the current glyph rasterizer as a temporary bridge.

Required next step:

- promote `current_frame.objects.pdl` and related frame metadata into the primary converter input
- stop treating the flat ASCII glyph field as the long-term graphical source
- keep ASCII as the audit output, not the only thing the RGB stage understands

## Converter Inputs

The long-term converter should consume structured frame truth such as:

- object id
- role/type
- bounds
- z order
- focused/selected state
- nav/index id
- action/command seam
- label text
- fg/bg/border colors
- source region such as `${game_map}` or taskbar

This allows Wraith to produce a presentable graphical desktop while preserving deterministic audit seams.

For nested Wraith projects, converter source declarations should originate in:

```txt
projects/wraith/wraith-projects/<project>/session/scene.objects.pdl
```

Examples:

- `role=zslice_piece` with `source=pieces/cube_probe/artifact.txt`
- `role=tile_zmap` with `source=maps/map_01_z0.txt`
- `role=rgba_extrusion` with `source=pieces/sample_emoji/voxels_8.csv`

The host manager may import these rows. It must not synthesize these project-specific declarations from C branches.

## Input Parity

GL is not complete if it only mirrors shapes.

The Wraith rule is:

- keyboard nav in GL must resolve against the same actionable controls as ASCII
- mouse clicks in GL must hit the same semantic targets as the indexed/nav model
- the user should be able to audit what control will fire before activating it

That means the semantic scene has to carry enough interaction truth for both theaters:

- stable control ids
- visible nav/index numbers where appropriate
- action command seams
- focus/hover/active state

If ASCII says a control is index `8`, GL should make that fact visible enough for debugging and input parity.

## Layout Parity vs Truth Parity

Wraith is now closer on truth parity than on arrangement parity.

That distinction matters:

- truth parity means ASCII and GL agree on what exists
  - open windows
  - focus
  - control ids
  - nav indices
  - action seams
- arrangement parity means ASCII and GL also feel similarly organized
  - row rhythm
  - spacing
  - padding
  - grouping
  - alignment
  - framing context such as `View:` and taskbar shells

Current reality:

- ASCII is still the authoritative layout composition path
- GL is reconstructing presentation from semantic objects after the fact
- so GL can now be semantically correct while still looking less orderly than ASCII

Why GL still looks less arranged:

- ASCII gets exact line composition from the shell layout and parser
- ASCII inherits the terminal grid naturally
- GL currently consumes individually positioned semantic objects
- individually correct objects do not automatically produce a well-composed row system

That means the next maturity step is not more source truth.

It is better semantic layout structure.

## Needed Semantic Row Primitives

To close the arrangement gap, Wraith should stop thinking only in terms of isolated text items and panels.

It should introduce higher-level row/container primitives such as:

- `taskbar_row`
- `debug_row`
- `summary_row`
- later `launcher_row`
- later `window_chrome_row`

Each row primitive should own:

- left/right padding
- item spacing
- row bounds
- alignment policy
- optional prefix text
- optional frame text
- child control order
- a containment chain that matches `parent_id` and `container_id` in the exported scene

This will let GL preserve the cleaner TPMOS reading shape without falling back to raw glyph mirroring.

## Immediate Design Rule

Do not treat loose text-object export as the final semantic layout model.

Use it as a bridge only.

The next semantic exporter iteration should group related controls into row-level containers so GL can render:

- taskbar rows as taskbar rows
- debug selector rows as debug selector rows
- desktop summary as a summary row

That is the path to making GL look as intentional as ASCII while still remaining a real semantic presentation layer.

## Debug Folding Standard

Wraith should reduce frame spam by using the existing native fold convention instead of permanently printing all diagnostics.

Rules:

- use `[+]` / `[-]` label markers already documented in TPMOS standards
- keep fold state persistent through the existing `fold_ID` pattern
- collapse heavy debug groups by default
- expand only when the user or agent wants the extra detail

Good fold candidates:

- desktop debug
- parser bypass debug
- raw input ledger preview
- frame diff preview
- inspector metadata

This preserves auditability without burying the actual UI frame.

## Scene Primitive Standard

## Wraith GL Standard

Wraith GL should now be governed alongside this document by [WRAITH-GL-STD.md](/home/no/Desktop/🤖️🪤️🏠️/🚽️🧻️/🚽️🥡️-00.00/ZEST-00.00/x0.parent-level-dev-env-01.01/WRAITH-GL-STD.md).

That standard formalizes:

- intent layer
- semantic scene layer
- render receipt layer
- row/container primitive expectations
- agent comparison workflow

This architecture document defines the model.
`WRAITH-GL-STD.md` defines how that model should be audited at runtime.

## Immediate GL Audit Bundle

The minimum useful Wraith GL audit bundle should now include:

- `projects/wraith-alpha/session/gl_intent.pdl`
- `pieces/display/current_frame.meta.pdl`
- `pieces/display/current_frame.objects.pdl`
- `pieces/display/current_frame.desktop_state.pdl`
- `projects/wraith-alpha/session/rgb/current_frame.receipt.pdl`
- `projects/wraith-alpha/session/rgb/gl_display.receipt.pdl`
- `projects/wraith-alpha/session/rgb/gl_input.receipt.pdl`

The receipt should be able to explain both geometry and hierarchy. If a row says it lives under `taskbar_row`, the export and the receipt should agree on that chain exactly.

The intent file is especially important because it gives agents a declared target layout instead of forcing them to infer intent from loose object coordinates.

## Receipt Reading Guide

Receipts are the audit chain between Wraith session truth, RGB conversion, and the GL window.

Read them in this order:

1. `pieces/display/current_frame.meta.pdl`

This is the frame-level declaration. It should identify the source project, layout, cell size, focused object, mouse state, and display contract.

2. `pieces/display/current_frame.objects.pdl`

This is the semantic scene. Each `OBJECT` should expose role, bounds, z-order, focus state, nav id, label, action, parent/container, and ancestor chain.

3. `projects/wraith-alpha/session/rgb/current_frame.receipt.pdl`

This is the RGB converter receipt. It explains what the converter accepted, how each object was projected, and what pixel rectangles were derived.

Important fields:

- `render_checksum_fnv1a64` is the checksum of the RGBA buffer written by the RGB daemon.
- `cell_width_px` / `cell_height_px` define semantic grid projection.
- `glyph_width_px` / `glyph_height_px` define actual font bitmap size inside each cell.
- `px_x0/px_y0/px_x1/px_y1` are the object/hit rectangle bounds.
- `text_px_x0/text_px_y0/text_px_x1/text_px_y1` are the actual text glyph bounds.
- `focus_px_x0/focus_px_y0/focus_px_x1/focus_px_y1` are the focus rectangle bounds.
- `hit_px_x0/hit_px_y0/hit_px_x1/hit_px_y1` are the clickable/action bounds.

4. `projects/wraith-alpha/session/rgb/gl_display.receipt.pdl`

This is the GL presenter display receipt. It explains what RGBA frame the GL window actually uploaded and how it mapped that texture into the current window.

Important fields:

- `loaded_rgba_checksum_fnv1a64` should match `render_checksum_fnv1a64` from `current_frame.receipt.pdl`.
- `loaded_rgba_bytes` should match `expected_rgba_bytes`.
- `loaded_frame_partial` should be `0`; `1` means GL displayed a missing/partial frame.
- `texture_view_x/y/w/h` and `display_scale_x/y` explain fullscreen/windowed letterboxing.

5. `projects/wraith-alpha/session/rgb/gl_input.receipt.pdl`

This is the GL input transform receipt. It shows how a raw GLUT mouse coordinate became a texture coordinate before hit testing.

Important fields:

- `raw_mouse_x/y` are OS/window coordinates.
- `texture_view_x/y/w/h` is the drawn texture rectangle inside the GL window.
- `texture_mouse_x/y` is the coordinate sent back to Wraith session input.
- `inside_texture=0` means the click should not route to Wraith controls.

Receipt parity rule:

- If `current_frame.receipt.pdl` and `gl_display.receipt.pdl` checksums differ, the screenshot may be showing a stale or partial GL texture.
- If checksums match but layout looks wrong, inspect per-object `text_px_*`, `hit_px_*`, and `focus_px_*`.
- If mouse input is wrong only in fullscreen, inspect `gl_input.receipt.pdl` viewport and scale fields before changing ASCII mouse offsets.

## First Intent Scope

The first intended rows that should be declared explicitly are:

- `taskbar_row`
- `debug_row`
- `summary_row`

These rows carry the highest value because they are always visible and strongly affect whether GL feels orderly or stitched together.

To get closer to `chtmgl` while staying auditable, Wraith should normalize UI into shared primitives before theater-specific rendering.

Minimum primitive set:

- window
- titlebar
- button
- panel
- label
- taskbar item
- tile
- sprite

Each primitive should be able to expose:

- stable id
- bounds
- z index
- text/label
- focus/selected state
- visibility state
- style intent such as fill color, border color, glyph/ascii fallback, or asset id

This gives the audit presenter enough truth to be useful and the GL presenter enough truth to draw meaningful shapes.

## Map Surface Standard

For now, Wraith should follow the same language already used by both `fuzz-op` and `fuzz-op-gl`.

Current standard:

- `game_map` = the project-owned map payload
- `INTERACT` = enter direct control of the map surface
- `is_map_control` = current input-routing mode for that surface

This is more important than introducing a new top-level `canvas` term right now.

Rules:

- the primary layout payload should stay `${game_map}`
- the user-facing action should stay `INTERACT`
- the session/runtime flag should stay `is_map_control`
- docs may describe `${game_map}` as a surface or canvas-like region, but the implementation contract should keep the existing names for now

For Wraith, this means the 3D cube probe and later `chtmgl-alpha` work should be modeled as:

- a project window
- a `${game_map}` region
- an `INTERACT` entry/exit seam
- an `is_map_control` state contract

That keeps naming uniform across ASCII and GL while the implementation is still settling.

2D-to-3D conversion is special to `${game_map}` surfaces only.

Current source standards:

- entity/cube source: `pieces/<id>/artifact.txt` z-slice piece files, matching Piececraft/fuzz-op-gl piece precedent
- world/map source: `maps/map_01_z*.txt` plus `assets/tiles/registry.txt` and `assets/tiles/*.tile.txt`, matching Piececraft world precedent
- image/emoji source: `pieces/<name>/voxels_<res>.csv`, matching Emoji Studio RGBA extrusion precedent

Normal desktop windows, taskbars, buttons, debug rows, and widget chrome should not enter this 2D-to-3D conversion path unless they explicitly contain a `${game_map}` surface.

## Wraith and CHTML / CHTMGL

Wraith should be capable of hosting CHTML-like and GLTPM/CHTMGL-like projects without making audit mode a second-class add-on.

Principles:

- layouts should resolve into shared scene primitives first
- ASCII audit should remain available for every hosted project
- GL/native rendering should be an alternate projection, not a separate logic path
- decorative richness may diverge, but interaction truth must not diverge

This means Wraith is not trying to make the ASCII frame look identical to the graphical frame. It is trying to make both frames faithfully describe the same live scene.

## 3D Probe Slice

Before reworking larger `chtmgl-alpha` surfaces, Wraith should use a narrow probe project to validate the audit model.

Recommended probe:

- `wraith-3d-cube`
- `piececraft-wraith` as the Wraith `${game_map}` / z-level validation target
- `chtmgl-wraith` as the Wraith markup/widget validation target
- `emoji-studio-wraith` as the Wraith RGBA/image extrusion validation target
- `wraith-ed` as the Wraith game-editor validation target, using OP-ED editor semantics over Wraith project-owned files

Purpose:

- verify how a simple 3D scene should appear in ASCII audit mode
- verify how the same scene should appear in graphical block mode
- decide what depth, bounds, and orientation metadata belong in the default frame versus folded inspector sections
- verify how `${game_map}` + `INTERACT` + `is_map_control` should read in both theaters
- verify the semantic RGB converter can render object/meta truth without depending on terminal glyph rasterization
- verify that a real z-level ASCII world can drive GL shapes through the same contract
- verify that a real CHTMGL markup window can drive GL widgets through the same contract
- verify that a real RGBA CSV image source can drive extruded 3D cells through the same contract
- verify that a real editor can load maps, place glyphs, create PAL event drafts, write saves, and emit export manifests without Wraith Alpha hardcoding project behavior

Rules for the probe:

- keep the geometry minimal
- log scene ids and transforms
- make frame-to-frame changes easy to diff
- use the results to define the audit format before expanding `chtmgl-alpha`
- keep all Wraith validation probes under `1.TPMOS_c_+rmmp.0102.0027/projects/wraith/wraith-projects/`
- do not create top-level TPMOS projects for `wraith-3d-cube`, `piececraft-wraith`, `chtmgl-wraith`, `emoji-studio-wraith`, or `wraith-ed`
- keep probe body text and scene source records in each project's `session/wraith_body.txt` and `session/scene.objects.pdl`
- do not add probe-specific branches to `wraith-alpha_manager.c`

Validation order:

- first: `wraith-3d-cube` for minimal semantic converter bring-up
- second: `piececraft-wraith` for `${game_map}` + z-level + `INTERACT` validation
- third: `emoji-studio-wraith` for RGBA/image extrusion validation
- fourth: `chtmgl-wraith` for markup/widget/window validation
- fifth: `wraith-ed` for game-editor validation with OP-ED style saves/events/exports

Why this order:

- `wraith-3d-cube` proves the converter seam with minimal noise
- `piececraft-wraith` should prove the map/world pipeline by porting from `piececraft-3d` without pretending it is the same codepath
- `emoji-studio-wraith` should prove the 2D-image-to-3D-extrusion surface path without contaminating the general desktop/widget renderer
- `chtmgl-wraith` should prove richer window markup by porting from `chtmgl-alpha` without pretending it is the same codepath
- `wraith-ed` should prove heavy editor workflows by borrowing OP-ED concepts without moving editor-specific behavior into Wraith Alpha

## Wraith Ed Validation Slice

`wraith-ed` is the game-editor validation project. It is not a replacement for `op-ed` yet; it is a Wraith-native probe that proves OP-ED-style functionality can run through Wraith's file-owned project bridge.

Reference responsibilities:

- `op-ed` supplies sovereign game folders, map/script/save concepts, PAL event-builder direction, and export goals.
- `piececraft-3d` supplies map/tile/z-level/game-map conventions.
- `piececraft-wraith` supplies the current Wraith `${game_map}` + `tile_zmap` semantic object pattern.
- `chtmgl-wraith` supplies the project-local panel/button/widget control vocabulary.

Required ownership:

- `wraith-ed/session/state.txt` owns editor state.
- `wraith-ed/session/history.txt` is the input queue.
- `wraith-ed/session/wraith_body.txt` is the ASCII audit body.
- `wraith-ed/session/scene.objects.pdl` is the GL/semantic bridge.
- `wraith-ed/games/<game>/maps/*.txt` owns editable maps.
- `wraith-ed/games/<game>/saves/*.txt` owns save snapshots.
- `wraith-ed/pal/events/*.pal` owns event drafts.
- `wraith-ed/exports/*.pdl` owns export manifests.
- `wraith-ed/ops/+x/wraith_project_input.+x` owns editor behavior.

Wraith Alpha may launch the project, route `INTERACT`, append key history, and import body/scene files. Wraith Alpha must not know how map loading, glyph placement, PAL event creation, save writing, or export generation works.

## Practical RGB Direction

For J25, the Wraith RGB path should evolve in this order:

- keep the existing glyph raster mirror only as a temporary fallback
- formalize frame objects/meta as the converter input contract
- build an object-to-RGB primitive converter for windows, buttons, panels, and `${game_map}` regions
- keep GL as a thin RGB presenter
- keep validation-project scene declarations file-owned via `session/scene.objects.pdl`
- validate cube/entity conversion through `wraith-3d-cube`, using `pieces/<id>/artifact.txt` z-slice source
- validate `${game_map}` world conversion through `piececraft-wraith`, using `piececraft-3d` as source reference only
- validate RGBA/image extrusion through `emoji-studio-wraith`, using Emoji Studio as source reference only
- validate markup/widget conversion through `chtmgl-wraith`, using `chtmgl-alpha` as source reference only
- validate game-editor flows through `wraith-ed`, using `op-ed`, `piececraft-3d`, and `chtmgl-wraith` as source references only
- treat any case where the graphical result depends on hidden GL-only state as an architectural bug

## Practical Interpretation

For J25, the Wraith direction is:

- build a real session environment
- make `terminal` and `blank-project` real project-backed window actions
- preserve whole-session save/load semantics
- reuse Ops instead of hardcoding bespoke flows
- keep manager code thin
- keep validation-project bodies and scene records in project files, not in Wraith Alpha C branches
- keep the frame contract explicit
- make audit ASCII and graphical block views two presenters over one scene truth
- treat the current ASCII-to-glyph RGBA mirror as a temporary bridge, not the final architecture
- build toward an object/meta-driven RGB shape converter as the real graphical path
- collapse nonessential debug output behind native fold controls
- standardize scene/audit payloads before adding richer visual complexity
- use a narrow `wraith-3d-cube` probe before refactoring `chtmgl-alpha`
- on a fresh Wraith Alpha launch, bootstrap a clean session registry instead of inheriting stale terminal instances from the previous run
- treat `projects/wraith-alpha/manager/gui_state.txt` and the alpha session files as startup state, not durable cross-run truth, when bootstrapping a fresh desktop

## Related Files

- [wrai-tasks-j25.txt](./wrai-tasks-j25.txt)
- [projects/op-ed/README.md](./1.TPMOS_c_+rmmp.0102.0027/projects/op-ed/README.md)
- [projects/op-ed/manager/op-ed_manager.c](./1.TPMOS_c_+rmmp.0102.0027/projects/op-ed/manager/op-ed_manager.c)
- [projects/wraith-alpha/manager/wraith-alpha_manager.c](./1.TPMOS_c_+rmmp.0102.0027/projects/wraith-alpha/manager/wraith-alpha_manager.c)
- [projects/wraith-alpha/plugins/wraith_rgb_daemon.c](./1.TPMOS_c_+rmmp.0102.0027/projects/wraith-alpha/plugins/wraith_rgb_daemon.c)
- [projects/wraith-alpha/ops/wraith_gl.c](./1.TPMOS_c_+rmmp.0102.0027/projects/wraith-alpha/ops/wraith_gl.c)
- [projects/wraith/wraith-projects/wraith-3d-cube](./1.TPMOS_c_+rmmp.0102.0027/projects/wraith/wraith-projects/wraith-3d-cube)
- [projects/wraith/wraith-projects/wraith-ed](./1.TPMOS_c_+rmmp.0102.0027/projects/wraith/wraith-projects/wraith-ed)
- [projects/piececraft-3d](./1.TPMOS_c_+rmmp.0102.0027/projects/piececraft-3d)
- [projects/chtmgl-alpha](./1.TPMOS_c_+rmmp.0102.0027/projects/chtmgl-alpha)
