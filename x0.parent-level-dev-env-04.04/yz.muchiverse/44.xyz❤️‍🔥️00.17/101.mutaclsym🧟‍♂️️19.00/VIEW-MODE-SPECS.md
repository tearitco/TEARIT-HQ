# Mutaclysm View Mode Tech Specs

## Single Screen
- ONE window opens when mutaclysm launches (no separate board-viewer GL window)
- Map renders directly in the main CHTPM frame alongside menu bar + hero stats
- Same screen as old mutaclysm, but with piececraft z-levels

## Interact Mode (toggle with `i`)
- All view mode toggles (0-4) ONLY work when interact mode is active
- Interact mode shows a cursor on the map
- Arrow keys move cursor, Enter/Space acts on tile
- Exit interact mode with `ESC` or `i` again

## View Mode Toggles (keys 0-4, only in interact mode)

### Mode 0: Emoji 2D (flat top-down)
- Classic roguelike view - one emoji per tile
- Single z-layer slice (current_z)
- Hero, monsters, items overlay on their tiles
- Same as old mutaclysm look

### Mode 1: First-Person 3D
- Eye AT the cursor/hero position, looking forward
- See walls rising around you
- Z-levels visible as height

### Mode 2: Third-Person 3D (default 3D)
- Eye elevated behind and above cursor/hero
- Angled down to see the terrain
- Best for seeing wall heights and z-level differences

### Mode 3: Free-Roam 3D
- Detached camera, NOT tied to cursor position
- WASD pans independently
- C/V changes camera height
- Explore the map from any angle

### Mode 4: Bird's-Eye 3D
- Fully detached, looking straight down
- Absolute map coordinates
- WASD pans, C/V changes height
- Top-down 3D view showing wall heights

## Z-Level Controls (in interact mode)
- `z`/`x`: Change hero's vertical position (pos_z)
- `c`/`v`: Change camera height offset (modes 3/4 only)

## Rendering Pipeline
- Mode 0: mua_compose_frame renders emoji grid from current z-layer chunk file
- Modes 1-4: bv_render_3d raymarches full 32-layer voxel grid, writes rgb_frame_3d_overlay.raw, chtpm_rgb_render composites via MAP3D_MARKER (0x01)

## Wall Height Proof
- Walls should be visibly taller than floor tiles
- In 2D mode: walls use different emoji than floor
- In 3D modes: walls physically rise in the raymarch, proving z-levels work
- This is the "leveled up mutaclysm" proof - old mutaclysm was flat, now has piececraft voxel depth

## State Persistence
- render_mode (0 or 1) persisted in hero/state.txt or bv_state.txt
- camera_mode (1-4) persisted separately
- current_z persisted for z-layer navigation
