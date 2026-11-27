# Heavy sprint plan: a shippable playable tutorial, then vehicles with doors, axles, engines and planes (plan, 2026-11-27)

Labels: **SAID** owner 2026-11-27; **OBS** seen in the repo or a run; **DOC** my reasoning, not run. Nothing in sections 3 to 6 is built.
Companions: `RMMC-TUTORIAL-FIRST-PERSON-DRIVE-HANDOFF-AND-ROADMAP-2026-11-26.md` (what exists), `NU-CTRL-NEW-CONTROLS-PARADIGM-JOYSTICK-FIRST-2026-11-26.md` (pad), `BOX-LIMB-RIG-FOR-WALKING-CHARACTERS-2026-11-25.md` (standard animation set), `VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md` (force models, gyms, owner: fun ruleset first), `VEHICLE-CRAFTING-AND-MODDING-DESIGN-2026-11-05.md` (parts, chop shops), `GTA-HQ-DESIGN.md` (island), decision log D143, D144.

## 1. What the owner asked (SAID, 2026-11-27)
"Push, then a really heavy duty sprint, lots of tokens left": what task would give the most momentum toward AAA-shippable playable tutorials, proof of mechanics, Minecraft options, crafting, fixing the inventories and displays; and "going back to the GTA with these new skills": cars (axles, engines), planes, doors, entry, realistic functionality as soon as possible. Document it.

## 2. Constraints the sprint must respect (OBS, from this week)
- Two thermal-correlated hard freezes in 24 hours (2026-11-25 14:02, 2026-11-26 13:10); the guard read 90 C at 00:32 today with the house at 1 % (`hq-cpu-safety.md` section 11). **A heavy sprint is heavy on tokens, not on the CPU**: reading, writing and reasoning cost the machine nothing; compiles, harness runs, ffmpeg and the 3D engine do. Rules: one heavy local job at a time through `cpu_gate --wait`, read the die temperature first (never start at 75 C or more), offload builds and long runs to debil (`debil@10.0.0.16`) or the Mac, and keep parallel agents to reading, designing and writing, not building.
- Every rule is an op (D92); the ledger is the only authority; relay keys, never xdotool; commit by pathspec; push only when asked.

## 3. Recommended sprint: "The first twenty minutes" (one spine, five tracks)
**Why one spine.** Most of the open work (inventory display, crafting, tutorial, pad, vehicles) fails the same test: a stranger cannot tell what to do or what happened. The spine is a harness case that plays the first twenty minutes **as a stranger who may only use what the screen says**, from a fresh ledger to "built and entered a vehicle". Every track below is a step that makes that case pass further. Measured by the case's furthest passing step, which only moves when a human could do it.
| Track | Goal (what a player gets) | Proof that can fail | Suits |
|---|---|---|---|
| **A. Stranger case** | the spine itself: scripted play from the bar's wording only, fails the first time a label, key or help row lies | case `rmmc_stranger` (new), mutation: rename a key, it must fail at that step | Opus: needs judgement about what a screen "says" |
| **B. Inventory and displays** | one bag view that is always true (hotbar, bag, held item, counts, stack limits), a crafting grid with recipes from rows, the HUD default decided | the existing `rmmc_help`/`rmmc_verbs` plus a display-vs-ledger case (what is drawn equals what `vox_op bag` derives) | Sonnet or Opus; mostly data and renderer rows |
| **C. Crafting and Minecraft options** | recipe table in `.pdl`, `craft` as an op with a refusal that says why, creative/survival as the existing mode op (D142), a world-options page (seed, size, mode) | craft refused with the wording on the bar (closes the open "place/craft refused, reason unproven"); a recipe with a missing input must refuse | Opus |
| **D. Pad as intents** | `pad_map_op` generating the map from a style profile and the book's `keybinds.pdl` (D143), `cycle` op, Start enters In, L/R cycle the hotbar | case `joy_map` grows: every intent in the profile resolves to a key the book handles | Sonnet |
| **E. Vehicles, first cut** | section 4 to 6: enter, drive, exit a car on a flat gym map, doors that open | section 6 invariants | Fable or Opus (design heavy, then rows) |
Order: A first (it tells the others what to fix), then B and C together, D beside them, E last because it needs B's seats/bag and D's pad intents (`interact`, `aim`, `crouch`).
**Honest scope:** tracks A to D are a sprint; E is the next sprint's spine if A to D take the time. Do not start E's physics before the stranger case reaches "craft a tool".

