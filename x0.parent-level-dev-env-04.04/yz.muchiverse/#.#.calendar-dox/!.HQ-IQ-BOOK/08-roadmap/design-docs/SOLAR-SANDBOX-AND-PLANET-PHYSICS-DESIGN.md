# Solar sandbox and planet physics (design + first build spec)

Status: DESIGN written 2026-10-07 by claude; step 1 (the sandbox desk) is handed to a build agent in the `claude-alpha` branch, the physics layer is design only.
Owner (2026-10-07): a whole solar system on one page, each planet on its own page; click a planet to go in, leave to go back out to travel; the next 16 pages could be another system; emoji entities with menus and teleports as a sandbox of scale; later real solar-system data, a planet picture next to "time", random generated planets per book (a ball of voxels, earthlike, tweakable in settings), "quantum tunneling" between systems; **"build gravity and physics on the planet, and in game derive from this, implying space travel, weather, thermodynamics, physical chemistry etc."** The wormhole/inode "book of planets" idea is the same structure (section 5).

## 1. What exists
- `@.apps/piececraft-xyz/ops/pc_clock_daemon.c`: sun and moon are real pieces (`pieces/sun_01`, `moon_01`) whose positions come from one shared elliptical-orbit function around the planet (ORBIT_A/B/HEIGHT constants); the renderer just reads positions. `xyz-ngn-plan.md` also names Mars as an orbiting body. Orbiting bodies as entities is built.
- Map transfer with an access list (`game.pdl` `MAP` rows) and entity right-click menus (`meta.pdl` `METHOD` lines): the doors.
- WSR has `weather_update` and the toy shows `temperature` (not read in detail; reuse candidates).
- Not found: any gravity, atmosphere, thermodynamics or chemistry model in the house.

## 2. Structure: a tree of pages, doors as rows
- **System page** (hub): emoji bodies (sun, planets, moons) at a toy scale, each an entity with a menu: `Enter <planet>`, `Teleport to <other>`. Optional orbit motion via the existing orbit function.
- **Planet page**: the planet's surface; a `Leave orbit` row returns to the system page. Next pages can be that planet's moon, regions, or the next system's hub.
- A **system file** `system.pdl`: `STAR | name | mass | radius | temp | ...`, `BODY | name | parent | page | a | e | mass | radius | rotation_period | albedo | atmosphere=...`. Another system = another file; a hop between systems = a travel row with a bigger cost (the "wormhole" is that row with cost near zero). A "book of planets" = a page of `PLANET | name | path` pointer rows (like desk reference rows, never copies).
- Travel rows: `TRAVEL | from | to | delta_v | duration`.

## 3. The physics layer: derived, never stored
**Stored per body (a few numbers, seedable, editable in settings):** mass, radius, semi-major axis, rotation period, axial tilt, albedo, atmosphere (surface pressure and mean molar mass, greenhouse strength), water fraction. **Everything below is computed from those by pure functions** (the derived-mirror rule: nothing hand-edited, always recomputable; Earth, Moon and Mars are test vectors):
| derived | formula (standard, simplified) | game effect |
|---|---|---|
| surface gravity | g = G M / R^2 | jump height, carry weight, fall damage, projectile range, item weight |
| escape velocity | v = sqrt(2 G M / R) | launch cost |
| orbital period | T = 2 pi sqrt(a^3 / (G M_star)) | year length, calendar, orbit animation speed |
| day length | rotation period | day/night cycle, the place clock phases |
| equilibrium temperature | T = T_star sqrt(R_star / 2a) (1 - albedo)^(1/4), plus a greenhouse offset | climate band, crop and need tuning (cold, heat) |
| insolation by latitude/season | flux x cos(zenith), tilt over the year | seasons, plant growth, solar power |
| scale height and pressure | H = k T / (m g); p(h) = p0 exp(-h/H) | altitude effects, breathable air, drag |
| water phase / boiling point | phase diagram; boiling point falls with pressure | thirst/cooking/farming rules, ice vs water vs vapor |
| weather | latitude temperature gradient + rotation (Coriolis) drive wind bands; humidity from temperature and water fraction | rain, storms, drought events (feeds the existing weather op) |
| space travel cost | Hohmann transfer delta-v between orbits; fuel from the rocket equation | `TRAVEL` cost and duration; a real reason Mars costs more than the Moon |
| thermodynamics | heat flow, ideal gas law PV = nRT | insulation, clothing, heating needs; links to the clothes commodity |
| physical chemistry | Arrhenius rate k = A exp(-Ea/RT); phase and solubility by T and p | cooking, crafting, spoilage and fermentation rates |
**Realism is a tunable:** each effect has a strength joint (0 = off, 1 = real) in a `physics_tunables.pdl`, so the sandbox can start with gravity only and add the rest one by one, and so a "game physics" mode stays playable.

## 4. How the game uses it (the implication chain)
planet parameters -> derived values -> **events read derived values** (a jump event reads g; a rocket launch reads escape velocity and fuel; a farm tick reads temperature, insolation and water; weather events read the wind/humidity bands; needs read temperature and air). No gameplay rule hardcodes a planet: change the planet's rows and every effect follows. This is the "everything is an event" rule applied to physics, and it is also what lets the DSR economy, survival gathering (`SURVIVAL-GATHERING-FARMING-MINING-WATER-HANDOFF.md`) and mutaclysm-as-events share one world.

## 5. Code and data rules
Pure formulas = **one text-included `.c`** (no header+link); stateful ticking (weather state, orbit state) = a separate op reading ledgers by cursor; seeds make a generated planet reproducible; every formula gets a pal-harness vector set (Earth g = 9.81 m/s^2, escape 11.2 km/s, Moon g = 1.62, Mars g = 3.71 and escape 5.0 km/s, Earth equilibrium temperature about 255 K without greenhouse, boiling point drops with altitude) and a case that is shown to fail on a wrong constant. Generated planets (random "ball of voxels", earthlike, settings tweak, a picture beside "time", real solar-system data later, switching systems) are later layers on the same rows; the picture is a render of the planet file.

## 6. Build order
1. **Sandbox desk** (building now, alpha): `solar-sandbox` with a system page and Earth, Moon, Mars as emoji entities with Enter/Leave menus and a harness over the transfers.
2. `system.pdl` / `BODY` rows and the pure derivation include (gravity, escape velocity, orbital period, day length) + harness vectors; show derived values in an entity menu or info row.
3. Climate: equilibrium temperature, insolation, pressure and boiling point; feed `temperature` and the needs design.
4. Travel cost from delta-v; launch/transfer events using it.
5. Weather bands and events; thermodynamics and chemistry rates for crafting/farming.
6. Random planet generator + settings + picture; real solar-system data; second system; inter-system travel.

## 7. Open questions
1. First system: Earth, Moon, Mars (building this), or all eight planets?
2. Realism default for the sandbox: gravity only first (recommended) or everything at once?
3. Scale: true proportions (unplayable) or a toy scale with true ratios shown in the info row?
4. Does travel take game time, or instant first?
