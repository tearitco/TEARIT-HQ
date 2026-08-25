# Wraith Ed Assumptions

Reference projects:
- `projects/op-ed`
- `projects/piececraft-3d`
- `projects/wraith-alpha/wraith-projects/piececraft-wraith`
- `projects/wraith-alpha/wraith-projects/chtmgl-wraith`

Assumptions being tested:
- `wraith-ed` should be a Wraith internal project under `projects/wraith-alpha/wraith-projects/`, not a top-level TPMOS project.
- game-editor truth belongs in project files: maps, saves, PAL events, exports, `session/state.txt`, `session/history.txt`, `session/wraith_body.txt`, and `session/scene.objects.pdl`.
- map editing uses the `${game_map}` payload surface.
- editor controls are project-local CHTMGL-style controls targeting `${game_map}` or project editor actions.
- `op-ed` sovereign games map to `wraith-ed/games/<game>/`.
- PAL event creation starts as auditable event-draft files before becoming a full block builder.
- exports should be manifests/receipts first, then bundled runtime artifacts later.

Current limits:
- this is not yet a full `op-ed` port.
- no file browser UI yet.
- no full PAL block-builder UI yet.
- no real game-save loader beyond the first save snapshot convention.
- map editing is single z-level for now.

Next corrections:
- add file-browser style game/map selection from `op-ed`.
- add CHTMGL panels for PAL event block selection.
- add Piececraft-style z-level and camera controls.
- add export bundler receipts that name every copied file.
