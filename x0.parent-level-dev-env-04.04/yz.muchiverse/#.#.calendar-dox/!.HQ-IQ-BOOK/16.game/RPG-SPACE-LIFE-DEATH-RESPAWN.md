# rpg-space: life, death, respawn and drops (first slice, built 2026-10-25)

Owner direction 2026-10-24/25 (notes: RPG-SPACE-DEATH-RESPAWN-GRAVITY-NOTES.md): "they need to die and respawn. often. so they lose their stuff, it drops near their death place." Answers: everyone dies, pets included; causes hunger + damage (vacuum / ozone / suit later, all by events); respawn on the ship bridge; HALF the items drop, anyone can take them; the drop share and permadeath / per-session death are toggled in `.pdl`.

## Where it is
- Behaviour: `@.apps/rpg-space/ops/rpg_space.c` (section 75 block "LIFE, DEATH, RESPAWN, DROPS"); per-tick hook = new core section 79 (`petslike_core.c`, right after `long ms = now_ms()`; other skins define none).
- Config (re-read every 2 s, no rebuild): `@.apps/rpg-space/life.pdl` rows `LIFE | key | value`: `enabled`, `hunger_ms` (60000 = +1 hunger per minute, 0..100), `starve_hp`, `drop_pct` (50), `death_mode` (`respawn` | `permadeath` | `session`), `respawn_room/x/y` (bridge 7,5), `take_range` (2).
- State: `state/life_<id>.txt` (hp, hunger, dead, deaths, inv=item:n,...), `state/drops.txt` (`id|room|x|y|n|item|0`), log rows `DEATH|RESPAWN|TAKE` in `state/events.txt`.
- Verbs (also what event pages call, like RPG Maker's death state / common events): `rpg_space.+x life status [id] | hurt <id> <n> | feed <id> <n> | give <id> <item> [n] | set <id> hunger|hp <n> | take <drop#> [id] | reset`.

## Rules as built
Hunger rises 1 per `hunger_ms`; at 100 each step costs `starve_hp`; hp <= 0 dies. Death: `drop_pct` of every stack falls on the cell where they stood (one drop row per stack); then `respawn` puts them on the bridge cell with full hp and hunger 0, `permadeath` / `session` leave them dead (flag only). `take` moves a drop into the taker's bag when within `take_range` cells in the same room (the dead cannot take).

## Evidence (live, 2026-10-25, the real daemon + CLI verbs)
`give` 6 bread + 10 coin; `hurt 500` -> DEATH row "dropped 8 (50%)", RESPAWN bridge 7,5, hp back to full, inv bread 3 coin 5, drops 3 bread + 5 coin at the death cell; `take` far away refused ("too far, range 2"), near took both back (inv 6/10); permadeath toggled in life.pdl: ember `hp 0 DEAD`, drop of 2 berries; reverted. Hunger clock: hunger_ms 1500 + starve_hp 30 + `set hunger 100`: the pet starved to death by itself, dropped half its fish, respawned (config restored to 60000 / 1).
Bugs found and fixed on the way: a nested `strtok` kept the dropped half in the bag; the daemon cached life files and overwrote edits from verbs (now re-read just before each hunger step).

## Not built / open
Drops are NOT drawn in the 3D / 2D scene yet (data + log only; next: events.txt rows or ui vars); no HP bar or hunger in the UI; permadeath / session leave the character walking (flag only, no removal); no damage source except the `hurt` verb (combat, vacuum, ozone and suits are events to come); no starting inventory (use `give`); the hunger clock runs also while In: is on; drops are not tied to the shop's catalog items; death/respawn event pages (events/death, events/respawn) not written; gravity, falling ship and sky drop (the other notes) untouched.
