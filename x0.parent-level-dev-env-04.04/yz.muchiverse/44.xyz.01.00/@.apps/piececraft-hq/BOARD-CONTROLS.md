# piececraft-hq — board window controls & features (local reference)

This is the app-local companion to the book's
`08-roadmap/design-docs/pchq-vs-muta.md` / `pchq-vs-tpmos.md`. It
documents what the **board window** (`open_pchq_board.sh` →
`khtpm_core_render` + `pchq-board.xhtpm`, mirroring a live board-viewer
engine session) actually does today, key by key.

Last verified: 2026-09-09.

---

## 1. The chrome bar (`x` `!` `_` + toolbar)

`pchq-board.xhtpm` page `main` draws, top-right → left:

| Item | Verb | Notes |
|---|---|---|
| `x` | `CLOSE` | closes the board window (engine session keeps running) |
| `!` | `TOGGLE_FULLSCREEN` | grow/restore to the display size |
| `_` | `MINIMIZE` | to the livedesk taskbar |
| `In: on/off` | Interact-mode toggle | arms keyboard/mouse forwarding to the engine (see §2) |
| `File` | `file-hq` | opens the File Explorer widget → pick a map to load |
| `Desk` | `menu desk` | dropdown of desks (maps) for the active world |
| `Menu` / `Player` / `${clock}` | stubs / readout | `Menu`+`Player` are layout stubs; the clock is the **game** clock (§4) |

**Fixed 2026-09-09:** mouse clicks on `!` / `_` were being swallowed by
the window drag-start zone; and clicking the clock (`action="void"`)
closed the whole window (`void` → `g_quit`). Both fixed in
`khtpm_core_render.c` (`g_canvas_chrome_left_x`; `void` no-op for
persistent/canvas windows). `!` toggles `TOGGLE_FULLSCREEN` but for a
fixed-size canvas window that only repositions to 0,0 — a true
fill-screen (canvas blit scaling) is still to do.

---

## 2. Interact Mode

Click **`In:`** (or click the play canvas) to arm. While armed the
board window forwards input to the engine:

- every key **except** Enter/Esc → `pieces/apps/player_app/interact_relay.txt` (the pal camera loop)
- Enter (`13`) / Esc (`27`) → `pieces/keyboard/history.txt` (the `board_viewer.chtpm` parser state machine)

Esc exits Interact Mode. Routing 13/27 to only the parser file (and
everything else to only the relay) is what fixed the old
"double-arrow" / "camera moves twice per key" bug.

---

## 3. Camera / POV / cursor keys

Bindings resolve **`pieces/system/keybinds.pdl` → `pieces/system/arrow_config.txt` (`key_*`) → built-in default**.
Edit `keybinds.pdl` and relaunch — no rebuild. `0`, `` ` `` and `1`–`4`
are **non-modal** (work from any view); the rotate / pan / height keys
act only while `render_mode == 1` (3D).

**Changed 2026-09-09:** the rotate / pan / height keys now fire in **all
four** 3D POV modes (previously several were gated to 3·4 only). Key
`5` mirrors `f`. `w↔s` and `a↔d` pan directions were flipped so modes
1·2·3 match 4.

| Key | Action | Modes |
|---|---|---|
| `0` | toggle 2D ⇄ 3D | any |
| `` ` `` | always → 2D Chinese / ASCII terminal view (`view_2d_style=ascii`) | any |
| `1` `2` `3` `4` | POV: first-person / third-person / free-roam / bird's-eye — **switches to 3D** | any |
| `q` `e` | yaw left / right | 1·2·3·4 |
| `r` `t` | pitch down / up | 1·2·3·4 |
| `w` `a` `s` `d` | pan | 1·2·3·4 |
| `c` `v` | camera height − / + | 1·2·3·4 |
| `f` / `5` | reset camera to the mode default | 1·2·3·4 |
| `z` `x` | move the xelector (cursor) down / up a z-level | any |
| `8` | snap the xelector back to the hero | any |
| arrows | move the xelector on the board plane | any |

`camera_mode` defaults to **2** (third-person). In modes 1·2 the pan /
height offsets are added on top of the hero-follow anchor and are
zeroed when you press `1`, `2`, or `f`/`5`.

