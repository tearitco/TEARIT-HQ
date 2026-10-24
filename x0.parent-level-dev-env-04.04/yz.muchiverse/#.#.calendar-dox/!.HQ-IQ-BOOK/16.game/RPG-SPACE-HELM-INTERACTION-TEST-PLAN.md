# rpg-space: interacting with the pilot console, and testing vehicle driving, as a human would

Written 2026-10-24. Branch `claude`. This is a how-to and a test plan, plus an honest record of what was tried. Nothing about driving is built yet.


## 0. UPDATE 2026-10-24 (later the same day): the helm exists and was driven as a person would

Built: event page `events/helm/` (trigger `action`, command `helm on`), its AREA file `events/helm/area.pdl` (see `EVENT-AREA-SELECTION-DESIGN.md`), a `HOT` row with an event name (`hotspots.pdl`), the verb `helm on|off|move dx dy|status`, the galaxy grid (`POS` rows in `planets.pdl`, ship cell in `state/ship.txt`), and three core hooks in `petslike_core.c` (75 file scope, 76 key loop, 77 Enter, 78 helm steering).

Interaction rule chosen: **(a)**: the controlled character stands next to the console cell (not diagonal) and Enter is pressed. The xelector is not used for it.

Steering: the engine eats the arrow keys itself and empties its relay, so the daemon never sees them. At the helm the daemon reads **where the engine moved the xelector**, takes the direction from its offset to the character (camera aware), flies the ship one cell and puts the xelector back. One move per key; 1.5 s settle after a move (a planet change reloads the scene and moves the xelector by itself).

Run live (real X keys into the window, ship started at 1,5):
- Enter at the console (hero 7,4, In: on): `HELM|..|on|4 boxes, 4096 voxels` and `EVENT|..|helm|action|bridge|8,4` in `state/events.txt`, `flag_helm=1`.
- Up, Right, Right, Right, Down, Left: ship 1,5 -> 2,4 (first key also took a stale xelector offset: fixed by the debounce) -> 3,4 -> 4,4 (`PLANET | hell`, `DOOR|..|ship|none|hell`) -> 5,4 (deep space) -> 5,5 -> 4,5. Xelector stayed on 7,4 and Harold on 7,4 throughout.
- Esc: `flag_helm=0`, `flag_int=0`, engine flag 0, Harold wandered (7,4 -> 7,9 in 15 s).
- Negative: Enter three cells from the console with In: on: no EVENT row, `flag_helm` stays 0. `helm move` with the helm off: "not at the helm". Edge of the grid refuses.

NOT done / seen: no screenshot (the window grab is black on this GPU path; the evidence is state files); the per-pid relay was not used (it does not move the xelector); a first Enter test in the wrong room (Harold had walked through a door) looked like a failure and was my test error; the keys are sometimes lost when the window loses focus (one Esc did nothing until focus was set again); the hull does not move (the planet under it is swapped); no minimap shows the ship cell; the `101.mutaclsym` prisc error still appears in `events/helm/master_ledger.txt` and the page still runs; the debug trace `enter-in-engine` in `state/int_trace.txt` stays on.

The sections below are the original plan written before this was built.

## 1. Status, in one paragraph

**Vehicle driving has not been tested, because it does not exist.** The bridge has an event-voxel *marker* for the helm console (`@.apps/rpg-space/hotspots.pdl`, `HOT | bridge | 8 | 4 | 0 | helm console ...`), which draws an orange outline while In: is off. There is **no pilot event page**: `@.apps/rpg-space/events/` holds only `newplanet`, `planet`, `start`, `zoom_in`, `zoom_out`, all `trigger=ui` (menu verbs). There is **no helm flag** (`state/flag_helm.txt` does not exist), no key routing for flying, and no ship position. In gta-hq the car is a set of parts assembled on a sprite (`car_frame`, `car_wheel`, `car_seat` ...), not something you drive. So no vehicle has ever been driven in any pets-like board.

