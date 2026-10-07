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
**CORRECTED by the owner, see section 8: each row of the table below is a swappable event-page node, not a fixed formula. The formulas are only the default contents of those nodes.**

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

## 8. Owner correction (2026-10-07): gravity is a node, not a built-in
"Gravity is just a node that can be tuned, swapped out, etc., an event page, related to other event pages (orbit, whatever)." So physics is **not a hardwired formula layer**; it is **event pages wired together**, which is the house's own rule (NIGHT 20/26: everything is an event; as few hardcoded mechanics as possible).
- **A physics node = an event page** with declared **inputs**, **outputs** and an **implementation**: `NODE | id | kind=gravity | in=mass,radius | out=g | impl=newton`. The formulas in section 3 become the **default implementations** (`impl=newton`, `impl=kepler`, `impl=stefan_boltzmann`, ...), shipped as ordinary pages; none is special.
- **Swappable and tunable:** an `impl` can be replaced by `constant` (g = 9.81 forever), a `scaled` wrapper (multiply by a tunable), a table, a script page, or a model-free custom rule. Tunables are named joints in a `.pdl`, editable in an X11-HQ editor (the tomom-hq style: bounded steps, audit rows, undo). Changing the node changes everything downstream, with no code change.
- **Related to other pages:** nodes link as rows (`LINK | orbit.period -> calendar.year`, `LINK | gravity.g -> player.jump`, `LINK | gravity.g -> atmosphere.scale_height`, `LINK | temperature -> weather.humidity`). The house already has the primitive: **"Call Common Event"** (`call_event_op`, an event page that runs another page by name and trigger). A downstream page calls or reads an upstream node's output; there is no global physics engine.
- **Scope of an override:** resolution order **page > planet > system > house default**, so one planet can have a weird gravity page, or the sandbox can run "gravity off" while the rest stays real, or a person can swap in a different orbit model per system.
- **Derived-not-stored still holds for outputs:** a node's outputs are recomputed (or cached from the ledger) and never hand-edited; what is stored is the **node wiring and tunables** and the body numbers.
- **Cost rule:** nodes must be cheap and idempotent (pure in their inputs) because real-time mode (see `REAL-3D-PLANET-EXPLORATION-AND-REALTIME-MODE-DESIGN.md`) may read gravity every tick; cache an output until an input changes (an input-changed marker, not mtime).
- **Updated build order step 2:** define the `NODE` / `LINK` row formats and a node-evaluation op (reads rows, runs the page for the chosen impl, writes the output row), then ship `newton` gravity and `kepler` orbit as the first two default pages, with harness vectors (Earth, Moon, Mars) **and** cases that swap the impl and prove downstream values change and a `constant` swap is honored.
- **Open:** (a) is a node evaluated by a compiled op (fast, my recommendation for the built-in impls) or only as pal event pages (slower, fully swappable)? Likely both: built-in ops by default, a page can override; (b) wiring format: separate `LINK` rows (proposed) or inline `in=` references only?

## Build report: step 1 (claude, alpha, 2026-10-07)
**Built** (tracked template files in `&.widgits/solar-sandbox/`, see its README for the by-hand install into one session): four desks `solar-system`, `solar-earth`, `solar-moon`, `solar-mars` (`desks/*.pdl`, `DESK` rows); emoji entities `sol` 🌞, `earth` 🌍, `moon` 🌕, `mars` 🔴 on the system page with rows `Enter <name>` and `Teleport to <other>`; per body page a `<body>_surface` entity (rows `Leave orbit` + `Teleport to <other>`) and a shared `leave_orbit` 🚀 entity; `game.pdl` with `MAP` rows for the 4 pages. Every row is a `METHOD` action: an inline `sh -c` that resolves user and session, then runs the existing `events-hq/ops/+x/mr_transfer_desk.+x` (no second teleport). `menu.chtpm` files were generated with the existing `meta_to_menu_chtpm.py`.
**Harness:** `harness/solar_sandbox.pal` + `cases/solar_sandbox.pdl`, run against a scratch house with the real op. Verdict: `VERDICT|PASS|passed=41|failed=0`. Covers Enter Earth, Leave orbit, Teleport Earth->Mars, Moon->Earth, rocket leave, access list lists the 4 pages, and the negative: with `solar-moon` removed from `game.pdl` (play mode) `Enter Moon` exits 3 with `refused: map 'solar-moon' is not available`, the player stays and the refusal is ledgered; a nonexistent page is also refused. A temp copy with two wrong expectations gave `VERDICT|FAIL` (2 failures), then was deleted.
**Not verified (no GUI):** that right-click shows the rows and that clicking one runs them (the harness runs the exact action string the way `khtpm_entity.c` composes it, `sh -c "<action> '<pkg>' '<house>' &"`, but synchronously); that the desk really respawns the entities (scratch has no `khtpm_entity.+x`, so spawn is skipped); that emoji-only entities render without a `sprite.csv` (the sample `cat` has one; `cursword` has none); the real `sessions/session.pdl` `active_session` lookup (the fallback is `s1`).
**Deviations / limits:** no desk or `game.pdl` is installed in any user session (`xyzfs/users` is untracked user data); `DESK` rows use the documented 9-column form (`x_px | y_px | cell_x | cell_y | glyph | n`), not x/y/z/w; bodies are static (the orbit function in `pc_clock_daemon.c` was not reused); no `cursword` row on the sandbox desks; no behavior-bank seed for the new harness; a refusal in play mode is only visible as the op's exit code and ledger line (the menu row discards it, per the design's "the event decides what the player sees"); the house cannot yet show "only reachable maps" in a menu, so unlisted targets are offered but refused.

## Build report: step 2, physics nodes (claude, alpha, 2026-10-07)
**Built** in `&.widgits/physics-nodes/`: README with the row formats (`CONST`, `BODY`, `NODE`, `LINK`, `TUNABLE`, `OVERRIDE`), `system.pdl` (Sun, Earth, Moon, Mars, real SI values with sources), `physics_tunables.pdl`, and the compiled op `phys_node_eval` (inline formulas, no shared header: only one op uses them). Impls: gravity `newton` / `scaled` / `constant`, orbit `kepler`, `escape_velocity` `standard` (sqrt(2 g R), linked to `gravity.g`, so it equals sqrt(2GM/R) under newton and follows any swap). Pull model over LINK rows; output rows `OUT | body.node | key | value | impl | h=<fnv hash of impl, inputs, tunables, G>` in an append-only `data/phys_out.txt`; unchanged hash appends nothing. Override order page > planet > system > default (page passed as an optional 4th argument).
**Harness:** `harness/phys_nodes.pal` + `cases/phys_nodes.pdl`: `VERDICT|PASS|passed=31|failed=0`. Earth g 9.82, Moon 1.62, Mars 3.73 (mean radius; 3.71 is the equatorial figure), Earth escape 11.19 km/s, Mars 5.03, Earth year 365.26 d; Earth gravity swapped to `constant` gives escape 7.98 km/s; scaled factor 0.5 then 0.25 recomputes Mars escape; page override beats planet; unknown impl exits 2 with no row and a byte-identical ledger; rerun with unchanged inputs appends 0 rows. A temp copy with two wrong expectations gave `VERDICT|FAIL` (2 failures), then was deleted.
**Limits:** values are checked by substring on `%.6g` output (a prefix is the tolerance); step 3 nodes (temperature, pressure, travel delta-v) and a script-page impl are not built; `day length` is a body property, not a node; the op is not yet called from any entity menu or the game.
