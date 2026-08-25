# mutaclsym — handoff / progress checklist

Quick-scan status doc. This is NOT the architecture reference — that's
`01-cdda-architecture.md` (dense, phase-by-phase, has the actual design
reasoning and citations). Read this first to orient, then go deep in
that doc for whatever phase you're touching. Two sibling docs live one
level up, at the repo root (`xxx.the.uhogs.xxx-book-0.0/`):
- `nav-refactor-2.txt` — full history of the numbered-choice/
  accumulator UI system (mutaclsym + egg-pals both), including a
  documented wrong-first-attempt and its correction.
- `platform-vision.txt` — the not-yet-built macro orchestrator
  (`locations.txt` launcher across mutaclsym/egg-pals/future games)
  and cross-game item/pet portability + trading vision. Pure vision,
  nothing implemented.

Egg-pals itself lives at
`egg-pals/z0.egg-pals+11=MAC+WIN/egg-pals/` (versioned path, not the
flat `egg-pals/` dir) — check there for its own `dox/`.

## Before you touch anything

- **Check for a live session first**: `ps aux | grep -E "prisc\+x pal|keyboard_input$|\./system/renderer"`.
  If any of those three are running, DO NOT write to `pieces/` directly
  (a real player may be mid-game) and don't run your own test binaries
  against the live tree either. Copy the whole project to a scratch dir
  and test there instead - this has been the standard practice all
  session and it matters: a real process-collision incident happened
  earlier (two of my own orphaned background test processes wrote to
  the same live `hero/state.txt` concurrently and produced corruption
  that looked exactly like a game-breaking bug). `kill -9 <pid>` +
  `sleep` + re-check `ps aux` before assuming a background test process
  is actually dead - `pkill -f <pattern>` is unreliable here since ops
  are invoked via relative paths (`./system/prisc+x`), so path-based
  patterns often don't match.
- **Always `./button.sh compile` then `./button.sh check`** before
  claiming anything works. Zero warnings is the bar, not "it compiles."
- Rebuilding binaries WHILE a live session is running is itself a real
  risk, not just running your own test processes against it - the
  live session's very next keypress execs whatever's currently on disk
  in `ops/+x/`, including a transiently broken mid-edit version. Prefer
  finishing + testing a change in a scratch copy first, then rebuilding
  the real tree only once it's known-good.
- **Verify `pwd` before any script that writes RELATIVE paths**,
  especially a `python3`/heredoc block - the harness's shell cwd resets
  to the real project directory between tool calls with no warning, so
  a script that assumes it's still sitting in a scratch copy from two
  calls ago will silently write into the live tree instead. This
  happened for real while authoring the bigger `map_02` below (caught
  immediately by checking `wc -l` on the output and cross-referencing
  against the real project path - no live session was active at the
  time and the hero wasn't on `map_02`, so no actual damage, but it
  could easily have gone the other way). Always `cd "$TEST"` (or use
  absolute paths throughout) inside the SAME command block that does
  the writing, not a `cd` from an earlier, separate tool call.

## Status checklist

- [x] Phase 1 — terrain/furniture/map transitions
- [x] Phase 2 — items/inventory (physical `rename()`-based, russian-doll
      nesting - a piece's location IS which directory it's in)
- [x] Phase 3 — metabolism (hunger/thirst/stamina, verified tick order)
- [x] Phase 4 — monsters/NPCs: chase AI + bidirectional melee. Ranged
      combat, corpses/loot, real NPCs (non-hostile/dialog) still open.
- [x] Phase 5 — crafting, with a real overlay picker panel (not just
      auto-craft-first-satisfiable)
- [x] Inventory/examine overlay panel (second panel type, proved the
      overlay mechanism is genuinely reusable)
- [x] Persistent message log (`pieces/display/message_log.txt`) -
      replaced a single `msg` field that silently clobbered same-turn
      messages from different ops
- [x] Numbered-choice/accumulator UI (mutaclsym action bar + egg-pals
      menus) - see `nav-refactor-2.txt` for the full history including
      the corrected wrong-first-attempt
- [x] §5a Camera/viewport - maps can now be bigger than the 40x16
      terminal viewport, camera follows hero, clamped at edges.
