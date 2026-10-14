# gta-hq design (roadmap G0) - DRAFT 1, 2026-10-13

Status: **draft for the owner. Nothing here is built.** Labels: SAID = owner's words; READ = from files, not run; MINE = claude's proposal, needs the owner's yes. Grok keeps its gta code as is and starts no new gta work until this page is agreed.

## 1. What the owner wants (SAID)
- A much larger island, with its size chosen from a menu.
- Better pixel graphics and animation, toward GTA-level looks from Stable Diffusion alone.
- `.brix`-style parts that evolve and scale. A middle renderer op derives the level of detail from parts, distance and a detail setting. There is no separate close/far art file.
- Art by hand-trained Stable Diffusion: word banks and concept banks for text-to-image, hand-tuned words and outputs, delegated to tomom/gemma/API workers where possible, claude as final judge only. Nothing generated at runtime.
- A scripted playthrough of gta at the end.

## 2. What exists (READ, not run)
- `@.apps/gta-hq`: a dsr-rpg clone, `gta_hq.c` 1423 lines. `rooms.pdl`: the island is one OVERMAP room ("Los Vientos", 20 x 13 cells at 32 px, biome `worldmap`) plus five interiors reached by door events.
- `shared/gta-tiles.pdl`: 37 GTA roles mapped to real CDDA ids, plus 7 GAP rows (no car frame/engine/roof, no base person sprite, no crosswalk/curb). `shared/brixels.pdl`: seed pieces, sockets, plugs and a car and a person assembly (design: `BRIXELS-DESIGN.md`). No code reads the brixel seed yet.
- Los Vientos play slices 1-2 (walk to a door, arrive inside, return) are not confirmed by anyone who watched them. The car is not drivable.

## 3. Proposed design (MINE)
### 3.1 Island size from a menu
The island is a z-cell `column`/`building`-style world (`Z-CELL-FORMAT.md`) at ground level, generated from a size preset, not hand-drawn. Presets (cells at 32 px): Small 20x13 (today), Medium 40x26, Large 80x52, Huge 160x104. Choosing one regenerates the island from a seed. Hand-authored places (the five interiors) are placed on it by name. **Proof G1:** pick a size, island regenerates, CPU measured per size.

### 3.2 Detail level by one renderer op
One op takes (an assembly, distance to the viewer, detail setting) and returns what to draw:
- far: one coloured cell or one flat icon per entity (the abstraction; matches a `ZREGION` stand-in);
- mid: the assembled sprite as one picture;
- near: the parts drawn separately, with their own state (door open, glass broken, wheel turning).
The same brixel assembly feeds all three, so nothing is stored per distance. The detail setting shifts the two distance thresholds. **Proof G2:** the same car at three distances from the same parts, three screenshots.

### 3.3 Animation
A part may carry frames; the op picks a frame from the entity's state and a clock. A walking person is parts with leg/arm frames swinging with step count; a wheel turns with speed. States come from the entity's FSM (the same state that feeds the decision ledger), so animation and behaviour share one source. Details are open (section 5).

### 3.4 Word bank to image (by hand, off-line)
`word bank` (what a thing looks like, in words) -> `concept bank` entry (the word set plus weights, with the part it is for) -> prompt -> Stable Diffusion output -> hand-tuned words/outputs -> judged by claude -> the accepted image becomes a part's picture in the brixel concept row. Nothing is generated while the game runs. Workers (tomom, gemma on the Mac 10.0.0.144, a free API) draft; the owner and I decide. **Proof G3:** one concept goes through the whole chain and the judged image appears on a brixel.
Not known yet: whether Stable Diffusion can run here at all, or only through an API, and what it costs. That decides how much can be delegated.

## 4. Order
G0 this page agreed -> G1 island sizes -> G2 detail op (needs brixel parts that exist) -> G3 word-bank pilot -> G4 scripted playthrough. Vehicles and flight come from the shared spine (M1 done on paper, M2, F1), not from here.

## 5. Questions for the owner (answer any, in any order)
1. Size presets: are 20x13 / 40x26 / 80x52 / 160x104 the right range? How large should "much larger" be in the end?
2. Is the island one flat overmap, or does gta also want height (hills, rooftops, bridges) as z-levels from the start?
3. "GTA-level graphics": top-down like today, or an angled/isometric view for gta? That changes what art we ask Stable Diffusion for (angle, size, shadows).
4. Sprite size: keep 32 px cells with larger parts (a car spanning several cells), or raise the cell size? Which pixel size do you want people at?
5. Animation: how many frames for walk and wheel turn is enough for the first proof (2, 4, 8)? Is a frame-by-frame sprite okay, or must parts rotate smoothly?
6. Stable Diffusion: do you have a way to run it (the Mac, a GPU, an API key you are happy to spend on)? Which models can I use for drafting that you consider free?
7. What is the GTA part to prove first: driving a car, a pedestrian crowd, the shops/jobs, police? One thing for the first playthrough.
8. Who judges the art when the models drafted it: I look at the image and say yes/no, you look at a contact sheet, or both?

## 6. Not verified
No gta board was opened, no brixel code exists, no Stable Diffusion run was tried. The presets and the op's three levels are proposals.
