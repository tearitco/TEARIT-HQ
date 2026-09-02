# completed-sym-list.md — symlink-migration Step 2 progress

Living checklist of the 19 projects needing Step 2 (`persist_session_state()` wiring etc.) after
the mechanical `ln -s` → `cp -r` swap (Step 1, already done house-wide). Full context:
`sim-smell-fix.md`.

## PROTOCOL (updated 2026-08-21 by human)
**ALL code fixes first, testing batched at the end.** This doc is the restart anchor: after each
project's fix, its entry gets a status + report + exact "resume from here" pointer. If cut off,
start at the first non-DONE entry in the queue below.

Status legend:
- **FIXED+TESTED** — fix applied, agent-tested (harness and/or live injection), awaiting human re-test
- **FIXED-UNTESTED** — fix applied, NOT yet tested (batched for end)
- **TODO** — not started

---

## ✅ FIXED+TESTED

### 1. `0.user-pal👤️/00.login-signup` — DONE 2026-08-20/21
- Added `persist_session_state()` to EXIT trap: copies back `users/`, `current_login.txt`,
  `xyzfs/session.pdl` before session dir is deleted.
- Added `export HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"` — without it the ops' house-root
  walk (broken by copies) silently created new-user xyzfs trees under `pieces/xyzfs/` (garbage;
  cleaned up).
- Fixed its own harness scenario (`demo_login_signup.sh`): mid-session assertions now read the
  SESSION copy; xyzfs tree assertions moved to real HOUSE-level layout; added post-exit
  save-data-survival check. Harness: FAIL → 17/17 PASS.
- **Agent live-tested with real account `jb`**: launch → logout → type jb → Log In → kill →
  everything persisted at real roots. Report: `../FRAME_REPORT_20260821-0000_login-signup-jb-persistence.txt`
- `.pre-symlink-swap` backup deleted after signoff.

**Human test**: `cd 0.user-pal👤️/00.login-signup && bash button.sh run` → log in/out with jb →
Ctrl+C → check `users/`, `current_login.txt`, `xyzfs/session.pdl`. Machine left logged in as jb.

### 2. `0.user-pal👤️/01.avatar-creation👤️` — DONE 2026-08-21
- Added `persist_session_state()` to button.sh: copies back session `pieces/world_01/` (clones,
  DNA/skin changes, inventory, local tokens) into real root before `rm -rf` of session dir.
  Merge-copy semantics (adds/overwrites, never deletes).
- Deliberately NOT persisted: `avatar_window_pids.txt` (truncated every launch by design).
- NO `HOUSE_ROOT` export needed here: all identity ops already take `USERPAL_LOGIN_ROOT`
  (exported by button.sh to real login-signup), and their house-root walk starts from that real
  path, so it resolves correctly under copies.