- [x] `map_02` expanded to a real 80x32 outdoor area (tree-bordered
      forest, dirt path, a small building with door/window, a pond,
      a rubble/ruin patch) - the first real map that actually exercises
      the camera. Verified: camera correctly clamps/centers at multiple
      positions across the map (not just the synthetic 80x32 test map
      used to verify the camera code itself), collision against trees
      works, both transitions (`map_start`↔`map_02`, door at (30,11) ↔
      stairs at (4,4)) land at the correct coordinates. **No items or
      monsters placed on it yet** - structurally sound and playable,
      but empty/safe. Small, low-risk follow-up.
- [x] Phase 7 — Save/load + title screen (New Game / Load), modeled on
      real 1.TPMOS op-ed's save-folder pattern. Single-process
      title→gameplay hand-off, no restart.
- [x] Bug fix: `active_panel` field corruption (glued-together lines
      from a since-identified rebuild-during-live-play race) could
      permanently block movement while turns kept silently ticking.
      Fixed at the source (self-healing) and defensively (fail-open).
      Added a `Last key: N` HUD indicator and a "You can't go that
      way." blocked-movement message as part of the same fix.
- [x] Optional GL/RGB mirror (`ops/compose_rgb_frame.c` +
      `system/gl_mirror.c`, both CPU-computed-RGB / one-quad-blit-only
      per the RISC-V-portability constraint - see
      `2.muchi-verse/GRAND-ARCHITECTURE.md`'s GOVERNING CONSTRAINT and
      §0b for the full writeup). Tiles, HUD text (ported bitmap-font
      pipeline), and keyboard/arrow input forwarding into the GL window
      are all working and verified. `./button.sh run` now launches it
      AUTOMATICALLY alongside the terminal (best-effort - skips
      gracefully if `system/gl_mirror` wasn't built, e.g. no GLUT dev
      libs/no display; set `NO_GL=1` to opt out explicitly). This was a
      real gap between stated intent ("pop up and be controllable at
      the same time") and what had actually been wired - `gl` used to
      be a fully separate manual verb, not part of plain `run`. Fixed
      2026-07-16 by starting `system/gl_mirror` alongside `renderer`/
      `prisc+x` in `run`'s own backgrounded-process block, tracked and
      killed the same way. `./button.sh gl` still exists, now for
      (re-)launching just the window standalone (e.g. after a
      `NO_GL=1` run, or reopening a closed window without restarting
      the whole game).

      **Real regression found and fixed the same day, in two parts**:
      auto-launching broke BOTH the terminal and the GL window's own
      input.
      1. `gl_mirror.c` didn't catch `SIGHUP` (unlike `keyboard_input.c`,
         which always did) - closing the controlling terminal delivered
         an uncaught SIGHUP, skipping `atexit()` entirely and leaving
         `pieces/system/gl_focus.lock` stale on disk. Fixed by adding
         the same `SIGHUP` handler `keyboard_input.c` already had.
         `button.sh run`/`kill` also now defensively `rm -f` that lock
         at startup, matching real 1.TPMOS's own `run_chtpm.sh`
         "JOYSTICK FIX" precedent (its own comment documents hitting
         this exact stale-lock class of bug once already).
      2. **The bigger one, a real design mismatch, not just a stale-
         lock edge case**: `keyboard_input.c`'s `append_key()` used to
         check that same `gl_focus.lock` file and silently back off
         from writing to `history.txt` whenever it existed - written
         unconditionally the instant `gl_mirror` starts, REGARDLESS of
         which window actually has real OS input focus. Since two
         separate real GUI windows (a terminal emulator and a GLUT
         window) already can't both receive the same keystroke - the
         OS/window manager only ever delivers it to whichever one
         currently has focus - this file-based lock was solving a
         problem that doesn't exist for plain keyboard-vs-keyboard
         arbitration, and was actively harmful: it made the terminal
         permanently deaf to real keystrokes from the moment
         `gl_mirror` started, even while the terminal still had genuine
         focus. Root-caused by reading real 1.TPMOS's own comment on
         this check (it exists there specifically because
         `joystick_input.c` ALSO shares the lock, and a joystick reading
         a raw device node has NO window-focus concept at all - the
         lock is the only arbitration mechanism that case has).
         Confirmed via `z0.egg-pals+plats-8.00`'s own
         `system/keyboard_input.c` (same pal family, has its own GL
         window via `system/egg_window.c`): it has NO such check at
         all, because `egg_window`'s input is mouse-driven, not a
         second competing keyboard source - proof this check was never
         a universal requirement, only a joystick-specific one.
         mutaclsym has no joystick (see below). Removed the check
         entirely; OS-level window focus is the real, sufficient
         arbiter, exactly matching the direct requirement stated for
         this fix ("terminal should have auto focus unless gl window
         is clicked"). `gl_mirror.c` still WRITES the lock file (kept,
         harmless, ready for a real future joystick reader to consume
         the same way `keyboard_input.c` used to) - only
         `keyboard_input.c`'s own consumption of it was wrong.

         A separate attempt to fix this by forcing real X11 input focus
         onto the GL window in code (`XSetInputFocus`/`XRaiseWindow`
         right after `glutCreateWindow()`/on first `reshape()`) was
         tried, tested (confirmed via `XTestFakeKeyEvent`-synthesized
         keypresses that `gl_mirror`'s own GLUT callbacks work correctly
         whenever the window genuinely HAS focus - that part was never
         broken), found NOT to reliably move real focus under a Wayland/
         Xwayland session (confirmed via direct `XGetInputFocus`
         before/after - unmoved), and was reverted per direct
         instruction not to invent a new mechanism when an existing,
         simpler one (removing the over-applied lock check) already
         does the job using the OS's own native behavior.

         Both directions re-verified after the real fix, via a real
         pty-driven `keyboard_input` process (arrow-up + quit correctly
         appended to `history.txt` while `gl_mirror`'s lock file was
         genuinely present) and a real `XTestFakeKeyEvent`-synthesized
         keypress into the actual GL window (also correctly appended)
         - both paths work independently, arbitrated purely by which
         window has real OS focus, no file lock involved anymore.

      `ops/+x/dump_rgb_png.+x` dumps the current frame as a real
      viewable PNG (debug-only, not wired into the game loop). Joystick
      input has a real, understood porting path (see §0b) but isn't
      implemented - no hardware here to test it against. If it's ever
      built, THAT is the real, legitimate use case for a
      `gl_focus.lock`-style check in `keyboard_input.c` again - see the
      real 1.TPMOS precedent this section cites.

      **Two more real bugs found and fixed the same day**:
      3. **GL visibly lagged behind the ASCII renderer** during rapid
         movement. Root cause: `gl_mirror.c`'s own change-detection
         watched `rgb_frame.raw`'s `stat()` (size + mtime) directly -
         but that file is a FIXED-size RGBA32 buffer (always exactly
         `WIDTH*HEIGHT*4` bytes), so its size never changes between
         frames, and POSIX `st_mtime` only has 1-SECOND resolution (not
         the nanosecond `st_mtim`) - multiple frames written within the
         same wall-clock second (normal while holding a movement key)
         were silently missed. `system/renderer.c` never had this
         problem because it already watches a dedicated, monotonically-
         growing pulse file instead (`pieces/display/frame_changed.txt`,
         grown by 2 bytes every tick via `prisc+x.c`'s own
         `OP_HIT_FRAME` - see that file's own header comment: "pulse
         marker file's SIZE, never mtime"). Fixed by making
         `gl_mirror.c` watch that exact same file instead of inventing
         its own weaker mechanism - re-verified by writing 6 rapid,
         distinct frames within one wall-clock second and confirming
         `gl_mirror`'s own receipt checksum matched the final frame's
         real checksum exactly, not a stale earlier one.
      4. **Title screen had no arrow-key navigation** - `ops/
         title_input.c` only ever handled digits + Enter (by original
         design, not a regression), so testing arrows there looked like
         "input is broken" when it was really just an unimplemented
         screen. Added real `ARROW_UP`/`ARROW_DOWN` handling (clamped,
         not wrapped, at either end of the New Game/Load list) as an
         ADDITION alongside the existing digit-jump input, not a
         replacement - both work simultaneously now, matching
         gameplay's own wasd-or-arrows dual input. `ops/
         compose_title_frame.c`'s hint line updated to mention it.

      **RESOLVED** (was flagged "open, unresolved" earlier the same
      session): real arrow-key movement via the GL window was initially
      reported as still not moving the hero, despite the write/read
      mechanism testing sound end-to-end in isolation. Root cause turned
      out to be exactly the title-screen limitation named in item 4
      above, not a GL-specific bug - the user was testing at the title
      screen (arrows genuinely didn't do anything there, correctly, by
      the pre-existing digit-only design) and concluded input was broken
      generally. Confirmed working correctly once past the title screen
      into real gameplay. User's own words after retesting: "everything
      is working as expected now."

      **5. GL never showed the action-bar footer at all** ("I don't see
      the ASCII options in GL") - a real, narrow, now-fixed gap, NOT the
      same "menu needs to be piece.pdl-driven" problem wsr-pal once had
      (mutaclsym's ASCII action bar was ALREADY piece.pdl METHOD-table-
      driven from the start - see `dox/03-menu-parser-and-interact-
      mode.txt` for the full investigation). `ops/compose_rgb_frame.c`
      simply never got the matching footer row `compose_frame.c`
      already has. Fixed by adding `build_action_footer()` - a direct
      copy of `compose_frame.c`'s own `build_choice_footer()`, same
      `pdl_reader.+x hero list_methods` call. `FOOTER_ROWS` 1->2,
      `gl_mirror.c`'s `HEIGHT` 288->304 to match. Live-verified via
      `dump_rgb_png.+x`: the `[>]` cursor marker correctly tracks the
      real `action_cursor` value. Known limitation, not fixed this pass:
      the GL frame's fixed 640px/80-char width clips the footer once
      several actions are visible (a terminal doesn't have this problem
      - it just keeps printing). See `03-menu-parser-and-interact-
      mode.txt` §4 for the options considered and left open.

      A bigger, explicitly NOT-started-this-pass question was also
      raised: should mutaclsym's whole menu/action-bar system be
      "redone using chtpm_parser but in pal, as ops," matching wsr-pal's
      own proven pal-native METHOD-table rebuild (`ops/wsr_menu_input.c`
      + `ops/wsr_compose_frame.c` in `yz.muchiverse/2.muchi-verse/
      wsr-pal/`)? Investigated and written up in full in `dox/
      03-menu-parser-and-interact-mode.txt`, including real 1.TPMOS's
      `is_map_control` "interact mode" concept (`wraith-alpha_manager.c`)
      the user specifically asked about - confirmed NOT currently needed
      for mutaclsym's feature set (nav already passes through for free
      since movement and menu-digit keys never overlap), but named
      explicitly for when it WOULD become real work. Read that doc
      before starting any menu-system rework - it has the concrete
      blanks left for exactly that.

      Previously-flagged remaining explanation, kept here for the
      record even though the actual root cause turned out to be the
      title-screen issue above: the GL window may still not receive real
      OS-level keyboard focus at
      all in the field (the same class of issue already confirmed once
      this session - `XSetInputFocus` failing to move real focus under
      a Wayland/Xwayland test session), meaning GLUT's callbacks may
      simply never fire there, rather than firing and the resulting
      keycode somehow failing to reach `move_player.c`. Needs a live
      diagnostic (e.g., a counter file `gl_mirror.c` increments on
      every real callback invocation, checked against whether it's
      moving at all) to actually distinguish "callback never fires" from
      "callback fires but the write/read chain misbehaves" - not yet
      built.
- [x] **SUPERSEDED, see the real xlector entry further down**: the
      "Interact mode" writeup immediately below (arrow-cursor moving the
      ACTION BAR, not the map) was the first, wrong attempt at this
      feature - the user reported no visible difference in actual play
      and pointed back at `01-cdda-architecture.md`'s own real xlector/
      active-target citation. Left in place for the historical record
      (the GL panel rendering / bug-fix work it describes is still real
      and still stands), but do not treat its own "interact_mode"
      description as current - see the later, corrected entry for what
      `interact_mode` actually does today.
- [x] **"Interact mode"** (`hero/state.txt`'s `interact_mode` field) +
      **GL panel rendering**, both built and live-verified 2026-07-16.
      Adapted from real 1.TPMOS's `is_map_control` (`wraith-alpha_
      manager.c`) - researched in full, documented in `dox/
      04-chtpm-parser-research-and-interact-mode.txt` alongside a full
      `chtpm_parser.c` writeup (METHOD-table parsing, its real ESC_KEY=27
      universal-escape convention, confirmed-and-corrected citation for
      its wraparound arrow-cursor-nav code, joystick keycode conventions).
      `'i'`/`'I'` toggles `interact_mode` (outside a panel only); while
      on, `ARROW_UP`/`ARROW_DOWN` move the action-bar cursor instead of
      the hero (`move_player.c` suspends movement, same fail-open
      reasoning as its existing `active_panel != "none"` suspend).
      Digits still directly jump the action bar regardless - a pure
      addition, never required for keyboard play. `27` (Escape) closes
      an open panel without acting, or exits `interact_mode` if none is
      open - required ZERO changes to `keyboard_input.c`/`gl_mirror.c`
      (confirmed both already forward a bare Escape as raw byte 27
      unmodified). Panels also gained `ARROW_UP`/`ARROW_DOWN` cursor
      movement (previously digit-only), independent of `interact_mode`'s
      value - a real usability improvement for keyboard players too.
      **GL panel rendering** (the "id like to see menu in gl" ask): a
      new `draw_panel_box_gl()` in `ops/compose_rgb_frame.c` mirrors
      `compose_frame.c`'s own `draw_panel_box()` exactly (same content -
      recipe/inventory formatting duplicated directly from that file's
      `main()` - just a filled background rectangle instead of
      space-padding a text grid, then the same row text blitted via the
      already-built `blit_text()`). GL's action-bar footer also shows
      `[i] Interact[ON]`/"Menu" vs "Move", matching ASCII's own updated
      hint. **Joystick input was explicitly deferred** to a later pass
      per direct instruction - fully designed (translating a joystick's
      d-pad/buttons directly into mutaclsym's existing keycodes rather
      than the reference's own separate numeric range, so no downstream
      op needs any changes once it's built) and written up in the same
      new dox doc, not vague future work.

      Live-verified via the playable-interface method (real keycodes
      into `history.txt`, backgrounded `prisc+x`/`renderer`, reading
      `current_frame.txt`/`hero/state.txt`): `'i'` toggles
      `interact_mode` and the footer hint together; `ARROW_DOWN` walked
      the action-bar cursor 2→3→4→5, landing on Craft; `Enter` opened
      the real craft panel; `ARROW_DOWN` moved its cursor 1→2; `27`
      closed the panel (interact_mode stayed 1); a second `27` exited
      interact_mode (footer reverted to "Move"/no `[ON]`); `'d'` then
      moved the hero normally (pos_x 4→5, confirming zero regression);
      digit `'3'` still directly jumped the action-bar cursor to Drop.
      GL panel rendering separately confirmed via `dump_rgb_png.+x` with
      the craft panel open - background fill, border, cursor marker, and
      recipe list all rendered correctly, matching ASCII exactly.
- [x] Bug fix (found via the GL mirror's debug PNG, not by inspection):
      `move_player.c` and `tick_monsters.c`'s `glyph_walkable()` were
      treating the wall glyph's own registry row (`#|t_wall|...` - `#`
      IS the wall glyph) as a comment line and skipping it, because the
      comment test was just `line[0] == '#'`. Invisible until now by
      pure coincidence (the miss-fallback, "not walkable", happens to
      already be the correct answer for walls). Fixed in both files
      plus `compose_rgb_frame.c`'s own copy of the same lookup shape -
      the real test is `line[1] == '|'` (a data row is always exactly
      one glyph char before its first pipe), not `line[0] == '#'`.
