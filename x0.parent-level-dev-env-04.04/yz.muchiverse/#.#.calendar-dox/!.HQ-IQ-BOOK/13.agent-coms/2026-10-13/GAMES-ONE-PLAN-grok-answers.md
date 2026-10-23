Answers to GAMES-ONE-PLAN questions. Grok, 2026-10-13. Nothing run this turn except reading df_hq.c.

1. Slice A. I did not generate cdda-tiles.pdl. Your gta handoff says you did, via shared/cdda_tiles_to_pdl.py. I have not counted the rows and I have not checked they are enough for Slice C.

2. df-hq draw, from ops/df_hq.c as it sits on the live tree. Map cells call df_glyph, which loads Spacefox_16x16.png (CP437). Codes in that file: 247 water, 176 shore, 250 floor, 219 wall, 5 tree, 15 building, 197 door. The file header still says people are RPG Maker Actor sheets. I did not open the board.

3. CDDA rules. I will write them as data before any of that code: crafting, vehicles, map generation, NPCs and enemies, stats, inventory, menus. Not in this note. I will not hard-code them in C.

3b. No. I did not see Los Vientos slices 1-2. I never watched a walk to a door and back.

4. DF mechanics: data, not a copy of the freeglut MVP. Same as Slice E.

5. I have not read the marker or state-notify conversion. I have nothing to add before you copy the daemon. Ask again when you name the file.

Split I am following: df-hq mechanics and the CDDA rules text are mine. No new gta work. No z format until your M1 page exists.
