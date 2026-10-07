# Xelector, cursword and possession (pc-hq `9` key)

Written 2026-10-06 (claude) from the owner's words. **Design only: nothing in this file is built yet** except where marked "today". Code facts under "Today" were read in the source on
2026-10-06 (`&.widgits/board-viewer/ops/bv_menu_input.c`, `@.apps/hotbar-hq/ops/hotbar_manager.c`, `@.apps/piececraft-hq/ops/pchq_board_projector.c`).

## 1. The model (owner, 2026-10-06)

> "the cursword is the possessor for desk. xelector is just a mouse pre-cursword to cursword but similar."

| Thing | What it is | Where it lives |
|---|---|---|
| **cursword** | The **possessor** on the desk: the player's own cursor-entity, the one that holds the hotbar and acts in the world. | The user's pal `xyzfs/users/<uuid>/home/livedesk/pals/cursword` (seed: `xyzfs/_seed/livedesk/pals/cursword`). |
| **xelector** | A **mouse-like pre-cursword**: a free cursor (the coloured cube) used while no body is possessed. It has no gravity or collision, follows the mouse (click/drag, built 2026-10-06), and holds a hotbar of its own. Similar to cursword, but cheaper: it is the possessor *before* possession. | pc-hq piece `@.apps/piececraft-hq/pieces/xelector_01` (`state.txt`: `pos_x/y/z`, `possessed_id`, `last_possessed_id`). |
| **hero** | A body on a map that a possessor can enter. Later, the **designated** body for a game ("designated hero"). | pc-hq piece `pieces/hero_01` today. |

So possession is a chain, and the xelector is its first link:

```
xelector (free cursor)  --9-->  cursword (possessor, default)  --later-->  designated hero (if the map has one)
```

## 2. The rule the owner asked for