- [x] Monster `decision_mode` (preset + weighted, matching the family's
      `preset|weighted|rl|llm|human` chassis - see
      GAME-AI-SPEED-DOCTRINE.txt): `ops/tick_monsters.c` reads a
      per-instance `decision_mode` field (default 0/preset when absent
      - full backward compat with every existing monster piece).
      `1` (weighted) reads that instance's own `flee_hp_pct` and steps
      AWAY from the hero once hp% drops below it, instead of always
      chasing. Deliberately only 2 tiers, not 5 - a monster's chase-or-
      flee decision doesn't need GOAP/BT/rl/llm/human (doctrine §3).
      Live-verified: a `zombie_child` (weighted, flee_hp_pct=60) fled
      the instant a melee hit dropped it to 50% hp; a `zombie`
      (preset, no decision_mode field at all) kept chasing unchanged.
- [x] `map_02` populated - 4 monsters (2 preset zombie, 2 weighted
      zombie_child), 8 items, authored into `world_01_template/map_02`
      and mirrored byte-identical onto the live `world_01/map_02`.
      **Note**: `map_02` was found to still be the old 40x16 size at
      the start of this pass, not the 80x32 this doc previously
      claimed - either that expansion never got synced here or was
      reverted. Decision (confirmed with the user): don't re-author an
      80x32 map_02 by hand - the real procgen work below now supersedes
      that need, so map_02 stays 40x16.