**GPU renderer:** `pieces/system/arrow_config.txt : use_gpu_render=1`
routes 3D frames through the resident `bv_render_3d --daemon` EGL
raymarch (see `08-roadmap/design-docs/BV-GPU-RENDER-DESIGN.md`).
`motion_lod_step=2` drops render resolution while a move key is held.

The `5`-`8` "one map" remap and the cursword shared layer are
abandoned — `1`-`4` are POV, same as mutaclysm.

---

## 4. Possession (`Enter` / `9`) — mutaclysm `ops/choice.c` parity

- **`Enter`** on the hero's tile → possess `hero_01` (if its
  `pieces/hero_01/piece.pdl` has `possessable`, default yes).
- **`9`** while possessing → release only. Blocked if
  `piece.pdl` `de_possessible = true`. Records `last_possessed_id`,
  steps the xelector onto the entity's cell.
- **`9`** while *not* possessing, with a `last_possessed_id` set →
  reverse-jump: snap to that entity and re-possess.

**Game verbs are possession-gated** — they only fire while possessing
`hero_01`:

| Key | Verb |
|---|---|
| space | `JUMP` |
| `g` | `MINE` |
| `h` | `BUILD` |
| `/` | `END_TURN` |
| `.` | `TOGGLE_AUTOTICK` |
| `,` | `CYCLE_TICK_SPEED` |

Verbs are appended to the host inbox
(`pieces/system/widget_cmds/inbox.txt`) and drained by
`ops/pc_menu_input.c`.

---

## 5. Day / night + the sun

The lighting engine lives in `&.widgits/board-viewer/ops/bv_render_3d.c`
(`compute_sun_light_level`, `clear_sky`, `load_celestial_body`), gated
by `lighting_enabled = 1` in `pieces/system/arrow_config.txt`. It reads
`pieces/sun_01` / `pieces/moon_01` and the game clock
`pieces/world_01/state.txt : game_time_epoch_sec`. **3D mode only.**

The clock only advances while `pc_clock_daemon` is running **and**
`autotick_enabled = 1`:

1. Possess the hero (`Enter` on its tile).
2. Press **`.`** → `autotick_enabled = 1` and the clock daemon is
   launched if it isn't already (fixed 2026-09-09 — previously the
   daemon was only started at fresh world-gen, so `.` on an
   already-"playing" world flipped a flag with nothing acting on it and
   the sun stayed frozen).
3. Press **`,`** to cycle speed. **The names are inverted from
   intuition** — fastest → slowest: `cent` (≈ 1 game-hour / real-sec,
   full day-night in ~24 s) · `sec` · `min` (default) · `hour` (≈ real
   time) · `day` (barely moves). For a visible sun arc use `cent`.

At exactly solar noon the sun disc sits straight overhead, usually
outside the camera's vertical FOV — you'll see bright sky but no disc
until the clock moves it toward the horizon.

---

## 6. The toolbar clock

`${clock}` is the **game** clock (`world_01/state.txt`
`game_time_epoch_sec`, `HH:MM` of the in-game day), not wall-clock time
— published by `ops/pchq_board_projector.c`. Shows `--:--` before a
world has a clock value.

---

## 7. Key files

| Path | Role |
|---|---|
| `pchq-board.xhtpm` / `pchq-board.css` | the board window template |
| `open_pchq_board.sh` | the launcher (standard x11-hq shape) |
| `button.sh` | terminal / `PCHQ_ENGINE_MODE` engine host |
| `ops/pchq_board_projector.c` | publishes `state/ui.txt` (session, canvas, clock, menus) |
| `ops/pchq_board_action.sh` | toolbar verbs → board-viewer history files |
| `ops/pc_menu_input.c` | drains the host inbox; autotick + clock daemon |
| `ops/pc_clock_daemon.c` | the always-on game clock |
| `pieces/system/keybinds.pdl` | all key bindings (KEY / VERB / AXIS) |
| `pieces/system/arrow_config.txt` | 3D geometry + `lighting_enabled` + `default_camera_mode` |
| `&.widgits/board-viewer/ops/bv_menu_input.c` | camera / POV / possession input |
| `&.widgits/board-viewer/ops/bv_render_3d.c` | raymarch + sun/sky/shadows |
