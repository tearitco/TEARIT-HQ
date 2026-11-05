# Vehicle physics and proving grounds: mass, materials, engines, fuel, new test maps (design, 2026-11-05)

Labels: **SAID** owner 2026-11-05; **OBS** seen in the repo; **DOC** my reasoning, not run. Companion: `VEHICLE-CRAFTING-AND-MODDING-DESIGN-2026-11-05.md` (parts and recipes), `GYMS-ZOOS-MUSEUMS-IN-GAME-DOCS-2026-11-05.md`, `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md`.

## 1. What the owner asked (SAID)
Ships should aim to be **mechanically and physically sound**: weight/mass from materials, an engine (even sci-fi) with fuel or equivalent, mathematical models that map to a larger physical simulation. Make **new maps for testing a car, boat, plane and so on, with realistic physics, and house-compliant**. Document and roadmap it.

## 2. What exists (OBS)
- The house already decided: **"Fun first; room for realism in the data; rulesets, not forks"** (collision design 4c): block rows carry `composition`, `density`, `melting_point`, `specific_heat` columns that the default game ignores; "the fun ruleset and a realistic ruleset are different `.pdl` files over the same state files; the engine and the event pages do not change".
- `scale.pdl` (a scale ladder and one universe seed), `environment.pdl` (air, temperature per room), `reactions.pdl`, `collision.pdl`, `hold.pdl`; helm modes, ship damage; a single physics-adjacent unit system is not defined yet (DOC: the reason the numbers are loose).
- No ground, water or air vehicle physics exists; only space flight (one grid step per key).

## 3. The physics model (DOC; standard textbook relations; SI units in the realistic ruleset)
Parts carry **material rows** (`MATERIAL | id | density | strength | melting_point | cost | ...`); a part has a volume, so **mass = density x volume**, summed over parts plus fuel plus cargo. One state per vehicle: position, velocity, orientation, fuel mass.
| Domain | Forces (realistic ruleset) | Derived numbers worth showing |
|---|---|---|
| All | gravity m g; semi-implicit Euler at a fixed step (deterministic: replays and harnesses need it) | mass, centre of mass |
| Rocket / sci-fi drive | thrust T, specific impulse Isp: fuel burn rate = T / (Isp g0); **Tsiolkovsky delta-v = Isp g0 ln(m0/m1)** | delta-v, thrust-to-weight, burn time |
| Space | gravity of bodies from the seed/scale ladder, circular speed sqrt(GM/r) | orbit speed, escape speed |
| Boat | buoyancy = fluid density x displaced volume x g; hull drag 1/2 rho v^2 Cd A | draft (float depth), stability, top speed from power |
| Plane / drone | lift 1/2 rho v^2 S CL; drag; thrust; rotor thrust ~ disc loading | stall speed, climb rate, payload |
| Car / truck | wheel friction mu N, engine power P = F v, rolling resistance, slope | acceleration, top speed, hill-climb |
| Mecha / legged | joint torque limits, support polygon, step height (ties to stairs D12) | max carry, step, fall |
| Sci-fi engines | an engine row declares an **energy source** (fuel, battery, reactor), an output (thrust, power) and an efficiency, so "fusion torch" is a row with big Isp, not new code | power budget, heat |
| Damage | material strength vs impulse; breakage into remnants (existing) | wreck parts |
The **fun ruleset** maps the same parts to simple numbers (speed, armor, cargo). One data model, two rulesets: the existing rule.

## 4. Why this is verifiable (the key to agent-built content)
Each relation has an **analytic oracle** a harness can check without a human: free fall distance, terminal velocity (drag = weight), Archimedes float depth, rocket-equation delta-v against a simulated burn, circular-orbit period, friction-limited braking distance, hill-climb limit. A test passes if simulation matches the formula within a stated tolerance; **mutation proof**: perturb a constant and the case must fail (AGENTS.md: a passing assertion must be able to fail). Handling "feel" stays a human judgement; invariants are mechanical.