## 4. Vehicles: the model (DOC; extends, does not replace, the 2026-11-05 docs)
**One idea:** a vehicle is an **entity made of part rows with seats and doors**, driven by the same verbs a character uses. Nothing about a car is code; the code is the verbs (`enter`, `exit`, `door`, `throttle`, `steer`, `brake`) and one integrator op (`vehicle_step`, already the plan in the physics doc section 7).
- **Parts as rows** (new columns on the existing part catalog, `MATERIAL` from the physics doc): `PART | id | kind | material | volume | mount | ...` with kinds `chassis`, `axle`, `wheel`, `engine`, `gearbox`, `tank`, `seat`, `door`, `wing`, `rotor`, `hull`.
- **Axle:** `AXLE | id | position | steered | driven | track_width | max_load`. Wheels attach to an axle's two ends. Driven axles share engine torque (open differential in the fun ruleset: equal split; a limited-slip row later). Steered axles turn by the steer input times a max angle row.
- **Engine:** `ENGINE | id | source | max_power | peak_rpm | torque_curve | efficiency | mass`. A torque curve is a short row list (rpm, torque), the gearbox rows give ratios, the wheel radius turns it into force. Fuel is a gauge in the fun ruleset (burn rate from power); `source` (fuel, battery, reactor) is the physics doc's energy row, so "electric" and "fusion" are rows, not code.
- **Derived numbers, never stored:** mass = sum(density x volume) + fuel + cargo; top speed = where drive force meets drag; acceleration from power and mass; these appear on a **weigh station** and the vehicle's own instrument panel. One authority per fact: they come from the parts.
- **Seats and doors:** `SEAT | id | role (driver, passenger, gunner) | position | door` and `DOOR | id | seat | hinge | open_angle | locked`. A door is a part with state; `enter` is refused while its door is closed or locked, with the reason on the bar ("the door is locked").
- **Planes and boats:** the same model with other force rows (wing lift, rotor thrust, hull buoyancy). Planes come after cars because stability is the fiddly part (physics doc section 7: L effort). Fun ruleset first (owner 2026-11-05): lift and drag as simple curves, stall speed a derived number, a "forgiving" flag for beginners.
- **Realistic mode** stays a ruleset switch: same rows, the real relations from the physics doc section 3 (the `MATERIAL`, torque and Isp columns are already there so no schema change).

## 5. Entry, exit and possession (DOC; ties to existing rules)
- `enter <vehicle> <seat>` and `exit` are **ledger verbs** (ops, D92). Entering the driver seat = **possession**: the owner's standing rule (xelector possession is the controlling look, no special hero) already says any possession is the controlling look, so the camera and the pad then drive the vehicle with no new input system. Exit releases possession and places the character at the door cell (refused when blocked).
- The pad's intents (D143) are the vehicle's controls: stick = steer/throttle, `primary` = accelerate, `secondary` = brake/reverse, `interact` = enter/exit/door, `cycle` = next camera. A driving style profile and an on-foot style profile are two profiles over the same intents, switched by the possession row.
- **Animations are the standard set** (`BOX-LIMB-RIG` standard animation list): `enter_car`, `exit_car`, `sit_driver`, `open_door` are clips chained ledger row -> pose -> state -> anim like every other, one clip per verb, shared by every vehicle (trunk), a vehicle may override (branch, D87).
- First person from a seat: the camera sits at the seat part's position; the existing first-person rule (key `p`) applies.

## 6. Proof of mechanics (DOC; what a harness can check without a human)
All run in a scratch house on a flat gym map, headless, deterministic fixed step. Each needs a mutation that makes it fail.
1. **Mass comes from parts:** a car's mass equals the sum of its part masses (change one part's density: mass moves by exactly that).
2. **Top speed follows the parts:** at full throttle on the flat, speed converges to the value where drive force equals drag; a weaker engine row lowers it, a taller gear raises it.
3. **Axle drive:** only driven axles propel; a vehicle with no driven axle does not move on throttle.
4. **Steering:** a steered axle at max angle gives a turn radius within tolerance of wheelbase over tan(angle).
5. **Braking distance** on dry vs wet friction rows ordered correctly (wet longer).
6. **Door gate:** `enter` is refused with the door closed or locked, accepted when open, and the refusal wording is on the bar.
7. **Possession round trip:** enter, drive 10 cells, exit: character is at the door cell, the vehicle is where it stopped, the ledger holds the rows, and replay from the ledger reproduces both positions.
8. **Plane (later):** below stall speed no climb; above it climb follows the lift row.
Handling "feel" stays a human judgement (the owner drives it); the invariants are mechanical.

## 7. The first cut of vehicles (smallest slice that is a real car)
Flat gym map (the physics doc's test track, reduced to one straight and one skidpad) + one car of 1 chassis, 2 axles, 4 wheels, 1 engine, 2 seats, 2 doors + verbs `enter exit door throttle steer brake` + `vehicle_step` for the car domain only + invariants 1 to 3 and 6 to 7. Planes, boats, damage, crafting a car and the chop shop follow the 2026-11-05 docs' own slice order. Effort (DOC, not measured): about 3 to 5 sessions to create, 3 to validate, mostly data rows once `vehicle_step` exists.

## 8. Open decisions for the owner
1. Spine = "the first twenty minutes" (recommended) or go straight to vehicles?
2. Fun-only physics first (owner 2026-11-05, kept) or a thin realistic mode in the first cut?
3. Do doors open by the `interact` button (recommended) or by walking into them?
4. Do vehicles use the same hotbar/bag for cargo, or a separate trunk inventory row set?
5. Which model does which track (section 3 "Suits" is a suggestion): the owner chooses what to spend.

## 9. External references
Repo `https://github.com/tearitco/TEARIT-HQ.git`, branch `claude`. Decision log D143 (pad intents), D144 (this plan). Existing GTA app folder `44.xyz.01.00/@.apps/gta-hq/` (OBS: board and catalog exist; no vehicle rows read). rmmc: `@.apps/rmmc/`, rigs `@.apps/rmmc/rigs/biped_anim.pdl`. CPU: `03-pitfalls/hq-cpu-safety.md` section 11.