- Fixed BOTH harness scenarios (stale assertions, same class as project 1):
  - `demo_menu_fx.sh`: wallet reset/read moved `$LOGIN/$XYZ/...` → `$HOUSE/$XYZ/...`
    (xyzfs trees live at HOUSE level = parent of `0.user-pal`, NOT inside login-signup).
    Result: FAIL → **OVERALL: PASS**.
  - `demo_session_character_window.sh`: same path fixes ×4; cleanup now deletes test user's
    tree at house level; open-window assertion updated `Opened desktop window` → `Opened chara
    window` (op's actual message). Result: FAIL(3) → **OVERALL: PASS**.
- **Agent live-tested via keyboard injection** (`tk_inject_key` into session history.txt):
  booted session as jb → Enter→Faucet → KEY:2 claim → "Claimed 10 tokens! Balance: 10" written
  to jb's REAL house-level wallet via USERPAL_LOGIN_ROOT ✓ → killed session uncleanly →
  hydrated clone `5dfb09e9…` correctly merged back into real `pieces/world_01/map_lobby/` ✓.
  Persistence works even on unclean termination.
- Pre-existing quirk found (NOT caused by this migration, left as-is): sending TERM to
  `button.sh run` from outside while `keyboard_input` runs in foreground defers the EXIT trap
  until keyboard_input exits → session dir + one prisc module can leak. Normal Ctrl+C flow is
  unaffected. Optional future hardening: run keyboard_input with `&` + `wait`.
- Test debris cleaned (test users, stray sessions, 0.user-pal-level xyzfs debris). NOTE:
  avatar-creation has a stale local `xyzfs/` tree (session.pdl says user e84ab629…) — legacy
  debris from pre-migration runs when ops resolved roots locally; harmless (env hatches now
  bypass it), left untouched.

**Human test**: `cd 0.user-pal👤️/01.avatar-creation👤️ && bash button.sh run` → mint/customize a
clone → Ctrl+C → verify clone still there on next launch. Harness:
`bash test-harn-same/scenarios/demo_menu_fx.sh && bash test-harn-same/scenarios/demo_session_character_window.sh`

---

## ✅ FIXED+TESTED — batch testing COMPLETE (2026-08-21)

All patched 2026-08-21 by agent via uniform batch patcher (`persist_session_state()` inserted
into button.sh + called in the EXIT/INT/TERM trap right before `rm -rf` of the session dir).
Every patch: merge-copy semantics (adds/overwrites, never deletes), volatile files NOT copied,
all `bash -n` syntax-checked, plus `mkdir -p` guards on every copy line (fresh-checkout safe).
Copy-back sets mirror each project's own copy-IN list:

| # | Project | Copy-back set | Test result |
|---|---|---|---|
| 3 | `@.apps/aomorai-editor` | `pieces/system/{config,board,entities}.txt`, `widget_cmds/`, `board_widget_bridge.txt` | boot+clean-shutdown smoke PASS; mechanical PASS |
| 4 | `@.apps/civ-txt` | `pieces/system/{config,board,terrain_legend,entities}.txt`, `widget_cmds/`, `board_widget_bridge.txt` | **LIVE MUTATION PASS** (turn=2/treasury=99 → real root); harness demo_setup_and_turn PASS |
| 5 | `@.apps/my-biotech` | `pieces/system/config.txt` | harness fda_review PASS; research_and_end_turn **PASS after fixes** (see below) |
| 6 | `@.apps/my-lawyer` | `pieces/system/config.txt` | harness demo_ping PASS |
| 7 | `@.apps/myne-qrypto/qtc` | `wallets/`, `data/` | **LIVE MUTATION PASS** (pending_tx.txt → real root); signup_login_wallet harness **PASS after fixes** incl. new post-exit persistence assertion |
| 8 | `@.apps/tactics-txt` | `pieces/system/{config,units,board,terrain_legend,entities}.txt`, `widget_cmds/`, `board_widget_bridge.txt` | harness demo_setup_and_battle PASS |
| 9 | `@.apps/TSC_ELO` | `pieces/system/config.txt` | boot smoke PASS. pvp_duel FAIL = documented PRE-EXISTING (BOOT state, unrelated) |
| 10 | `@.apps/TSC_ELO/widgets/setup` | `projects/setup/pieces/setup/` | boot smoke PASS (cleanup slower than 3s but completes) |
| 11 | `+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00` | `location.txt` | boot smoke PASS |
| 12 | `&.widgits/file-menu` | `projects/file-menu/pieces/file-menu/` | boot smoke PASS |
| 13 | `&.widgits/tile-picker` | `pieces/system/picker_items.txt` | boot smoke PASS |
| 14 | `&.widgits/yahoo-broker` | `projects/yahoo-broker/pieces/{yahoo_broker,broker_widget}/`, `data/` | host-launched widget (`run-widget <root>`) — mechanical validation only; live test needs a host project |
| 15 | `@.apps/text-editor-xyz` | `docs/` → lands in REAL editor root `102.editor-📄️00.00/docs/` (EDITOR_DIR glob) | **LIVE MUTATION PASS** (persist_note.txt landed in 102.editor docs) |
| 16 | `@.apps/yahoo-app` | `data/` via `$BANK_SESSION` (/tmp session) | boot smoke PASS (self-exits on stdin EOF, trap cleans /tmp session) |

### Harness scenario fixes made during batch testing
- `qtc/test-harn-same/scenarios/demo_signup_login_wallet.sh`: mid-run wallet assertion read the
  REAL root (only worked under symlinks) → now reads the SESSION path mid-run + NEW step-8
  post-exit check that the wallet persisted to real `wallets/` via copy-back. Also: session now
  launched with `setsid` so the cleanup death-sweep can't kill the scenario itself.
- `my-biotech/test-harn-same/scenarios/demo_research_and_end_turn.sh`: same `setsid` fix
  (scenario was group-killed during its own cleanup after printing OVERALL: PASS).

### Known non-blockers
- First-run flake: biotech research scenario's 30s frame window can be eaten by cold-start
  compile; second run passed fully. Not a persistence issue.
- External `kill -TERM` on a running `button.sh run` defers the EXIT trap while keyboard_input
  is foreground (pre-existing everywhere); killing keyboard_input (or Ctrl+C in-UI) fires it
  correctly. Optional future hardening: run keyboard_input with `&` + `wait`.
- Test debris cleaned after testing: harnesstest* wallets, leftover sessions, strays.

### NO-OP — verified stateless, nothing to do
- `&.widgits/context-menu`, `&.widgits/event-editor`, `&.widgits/event-ez`: no persistent data
  copied into sessions and no state files at real root (`pieces/system/` empty); their ops write
  only per-session scratch (gui_state, display frames) or take explicit destination args
  (event-editor's `ee_import_to_world <world_01>`). Step 2 does not apply.

**Human re-test**: pick any row and do its recipe — run app, change listed state via UI, quit,
relaunch, confirm it survived. Restart pointer: nothing left to fix or agent-test; awaiting your
signoff per project (then delete that project's `.pre-symlink-swap` backup if you want).

## ⏳ TODO — Step 2 not started yet (0 left)

(none — fix phase AND agent-side batch testing complete 2026-08-21; only human signoff remains)

Notes for batch-testing phase:
- TSC_ELO harness has a known PRE-EXISTING failure unrelated to this migration (sim-smell-fix.md).
- Harness runner (from house root): `bash "$.crypts/harness-runner.sh" "<project-name-substring>"`.
- Machine currently logged in as jb (login-signup session.pdl).