- [x] Procedural generation v1 - see `dox/02-procgen-design.txt` for
      the full write-up. `ops/generate_map.c` (new one-shot authoring
      op, `./button.sh generate <map_id> <seed> [w] [h]`): seeded
      xorshift32 PRNG, three composable passes (biome patch-scatter,
      room/corridor structure, monster/item population reusing the
      real decision_mode mix above and the real item/monster
      registries), wires a bidirectional stairway back into an
      existing map. Verified: reproducible given the same seed,
      camera clamps correctly at both corners of a real generated
      80x40 map (the actual "bigger than one screen, on real not
      synthetic content" proof this doc's old Phase 6 entry deferred),
      chase AI confirmed working across it. A real map (`map_gen_01`,
      linked from `map_02` at (36,13)) now exists in both
      `world_01`/`world_01_template`, reachable through normal play.
- [x] Multi-Z buildings - decision made: **per-floor map files**, not a
      z-level field on one map. Each floor is an ordinary map directory
      (`building_01_gf`, `building_01_f2`), linked to the one above/
      below via the exact same `transitions.txt` `<`/`>` mechanism every
      other inter-map link already uses (`map_start`<->`map_02`,
      `map_02`<->`map_gen_01`) - zero new movement/collision/rendering
      code needed, since from the engine's perspective "one floor up" is
      identical to "next area over." The only real code change:
      `compose_frame.c`/`compose_rgb_frame.c` now read an optional `z`
      field from a map's own `state.txt` and show `Floor: N` in both
      renderers' headers when nonzero - purely informational, defaults
      to absent/0 so every existing outdoor map's header is unchanged.
      A real 2-floor building (`building_01_gf`/`building_01_f2`) is
      wired into `map_02` at (28,7) and live-verified end to end: enter
      from outside, climb to floor 2 (monster chase confirmed active
      there), descend, exit back outside, landing at the correct
      coordinates every hop; both ASCII and RGB mirrors show the
      matching `Floor: N`. No dedicated design doc for this one - it's
      content (two map directories + transitions.txt entries) plus two
      small renderer tweaks, thin enough that this checklist entry is
      the whole write-up. Read `pieces/world_01/building_01_gf/` and
      `building_01_f2/` directly for the exact shape if extending it.
- [x] **Real xlector/active-target interact mode**, replacing the
      superseded action-bar-cursor build noted above. Ported from real
      1.TPMOS's `projects/fuzz-op/manager/fuzz-op_manager.c` (read in
      full, not excerpted) - see `dox/04-chtpm-parser-research-and-
      interact-mode.txt` §2 for the complete writeup. `'i'`/`'I'` toggles
      `interact_mode`; while on, `hero/state.txt`'s `xlector_pos_x/y`
      (reset to the hero's own position on every entry) move freely and
      uncollided around the map (`ops/move_player.c`) - a genuine "look
      around" cursor, not a menu-navigation toggle. `Enter` examines
      whatever's at the cursor's tile (`ops/choice.c`'s new
      `examine_at()`) and logs a real line to `message_log.txt`; digits
      are a no-op (no action-bar menu exists in this mode); `27` always
      exits back to hero control first, matching real chtpm_parser.c's
      own unconditional-ESC-first convention. Rendered as glyph `'X'`
      (cyan in GL) on top of everything, footer fully replaced with
      `[wasd/arrows] Look  [enter] Examine  [esc] Back` while active.
      Full verb-redirection (monsters/items have no `piece.pdl` METHOD
      table yet) and combat-targeting-via-cursor are explicitly out of
      scope this pass, named not silently skipped.

      **Real bug found and fixed during this build's own end-to-end
      test**: `ops/choice.c` and `ops/move_player.c` both parsed
      `map_id`/`active_panel` out of `hero/state.txt` via
      `v[strcspn(v, "\n")] = '\0'` where `v` aliased directly into the
      shared line-parsing buffer - stripping the newline in place
      corrupted that line permanently for the rest of the op's run,
      merging it with whatever field followed on write-back (hit for
      real: produced a `map_id=map_starthp=100` glued line the instant
      `'i'` was pressed). This is the SAME bug class the existing
      `active_panel` "self-heal back to none" comment already
      documented hitting once before, from `move_player.c`'s own normal
      hero-movement write-back path (which had no dedicated
      `active_panel` branch at all, in either of that file's two
      write-back loops) - meaning ordinary movement had likely been
      corrupting it on every single turn already, silently, before this
      pass. Fixed at the actual root cause in both files: copy into a
      local buffer via `snprintf` before stripping the newline, so the
      shared line buffer is never mutated. Live-verified end to end
      after the fix (playable-interface method): cursor entry/reset
      writes `hero/state.txt` cleanly, free movement crosses a solid
      wall row unobstructed, examine correctly identifies a real item
      (`item_rock_01`) and a real monster (`zombie_02`, including after
      it moved via `tick_monsters` mid-test) with zero corruption
      across a dozen-plus keypresses, `27` exits cleanly, and a
      dedicated regression pass confirmed ordinary `interact_mode=0`
      movement now also round-trips `map_id`/`active_panel` correctly
      (previously silently corrupting on every turn, per the finding
      above). GL panel/cursor rendering re-confirmed via
      `dump_rgb_png.+x`.
- [x] **ASCII<->emoji display toggle** (`'e'`/`'E'`, `hero/state.txt`'s
      `emoji_mode` field), terminal and GL both. Real precedent
      researched first, not guessed: `op-ed_manager.c:1079`/
      `fuzz-op_manager.c:693-705` (single boolean flag, one key, flips
      per-project display) - see `2.muchi-verse/GRAND-ARCHITECTURE.md`
      §0a for the full citation, including the mistake NOT repeated
      here: op-ed/fuzz-op's own glyph->emoji mapping is two separately-
      hardcoded C tables that actively disagree with each other. Ported
      the doc's own recommended fix instead - one `unicode=` field
      added to each of the four content registries (`terrain_types.txt`,
      `furniture_types.txt`, `items.txt`, `monster_types.txt`), so the
      mapping can never drift out of sync with itself.
      Terminal (`ops/compose_frame.c`): resolves each glyph's emoji by
      IDENTITY at the point it's placed onto the map grid (item_id/
      monster_type/hardcoded hero '@'/xlector 'X'), not by re-deriving
      it from the flattened ascii character afterward - a real, not
      hypothetical, distinction: this project's own registries have
      three genuine glyph collisions between terrain and items ('=', '~',
      '%' - pavement/scrap_metal, shallow-water/rag, rubble/jerky), so a
      naive shared char->emoji table would have silently mis-rendered
      whichever item happened to be sitting on the matching terrain.
      Panel overlays (craft/inventory) stay plain ASCII UI chrome even
      in emoji mode - `draw_panel_box()` mirrors its own text into the
      parallel emoji-viewport buffer unchanged, verified live (no
      corruption/garbling under an open craft panel with emoji on).
      GL (`ops/compose_rgb_frame.c`) - **CORRECTED after direct
      instruction to look at wraith-alpha specifically** ("i see the
      emojis in ascii, but not yet in gl. look at wraith-alpha for how
      it converts emojis to rgb"): the first pass here was a themed
      flat-color swap only (`rgb_top_emoji`), reasoning that mutaclsym's
      GL mirror has no font/glyph rasterization for tiles at all so
      there was "nothing to draw the emoji with." That reasoning was
      wrong - wraith-alpha (`wraith_rgb_daemon.c`'s `blit_codepoint()`/
      `get_emoji_bitmap()`/`load_emoji_bitmap_from_disk()`, confirmed by
      direct read) has a genuine, working emoji-to-RGB pipeline: a
      one-shot tool (`1.TPMOS_c_+rmmp.0102.0028/pieces/system/
      emoji_extract/emoji_gen_atlas.c`) uses real FreeType
      (`FT_Load_Char` with `FT_LOAD_COLOR`) to decode
      `NotoColorEmoji.ttf`'s actual embedded color bitmap glyphs into a
      PNG, then `emoji_xtract.c` downsamples it into a plain-text RGBA
      CSV (`# resolution=N` header, one `r,g,b,a` row per pixel) - the
      daemon then just reads that pre-generated CSV and blits real
      pixels, no live FreeType calls in the per-frame hot path (that
      file's own header comment documents a real prior bug from trying
      exactly that).
      Ported the same two-stage shape into mutaclsym: both tools
      (already built, `NotoColorEmoji.ttf` confirmed present on this
      machine) were run once per registry entry to generate real
      `voxels_16.csv` assets - N=16 chosen to match `TILE_PX` exactly,
      so no runtime scaling is needed - under a new
      `pieces/registry/emoji_assets/<id>/` per entry (39 total: 13
      terrain + 6 furniture + 16 items + 2 monsters + hero + xlector),
      keyed by this project's own registry `id` rather than the real
      precedent's hex-codepoint directory naming (no cross-content
      asset sharing needed here, every row already has a unique id).
      `compose_rgb_frame.c` gained `glyph_asset_id()` (same identity-at-
      draw-time resolution as the terminal's own `cell_emoji`, not
      re-derived from the flattened glyph afterward - same real '=' /
      '~' / '%' terrain-vs-item collisions apply here too),
      `load_emoji_voxels()` (reads the CSV, single-entry cache since
      adjacent tiles are often the same asset), and `blit_emoji_tile()`
      (alpha-composites the real 16x16 pixels on top of the flat
      `rgb_top`/`rgb_top_emoji` base color, which still matters as the
      background showing through transparent emoji pixels and as the
      fallback if an asset is missing). Live-verified via
      `dump_rgb_png.+x`: real brick texture on wall tiles, distinct
      readable icons per furniture/item/monster, all visibly different
      from - not just a recolored version of - the flat-color first
      pass; toggling back to ascii mode reverts the GL frame exactly to
      its pre-feature palette; craft panel overlay still draws correctly
      on top with zero corruption; `hero/state.txt` round-trips clean
      throughout.
