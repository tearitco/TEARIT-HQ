# Centroid, the first look (2026-11-06)

Status: **design only. Nothing here is built.** Labels: SAID = the owner on 2026-11-06; READ = lore skill and the 2026-11-05 University gap doc; OBS = files opened for this note; DOC = the recommendation.

The owner asked for the Centroid design, and said the important part is the visual. The first reading of that (one flat editor page, two sheet bindings, a key) is **withdrawn** the same day. The owner corrected it with a picture: one big open campus, several stories, towers, a water court, a grand stair, roofs you can stand on. It is built from **voxels**, so the picture stresses the engine. The RPG Maker sheets and the studio sheets are two skins of that same volume, still swapped by one view field, after the volume exists.

The picture is the owner's attachment of 2026-11-06 (a grey-stone campus on a moat, cliff trees around it, a clock tower, colonnades, roof gardens). It is the target look. It is not a map to trace tile by tile, and it is not a claim about which tool drew it.

## 1. What this slice is

Three floating islands in one open volume. The **main** island is the campus in the 2026-11-06 picture, reduced to masses the engine can hold: a gate stair of many steps, a front court, one tower, one colonnade, one roof you can walk that has a room under it. **Two smaller islands** sit off it. A bridge joins the main island to each of them. Nothing else joins them. Under the grass, under the bridges, and between the islands is sky, the same rule as the floating-island picture written up in `EDIT-ISLANDS-DESIGN.md` (the owner's centroid screenshot of grass, dirt cliffs, pines, and wooden bridges). That town is five walkable islands and is already the editor's start room. This campus is three. The bridges are the engine test as much as the tower: a walkway with no ground under it, held up at the two ends.

Several stories on the main island's footprint. No loading seam between the three islands.

That is the engine test. D12's rule A (one height per cell) draws a stair and blocks a cliff, and its own note says it cannot put two floors on the same x,y. This picture is that case. So the campus is voxels, the shape D29 already chose for buildings (stacked boxes: roof, door, window) and the chunk shape mutaclysm already stores (`19.00-VOXEL-3D-ARCHITECTURE.md`: a column of z slices). D12 stays the rule for a flat board. It is not the campus.

Working name, owner can rename: world **Centroid**, place **Spiral Court**. "Spiral" is from the screenplay's "report to Spiral Alley" (READ). The draft name Diagonal Alley is not used. This is not `map071` and not the Islands start room. The editor's `rooms.pdl` is a flat grid (`ROOM | ... | cols | rows`). A new book there cannot hold this volume until a room can be a voxel chunk. The book comes after the volume renders. The first authority is the chunk, not a second Islands page.

## 2. The choice

Question: what is the first Centroid, after the picture?

| Option | One authority | Modular | The 10th wing | Long term | House shape | Cost |
|---|---|---|---|---|---|---|
| **A. Flat editor page, sheet key** (the withdrawn reading) | one page | a skin is a row | a wing is another flat page | cannot stack a roof on a room | `rooms.pdl` | small, and it is not the picture |
| **B. One voxel volume, two skins, one view field** | the chunk is the campus; a skin only names which texels cover a face | a wing is more columns; a skin is a row | same | roofs, towers and the moat are the same object the board already marches | chunk files plus a `VIEW` row; harness counts solid columns and a roof-over-room | the engine's cost: draw, collision, and memory on one weak CPU |
| **C. Import the picture as a mesh** | the mesh is a second world | no | a new picture is a new mesh | the engine never learns voxels | an outside file | looks right once, teaches nothing |

**Recommendation: B.** Deciding reason: the owner named voxels and named the engine. The sheet swap survives as the skin. It is not the world. Logged as D37, which replaces the flat-page half of D36.

The view command is recorded when the key list is read at build time. This note does not assign a letter. It must not be a camera key. The pc-hq camera is the owner's and is not reset by this work.

## 3. What the eye should be able to compare

One camera on the volume. The pass is the engine, then the skin.

Engine, before either skin is judged:

- A roof voxel sits on the same column as a room voxel, and both draw.
- The gate stair is a run of voxels. A walker climbs it without a jump. A wall taller than the step limit blocks. That is D12's step rule applied per voxel face, not one height for the whole column.
- Water is a surface. The cliff trees sit on the bank, outside the building mass.
- Standing on the roof and standing in the room under it are two places. The 2D projection and the 3D view name the same column.
- The frame names its cost: columns drawn, and whether the machine stayed inside the guard. A campus that only exists while the guard is tripping has not passed.

Skin, after that massing is on screen:

- `rmmv` covers faces with the sheets the editor already paints with.
- `studio` covers the same faces with a palette-studio sheet of the same jobs (stone, roof, water, tree).
- The two screenshots differ in texels. The columns do not change.
- Until a studio sheet exists for a job, that face stays on `rmmv` and the corner says so. `out/good/` stays empty until a sheet is kept in writing.

## 4. Data, when it is built

The campus is a chunk column, the same idea mutaclysm already writes (one z slice per file under a chunk). A column may hold air, stone, a floor, and another floor. A face has a job (`stone`, `stair`, `roof`, `water`, `tree`, `door`). The skin binds a job to a sheet. `VIEW` picks the skin.

```
WORLD | centroid | Centroid
PLACE | court    | Spiral Court
BIND  | rmmv     | stone Outside_B | roof Outside_B | water World_A1 | tree Outside_B
BIND  | studio   | stone <sheet> | roof <sheet> | water <sheet> | tree <sheet>
VIEW  | rmmv
```

The first chunk is the gate stair, the front court, one tower, one roof over one room, a door in that room's wall, and **one bridge to one smaller island** (sky under the bridge). The second bridge and the rest of the picture are the same format added beside it, not a new kind of file. A removed bridge cell must leave the side island unreachable. The islands do not touch.

## 4b. Doors and insides (SAID 2026-11-06)

The campus uses Minecraft-like doors. A door is two voxels in the wall. Shut, it blocks. Open, it swings and the walker steps through into the room that is already there: the floor, the walls, and the roof on that same column. There is no second page and no teleport.

That is the opposite of the Islands town, whose `DOOR` rows send you to a bank, store, school, hotel, or cabin page. Those rows stay on that town. A campus door never writes a room id.

The picture of the door may use Mineclonia's wood door (the owner already called Mineclonia art safe). The open and shut state is ours: one bit on that voxel, the same bit in 2D and in 3D. The first proof is one door. Shut, the room cannot be entered. Open, the walker stands on the interior floor under the roof, and both views show the opening. An editor book is a later view of this chunk. It is not a flat `ROOM` row pretending to be the campus.

## 5. What this slice does not do

Shops, the Soul Bank, hotels, courts, the dungeon, the arena, party battles, and the 4x4 board stay in `CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md`. TSC's window conversion is someone else's job. This page does not claim the public "play now" line.

## 6. Proof, when it is built

One screenshot from the gate, one from the roof, one from the room under that roof. A harness counts a column that contains both a room voxel and a roof voxel, refuses a build that drops the roof, and checks that flipping `VIEW` changes no column. A missing studio sheet is reported. The run says the column count and goes through `cpu_gate`. A clean compile is not that proof.

## 7. Open, owner

1. Book title **Centroid** and page title **Spiral Court**, or other names.
2. First building: the school front, as recommended here, or the library (the screenplay puts the Soul Pen in a library; imported `map027` LIBRARY is almost unpainted).
