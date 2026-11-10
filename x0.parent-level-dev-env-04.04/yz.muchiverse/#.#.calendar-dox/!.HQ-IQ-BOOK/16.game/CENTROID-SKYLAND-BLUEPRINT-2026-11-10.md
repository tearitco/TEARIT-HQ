# Centroid skyland: the master-plan blueprint made from the owner's goal picture (2026-11-10)

Owner request, 2026-11-10: make a blueprint of the skyland in the picture `goal-centroid-skyland.jpeg`, "which should also include the land (and surrounding empty small sky islands as well, not in image)". Status: **a DATA PLAN, checked by an ad-hoc script; no engine, no voxels, no harness case yet.** Labels: **OBS** (I looked at the picture or ran something), **DOC** (my reading or reasoning), **OPEN** (owner decision or unknown).

Data file: `44.xyz.01.00/@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl` (106 lines, rows `ISLAND`, `ZONE`, `WATER`, `BUILD`, `WAYPOINT`, `ROUTE`, plus `SCALE`, `GRID`).

## 1. What the picture is (OBS) and what I did with it

The picture is a screenshot of a **third-party Minecraft build** from a Dynmap, titled in the image "The Evertale of Library" (with a Japanese subtitle) and credited in the image to its Dynmap author. It shows a floating landmass seen obliquely from the south-west, fifteen numbered red dots (a walking tour from **start** to **goal**), and six inset photos (the start pond, a wooden stage, a forest view, the library interior, a flower path, the fountain plaza).

**IP stance (DOC, in line with the house rule "ideas only, never their bodies"):** the picture is NOT in the repo (path only), the blueprint is OUR abstraction (masses, positions, zones, a route) with our own names, and it is not a block-by-block copy. Do not publish the screenshot or reuse the build's title. The owner called the picture "the goal"; the 2026-11-06 visual book treats such a picture as "the target look, not a map to trace tile by tile".