## 2. What was tried on 2026-10-24 (the first human-style attempt)

Setup: rpg-space window, bridge desk, Harold controlled (`ctl=player`), In: switched on by a click on the tab through the window relay (`MOUSE_EVENT: 1 60 47 1` then `... 0` into `#.desktop/entity_menu_history/<pid>.txt`; INT and the engine flag both went to 1).

| Step | Result |
|---|---|
| Eight `KEY_PRESSED: 200` (Up) lines into the window's per-pid relay | **Nothing moved.** Xelector stayed at (8,12). The per-pid relay does not forward arrows to the engine while In: is on. |
| `KEY_PRESSED: 13` (Enter) the same way | Xelector and Harold both ended at (7,7). Not the console. Not attributable to the key alone: someone else changed the planet during the test (`fire_log` row `planet cubia` that I did not send). Treat as inconclusive. |
| Real X key presses (`xdotool windowfocus <win>` then `xdotool key Up`) | **Works**: one cell per press, xelector (7,7) -> (7,6) -> (7,5) -> (7,4). |
| Real `Right` from (7,4), aiming at the console cell (8,4) | **Xelector did not move onto the console.** It stayed at (7,4) on Harold. |
| Real `Return` at (7,4) | **Nothing happened**: no chat line, no `fire_log` row, no new state file, no change of `ctl`/`int`. |

What this says, and what it does not:

- Said: with a character controlled, the xelector follows the arrow keys but cannot be parked on a blocked cell. The core snaps it back onto the character when the step is refused ("a bump snaps the xelector back onto the monster", `follow_xelector` in `petslike_core.c`). The console cell is a solid block, so **the xelector cannot be put on the console the way the design assumed.**
- Said: Enter in In: mode is "possess whoever stands at the xelector" (`possess_at_xelector`). With Harold on the xelector it re-chooses Harold; it is not an interaction with an event.
- Not said: that an "interact with the event under the xelector" key cannot exist. It does not exist yet.
- Not verified: whether the per-pid relay arrow failure is by design or a bug. The Esc key is documented to go through a different path there (`khtpm_core_render.c`, `handle_key` for real X keys versus `poll_agent_history` for relay lines).

## 3. How to drive the board like a person (the input methods, and which work)

| Goal | Method that works | Method that does NOT |
|---|---|---|
| Turn In: on or off | Click the tab: `MOUSE_EVENT: 1 60 47 1/0` into the window relay. Leave with a real `Escape` (`xdotool key Escape` on the focused window) or a second click. | `13` written to the engine's relay files (Enter is an ordinary key inside In: mode). |
| Move the xelector | Real X arrow keys: `xdotool windowfocus <win>; xdotool key Up`. Or `1000..1003` (bare decimals, left/right/up/down) appended to `<bv_session>/pieces/apps/player_app/interact_relay.txt`. | `KEY_PRESSED: 200..203` into the per-pid window relay (seen not to move it today). |
| Move the camera | `bv_keys.+x <bv_session> "qetrcv"` (q e yaw, r t pitch, c v height, `f` reset, `0` 2D/3D) | |
| Level of the xelector | `z` down, `x` up (the normal level is 17; `8` snaps it back to the character) | pressing `z` nine times (it sinks into the planet and the camera goes inside: solid green) |

Find the window: `xwininfo -root -tree | grep ' 1260x556+'` (windows carry no X name). Find the engine session: `bv_session=` in `@.apps/rpg-space/board/state/ui.txt`. Find the window pid: `ps -eo pid,args | grep khtpm_core_render | grep rpg-space`.

Rules while testing: say out loud when you drive the window; two people sending keys at once corrupted state twice (see the handoff). `xdotool windowfocus` steals keyboard focus. Log CPU to a file while the window is open.

## 4. What has to exist before the test can pass (the build list)

