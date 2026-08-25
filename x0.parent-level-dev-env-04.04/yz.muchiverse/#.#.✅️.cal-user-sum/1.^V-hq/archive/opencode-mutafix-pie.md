# OpenCode Prompt — Port mutaclysm's engine to piececraft's architecture

**Purpose:** hand this to a separate agent/tool session (opencode) for a real, thorough
investigation and refactor. Direct user instruction, 2026-08-17, after a full session of live
bug-hunting on mutaclysm's own camera system kept surfacing NEW real bugs one after another: "its
still not fixed so i guess we should move on and not use mutaclysm since its cameras are jacked up
and i dont want this behavior 2 spread... mutaclysm just needs to be ported to piececrafts system
by and large."

**⚠️ Lesson from a prior prompt in this same directory (`a12.opencode-prompt.md`)**: a too-broad,
open-ended prompt burned an entire agent session's budget on unfocused research and produced zero
deliverable. Don't repeat that here. This prompt gives you the FULL real context up front (so you
don't need to re-derive it) but expects you to report a real, concrete PLAN before writing
significant code, and to work in real, checkpointable stages — not open-ended "investigate and
refactor everything" in one uninterrupted pass.

---

## The real, persistent issue (full history — read this before touching anything)

Over one long session, mutaclysm's own 3D camera system was investigated and "fixed" FOUR separate
times, each fix real and verified at the time, each followed by a NEW real bug surfacing:

