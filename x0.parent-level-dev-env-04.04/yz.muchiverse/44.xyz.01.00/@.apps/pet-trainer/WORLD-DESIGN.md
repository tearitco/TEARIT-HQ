# Pet trainer: the home, the property, the planet (vision + design, written 2026-10-09, NOT built)

Owner vision (verbatim intent): a mini map shows where the pet is in the house. The house is a **doll house on a property** and the pet can **build more rooms** (like Dwarf Fortress) and put things in them, including outdoor things like farming. His neighbours, the **six tii-monsters**, live in the same world. The world is a **voxel planet**: "Dwarf Fortress meets Pokemon/Minecraft". Pets will later **borough** (visit/move to) other pets, who may like or dislike them.

## Layers (build bottom-up, each one usable alone)
1. **Rooms** (today): `rooms.pdl` has ROOM / DOOR / PLATFORM rows. A door is a teleport event. Auto doors are walked by the pet.
2. **House map**: rooms are cells on a grid (`home.pdl`: `CELL | x | y | z | room | kind`). The mini map draws the grid, the pet's cell highlighted, doors as links. Shown as a small overlay on the canvas (and as a page `map`).
3. **Build mode**: a "Build" verb adds a cell next to an existing one (costs items/coins from the pet's inventory), creates the door pair rows, and a room kind (bedroom, kitchen, workshop, farm plot...). Build is an event (`build_room`), so agents and users do it through the same events.
4. **Property**: outdoor cells (garden, farm, pond, path) on the same grid; crops grow on the pet clock (`time.pdl`), harvest gives items. The garden room is the first one.
5. **Neighbours**: each of the six pets owns a plot on the shared grid; the village page becomes the property map. "Borough" = visiting or moving into another pet's house through a door event; a `LIKE` ledger per pair (appended by conversations and gifts) decides the welcome.
6. **Voxel planet**: the property grid becomes a chunk of a voxel world (x, y, z blocks); the planet is many chunks. Rendering goes through the board-viewer 3D path (see CAMERA-DESIGN.md), so the pc-hq camera/POV keys explore it. Storage follows the house plan: files now, wraith-alpha in-memory DB later.

## Rules
- Everything is data rows + events (RPG Maker style), no per-feature C.
- The mini map is read-only; changes come from events, so agents, users and the pet all use one path.
- Per-pet state stays in the pet's folder; shared world (cells, plots, LIKE ledger) lives in `state/world/` as append-only ledgers with cursors (never mtime).
- Drag and drop (entities in and out of the game) targets these same cells later.

## First steps (small, provable)
1. `home.pdl` grid for bedroom, living room, garden and a mini map drawn in `pet_scene.c` with the pet's cell lit; PNG proof walking through a door.
2. `build_room` event + one new room kind; PNG proof of the map growing.
3. Crop growth on the clock.
4. Then neighbours and LIKE.

## Text input (owner request, same session)
The chat line is a single-line `<cli_io>`; long or multi-sentence messages (and the dictated speech from the planned mic mode) need more room. The house already has a multi-line `<text_area>` (text-edit-hq, pdl-read, h-ai-lab, sql-hq). Plan: replace the chat field with a `<text_area>` (Enter = new line, a Send button / Ctrl+Enter sends) and keep the old Enter-to-send for one-liners. First reproduce the owner's "Enter did nothing" report through the relay and the real field before changing it.