1. **Pressing `9` moves the xelector into cursword** (possess it). While possessed, the pc-hq hotbar holder is cursword (the holder picture and slots are cursword's), not the xelector's.
2. **Later**, on a map that has a designated hero, `9` moves it into that hero instead (cursword remains the possessor underneath: it is the desk's possessor, the hero is its body on the map).
3. **Some games start possessed**: xelector already in cursword, in the hero. So "possessed at start" is a per-game/map setting, not a key press.
4. **Debug / non-play build mode only.** In play mode the player does not drive a free xelector at all. (Which flag means "debug" is open, see §5.)
5. **If present.** Cursword should always exist, but some maps/sessions do not have it yet. If the designated body is missing, `9` must do nothing harmful (keep today's behaviour, or no-op) and say so.

## 3. Today (read in code, not run for this doc)

- `9` is `key_possess`, Enter is `key_possess_commit` (`bv_menu_input.c` ~791-880). It follows **mutaclysm parity**: Enter possesses the entity under the xelector (only `hero_01` is hard-coded as possessable), and `9` only **releases**
  possession, or, when not possessing and `last_possessed_id` is set, **reverse-jumps** back into it. It never possesses a fresh target by itself. So with `last_possessed_id=hero_01` (the owner's state today) `9` re-enters the hero, never cursword.
- Possession state is two keys in the xelector's `state.txt`: `possessed_id` (`none` or an entity id) and `last_possessed_id`. Entering writes the target's cell into the xelector's `pos_x/y/z` and the board viewer's `selector_x/y`, `current_z`.
- The hotbar holder for pc-hq (`hotbar_manager.c` `resolve_holder`, mode `pchq`): `possessed_id` if set, else `xelector_01`, resolved as `piececraft-hq/pieces/<id>`. **A `possessed_id` of `cursword` would resolve to a folder that does not exist** (cursword is a desk pal, not a pc-hq piece), so the hotbar would publish nothing. The desk mode resolves cursword by globbing the user's pals.
- Cursword already appears on the pc-hq board as an entity row: `state/ui.txt` has `ent_2_label=cursword`, `ent_2_id=cursword`, `ent_2_kind=page` (its x/y/z are `0,0,0` in the sample read: a page row without a map cell yet).
- `Player: ON/OFF` is the house-wide Play Mode flag, `#.desktop/khtpm_play_mode.state.txt` (`mode=on|off`), shown in pc-hq by `pchq_board_projector.c` and flipped by the taskbar and by `pchq_board_action.sh`.
- `load_xelector` in `bv_render_3d.c` draws the cube from the xelector's page row when the desk has one (and then follows the possessed entity's named cell), else from `pieces/xelector_01/state.txt`.

## 4. What building it needs (not done)

1. **Designated body setting.** One place that names the target, for example `possess_target | cursword` in the pc-hq options file (later `| hero_01` or a map property). Default `cursword`.
2. **`9` branch** in `bv_menu_input.c`: not possessing -> if the target exists, set `possessed_id` to it, jump to its cell if it has one, bump the screen marker; possessing -> release (today's code). Keep reverse-jump for `last_possessed_id` only as a fallback when no target is set.
3. **Holder resolution for non-piece targets.** `hotbar_manager.c` `resolve_holder` (mode `pchq`) must map `possessed_id=cursword` to the user's cursword pal dir (the glob the desk mode already uses) so the hotbar header and slots show cursword. Same for the holder picture (`hb_holder_sprite`).
4. **Cursword's cell on the map.** Possession needs a position to jump to. Cursword is `kind=page` with no map cell yet; decide whether it gets a cell on the board (its own entity row with x/y) or the xelector simply stays where it is and only the holder changes.
5. **Start-possessed** per map/game: initial `possessed_id` written when a game/map loads.
6. **Test**: scratch project (as done for the click helper): `9` possesses cursword, again releases, missing target is a no-op, hotbar publishes cursword's slots.

## 5. Owner answers (2026-10-06, same day) and what is still open

**Answered**

- **Q2, does the xelector cube draw while possessed?** Hide it ("hide, sure"). The owner added "or backspace": taken to mean Backspace is an acceptable way to get back out, or to hide it. **My reading, not confirmed:** cube hidden while possessed; Backspace is an extra release key next to `9`. Confirm which.
- **Q4, where is the designated hero set?** Two places:
  1. **Per "db" session**: the session's data names the designated body.
  2. **Per game, as a "starting position" entity.** This is a real entity placed on the map, and **the option to place it appears when you click any empty space** (the empty-space context menu gets "Set starting position"). When the game is **played**, this entity is where the game **starts from**.
- **What starting a play does** (owner): all in-game variables and events **restart**, and the player then plays **in livedesk or in pc-hq, depending on how it was started** (the play begins in the screen that launched it).

**What this adds to the build list (§4), nothing built yet**

7. **Starting-position entity**: kind e.g. `start`, one per game/map (placing a second moves the first, or is refused; decide). Stores a cell and, with the session's designated body, what the player enters (cursword now, hero later). Placed from the empty-space click menu in pc-hq (the in-board context menu `pc_entity_ctx.sh` / `pc_canvas_rclick.sh` already opens on a click; the empty-cell case needs the new row). A desk-side equivalent follows the same rule: entities are files, so it is a doc/entity folder (see `TASKS-AS-EVENT-DATA-DESIGN.md` for the doc-entity idea).
8. **Play start**: on Player ON, reset the in-game state (vars, event progress; this is exactly what the **save slots** record, `SAVE-SLOTS-DESIGN.md`: a start is "load the initial state" and an audit trail row), put the possessor on the starting-position entity, then hand the player the screen that launched play (livedesk or pc-hq). Needs a definition of "in-game vars" (which files are reset and which are never touched: user desk data, wallets and histories must not be wiped by a game start).
9. **Where the launch origin is remembered** (livedesk vs pc-hq): written when play is started, read by play start.

**Still open**

1. Confirm the Backspace meaning above.
2. Which flag means debug (non-play): `Player: OFF`, or something else? Not gated in code.
3. Does `9` while possessed release (today's rule)?
4. Does cursword get its own board cell, or is the starting-position entity its first cell?
5. One starting position per map, or per game across maps?
6. Which state counts as "in-game" and is reset on start (and which is protected, e.g. `xyzfs/users`).

## 6. Related, queued (owner, 2026-10-06): more taskbar headers on the pc-hq top bar

pc-hq's top toolbar has only: In, book, page, Menu, Player, clock. The owner wants more of the livedesk taskbar headers on it, e.g. **db** and **plugins**, as the kind that **can change per book/session and are not handled by the "super user"** (so they are session-scoped, not global admin headers).
Not started. Notes for whoever builds it: pc-hq has its own hand-written dropdowns (`pchq-board.xhtpm`, `ops/pchq_board_action.sh`), not the taskbar's `livedesk_taskbar.pdl` rows, so each header is a new tab + dropdown rows there; the renderer's toolbar layout is shared and window-size mutations must stay idempotent
(memory `khtpm-shared-layout-caution`); decide first whether the list of headers comes from the book/session data (so it varies) or is fixed in the template.