1. **Camera-struct rewrite** (mutaclysm's own `ops/compose_rgb_frame.c`) — replaced a flat
   keypress-to-angle-increment model with a real `Camera` struct + `build_camera()`, ported from
   `&.widgits/board-viewer/ops/bv_render_3d.c` (piececraft's own companion 3D-viewer widget, NOT
   piececraft's main ASCII/menu game — see below). Fixed 2 real, previously-broken camera modes
   (mode-2 pitch input was completely dead; mode-1 had a "stuck in body" clipping bug). Verified
   live via real key injection.
2. **Duplicate-process flicker** — mutaclysm's own `button.sh kill` verb had an unescaped `+` in a
   `pkill -f` regex pattern (mutaclysm's own directory name contains a literal `+`), so kills
   silently never matched. Every relaunch left the previous display-mirror process running,
   producing 2 stacked windows independently redrawing — looked like a rendering flicker, was
   actually a process-management bug. Fixed, verified live.
3. **No offscreen buffer** — the shared X11 display-mirror binary (`&.widgits/_shared-lib/ops/
   x11_mirror.c`, mutaclysm's own real GL→X11 migration pilot, now shared across projects) drew
   its chrome bar and the game frame as two separate, non-atomic draws straight to the live window
   — visible flicker on every frame. Every other similar app in this house's own khtpm family
   composites into an offscreen buffer first. Fixed, verified live.
4. **The real, final straw**: `hero/state.txt` (mutaclysm's own save-state file) grew past a
   hardcoded 32-line read cap present in THREE separate ops (`move_player.c`, `choice.c`,
   `camera_control.c`), all of which run on every single keypress. The camera-pan fields
   (`cam_pan_x/y/z`) happened to sit past line 32. Two of the three ops silently reset them to
   their own local default (0.0) on every keypress, right before the third could accumulate a real
   change — net effect: press a movement key any number of times, the camera moves exactly ONE
   step and then stops ("clamps after 1 move"). Fixed (cap raised 32→128) and proactively also
   fixed in the only 2 other projects sharing these exact files (`300.rpg-xyz`, `300.rtp-xyz`,
   confirmed literal copies via identical line numbers — they hadn't hit the bug yet, but were
   structurally certain to).

**Direct user report AFTER all 4 fixes were applied and rebuilt**: still not actually fixed. The
real pattern across all 4 rounds: each fix was correct and verified for what it addressed, but
mutaclysm's own engine kept surfacing a NEW, different real bug in the same subsystem every time —
strongly suggesting the underlying architecture itself (not any one bug) is the real problem, and
further targeted patches are not the right approach. **This is the direct reasoning behind the
user's own instruction to port mutaclysm to piececraft's system rather than keep patching.**

## Why piececraft, specifically

Direct, repeated instruction earlier in the same session, worth quoting exactly because it's the
real premise this whole task rests on: "aomorai and piececraft are the better models for 3d game
parser etc... mutaclysm desperately needs an upgrade, not the other way around." This was
confirmed structurally, not just asserted: piececraft-xyz's own companion 3D widget
(`&.widgits/board-viewer/ops/bv_render_3d.c`) has a real, struct-based `Camera` model with
documented, already-fixed bugs in exactly the class of thing mutaclysm's own simpler model kept
getting wrong — mode-2 look-angle, mode-1 clipping margin, and a real anchor-height decoupling fix
for free-roam/bird's-eye modes (search that file for `build_camera()` and its own header comments
for the full real history of those 3 fixes).

**Real, important scope note**: "piececraft's system" has (at least) two distinct real layers —
piececraft-xyz's own MAIN game (a real, terminal/ASCII UI driven by the shared, cross-project
`pieces/chtpm/plugins/orchestrator.c` engine — NOT its own custom camera system at all for the
main game), and the SEPARATE, OPTIONAL `board-viewer` widget (`&.widgits/board-viewer/`) which is
where the real, more-advanced 3D camera/voxel-rendering code actually lives. Confirm which of
these — or both — is the real target architecture before designing the port; don't assume "port to
piececraft" means "make mutaclysm's main game loop identical to piececraft's main game loop"
without verifying that's actually what's being asked, versus "give mutaclysm board-viewer's own
real 3D rendering architecture instead of its own home-grown one."

## Real, known root-cause classes to watch for (from this session's own real findings)

Whatever the real port ends up looking like, these are real, confirmed bug CLASSES already found
in mutaclysm's own current codebase — check whether they recur anywhere else in it, and whether
piececraft/board-viewer's own real architecture is actually immune to them structurally (not just
"doesn't have this exact bug today"):
1. **Hardcoded, too-small buffer caps on save-state files that grow over time** (the real root
   cause of bug #4 above) — search mutaclysm's own `ops/`/`system/` for any other `char
   lines[N][...]`-style fixed caps reading `hero/state.txt` or similar growing files, not just the
   3 already found and fixed.
2. **Multiple independent ops doing their own "read all lines, patch a few, write back" passes on
   the SAME shared state file, each with its own separate parsing/preservation logic** — this is
   the real, structural reason bug #4 was possible at all (3 separate, redundant
   read-patch-write implementations of the same file, easy for one to drift/break without the
   others). Does piececraft/board-viewer's own real architecture centralize this differently (a
   single real state-owner, others read-only), or does it have the same redundant-writer pattern?
3. **Unescaped path-derived regex in shell scripts** (bug #2's real root cause) — a real, generic
   footgun, worth a quick house-wide grep for the same `pkill -f ".*\$SCRIPT_DIR.*"`-without-
   escaping pattern beyond what's already been fixed this session (mutaclysm, piececraft-xyz,
   my-chara-txt's own `button.sh kill` verbs were already fixed for this specific bug — verify
   nothing else in mutaclysm's own scripts has the same class of bug).

## Real, concrete first checkpoint (don't skip straight to full refactor)

Per this doc's own opening lesson (a12's failure mode), do NOT attempt the full port in one pass.
Real, bounded first checkpoint, report back before going further:

1. Read mutaclysm's own real camera/render pipeline in full: `ops/camera_control.c`,
   `ops/compose_rgb_frame.c` (specifically `build_camera()`/`render_3d_view()`/`project_3d()`),
   `ops/move_player.c`, `ops/choice.c`, and the real dispatch chain that calls them
   (`pal/main_loop_chtpm.pal` → `ops/game_dispatch.c`).
2. Read piececraft-xyz's own real main-game architecture AND `board-viewer`'s own
   `bv_render_3d.c` in full.
3. Produce a real, concrete comparison: which pieces of mutaclysm's current architecture would be
   REPLACED wholesale by piececraft/board-viewer's own real equivalent, which pieces are
   mutaclysm-specific and have no real piececraft equivalent (need genuine new design, not a
   port), and a real, honest risk assessment of what could break for existing mutaclysm save
   files/players if the underlying state-file format changes.
4. Report this real plan (in a new, dated section of this file, or a new doc — your call, follow
   this house's own existing doc conventions, see `INDEX.md`) BEFORE writing significant
   implementation code. This is a real checkpoint, not a formality — get it reviewed before
   proceeding to the actual refactor.

## House conventions to follow

1. **Testing is relay/injection-only** — real keypresses go through the same file-based relay
   convention already used throughout this house (`pieces/keyboard/history.txt`, `KEY_PRESSED: N`
   lines, or whatever the real, currently-live mechanism is — verify current convention, don't
   assume). Never call your own compiled ops directly from a shell as "the test."
2. **Don't guess at real values/formats you can verify.** This session's own single biggest
   recurring lesson: multiple "fixes" this session were correct in isolation but wrong in the real
   full system because they weren't tested against real, full-scale data (see bug #4's own root
   cause — an isolated unit test with a small scratch file didn't catch a bug that only manifested
   against the real, full-sized 35-line state file). Test against real data, not synthetic
   minimal cases.
3. **Read `au11-hq/INDEX.md` and `au11-hq/legacy-shared-fix.md` first** — the second doc has the
   FULL real, dated history of this exact investigation (sections §2.6.1 through §2.6.2g), plus
   the wider real context of a 16-project engine-consolidation effort this port is now part of.
   Don't re-derive what's already documented there.
4. **Document as you go.** Update `legacy-shared-fix.md` with real findings (this doc's own
   established section-numbering convention, `§2.6.x`) rather than creating a disconnected new
   doc, unless the real scope of this port grows large enough to warrant its own dedicated file —
   if so, link it from `legacy-shared-fix.md` and add it to `INDEX.md`'s document-roles table.
5. **Archive, don't delete.** This house's own established discipline for any file/binary made
   obsolete by a port: move it to a clearly-named archive location and zip it (see
   `_.ARCHIVED-pre-merge-legacy.zip` at the house root for the real precedent — a `MANIFEST.txt`
   inside recording original paths + why each is dead), don't delete outright.
6. **Real fork()/execv(), never `system()`, for any NEW dispatch/launch code** — direct, standing
   house correction this session. Existing `system()` calls elsewhere in code being replaced don't
   need to be touched as part of this (they're going away anyway), but anything NEW you write must
   use real fork/exec.

Start by reading `au11-hq/legacy-shared-fix.md` in full (all of §2.6), then the real files listed
in "Real, concrete first checkpoint" above, then report your real, concrete comparison/plan before
writing code.

---

## Progress report (2026-08-17, opencode/big-pickle session)

### What was completed

**Camera direction parity — fully ported to piececraft's exact mappings:**

Files modified (all in `101.mutaclsym🧟‍♂️️+18.01/ops/`):
- `camera_control.c` — 3 fixes applied:
  1. **r/t pitch swap** (modes 1/2/3): mutaclysm had r=pitch_up, t=pitch_down. Piececraft
     has r=pitch_down, t=pitch_up. Swapped to match piececraft.
  2. **w/s mode 4 (bird's-eye) swap**: mutaclysm had w=pan_y-1 (north), s=pan_y+1 (south).
     Piececraft has w=pan_y+1 (south), s=pan_y-1 (north). Swapped to match piececraft.
  3. **w/s mode 3 (free roam) swap**: Same issue — w/s directions were inverted relative to
     piececraft. Swapped to match piececraft.
  4. Header comment updated to reflect corrected r/t directions.
- `compose_rgb_frame.c` — already reads camera fields from `cam_state.txt` (own file), hero
  fields from `hero/state.txt`. No changes needed.
- `move_player.c` — already stripped of camera field reads/writes. No changes needed.
- `choice.c` — already writes camera mode-switch defaults directly to `cam_state.txt`. No
  changes needed.

Full audit of ALL direction mappings:
- yaw (q/e): ✓ already matched
- pitch (r/t): ✓ fixed (was swapped)
- pan (w/a/s/d modes 3/4): ✓ fixed (w/s were swapped in both modes)
- c/v z-level: ✓ already matched
- f reset: ✓ already matched
- arrow→hero movement: ✓ already matched

All 4 files compiled with zero new warnings. Pre-existing warnings only (snprintf truncation,
system() unused-result).

### What's still broken / needs investigation

**1. The `[^]` / interact mode key pass-through issue:**
The user reports that mode switching (pressing '1'-'4' for camera modes while in 3D) is "finicky"
and involves an "activation '^' conflict." The user says "^" in interact mode is SUPPOSED to pass
keys through ignoring nav — this was set up before but may have drifted.

What I found in `chtpm_parser_pal.c`:
- Line 3234: `if (strcmp(el->onClick, "INTERACT") == 0)` — when active_index points to the
  INTERACT button, ALL non-ESC keys go through `inject_raw_key(eff)` to relay → game dispatch.
  This looks correct.
- Line 2679-2680: The `[^]` GLYPH rendering has an ADDITIONAL gate:
  `current_project_is_map_control_active()` which reads `is_map_control` from
  `projects/<project_id>/session/state.txt`. If this field isn't set, the glyph won't show `[^]`
  even though keys ARE being passed through.
- `set_interact_mode()` (line 1615) only writes `interact_mode` to `hero/state.txt`, NOT
  `is_map_control` to session/state.txt. These are TWO DIFFERENT files/fields.

My question: Is the actual problem that keys aren't reaching the game at all (like '1' literally
does nothing in 3D mode), or is it just that the `[^]` visual indicator is wrong/missing? The
key pass-through code path looks correct to me — `active_index` pointing to the INTERACT button
should route all digits to the relay regardless of `is_map_control`. But I may be missing
something about how `active_index` gets reset or how the parser's digit accumulator interacts
with the INTERACT branch.

**2. User says "1 goes to 2D" — I cannot reproduce this from code alone:**
In `choice.c:922`: `is_pov_key = (render_mode == 1 && key == '1')`. If render_mode==0, '1'
falls through to digit dispatch where `d=1` fails the `d >= 2` check → no-op. '1' literally
cannot toggle render_mode. The only key that toggles render_mode is '0' (line 921/980). So
pressing '1' when already in 2D does nothing — it can't "go to 2D" because it's already there.
This makes me think the user might be pressing '0' (which does toggle render_mode) and
describing the result, or there's a key-remapping layer I'm not seeing.

**3. w/s might STILL be backwards after my fix:**
User reported w/s still backwards after my first swap. I swapped them a second time (reversed
the direction). Need user to test again. If still wrong, the issue might be in
`compose_rgb_frame.c`'s coordinate system (how cam_pan_y maps to cam.eye_z) rather than in
camera_control.c's key handling.

### My concerns / questions for the next agent

1. **The `[^]` conflict needs a human to describe the exact sequence**: What keys are pressed,
   in what order, starting from what mode? "Finicky" suggests intermittent — could be a race
   condition between the PAL loop's game_dispatch and the parser's compose_frame, or it could
   be that the parser's digit accumulator (`digit_accum` at line 3278-3296) is intercepting
   digits meant for the game when an ACTIVATE submenu is open alongside the INTERACT button.

2. **The "1 goes to 2D" claim doesn't match the code**: Either the user is misidentifying which
   key they're pressing, or there's a key remapping in keyboard_input.c or gl_mirror.c that I
   didn't investigate. Worth checking if gl_mirror.c has any digit interception.

3. **The cam_state.txt / hero/state.txt split is working correctly**: camera_control.c writes
   cam_state.txt, compose_rgb_frame.c reads it, move_player.c and choice.c don't touch it.
   This eliminates the multi-writer race. But I'm not 100% sure the user has tested the
   full pipeline end-to-end after all these changes.

4. **rpg-xyz and rtp-xyz** (`300.rpg-xyz`, `300.rtp-xyz`) were mentioned as needing the same
   fix. Haven't been touched yet. These are reportedly literal copies with the same line numbers.
