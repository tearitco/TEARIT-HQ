# Final report: the CPU night (2026-11-03) and the editor / standards / h-ai sprint (2026-11-04)

Branch `claude`, pushed to origin `claude`, `main`, `grok`. House root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`. Companion documents: `09-appendix/handoff-2026-11-03-editor-palettes-cpu-night.md` (state, test guide), `08-roadmap/design-docs/SPRINT-2026-11-04-EDITOR-STANDARDS-HAI.md` (sections 1-15, the working log with every number), `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md` (22 pitfalls), `03-pitfalls/hq-cpu-safety.md` (10-10i), `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md` (rows added).
Legend: **VERIFIED** = observed in a fresh run; **STATE-LEVEL** = driven through the verbs/files, not by mouse or key in the real window; **UNVERIFIED** = built or syntax-checked only.

## 1. What changed, by area

### 1.1 CPU and runtime footprint (night of 11-03)
| change | evidence |
|---|---|
| `chtpm_rgb_render` no longer started for khtpm boards (nothing reads `rgb_frame.raw`) | VERIFIED: house during 3D camera use 106% -> 72% of one core; canvas still updated (screenshot diff) |
| load governor for the 3D frame stream; `frame_history.txt` capped; orphan game clock owned by the resident daemon | VERIFIED (idle house 1%; no engine frames after input stops; 400-key flood leaves no backlog) |
| `cpu_gate` reads the CPU die sensor (k10temp/coretemp) | VERIFIED: gate 70 C vs raw 72 C; refused a run at 87-89 C twice during the sprint |
| `cpu_guard`: clears a stale `%` label when the taskbar manager is dead; trims `kh_focus_debug.log` (>1 MB) and `active_session.pdl` (>16 KB) and dormant clock ledgers (>1 MB) | VERIFIED: dead-manager label cleared in ~80 s on the real path; the real sweep trimmed the ledgers (4.7 MB -> 131 KB, 1.2 MB -> 131 KB) |
| `spawn_lint` sees `setsid` inside C strings (`c-string-setsid`), 10 of 11 sites converted to the registered funnel | VERIFIED negative test (new unregistered `system("setsid ...")` -> exit 3); converted launches tested by build + one registry row, not each menu click |
| renderer: `kh_focus_debug.log` capped at the source (256 KB -> `.old`) | VERIFIED build; windows pick it up at relaunch |

### 1.2 The editor (rpg-edit-hq)
- **Default sidebars from a file:** `layout.pdl` (`LAYOUT | left | palettes`, `LAYOUT | right | hai`), preference `sidebar_start=layout` (default; Settings tab). VERIFIED on a real open.
- **Palettes:** level stack (palettes > tile type > sheet > tileset > tiles), paging past 48 tiles ("tiles 49-96 of 512", prev/next), df / doom / ohrrpgce / my-palettes driven and armed (STATE-LEVEL; CDDA paging also in the real window).
- **Voices:** 16 distinct VOICE rows (7 Kokoro voices x speed); two compared (different wavs, 1.94 s vs 2.65 s). Audibility NOT confirmed: the sink is at 28% volume (owner's setting, untouched).
- **Chat pane:** the chat box had never run anything (action path `${PKG}/ops/...` pointed at the board folder): fixed. Wrapped rows (no overlap), 12 rows, name row bold + body row softer, colours you / h-ai / pets, `/clear` `/apply` `/no` `/help` typed commands (no buttons, no footer), model typography mapped to ASCII, answers appear without another event. VERIFIED in screenshots.
- **Renderer quirk found and fixed:** a pinned overlay bar with a text input and no item drew nothing (`if (n_it == 0)` in the layout); now `n_it == 0 && n_cl == 0`. VERIFIED after relaunch.
- **Clock and save/load:** Time tab rows (start, pause, +10 min, +6 h; plus the existing speeds, +1 h, +1 d, reinstall) STATE-LEVEL; skip/turn fire due events exactly once (daemon up: the daemon; daemon down: the verb's runner-equipped step) VERIFIED (5 ok rows, 1 chat line); load restores the clock (`allow_settime_back`) VERIFIED in the editor, applied by one-line source change to the other 11 skins (UNVERIFIED: syntax-checked, not run).

### 1.3 h-ai
- **Editor pane (`hai_op`)**: locate (live window pid, verified against /proc), context file, Groq `openai/gpt-oss-20b` with a one-tool whitelist (`write_event_file`), proposal in a file, `/apply` writes + recompiles the page (only `rpg_edit_hq_verb` and `pet_says` nodes, non-destructive verbs, plain values), `/no` drops, old files kept as `.hai.bak`; quota guard (3 s gap, 200/day), key never printed.
- **First delegated editor task, end to end (VERIFIED in the real window):** "In the dawn phase event, make Ember say good morning." -> proposal -> `/apply` -> page recompiled -> the page ran from the REAL game clock and the chat showed `Ember: Good morning` once; a second delegated request corrected the doubled name.
- **Toolbar h-ai (open-hai):** active-session pointer `#.desktop/active_session.pdl`, `hai_op where` verifies rows, `@editor <question>` injects the live state; new Groq backend. VERIFIED in the real open-hai window: gpt-oss-20b answered room, selector and armed brush all correctly (gemma3:1b: 1 of 3); negative control with a dead pointer row -> "no live editor session".
- **Model probes:** LAN llama3-groq-tool-use:8b returns clean tool calls (10-30 s); Groq 1-2 s, 1000 req/day and 8000 tokens/min.