1. **A way to interact with an event voxel.** Pick one and write it into the design, because the xelector cannot rest on a blocked cell:
   - (a) Enter while the controlled character stands next to a hotspot cell, facing it (the usual game rule, no xelector placement needed); or
   - (b) let the xelector enter hotspot cells even when the character cannot (the follow rule then must not snap back for a hotspot); or
   - (c) move the console to a floor cell the character can stand on and make "stand on it, press Enter" the trigger.
   Recommendation: (a). It matches `possess_at_xelector`'s habit of keying off Enter, and it works with the xelector where it already is.
2. **A helm event page** `events/helm/` (a common event with `COND | trigger | action` or the existing `ui` plus a new `touch`), whose command runs a verb `helm on`.
3. **`state/flag_helm.txt`** (0/1) written by `helm on|off` through the same `set_flag` the core uses for `int` and `ctl`.
4. **Key routing while helm is 1:** the arrow keys move the **ship** (a ship position in `state/ship.txt` or the look ledger), not the xelector; **Esc** clears the flag and gives the xelector back. Only the daemon needs to know: while helm is 1, `follow_xelector` is skipped and the arrow codes are read for the ship.
5. **A visible result.** Without a map that shows the ship moving (the minimap), the test has nothing to look at; the first version can move the ship between the existing planet pages by direction, logging `DOOR | time | ship | from | to` rows like the `planet` verb does.

## 5. The test procedure (to run once those exist)

Every step ends with a file or a picture the owner can see. A passing check must be negative-tested once (shown able to fail).

1. **Baseline.** Relaunch cleanly (end the whole session first: the engine session of an old launch is reused). Log CPU of the window to disk. Record `flag_helm`, `flag_int`, `flag_ctl`, the ship position, the xelector position.
2. **Engage In:.** Click the tab through the relay. Expect engine flag 1, `flag_int` 1, label `In: ON`.
3. **Walk to the console.** Real arrow keys until Harold stands on the cell chosen in 4.1 (record each key and the resulting hero cell; expect one cell per press).
4. **Interact.** One real `Return`. Expect, within 2 s: `flag_helm=1`, a chat or log line from the helm page, a `master_ledger` row for the helm event, and a screenshot showing the helm state (label or outline colour).
5. **Drive.** Four real arrow presses. Expect the ship position to change four times and the xelector to **not** move; expect the matching `DOOR | ... | ship | from | to` rows; screenshot of the minimap before and after.
6. **Disengage.** Real `Escape`. Expect `flag_helm=0`; the next arrow press moves the xelector (and Harold), not the ship. Expect In: still ON (Esc ends the helm first; a second Esc leaves In:).
7. **Leave In: with the pets still living.** Second `Escape`: engine flag 0, INT 0, label off, pets and Harold wander. Capture positions over 30 s.
8. **Negative checks.** (a) Enter away from the console: nothing happens. (b) With In: off, arrows must not move the ship or the xelector. (c) Arrow keys sent to the per-pid relay while helm is 1 do nothing (so a test script cannot fake it by the wrong path). (d) Turn the helm flag file off by hand mid-drive: arrows go back to the xelector.
9. **Evidence kept:** the key list with times, the state files before and after, three screenshots (console cell, ship moved, helm off), the CPU log, and `git show --stat` for what changed.

## 6. Side findings from this look

- Every `planet` event run writes `[Prisc Error] Could not open ops file: .../101.mutaclsym...+18.` into `events/planet/master_ledger.txt`, then still reports the event played. The verb works (the ship does change planet) but the event runner fails to open an ops file whose path contains the old `101.mutaclsym` folder name. Not investigated; worth a look before building more event pages on the same runner.
- The engine session's `bv_state.txt` lost three lines once (see the handoff), after which every key was ignored while the label still read ON. A scripted test must start by checking that the file still has `focused_project_root`.
- `rpg-space` and `rpg-dust` still carry two copies of the same painter; keep new helm code in the shared core or skin library, not a third copy.