## 5. Proving grounds: new maps (house-compliant), the vehicle gyms
Made as normal house levels (map files, event pages, hotspots, entity registry, rulesets as `.pdl`), not engine code. Candidates:
| Map | Contents | Instruments / lanes |
|---|---|---|
| **Test track** (car/truck) | flat straight, friction patches (dry, wet, ice), slopes, a ramp, a skidpad, a weighbridge | speedometer, 0-100 timer, braking lane, hill ladder (5/10/20/30 degrees) |
| **Test basin** (boat) | still water tank with depth markers, waves optional, a current lane | draft gauge, top speed, turn circle, capsize test |
| **Airfield / wind tunnel** (plane, drone) | long runway, altitude bands, wind lane, payload scale | stall speed, climb rate, hover hold, endurance |
| **Vacuum range / orbit gym** (spaceship) | no air, a small planet from the scale ladder | delta-v budget, burn-time, orbit insertion, docking |
| **Mecha yard** | stairs, gaps, slopes, a weight carry course | step height, carry limit, fall damage (depends on D12 stairs) |
| **Weigh station** | a scale that reads mass and centre of mass for any vehicle | mass breakdown by part |
Each lane is a `GYM` row (target, tolerance) and a harness case that drives a scripted run headlessly and compares to the oracle. A person can also walk in, drive, and see the instruments.

## 6. Design-choice test (D21, PROPOSED)
| Question | A. Physics hard-coded per vehicle | B. **One state + force-model rows per domain, two rulesets** (recommended) |
|---|---|---|
| One authority | numbers duplicated | derived from materials and parts |
| Modular / scalable | new code per vehicle | new rows; a new domain = a new force row set |
| Long term | diverges | matches "rulesets, not forks" |
| House shapes | C | op for the integrator, `.pdl` for rules, harness oracles |
| Cost / risk | cheap start | integrator op + units; risk of numeric instability: fixed step, small step tests, clamp |
**Recommendation: B**; start with one op (`vehicle_step`) and one domain.

## 7. Roadmap and effort (DOC, not measured; S about 1 session, M 3-5, L 10+)
| Step | Create | Validate |
|---|---|---|
| Units and `MATERIAL` rows; mass from parts | S | S (table-driven) |
| Integrator op `vehicle_step` + free fall / terminal velocity oracles | M | S |
| Space domain: rocket equation, fuel burn, thrust (extends the existing ship; vacuum range map) | M | M |
| Boat: buoyancy, drag; test basin | M | M |
| Car/truck: friction, power; test track | M | M |
| Plane/drone: lift, drag; airfield | L | L (stability is fiddly) |
| Mecha: legs, balance | L | L |
| Instruments + `GYM` rows + harness for each | S each | S |
| Fun ruleset mapping per class | M | M (human playtest) |
| Handling feel and balance | L | L (human) |
Order: **space (the engine already flies a ship) → boat → car → plane/drone → mecha**. Roadmap milestone **M11**. Footprint: the integrator runs only while a vehicle moves; a gym map idles at near zero.

## 8. Open questions
1. How realistic by default: ship the fun ruleset to players and keep realistic as a mode (recommended)? 2. SI units everywhere, or scale-ladder units? 3. Do vehicles feed rated PvP only in a "stock class"? 4. Do sci-fi engines get an energy economy tied to the chain/bank, or stay local? 5. Which proving-ground map first (recommend the vacuum range, then the basin)?

## 9. Owner answer 2026-11-05
Question 1: **fun only for now.** Build the fun ruleset first (simple speed, armor, cargo, fuel as a gauge); keep `MATERIAL`, density, thrust and Isp columns in the data so the realistic ruleset can be switched on later without a schema change; the proving-ground maps and gym lanes still use invariants (a car's top speed follows from its parts) but not the full oracle set. Milestone M11 is therefore scoped to the fun ruleset until the owner asks for realism. New test maps: `../22.lore-ip/MAP-ATLAS-VISUALS-LORE-AND-MAPS-TO-MAKE-2026-11-05.md` section 4.5.

## 10. Owner answers, round 3 (canon proposals adopted 2026-11-05)
**Realistic physics is a mode, not the default** (agrees with section 9); **build the vacuum range first** (answers question 5); the spaceship class first, starting with the SSOL. Questions 2-4 (units, rated PvP, sci-fi energy economy) stay open except: vehicles are **out of rated PvP at first**.
