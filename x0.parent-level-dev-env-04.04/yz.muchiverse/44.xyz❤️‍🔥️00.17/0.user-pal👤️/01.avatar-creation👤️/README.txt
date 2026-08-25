01.avatar-creation - Clone factory / avatar DNA editor
======================================================

Muchi-pals shaped manager for PLAYER avatars (not pets):

  Main -> Faucet (tokens) -> Store (free starter / buy clone)
       -> Avatars (list, sleep, open desktop window)
       -> Customize DNA (name, age, gender, skin emoji tones,
          hair/shirt/pants color, height, weight)

Storage
-------
Each clone gets a unique avatar UUID.

  Local (desktop windows, same contract as muchi-pals egg_window):
    pieces/world_01/map_lobby/<avatar_uuid>/state.txt

  Player filesystem (source of truth, multi-user):
    00.login-signup/xyzfs/users/<user_uuid>/home/avatars/<avatar_uuid>/
    00.login-signup/xyzfs/users/<user_uuid>/home/avatars/inventory.txt
    00.login-signup/xyzfs/users/<user_uuid>/home/wallet.txt   (tokens)

Login context comes from sibling 00.login-signup/current_login.txt
(USERPAL_LOGIN_ROOT). Log in there first so clones land in your xyzfs.

Desktop
-------
Open Desktop Window forks system/avatar_window (copy of muchi-pals
egg_window). Face emoji is species_emoji / skin_emoji; sprite built via
emoji_gen_atlas when available, else circle fallback.

Run
---
  cd 0.user-pal👤️/01.avatar-creation👤️
  ./button.sh compile
  # optional: log in via 00.login-signup first
  ./button.sh run
  ./button.sh demo    # mint one clone without GUI

Character creation is intentionally a near-copy of the pets menu economy
(faucet + store + list + per-entity actions), not a one-shot wizard.


Live GUI path that works (2026-07-27 menu-FX fix):
  Main → Faucet → Claim Tokens
       → Store  → Free Starter / Buy Clone
       → Avatars → Select → Cycle DNA / Sleep / Open Desktop Window
                 → Full DNA Editor (name/age)

  bash test-harn-same/scenarios/demo_menu_fx.sh

Root cause of dead menus: active_target_id stayed "main" so
${piece_methods} never loaded. Fixed via write_chtpm_bridge() +
panel_content filled from view.txt.