## 2. Bugs found and fixed (all with a cause)
1. Editor chat box ran a nonexistent binary (every typed line vanished).
2. `chtpm_rgb_render` composed 2 MB x 28/s for no reader.
3. Orphan game clock (owner 0, `keep`) ticking with every window closed.
4. `cpu_gate` read the wrong temperature sensor.
5. `%` tab frozen at a dead writer's last value.
6. Pinned bar with input but no item not drawn.
7. h-ai answers sat in the file until something else dirtied the daemon.
8. `kh_focus_debug.log` uncapped (16 MB in one window, 25 MB house-wide).
9. "Skip time" marked clock events `fail` (no event runner in the verb's step) so phase pages never ran; the obvious fix doubled every fire.
10. Load could not set the clock back (`refused:backwards`).
11. `usleep` implicitly declared in `pal_side.c`; `spawn_lint` blind to `setsid` in C strings.

## 3. Open problems and honest limits
- Not driven by mouse/relay in the window: the new Time menu rows; the palette kinds other than CDDA/RPG Maker; a turn that crosses a scheduled time in INT mode.
- The 11 other skins' binaries are not rebuilt; they still behave the old way until rebuilt (and their load line change is unrun).
- Speech never heard (sink volume).
- `open-hai` answers are read-only; markdown `**` and one missing glyph show in its font.
- The pet-chat command `fire <verb>` from a bare shell is read as chat text.
- Game state changed by testing: the editor's clock advanced about 2.2 days (cannot go back); `events/phase_dawn` now carries Ember's line (committed).
- 125 unregistered spawn sites remain in the baseline; `cpu_lint` backlog 569.
- Footprint of the new pieces: no polling processes; `hai_op` is one-shot (curl <= 90 s, niced); the guard sweep adds a few `stat` calls every 120 s.

## 4. Where things are (quick map)
`@.apps/rpg-edit-hq/`: `layout.pdl`, `ops/hai_op.c`, `ops/pal_side.c`, `ops/edit_prefs.c`, `ops/rpg_edit_hq.c`, `board/pchq-board.xhtpm|css`, `voices.pdl`, `events/phase_dawn/`. `&.widgits/open-hai/ops/khtpm_open_hai_manager.c` (`@editor`, Groq). `&.widgits/_shared-lib/petslike_core.c` (clock helpers). `&.hq-apps/proc-mon/ops/cpu_guard.c` (sweeps). `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` (overlay fix, debug-log cap).

## 5. Test guide (short; the long one is in the handoff)
Gate every round (`cpu_gate check`). Editor: relaunch through `cpu_gate run`, tap the chat field, type, Enter; `/help`. Delegated task: type the dawn request, `/apply`, run `play_event.sh events/phase_dawn <house> clock` or advance the clock past 06:00. Toolbar h-ai: `@editor which room am I in?`; negative: swap the pointer for a dead-pid row. Save/load: `save 8`, change the room and clock, `load 8`, check trainer and `clock_ms`. Restore: palette `_active` files by md5, the pointer file, `sessions/model.txt` (gemma3:1b).

## 6. Mouse (not started)
See the assessment given at the end of the session: sidebar rows, palette tiles, prev/next, the chat input and the clock rows are the click targets; relay `MOUSE_EVENT` first, real pointer after.