**What I see (OBS):**
- **Three levels of land:** a low forested **west lobe** (the tour starts at its far-left pond), a low **village lobe** in the south-east (wheat fields, timber houses with green roofs, a fountain plaza, a small green-spired chapel next to the goal), and a **higher plateau** at the back (north-east) with a cliff above the village lobe. A cliffy stepped ramp links the forest to the plateau near points 5 to 7.
- **The landmark:** a large pale-stone **cathedral/library** with many green copper spires and a taller central tower, on the plateau; pale blossom trees all around; a **river** winds across the plateau under a small bridge, and a **waterfall** drops from the plateau cliff onto the village lobe.
- **Two long thin waterfalls** hang from the lobes' south edges into the dark void; the underside of the land is rock tapering to a point; the sky is black.
- **No clock tower is visible** in this picture (the visual book's earlier picture had one). The cathedral's central spire is the only tower; I left a marked OPEN note there, not an invented tower.

**How I turned it into numbers (DOC, accuracy: about +-3 cells):** I read the red dots' pixel positions and the island outlines from the oblique image, mapped them onto a flat plan grid (x east, y south, z up in blocks), and corrected by hand for foreshortening. This is an estimate from ONE image with no depth information. Nothing was measured in the original build.

## 2. The blueprint in numbers (OBS: from the file and the check run)

`SCALE | cell_blocks | 4` (one plan cell = 4 x 4 blocks), `GRID 72 x 52` (288 x 208 blocks), z in blocks, island tops at 0 (the two lobes), 5 (the two link ramps) and 10 (the plateau).

| Island | Kind | Centre (x,y) | Radius (x,y) | Top z | Depth below | Plan cells |
|---|---|---|---|---|---|---|
| A forest lobe | main | 22, 35 | 17, 10.5 | 0 | 14 | 552 |
| B village lobe | main | 48, 39 | 18, 8.5 | 0 | 12 | 472 |
| P plateau | main | 48, 16.5 | 18, 12.5 | 10 | 16 | 708 |
| L west ramp (shoulder) | link | 28, 23 | 6, 6 | 5 | 6 | 112 |
| L2 east ramp | link | 60, 29.5 | 3.5, 4 | 5 | 6 | 40 |
| S1 to S7 (new, **empty**) | small | around the edge | 2.5 to 5 | -8 to +24 | 4 to 7 | 52, 20, 20, 20, 16, 14, 20 (162 total) |

Main and link islands cover 1884 plan cells (about 30,000 block columns, overlaps counted twice); the seven small islands add 162 cells. The small islands sit at different altitudes (S3 and S7 are below the lobes, S5 and S6 are far above the plateau) so the sky reads as full of land but each one is **bare grass and stone: no zone, building, waypoint or water is placed on any of them** (a checked rule).

**Zones (the land):** conifer forest, clearing, start-pond bank, dark-oak cliff, pale grove, lawn, two wheat fields, stone plaza, village, flower meadow. **Water:** the start pond, a stream that ends in a long fall into the void, the plateau river with a branch that goes over the cliff (fall 10 blocks, pool below), and a second long fall into the void on the village lobe. **Buildings (masses):** cathedral (14 x 8 cells, 16 blocks high, steep roof), a main spire and a ring of six smaller spires, the chapel, a footbridge, a fountain, a wooden stage, a hut, two house rows, a barn, and the east stair. **Tour:** waypoints 0 (start) to 15 (goal), 15 hops, **116.5 cells = 466 blocks** of walking, including two stair-ups (0 to 5 to 10) and one long stair-down on the east ramp (10 to 0).

Top-down preview (a projection made by the ad-hoc script; `A` forest lobe, `L` ramps, `P` plateau, `B` village lobe, digits 1-7 small islands, `#` buildings, `~` water, waypoints 0 to 9 then a to f for 10 to 15):

```
                                                  6666                  
                                                 666666                 
                                                  6666                  
                  2222                                                  
                 222222                    PPPPPPPPPP                   
                 222222                PPPPPPPPPPPPPPPPPP               
                  2222               PPPPPPPPPPPPPPPPPPPPPP             
                                    PPPPPPPPPPPPPPPPPPPPPPPP            
                                  PPPPPPPPPPPP##############PP     5555 
       111111                    PPPPPPPPPPPPP##############PPP    5555 
     1111111111                 PPPPPPPPPPPPPP##############PPPP   5555 
     1111111111                 PPPP~PPPPPPPPP#######a######PPPP   5555 
     1111111111                PPPPPPPPPPPPPPP##############PPPPP       
     1111111111                PPPPPPPPPPPPPPP##############PPPPP       
       111111                 PPPPPPPP~PPPPPPP##9########b##PPPPPP      
                              PPPPPPPPPPPPPPPP##############PPPPPP      
                              PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP      
                          LLLLPP7PPPPPP8~PP####PPPPPPPPPPPPPcPPPPP      
                        LLLLLLLLPPPPPPPPPPP####PPPPPPPPPPPPPPPPPPP      
                       LLLLLLLLLLPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP       
                       LLLLLLLLLLPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP       
                      LLLLLL6LLLLLPPPPPPPPPPPPPP~PPPPPPPPPPPPPPP        
                      LLLLLLLLLLLLPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP        
                      LLLLLLLLLLLLPPPPPPPPPPPPPPPPPPP~PPPPPPPPP         
  33                  LLLLLLLLLLLLPPPPPPPPPPPPPPPPPPPPPPPP~PPP          
 3333          AAAAAAAALLLLLLLLLL   PPPPPPPPPPPPPPPPPPPPPPPP            
 3333       AAAAAAAAAAALLLLLLLLLL    PPPPPPPPPPPPPPPPPPPPPPLLL          
 3333     AAAAAAAAAAAAAALLLLLLLL5A     PPPPPPPPPPPPPPPPPPLL##LL         
 3333    AAAAAAAAAAAAAAAAALLL####AA        PPPPP~PPPP    LL##LL         
  33    AAAAAAAAAAAAAAAAAAAAA#4##AAA                    LLL##LLL        
       AAAAAAAAAAAAAAAAAAAAAA####AAAA                    LL##LL     44  
      AAAAAAAAAAAAAAAAAA3AAAAAAAAAAAAA  BBBBBBBB~BBBBBBB LL##LL    4444 
     AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA###BBBBBBBBBBBBBBBBBBBBBLL     4444 
     AAAAAAAAAAAAAAAAA2AAAAAAAAAAAAA#f#BBBBBBBBBBBBBBBBBBBBBBB     4444 
     AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA###BBBBBBBBBBBBBBBBBBBBBBBB    4444 
     AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBBB    44  
     AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA####BBBBBB########BBBBBBBBBBB       
     AAA###A1AAAAAAAAAAAAAAAAAAAAAAA####BBBBBB########BBBBBBBBBBBB      
      AA###AA~AAAA~AAAAAAAAAAAAAAAAA####BBBBBB########BBBBBBBBBBBB      
       A###AAAAAAAAAAAAAAAAAAAAAAAAA####BBBBBB########BBBBBBBBBBBB      
        A0AAAAAAAAAAAAAAAAAAAAAAAAAABBBBB##BBB####BBBBdBBBBBBBBBBB      
         AAAAAAAAAAAAA~AAAAAAAAAAAABBBBBB##BBBBBBBBBBBBBBBBBBBBBB       
          AAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBeBBBBBBBBBBBBBBBBBB        
            AAAAAAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBBBBBBBBBBBB~         
               AAAAAAAAAA~AAA     BBBBBBBBBBBBBBBBBBBBBBBBBBBB          
                                    BBBBBBBBBBBBBBBBBBBBBBBB            
                                        BBBBBBBBBBBBBBBB                
                                                                        
                                  7777                                  
                                 777777                                 
                                 777777                                 
                                  7777                                  
```

## 3. How it was checked (OBS) and what that does NOT prove

An **ad-hoc script** (a throwaway `awk` command run from `/tmp`, **not committed**, so this is not yet a harness) read the data file and checked: every island lies inside the grid; all 16 waypoints are on their declared island; all 12 building footprints have their four corners on land; every small island's rim is at least 15% clear of every main island (the smallest margin was 1.26 against the 1.15 limit); nothing sits on a small island; the route runs 0 to 15 in order, flat hops do not change z, stairs-up rise, stairs-down fall, and no hop exceeds 25 cells; every water point is on land. **Result on the real file: all checks pass.**

Three deliberate breakages (on copies) each failed the right check: a waypoint moved into the void ("waypoint 9 outside island P"), a small island slid onto the forest lobe ("too close"), and a zone placed on a small island plus the cathedral pushed off the plateau (two FAIL lines). The unmodified control passed.

**The script found real mistakes in my first draft, which I fixed:** the plateau's rounded back left the cathedral's north corners in the void (plateau re-centred and enlarged); the chapel and the hut overhung their islands (moved); and the route from waypoint 12 down to the village crossed a 6-cell gap between the plateau and the village lobe (I added the east ramp island L2, the mirror of the west one).

**What this does NOT prove:** that the shapes match the original (all positions are estimates), that the engine can hold the volume, or that it is beautiful. The check is geometry only.

## 4. What is not in the blueprint (so nobody thinks it is)

- **No voxels.** The house chunk format is `CELL | x | y | z | job` (jobs `stone`, `stair`, `roof`, `water`, `tree`, `door`; see `@.apps/centroid-hq/data/court.pdl`, 156 cells for one 6 x 5 room). This plan is the layer above it: islands, zones, masses and a route. A **projection step** (plan to `CELL` rows, one column per plan cell, depth from the island's taper, stairs and water by rule) is not written. It is the next tool.
- No terrain relief inside an island (each island is a flat top at its `top_z`); no rock detail on the underside; no interiors (the library rooms); no tree placement; no building shapes beyond a footprint, wall height and roof kind; the spires' exact positions.
- No art: sheets and skins are the visual book's two-skin plan (RPG Maker sheet or generated sheet).

## 5. How it plugs into the Centroid work (DOC)

- **Master blueprint for the manager (centroid-live):** the 2026-11-06 plan says a manager "is shown the master blueprint and breaks it into jobs for employees". This file is that kind of blueprint: islands become `dig`/`haul`/`place` job lists, zones become plant and tree jobs, buildings become `craft`/`place` jobs, the route is the order a visitor sees it. The clock tower the owner mentioned is NOT in this picture; the cathedral's central spire is the natural place (OPEN).
- **centroid-test (harness-built):** build in stages, smallest first. Suggested order for Grok: (1) the blueprint checker as a C op and a case (the script above is the spec: it must fail on the three breakages); (2) the projection step to `CELL` rows for ONE small island and the chapel (hundreds of cells, not tens of thousands); (3) a phase table where a crew builds that piece from the blueprint and the placed columns must equal the projection; (4) then the next building. Never start with the whole skyland.
- **Scale warning (DOC, not measured):** the whole landmass is about 30,000 block columns; the visual book's first chunk was 156 cells. Whether the board engine and the weak CPU can draw it at all is OPEN; the blueprint has a `SCALE` row so a coarser cell can be chosen (for example 8 blocks per cell).

## 6. Open decisions for the owner

1. **Scale:** 4 blocks per cell (as drafted) or coarser to fit the engine? 2. **Names:** all names in the file are generic and ours; pick real Centroid names later (the lore skill's legal flags apply). 3. **The clock tower:** put one on the cathedral's central spire, or elsewhere? 4. **The small sky islands:** are seven enough, are the altitudes right, should any carry one tree or a ruin later? 5. **Fidelity:** how close to the picture must the outlines be (the current ones are ellipses), and is a hand-edited outline file wanted?
6. **Whether to keep or publish the picture** (third-party content; I recommend keeping it local and out of the repo and XO).

## 7. External references

Repo (`x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/`): `@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl` (this plan), `@.apps/centroid-hq/data/court.pdl` (the existing 156-cell chunk). BOOK: `16.game/CENTROID-VISUAL-BOOK-2026-11-06.md` (chunk and skin plan), `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md` (test, live, the manager and the blueprint idea), `13.agent-coms/GROK/HANDOFF-2026-11-10-DF-MINECRAFT-EVENTS-META-HARNESSES-AND-CENTROID.md` (the lane this feeds). The source picture: `/home/no/Downloads/goal-centroid-skyland.jpeg` (local only; not copied).
