# Per-screen module split - the real standard now, and the full
house-wide scope of what still needs it

Written 2026-07-31. Direct instruction: give every screen its own
`<module>`, proven here in muchi-pals first ("that is the true
standard i wanted... we will test that here before doing it
elsewhere"), then roll out to every other project and correct any
docs that argued otherwise (`04-module-and-manager-architecture.md`
already corrected).

## 1. What "proven here" actually means (so nobody re-derives this)

- TPMOS's own `orchestrator.c` (real source, `/home/no/Downloads/
  1.TPMOS_c_+rmmp.0100.0110/pieces/chtpm/plugins/orchestrator.c`)
  already runs keyboard/joystick/render/parser as threads in ONE
  always-alive process, separate from whatever `<module>` is running -
  every project in this house already has this via its own `system/
  orchestrator.c`. This part was never broken.
- The part that WAS missing everywhere: giving each SCREEN its own
  distinct `<module>` string. `launch_module()`'s real liveness check
  (`strcmp(launch_str, current_module_path) == 0 && current_module_pid
  > 0`) already supports this correctly - kill+relaunch only on a
  genuine module-string change, reuse otherwise - it was just never
  used that way anywhere in this house.
- Proven on muchi-pals: `pets_module.pal` prototyped first, confirmed
  via direct `debug.txt` evidence (real fork on navigation IN, real
  reuse while staying, real kill with no orphan on navigation OUT,
  correct first-frame render). Then rolled out to all six screens
  (`main_module.pal`/`user_module.pal`/`faucet_module.pal`/
  `store_module.pal`/`pets_module.pal`/`processes_module.pal`) and
  live-tested.
- Each new `.pal` module currently reuses the SAME shared ops
  (`muchi_menu_input`, `muchi_compose_frame`) every other screen's
  module also calls - only the module/process boundary changed.
  Splitting those shared ops into per-screen dedicated files (so each
  screen's own unique logic is genuinely separate code, not just a
  separate process wrapper around shared code) is a real, separate,
  NOT YET DONE next step - see §4.

## 2. Two real bugs found DURING this rollout, confirmed HOUSE-WIDE,
   not muchi-pals-specific

### 2.1 `clear_saved_active_index()` missing from EVERY local copy of
   `chtpm_parser_pal.c` in this house

This is the real, house-wide stale-focus-index fix (originally found
+fixed in `014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c` during
agy-txt/file-menu's own PITFALL-65 rebuild). Checked directly, every
project below is MISSING it (confirmed via `grep -c
clear_saved_active_index`, all returned 0):

  041.pal-chain⛓️
  041.pal-forum👥️
  044.pal-chat-irc👥️+2
  0.user-pal👤️/00.login-signup
  0.user-pal👤️/01.avatar-creation👤️
  045.muchi-pal-agent🤖️+1
  002.zoo__🦓️🐒️0000/02.z00-INK.lo.sur]PEN🏟️
  101.mutaclsym🧟‍♂️️+18.01, 300.rpg-xyz, 300.rtp-xyz

Only agy-txt/file-menu have it, because they're the only projects that
actually sync `chtpm_parser_pal.c` FROM `014.wsr-pal💸️📌️+2` - every
other project above keeps its own local, unsynced copy (confirmed
multiple times this session - the `sync_shared_op.sh` canonical source
these local copies are SUPPOSED to sync from,
`yz.muchiverse/2.muchi-verse/shared-ops/`, doesn't exist anywhere in
this house copy). The fix was ported into muchi-pals' own local copy
today (see `05-jul30-live-bugs-and-plan.md`) - the same port (copy the
function + insert a call before every real `parse_chtm()` layout-
transition site) needs to happen in every project listed above.
Logged as PITFALL 66.

### 2.2 Stray `DEBUG: Loaded directory_listing`/`last_key` printf spam

Present in some local copies, not others - inconsistent, purely
cosmetic (doesn't affect real behavior, just spams stdout on every
frame). Confirmed present in:

  041.pal-chain⛓️
  041.pal-forum👥️
  0.user-pal👤️/00.login-signup
  0.user-pal👤️/01.avatar-creation👤️
  002.zoo__🦓️🐒️0000/02.z00-INK.lo.sur]PEN🏟️

Confirmed ABSENT from: 044.pal-chat-irc👥️+2, 045.muchi-pal-agent🤖️+1,
101.mutaclsym🧟‍♂️️+18.01. Already removed from muchi-pals. Same 2-line
deletion (`load_vars()`'s own "modern layout" branch) fixes the rest.

### 2.3 The `${panel_content}` naming bug is NOT house-wide

Checked directly: pal-chain/pal-forum/pal-chat-irc's own layouts never
use `${panel_content}` - that rename (from `${game_map}`) was a
muchi-pals-specific choice. Not a house-wide gap, nothing to port
elsewhere for this one.

## 3. Full scope: what each project needs

| project | per-screen module split | clear_saved_active_index port | debug spam removal |
|---|---|---|---|
| 041.pal-chain⛓️ (6 screens) | needed | needed | needed |
| 041.pal-forum👥️ (6 screens) | needed | needed | needed |
| 044.pal-chat-irc👥️+2 (3 screens) | needed | needed | already clean |
| 0.user-pal👤️/00.login-signup (1 screen) | N/A - only 1 screen | needed | needed |
| 0.user-pal👤️/01.avatar-creation👤️ (6 screens) | needed | needed | needed |
| 045.muchi-pal-agent🤖️+1 (1 layout) | N/A - only 1 layout | needed | already clean |
| 002.zoo (1 layout) | N/A - only 1 layout | needed | needed |
| 101.mutaclsym🧟‍♂️️+18.01 (2 screens) + 300.rpg-xyz/300.rtp-xyz (own copies) | needed | needed | already clean |
| 01.muchi-pals-🥚️-13.01 (6 screens) | DONE 2026-07-31 | DONE | DONE |
| 102.agy-txt (2 module groups -> 4 real modules) | DONE 2026-07-31 | already had it (syncs from wsr-pal live) | already clean |
| &.widgits/file-menu (5 screens) | DONE 2026-07-31 | already had it (syncs from wsr-pal live) | already clean |

agy-txt/file-menu don't sync a LOCAL copy of chtpm_parser_pal.c at all
- they read wsr-pal's live copy directly, so they never had PITFALL 66
or the debug spam to begin with. Only the module split itself applied
to them.

## 6. REAL LESSON FROM THE agy-txt/file-menu ROLLOUT - read before doing
   the next project

Testing agy-txt's own K3 harness after the split produced 3 FAILs that
turned out to be 100% pre-existing (reproduced identically on the
fully-reverted, unmodified original code) - stale item-number
assumptions and an incompatible paste helper in demo_save_load.sh,
unrelated to the module split. Fixed those (see the script's own
2026-07-31 comments) before the split could even be properly verified.
**Always establish a clean pass on the UNMODIFIED project first** if a
harness fails after applying the split - don't assume the split is the
cause without checking.

Once agy-txt's own harness was genuinely fixed, the split itself
passed clean, repeatedly. file-menu's own harness (demo_save.sh) DID
fail because of the split, though - a real, expected consequence: the
main->browser_save transition now triggers a genuine kill+fork+exec
that didn't happen before (every screen shared one process), and the
harness's own fixed `sleep 0.5` after that specific navigation wasn't
enough real wall-clock time for the freshly-relaunched module to be
ready. Fixed by waiting on a real, deterministic signal
(`current_layout.txt` actually flipping to the target layout) instead
of guessing a bigger fixed sleep number - see demo_save.sh's own
2026-07-31 comment for the exact pattern to reuse.

**For every remaining project in the table above**: after applying the
split, expect any transition INTO a screen whose module differs from
where you just were to need this same kind of real settle-time check
in that project's own harness, if its own harness has tight, fixed
sleeps around that specific navigation. Not every transition needs
it (file-menu's own LOAD transition, using an existing 0.8s sleep, was
already fine) - only add the fix where a real failure demonstrates
it's actually needed, confirmed against the unmodified baseline first.

Projects with only one real screen (login-signup, muchi-pal-agent,
002.zoo) don't need the module split itself - there's only one screen
to give a module to, which is already what they have - but they still
need the `clear_saved_active_index()` port, since that bug can still
bite on OTHER real reset paths (active_target_id changes, GOTO:
commands) even within a single-layout project.

## 4. NOT yet done anywhere (including muchi-pals): splitting the
   shared ops themselves per screen

The module/process split (§1) reused the SAME shared compose/input ops
across every new per-screen module. The other half of the original
ask ("files that are shared... vs unique functionality... run as a
module in a new fork") - giving each screen its own dedicated op files
too, not just its own process wrapper around shared ones - is real,
separate, and not started anywhere yet. Worth its own pass once the
process-level split above is rolled out everywhere.

## 5. Docs updated as part of this

- `04-module-and-manager-architecture.md` - corrected, banner added at
  top pointing here.
- `#.haiku+/!.xyzos-pitfalls+1.txt` - PITFALL 66 added for §2.1.
- `#.haiku+/!.xyzos-standards+1.txt` - real per-screen-module guidance
  added, superseding §16's "shared module is sufficient" framing where
  it implied per-screen modules were unnecessary (§16's actual point -
  don't use INTERACT-gated modules for pure discrete menus - is still
  correct and unrelated to this).
- `#.haiku+/refactor-list-j30.txt` - new Phase 2 section added with the
  table in §3 above.
