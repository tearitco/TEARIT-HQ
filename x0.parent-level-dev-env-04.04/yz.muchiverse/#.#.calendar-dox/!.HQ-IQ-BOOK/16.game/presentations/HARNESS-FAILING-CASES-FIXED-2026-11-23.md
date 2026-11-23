# Failing harness cases, found and fixed (2026-11-22/23)

Owner order: "fix bugs and failing cases". Method: run EVERY `harness/*.pal` once, one at a time, niced behind `cpu_gate` (316 pal files, about 2 h on this machine), read each `results/<case>.txt.verdict.txt`, reproduce the failing step with the case's own command, find the cause, fix the code or the expectation and say which. A failing check was never deleted or weakened; where an expectation was stale it was replaced by one that asserts the new behaviour (and, where useful, a negative).

Result of the sweep: 49 cases red, 8 SKIP (live window needed: `live_*`, `irc_two_machine`, `deck_video`). After the fixes below 22 are green; the rest are blocked with evidence (last section).

## Fixed: cause, fix, counts (before -> after)

| Case | Cause (evidence) | Fix | Counts |
|---|---|---|---|
| `eat_page`, `page_follow`, `book_db`, `pet_cast`, `pvp_skin`, `play_cdda_hq`, `play_rpg_space_fridge`, `play_rpg_dust_planet_map`, `play_rpg_space_planet_map` | `setup.build` of the scratch copy of a pets-like skin failed: `petslike_core.c` includes `plug_ref.c` since 573f56a85 (D75, 2026-11-14) and no scratch-house case copied it. Reproduced with the case's own gcc line, the compiler names the missing include | `COPY`/`NEEDS plug_ref.c` in every case and phase table that lists `pet_footer.c` | eat_page 0/17 -> 17, page_follow -> 44, book_db 8/30 -> 30, pet_cast 6/24 -> 24, pvp_skin 2/33 -> 33, cdda 7/50 -> 50, fridge 6/29 -> 29, dust/space planet map 11/101 -> 101 |
| `play_df_hq`, `play_mcl_hq` | their phase tables had NO `NEEDS` rows at all (the scratch house had no core). Same table was already right for cdda | the cdda block (core, toolbar, sky, footer, plug_ref, nudge, notify, csv_frames, skin_lib, autotile, life, `CFLAGS -I`) added | df 3/36 -> 36, mcl 5/43 -> 43 |
| `play_rpg_space_blueprint` | (1) missing `petslike_life.c`; (2) `events/eat` (2026-10-28) is a 15th event and shifts every list index after `destroyed`: the case asked for 14 events and selected rows 10 and 6, which are now `planet` and `env_hit` | NEEDS row; 15 events; selection index 11/7 AND an assertion of the selected name (`selected: respawn`, `selected: environment`) so an index shift can no longer pass silently | 7/34 -> 34 |
| `pchq_levels` | (1) commit 6e5679010 (2026-10-29) made a map with no start cell and no event at x=8,y=8 spawn the hero at 8,8 (1,1 stays for cdda_sample's door and mineclonia): three expectations were stale. (2) `world_01/animals.txt` in the working tree is a runtime file the engine empties (0 bytes today) while the case read it as "the shipped state" | spawn rows now pos_x=8 and pos_y=8 for default, test_terraces, test_walls; the shipped animals.txt is read from git HEAD (`git archive`). Sabotage: pos_x=7 and `^hen,` both fail, restored and cmp'd | 95/99 -> 103/103 |
| `chutes` | a read raced the pets' figure projection (the check ran right after the p4 roll row, before events.txt was rewritten); it failed once in the sweep and passed on rerun | `EXPECT_HAS` -> `WAIT_HAS 8000` (same assertion, no longer racy) | 289/290 -> 290 |
| `digipet_needs`, `dsr_sim_step1`, `eden_conductor` | the clock catch-up cap is 12 per poll and the remainder collapses into a "caught up N" row since 285b13b3d (a 2,500-day backlog replayed 75 min at 100% CPU). A single `advance 30d` + `step` now fires 12 ticks, not 30 | a jump of N > 12 days is advanced in chunks of at most 12 days with one `step` each, so every day still ticks and nothing collapses (the cap itself stays covered by the lc_clock cases) | 103/111 -> 111, 129/156 -> 156, 222/231 -> 231 |
| `eden_install` | (1) `+x/install_eden.+x` was built 2026-10-07, before 3d4d45973 made `ctl.sh` derive its folder (stale binary, not in git): rebuilt. (2) the daemon fires at most once per 120 s and never two within 12 s (aa93e132a), the case waits 8 s for day 2 | rebuilt the binary; `ENV LC_CLOCK_MIN_FIRE_GAP_MS 0` and `LC_CLOCK_MIN_SPACING_MS 0` for that daemon run only | 115/119 -> 119 |
| `esc_first_publish` | the idle-cost viewer scan (`viewer_kinds`, N3) tests argv[0] for `khtpm_core_render` BEFORE joining the arguments; the stand-in was `sh -c sleep ... khtpm_core_render <path>` (argv[0] = sh), so no window was seen and nothing was published | the stand-in is a copy of `tail` named `khtpm_core_render` (as other cases do) | 3/7 -> 7 |
| `coin_shop_ui` | the same gate: the editor writes `ui.txt` only while a window of it exists; the case had no stand-in and had passed earlier only because a real editor window happened to be open | stand-in window added | 18/35 -> 35 |
| `harness_bank` | bank lanes (2cb5a61e3): `update` writes `bank/dev/` by default; the case still expected `bank/live/` | rows point at `bank/dev/`; new NEGATIVE: a developer run writes nothing to `bank/live/` | 15/19 -> 19 |
| `layout_check_editor` | the editor template uses 17 keys written only by a driven game (`gm_*`, `tp*`, `n_stamp`, `stamp_off`) and not declared in `layout_runtime_keys.txt` | declared with their writers (game_core.c, deskopoly_driver.c; the file's own rule: a named writer must exist, checked by grep) | 4/5 -> 5 |
| `pchq_playtest_action` | `stop` delegates to `doom_event.sh stop_game` since 960422130 and the scratch tree did not contain the script, so the verb did nothing | `COPY doom_event.sh` (it derives HOUSE from its own location) | 7/8 -> 8 |
| `plus_phone` | an `apps/rpg-edit` was installed in the live phone folder; the case asserts exactly two apps and removes the others by name | rpg-edit added to the removal list | 44/47 -> 47 |
| `quest_q019_phrases` | the "real original" conductor phrases file was overwritten by the worker's rows (468ebc4aa adopted them), so the judge saw no new rows and failed farm and question counts | the original is read from git (`468ebc4aa^`) | 33/36 -> 36 |
| `player_menu_playtest` | the Player menu rows are data in `#.desktop/livedesk_taskbar.pdl` since 2026-10-21; the scratch house had no copy, so the menu was one Cancel row (rebuilding the op gave the same output: not a stale binary) | `COPY livedesk_taskbar.pdl` | 8/19 -> 19 |
| `play_game_dev_hq` | the studio cast is now Ivo, Ness, Quill, Yara (d6321ad6e); the table named Rex and Pip. The staff reply is by the ACTIVE person (`g_active`), Ness after `select s2` | names updated | 17/21 -> 21 |
| spawn_lint (partly) | `teario-make/button.sh` and `teario-play/button.sh` used a bare `setsid nohup` | both go through `kh_spawn.sh` (owner 0 = keep, role tool); not run live (no windows) | see below |

## Blocked, with evidence

| Case | State | Why |
|---|---|---|
| `spawn_lint` 17/19 | 2 NEW sites | `@.apps/rta-board/button.sh:5` and `@.apps/vox/button.sh:5` are another agent's files this round (the baseline may only shrink; the same one-line conversion as teario-make applies) |
| `teario_platform` 67/70, `teario_tools` 70/71 | red | the uncommitted `axis=side` hunk in `game_core.c` (foreign, never to be committed) puts a new header line into the platform cells; its own case `teario_side_axis` is untracked. Not ours to change |
| `nb_crypto_random`, `nb_element_methods`, `nb_text_blob`, `nb_media` | red | `&.hq-apps/network/ops/+x/nb_js_worker.+x` is from 2026-10-05, source 2026-11-08; the case header says it needs wasm3, which is not in the repo (audit 2026-11-08 P1-5), so it "FAILS loudly" by design |
| `pet_stt_tts` 32/34 | `listen-text`, `listen-chat-line` | `state/stt/last.txt` is not written by the fake-`rec` path of `pet_listen.sh`; the other 32 rows (edge_tts + vosk) pass. Not diagnosed further (the case itself calls the Python of the pet-trainer) |
| `eden_loop` 304/305 | `fixed-seed-7-answer-is-the-known-tomom-hq-one` | expects the phrase "shining cosmos"; that string is nowhere in the tomom folder and the phone got the phrase-table sentence "Can you help me dig the new plot near the oak?" while `path=tomom` passed. The expectation looks like it was recorded from an older corpus; needs the owner to say which answer is right |

Not run: `live_*`, `irc_two_machine`, `deck_video` (SKIP by design, windows or network).

## Standing set observed in the sweep (same session)

vox 361, vox_desk 118, vox_keys 27, rta_keys 19, rta_board 42, rta_life 62, rta_pages 90, rta_city 11, mech_new 43, teario_gamekeys 24, teario_playwin 14, teario_make 64, teario_play 65, teario_window 56, game_new 233, in_session_follow 19, token_follow 23, work_router 40, work_ab_commit_scoped 8, tool_ab 25, tool_log 40, tool_score 39: all green.

## Lessons

- Scratch-house cases are lists of copies: when a shared file gains an `#include`, every list that copies its parent breaks. A generated phase table should list its needs from the compiler (`-M`), not by hand.
- A case that passes only while a live window happens to exist is not green (coin_shop_ui); the gates added for idle cost (viewer scan, fire gap, catch-up cap) all need an explicit stand-in or env in the case.
- Several sources named a "real original" or "shipped state" and read the working tree; a runtime file or an adopted merge makes that vacuous. Read it from git.
- One `python3 --version` was run by mistake at the start of the session (the house rule forbids even that); it touched nothing.
