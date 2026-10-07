# Save-game / load-game slots (tb "9 player" dropdown)

Owner request 2026-10-06: add `save-game` and `load-game` to the taskbar Player dropdown. Each opens a secondary dropdown of **16 slots** with a Cancel that goes back to the
Player dropdown. Later the slots record **played progress** (events already done, what the player created / destroyed, entity scores, menu state) so play can be scripted like
RPG Maker (time- and event-dependent) and **re-audited**. Needed before shipping this as a playtest / RPG-Maker-MV-compatible engine.

## What exists now (v1, 2026-10-06) — auditable, NON-destructive

- Menu: `livedesk_build_player_menu` adds `save-game` / `load-game`; internal sub-menu ids `PLAYER_MENU_SAVE` 161 / `PLAYER_MENU_LOAD` 162 (same technique as the clock's 151-154) list
  `slot NN  <saved_at>` or `slot NN  (empty)` and a `Cancel (back)` row (`livedesk:savegame-back` reopens the player menu). Load leaves empty slots inert.
- Op: `&.widgits/_shared-lib/ops/game_slot_op.c` (`game_slot_op.+x <savegames_dir> <pals_dir> save|load <1-16>`), run in the background, niced.
  - **save N**: writes `slot_NN/manifest.txt` (`sha256|size|path` for every file of the user's entity tree, sorted), `slot_NN/meta.pdl`, and appends `ledger.txt`.
  - **load N**: compares the current tree with the slot's manifest (same / changed / missing / new), writes `last_load.txt` (first 200 differences), `loaded_slot.txt`, and a ledger line.
    **It restores nothing.** The result line is in `savegames/last_result.txt`.
- Where: `xyzfs/users/<uuid>/home/livedesk/savegames/` — per user, so it lives in the user's data branch (`user/<name>`), never in code.
- Proof: `sh &.widgits/_shared-lib/ops/build_phone_ensure_op.sh && the pal harness harness/game_slots.pal (run from &.widgits/_shared-lib/harness, README there)` (13 checks on a scratch tree: counts, ledger append-only, entity tree
  byte-identical after save and after load, empty slot fails cleanly, bad slot/verb rejected). The menu code was driven through the real `ktb_hq_open` / `ktb_hq_activate` in a scratch harness
  (save-game opens 17 rows, Cancel (back) returns to the player cell, a slot press closes the menu and runs the op, load reports the comparison).
- Not yet seen on the real screen: the running taskbar still has the old binary; the new one is built and takes effect at the next taskbar start.

## Not done (decisions needed from the owner)

1. **Restore.** Load does not put anything back. Restoring means overwriting entities that are running (and their phones/wallets/histories). Options: (a) snapshot copy per slot (17 MB / 1,386 files for the
   owner today: 16 slots about 270 MB uncompressed), stop the world manager, restore, restart; (b) record only the *event ledger* and replay it (the RPG-Maker model: state = f(events)). (b) matches the
   stated goal (scripted, event-dependent, auditable) and is much smaller, but needs every state change to be an event first.
2. **What "progress" is**: events done, created/destroyed entities, scores, menu state. Today the manifest covers the entity tree only (not menu state, not `#.desktop`, not the pc-hq board).
3. **Slot labels / thumbnails** and an "overwrite this slot?" confirm (the generic `confirm=` popup exists for windows; the taskbar menu has none yet).
4. **Autosave slot**, and a HUD notice of `last_result.txt` (the result is only a file for now).
5. Windows build: the `system()` / `nice` shell-outs are POSIX only (same as the neighbouring menu rows).

## Scope of a slot (owner, 2026-10-06) - changes item 2 above

A slot captures **game data only**. Everything else is default and **out of time** (not game time; a save, load or game start never touches it). **Game time** exists only during play and is **saved as state in the slot** and restored on load.
Starting a play = loading the game's initial game-data state (same format as a slot) and putting the possessor on the game's single **starting-position entity** (`XELECTOR-CURSWORD-POSSESSION-DESIGN.md` section 5).

What this means for the v1 op (`game_slot_op.c`): it currently scans the user's **whole entity tree** (wallets, histories, phones included). That is wider than the rule, so before any restore is built it needs (a) a definition of the game-data set (a list or a marker on those entities, to be decided with the owner) and (b) a `game_time` value stored in `meta.pdl`. Until then v1 stays compare-only and non-destructive, which is why it is safe.
