# Pet trainer roadmap (kept current; written 2026-10-09 from the owner's requests)

Status: DONE = built and shown working (evidence in the commit message) / PARTIAL / TODO. Designs: CAMERA-DESIGN.md (INT camera/3D), WORLD-DESIGN.md (house, property, society, planet).

## Done
- Six pets from the RPG Maker DB (party), room / manage / world pages, trainer you walk in the village, header (INT, book:page, time, Menu, Player), small hotbar, bottom bar, CPU safe.
- Autonomy: wander, hop, speak (weighted words + replies, TTS per species), per-species hum, react to a shake; clock fires events (time.pdl: need_tick, meal_call, bedtime, wake, day_tick).
- Time: Time tab, Start/Stop run the pet clock (own livedesk clock), speeds, skips.
- Rooms: bedroom (big window, bed, computer), living room, garden; doors are teleport events (rooms.pdl); pet walks through auto doors.
- Day and night in outdoor views, event driven (phase_dawn/day/dusk/night -> daylight.txt).
- Key 5 = debug house mini map (home.pdl). Chat = cli_io rows=3; window is class=managed so Mutter/XWayland gives it focus (owner confirmed working 2026-10-09).
- Mic talk-back path (record -> offline vosk STT -> chat) + pal harness `pet_stt_tts` 34/34. HELD: owner asked to wait until proven with a real voice.

## Next (in this order)
1. **Platforms**: the pet jumps ON the bed and the computer desk (rooms.pdl PLATFORM rows) and walks along them; never breaks anything. TODO.
2. **build_room + map buttons**: Build verb adds a cell next to an existing one (home.pdl), door rows, room kind; menu buttons for the map; mini map as a real page, not only key 5. TODO.
3. **Chat as its own right panel** (frees the sidebar for build/map buttons). Needs the tab bar and footer outside the flex row (renderer/template) or a canvas overlay. PARTIAL (tried flex row: blank window, reverted).
4. **Farming/outdoor items in rooms**: crops on the clock (growth per day), harvest gives items. TODO.
5. **3D + camera in INT mode** (pc-hq POV 1-4, yaw/pitch/height; rooms stay side-on): scene data -> 2D from it -> 3D via board-viewer op. CAMERA-DESIGN.md. TODO (keys stored in camera.st today, nothing draws from them).
6. **Neighbours / borough**: six pets share the property map, LIKE ledger, visiting by door events. TODO.
7. **Society**: jobs ledger, trade, pet-to-pet talk, tunnels and towers (z), reproduction (cap + owner approval). WORLD-DESIGN.md. TODO.
8. **Voxel planet**: property grid becomes chunks of a voxel world, explored with the pc-hq camera. TODO.

## Also open
- Village as a real pc-hq board book (`pet_village`) with RPG Maker tiles + sprite animation (priority shift 2026-10-08, not started).
- Layout studio: `layout_op` + editor window (palette, drag/drop, play preview); layout_flow exists.
- Drag and drop of entities into/out of the game, onto the desktop and into other levels (documented only).
- Mic: real-voice test, then re-enable by default; larger vosk model if the small one is poor.
- Real-mouse test of the Menu/Time/Player dropdowns, hotbar slots and bottom bar; Esc-then-Back leaving Interact armed once; hotbar inv_use index base.
- Other windows that type (hotbar etc.) probably need `class="managed"` too (same XWayland focus trap).
- Verify live: pet speaking into chat and humming, react-to-shake, TTS of clock lines.
- Grok: regenerate E1M1 cells.txt / E1M2 map.txt (lost in the 2026-10-09 sync).
- Window width is renderer-limited (5 tabs set it).
