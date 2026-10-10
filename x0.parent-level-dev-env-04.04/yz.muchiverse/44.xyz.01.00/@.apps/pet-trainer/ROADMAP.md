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

## Done since the first roadmap (2026-10-09 evening)
- Platforms (jump on bed/desk/sofa/tv stand/chimney), taught verbs `climb` and `go`; rooftop reached by two stairs-up doors, overlooking the village with a flag on each pet's house.
- Room creation backend: `build_room` event (built_*.pdl overlay, door teleport events, refusals), nav buttons, pal harness `pet_build` 15/15.
- Phones: every pet has a house phone on its own phone server (state/server); `exchange`/`call`; talking to a village pet swaps numbers; phone chats show in chat as `ph <number> Name: text`; harness `pet_phones` 11/11.
- Chat is its own right-hand pane (canvas-overlay-right) that minimises to the bottom bar and pops back; left sidebar is free for game menus.
- Training: harness `pet_train` (48/48) teaches all six pets each new verb through the real concept-bank feedback; run it after every feature.
- CPU: window animation on an 800 ms beat, idle at half rate, clock label on a 3 s beat (renderer ~11-16%).

## Added by the owner, not built yet
- **Out into the town**: pets leave the house into the village/city, **build buildings in the city**, explore, and **broaden their maps** on the map screen (fog of war; the map grows as they explore). Town map becomes per-world state (overlay on world_map.txt), the mini map gets a town level.
- **Contacts**: pets make contacts in their phones while out (talking to someone swaps numbers: DONE for the village pets); later: phones ring for jobs, borough invitations, likes.
- Only six pets exist for now; more come from reproduction (capped, owner-approved).
- **Fullscreen**: the pet screen should behave like pc-hq's so it can go fullscreen (class user-resizable + canvas fills; scene zoom and the renderer's canvas_view.txt are in, the in-panel canvas fill for resizable windows is still to wire).
- Pet needs pacing: at 1 game minute per second the pets get hungry/dirty within ~15 real minutes; tune tick weights / self-care.

## Now (owner 2026-10-09): town + learning - see TOWN-ECONOMY-AND-LEARNING-DESIGN.md
1. Town overlay + build_building + doors as teleports; 2. place memory + proximity eating; 3. stores, jobs, buying; 4. pet-to-pet trade; 5. exploration fog + map growth; 6. tech tree + "teach me" provider ladder + lesson cards (human review, never auto-promote); 7. pacing. Train the pets (pet_train) after every step.

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

- **Chain economy** (`CHAIN-ECONOMY-DESIGN.md`): pet wallets on a new high-difficulty chain, miners/rooms/maintenance, purse = chain balance, NFTs (pets and grown food), banks/governments from the old chain with leases swept back at game end, Exchange HQ (rates averaged into preferred value, fee settings editable in the window) and Auction HQ, storage nodes/staking, businesses/stocks/dividends. The soul: pets drive the relay, learn by RL with the house harnesses, modify their own harnesses, watch human input.

- **Level builder + INT camera** (`LEVEL-BUILDER-AND-INT-CAMERA-DESIGN.md`, 2026-10-10): rooms become desks in a pc-hq book `pet_home` (Doom/TSOTS style: glyph grid, atlas, extrusion, events), a toggleable Build bar to place furniture/items in empty rooms (ASIC miners included), and INT on finally driving the 2D/3D camera (POV 1-4) through the board renderer. Design only; 6 questions for the owner at the end.

- **WSR economy + pets** (`WSR-ECONOMY-AND-PETS-EXPLORATION.md`, 2026-10-10): ran the WSR engine in scratch (ticks, order book, 59 quoting participants, 17 fills, registry, ledger); AI tiers (weighted real, rl stub, llm, human); no bondholders exist; plan: pets as holder pieces, money bridge via the exchange, bond ledger, RL on paper money, XOD tournaments.