- [ ] **Next**: more procgen passes (additional biome flavors, non-
      rectangular rooms, caves), more/bigger multi-Z buildings, vehicles
      - see `02-procgen-design.txt`'s own "explicitly deferred" section
      for the procgen half of that list.
- [ ] Vehicles - undesigned; needs a decision on whether a vehicle is
      multiple map cells directly or its own internal mini-grid
      (real CDDA's model). Don't start without deciding this first -
      flagged explicitly in `01-cdda-architecture.md` Phase 6.
- [ ] Phase 8 — professions/scenarios/starting presets, day-night,
      weather. Not designed yet.
- [ ] Orchestrator (`locations.txt` launcher) - small, mechanical, not
      blocked on anything. Pure vision right now, see
      `platform-vision.txt`. Could be picked up any time as a
      lower-stakes side quest.
- [ ] Cross-game item/pet portability + trading platform - far-term,
      genuinely hard (different item/pet schemas need a neutral "trade
      envelope" format). See `platform-vision.txt` §2/§3. Not started,
      not even fully designed.

## Next steps (as of this handoff)

Monster `decision_mode`, `map_02` population, and procgen v1 (all
above) are done and live-verified. See `02-procgen-design.txt` for
what's explicitly deferred in procgen specifically. Broader open items
unchanged: multi-Z buildings, vehicles, Phase 8, the orchestrator,
cross-game trading - see the checklist above.
