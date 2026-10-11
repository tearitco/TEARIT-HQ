# Pet house to village: a top-down pet-raising game (Pokemon / Monster Rancher / Digimon, Game Boy Advance feel)

2026-10-09, claude, from the owner's direction. **Design plus what exists; the village is not built.** Read `@.apps/pet-house/README.md` first.

## 1. Owner asks (2026-10-09)
1. The pet house menu options hide behind a tab or hotbar key, as one of the house's dynamic menus fed from `.pdl` / layouts (not a fixed sidebar).
2. The house gets a **door**. Outside is a **village**: top-down, several houses, NPC creatures like the pet that it can chat with.
3. Style: GBA Pokemon / Monster Rancher / Digimon.

## 2. What exists today (verified on screen or by scratch run, 2026-10-09)
- `@.apps/pet-house/`: care verbs, evolution, a chat the master can teach (`teach words = verb item`, good/bad), touch, weighted self-care at level 4, preferences and priorities learned through `joint_tune` (ledger rows), gravity physics (simulated), a room window (fridge, bed, bath) with the pet drawn by `pet_scene`.
- `layout-studio`: `layout_check`, five templates, `pet_gen` (seeded pets with arms, legs, 9 expressions, OBJ pose frames + sprite.csv frames).
- Known gaps found while building (do not skip): the in-window chat field showed but typed text did not arrive through the relay in my tests (the action contract is `<args> <package_dir> <house_root> <typed text>`, so the verb is `chat_input`); the window's registry position did not follow a window move, so gravity-on-move is only simulated; a canvas swallows sibling rows (put controls in the sidebar); the pet window does not yet animate smoothly.

## 3. Hidden menu (ask 1)
Why this way: the house already has one pattern for menus that are data, not layout: a manager publishes `n_<menu>` and `<menu>_<i>_label` rows and the layout `<repeat>`s them (the pc-hq Desk menu with search); a hotbar slot or a tab toggles it (`hotbar_toggle.sh`, `ov-slide-x` overlay rows, see the layout-studio sandbox `test-menu`). Reusing it means: the menu rows live in `pet-house/menu.pdl` (MENU | id | label | verb | arg | needs_level), a weak agent adds an option by adding a row, `layout_check` validates the layout, and nothing is drawn until the key is pressed.
Build: (a) `menu.pdl` + `pet_event.sh menu` publishing `n_menu`/`menu_<i>_*` into ui.txt (rows with `needs_level` above the pet's level are omitted: abilities unlock menu entries); (b) window: one tab/hotbar toggle row, the `<repeat>` list as an overlay row; (c) PNG with the menu hidden and shown.

## 4. The village (asks 2 and 3)
It is a pc-hq book, the same machinery as Doom and TSOTS:
- Book `pet_village`: desks `village` (outdoors, about 40x30 tiles), `house_player` (the room we have), `house_<n>` interiors, `shop`, later `arena`.
- **Door** = an `on-touch` event on the door glyph: verb `go <desk> <x> <y>` (one `CONFIRM_SET_DESK` line; the inbox keeps one line). Entering the house runs `pet_event.sh status` so the room window shows the same pet.
- **NPC creatures** = events on the village desk, each with a `pet_gen` seed, its own `lexicon.pdl`, its own state dir under `npcs/<id>/`. Talking = the same `chat` verb with the NPC's lexicon; a taught word can be learned from an NPC (copy a LEX row at lower weight; the player's pet is the learner). NPC wander = a weighted move event on the day tick.
- Look: 16x16 tile atlas (`cells.txt` + `cells.rgba`, the TSOTS format), 2D top-down view, creatures as the 16x24 frames `pet_gen` already writes (walk animation exists). GBA feel = small tiles, 240x160 aspect window, 4-colour-ish palette option later.
- Needs from the lanes: Grok writes the village map, house interiors and events (data lane); claude does the renderer pieces (door transfer, hide-dead, camera follow of the pet), the pet window and the verbs.
- Do not build yet: battles, evolution trees across species, breeding. They sit on the same weights/feedback loop.

## 5. Order
1. Fix the chat input and window-position tracking (proof by PNG and a state diff).
2. `menu.pdl` hidden menu (section 3).
3. Door + `house_player` as a pc-hq desk, `village` with 3 houses and 3 NPCs (Grok's data), chat with an NPC.
4. NPC lexicon exchange, wander, the pet follows the camera.
