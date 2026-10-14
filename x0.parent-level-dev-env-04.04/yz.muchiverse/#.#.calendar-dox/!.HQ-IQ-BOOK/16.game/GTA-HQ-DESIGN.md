# gta-hq design (roadmap G0) - DRAFT 2, 2026-10-13 (owner answered the 8 questions)

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

## 5. Owner answers (SAID, 2026-10-13) and what they change
1. **Size:** "double each check." MINE reading: each size step doubles the one before (20x13, 40x26, 80x52, 160x104, then 320x208 ...), the menu can keep going as far as the machine allows, and each size is measured before the next is offered. If you meant something else (for example doubling the check, i.e. verifying twice), tell me.
2. **z-levels: yes.** The island is a z world from the start (hills, rooftops, bridges, interiors as levels), using `Z-CELL-FORMAT.md`. Gta therefore needs the ground column and ramps/stairs to rooftops, and its interiors become `building` worlds instead of teleport rooms. Door events stay the transfer mechanism.
3. **Views:** "all views usual 1234 0 `\" - gta supports the same camera/POV keys as the other boards (the `1`-`4` views, `0` for 2D/3D, backtick for the emoji set, backslash for the vocabulary cycle). The detail op must therefore work for every view, not just top-down. Which exact set each key gives follows the existing pc-hq camera model (`pchq-camera-pov-keys` notes), not something new.
4. **Sprite size:** normal (32 px cells) for now.
5. **Animation:** simple but verifiable, flexible. First proof: a 2-frame walk and a 2-frame wheel, checked by a state file that shows the frame index changing as the entity moves.
6. **Stable Diffusion:** run something in python for now; aim for an in-house model written in events/pal/ops like everything else. The plan is `WORD-BANK-TO-IMAGE.md` in this folder (written before going forward, as you asked).
7. **First playthrough:** whatever opens up bottlenecks and is visually verifiable. So the first gta proof is chosen by what removes a blocker and can be photographed (an island regenerating at size N, a car at three distances), not by game content.
8. **Judging:** both - I judge a drafted image and say yes/no with a reason; you look at a contact sheet. An image is only accepted into a brixel when both say yes.

## 6. Not verified
No gta board was opened, no brixel code exists, no Stable Diffusion run was tried. The presets and the op's three levels are proposals.
