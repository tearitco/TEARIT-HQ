# Centroid, the first look (2026-11-06)

Status: **design only. Nothing here is built.** Labels: SAID = the owner on 2026-11-06; READ = lore skill and the 2026-11-05 University gap doc; OBS = files opened for this note; DOC = the recommendation.

The owner asked for the Centroid design, and said the important part is the visual: one place drawn twice, once with the RPG Maker sheets and once with the sheets the palette studio already generates, then look and see which is better. A key could swap them. A new book in the editor is the other shape.

## 1. What this slice is

One outdoor court of the University, small enough to see in 2D and in 3D at once. Grass, a short stair-run (three cells, heights 1, 2, 3), one building with a door, one path, sky around the edges. The building is a school front, because the Centroid is a school (READ: pillar 13, "School of the Multiverse"). The shop quarter is not on this page. The draft name Diagonal Alley is not used.

The page is new. It is not `map071` (that map is the 4-player board) and it is not the editor's Islands start room (`rooms.pdl` `ROOM | start | Islands`). Those stay as they are.

Working names, owner can rename: book **Centroid**, page **Spiral Court**. "Spiral" is from the screenplay's "report to Spiral Alley" (READ). It is a working title, not a shipped sign.

## 2. The choice

Question: where do the two looks live?

| Option | One authority | Modular | The 10th look | Long term | House shape | Cost |
|---|---|---|---|---|---|---|
| **A. One page, two sheet bindings, one key** | the cells, heights and doors are one page; only the sheet id changes | a third look is another binding row | same | the kept look deletes the other binding | one `.pdl` page, two sheet-set rows, one view command | small; the painter already takes a sheet name (`rpg_edit_hq.c` `sheet("tilesets", ...)`) |
| **B. Two pages in one new book** | the layout is copied | the copies drift | a tenth look is a tenth page | two courts to maintain | two ROOM rows | the comparison lies as soon as one page is edited |
| **C. A second book whose only job is the generated look** | two books, two authorities | worse than B | a book per art pass | the editor's book list fills with looks | a new book | same drift, plus a book that is not a game |

**Recommendation: A.** Deciding reason: the thing being judged is the art, so the geometry has to be the same picture. A key (or the same command from the palette bar) swaps which sheet set the painter samples. The book is still new, because the University is not the Islands town. Logged as D36.

The key is a view command, recorded when the editor's key list is read at build time. This note does not assign a letter. It must not be a camera key. The pc-hq camera is the owner's and is not reset by this work.

## 3. What the eye should be able to compare

Same camera, same minute, both looks:

- Floor tiles meet without a seam on the grass and on the stair tops.
- The stair reads as a walk up, not a jump, in 2D and in 3D. Heights are the extrusion values already used for drawing. The walk rule (D12, owner already chose height plus a step limit) is the next code after the picture; the first picture only has to show the steps.
- The building reads as one building: wall, roof, door. The door is a floor cell, the same way the Islands doors are.
- A person can stand on the path and the two screenshots differ only in the pixels of the sheets.

Sheet set `rmmv` is the sheets the editor already paints with (`Outside_A5`, `Outside_B`, `World_B`, and the door sheet the Islands room uses). Sheet set `studio` is a palette-studio render of the same cell jobs (ground, wall, door), saved as sheets, not as a second map. Until a studio sheet exists for a job, that job stays on `rmmv` and the corner of the screen says which jobs are still the old art. `out/good/` stays empty until the owner keeps a sheet in writing. Nothing in this slice copies a sheet into `0.good/`.

## 4. Data, when it is built

One book file for the University, first page only:

```
BOOK  | centroid | Centroid
ROOM  | court    | Spiral Court | over | 24 | 17 | 24
BIND  | rmmv     | ground Outside_A5 | wall Outside_B | door !Door1
BIND  | studio   | ground <studio sheet> | wall <studio sheet> | door <studio sheet>
VIEW  | rmmv
```

`VIEW` is the live binding. The key writes that one field. Cells store a job (`ground`, `stair`, `wall`, `door`, `sky`), not a sheet coordinate. The binding turns a job into a cell on a sheet. That is what lets the key change the art without rewriting the page.

Heights stay the one field D12 already chose. A wall is a cell whose height is above the step limit. Sky is no floor.

## 5. What this slice does not do

Shops, the Soul Bank, hotels, courts, the dungeon, the arena, party battles, and the 4x4 board stay in `CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md`. TSC's window conversion is someone else's job. This page does not claim the public "play now" line.

## 6. Proof, when it is built

Two screenshots of Spiral Court, same camera: `view-rmmv` and `view-studio`. A harness checks that flipping `VIEW` does not change a single cell job or height, and that a missing studio sheet is reported instead of sampling a random PNG. A clean compile is not that proof.

## 7. Open, owner

1. Book title **Centroid** and page title **Spiral Court**, or other names.
2. First building: the school front, as recommended here, or the library (the screenplay puts the Soul Pen in a library; imported `map027` LIBRARY is almost unpainted).
