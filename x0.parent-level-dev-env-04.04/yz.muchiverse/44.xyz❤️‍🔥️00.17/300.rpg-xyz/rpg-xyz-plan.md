# rpg-xyz / rtp-xyz - Architecture & Implementation Plan

## What We're Building

Two parallel RPG Maker-like experiences in xyz-os. Both start from mutaclysm
but differ in how editing tools are integrated:

### rpg-xyz - Widget-Toolchain Variant
Game engine + default project. Editing tools are **separate widgets**
(event-editor, map-picker, tile-picker, project-menu) communicating via
ledger discovery + file-mediated cmd bus. Modular, multi-process.

### rtp-xyz - Built-In Tools Variant
Game engine has editing tools **embedded inside** as CHTPM-loaded modules
(like slop-ed-dev's manager daemon). Map editor, event editor, project
selection all live inside the engine session via layout switching.

Both share: game engine core, registry data, project file formats.
Difference is **where the tools live** -- separate widgets vs integrated daemons.

## Current Assets

### 101.mutaclsym - Complete game engine
- 22+ ops: movement, camera, turn system, inventory, crafting, combat,
  saving, world I/O, map gen, tile placement, widget cmd bridge
- Map format: dir-based with `map.txt` (terrain grid 40x16), `furniture.txt`,
  `transitions.txt`, `monsters/`, `items/`, `hero/`
- Registries: terrain, furniture, monsters, items, recipes, emoji assets
- Dual render path (ASCII terminal + GL window)
- PAL loop + CHTPM game layout
- world_01/ (live) + world_01_template/ (new-game seed)

### 201.rpg-maker-clone - Standalone RPG editor (freeglut)
- 4 modes: Title / Map Editor / Event Editor / Play
- Map format: `map.txt` + `map_obj.txt`, 36x28
- Event format: `events/ev_X_Y/` with state.txt + event.pal + event.ir.pdl
- Switches: switches.pdl
- Demo quest: crystal switch -> guard dialogue -> warp
- Limitations: no text input, no insert palette, primitive GLUT visuals

### slop-ed-dev (TPMOS) - Sovereign RPG Editor reference
- Games are self-contained dirs in `games/`
- Editor tools loaded as CHTPM `<module>` daemons
- Two daemons: slop-ed-dev_manager + pal_editor_module
- File-as-database, daemon-as-controller architecture
- piece.pdl uses METHOD sections to map events to handlers

### &.widgits/ - Widget toolchain
- event-editor: create/edit event packages, desktop import/export
- map-picker: list/switch maps
- tile-picker: set brush, place tiles, desktop stamps
- file-menu: SAVE/LOAD reference
- All share: ledger discovery, cmd bus (inbox/status), GL pipeline

### xyzfs user space
- `xyzfs/users/<uuid>/home/runtime/ledger.txt` exists
- `xyzfs/users/<uuid>/home/projects/` does NOT exist yet

---

## Architecture

### rpg-xyz - Widget-Toolchain Variant

```
rpg-xyz/                          <- copy of mutaclysm engine
  button.sh                       <- compile/run (loads default project)
  system/                         <- daemons
  ops/                            <- game engine ops
  pal/                            <- main_loop_chtpm.pal
  pieces/
    chtpm/layouts/                <- game.chtpm
    registry/                     <- shared game data
    world_01/                     <- DEFAULT project (has content)
      maps/map_start/             <- terrain + objects + events + hero
      monsters/                   <- default monsters
      items/                      <- default items
    world_template/               <- blank seed for new projects

&.widgits/project-menu/           <- NEW separate widget
  button.sh
  ops/                            <- pm_list, pm_new, pm_delete, pm_open, compose_frame
  pal/main_loop_chtpm.pal
  pieces/chtpm/layouts/project_menu.chtpm
```

### rtp-xyz - Built-In Tools Variant

```
rtp-xyz/                          <- copy of mutaclysm + integrated tools
  button.sh                       <- compile/run (loads default project)
  system/                         <- daemons
  ops/                            <- game ops + tool ops
  manager/                        <- like slop-ed-dev/manager/
    rtp_manager.c                 <- main editor daemon (+x)
    rtp_map_editor.c              <- map editor module (+x)
    rtp_event_editor.c            <- event editor module (+x)
    rtp_palette.c                 <- tile/entity palette (+x)
    state.txt
    gui_state.txt
  pal/
  pieces/
    chtpm/layouts/
      game.chtpm                  <- play mode
      map_editor.chtpm            <- map editing
      event_editor.chtpm          <- event editing
      project_menu.chtpm          <- project selection
      palette.chtpm               <- tile/entity palette
    world_01/                     <- DEFAULT project
```

Layouts declare modules via CHTPM `<module>` tags, like slop-ed-dev.

---

## Default Project Content (Same for Both)

Start with a playable default project inspired by:
- slop-ed-dev: default-0000 (walls/floors/resources, 20x10)
- 201.rpg-maker-clone: demo factory (guard NPC, crystal switch, warps)
- mutaclysm: world_01 (complex terrain, buildings, 40x16)

```
pieces/world_01/
  project.pdl               <- project_id=rpg-xyz/rtp-xyz, title=My Quest
  switches.pdl              <- door_open=0, met_guard=0
  maps/map_start/
    map.txt                 <- terrain (walls, floor, water, trees)
    map_obj.txt             <- objects (crates, plants)
    state.txt               <- id=map_start, width=40, height=16
    transitions.txt         <- exits to other maps
    furniture.txt           <- placed furniture
    hero/state.txt          <- spawn coords
    monsters/               <- 1-2 starter monsters
    items/                  <- potion, sword
    events/
      ev_3_5/               <- guard NPC (dialogue)
      ev_12_7/              <- crystal switch (opens door)
```

---

## Milestones

### M1: Copy + Default Project
1. `cp -a` mutaclysm -> rpg-xyz/ and rtp-xyz/
2. Rename project.pdl metadata for each variant
3. Populate default project with terrain, NPC, switch, items, monsters
4. `button.sh run` loads default project (no --project flag)
5. Move plan docs into each variant dir

### M2: project-menu widget (separate, for rpg-xyz)
1. Create &.widgits/project-menu/ (file-menu pattern)
2. Ops: list/new/delete/open projects in xyzfs
3. GL pipeline + CHTPM layout
4. Ledger registration + discovery

### M3: rtp-xyz integrated tools (daemon modules)
1. rtp_manager.c - project/map/event orchestration
2. rtp_map_editor.c - paint tiles, place entities
3. rtp_event_editor.c - block-building for event.pal
4. CHTPM layouts with <module> declarations
5. Layout switching via href/F-key

### M4: Drag-drop between tools and engine
1. File handoff via #.desktop/ tray (existing ee_export/import)
2. Tile-picker stamp -> desktop -> drop into map
3. Event-editor package -> desktop -> import into project

### M5: Polish and compare
1. Play default quest in both variants
2. Identify gaps
3. Decide direction

---

## Decisions

| Question | Answer |
|----------|--------|
| No --project flag? | Yes. button.sh run loads default project. Multi-project via project-menu widget or integrated layout. |
| Default project has content? | Yes. Terrain, NPC, switch, items, monsters - playable. |
| rpg-xyz vs rtp-xyz? | Both. Build in parallel, compare. |
| Drag-drop mechanism? | File handoff via #.desktop/. XDND abandoned (WM bug, never worked). |
| Game data location? | xyzfs/users/<uuid>/home/projects/ for user projects. Default in install tree. |
| 201.rpg-maker-clone format? | Keep for reference. event-editor already speaks same .pal/.ir.pdl. |
