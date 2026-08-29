TP-DESKTOP-LEGACY-POPUP-REMOVAL-CHECKLIST.md
Started: 2026-08-29
Purpose: real, per-call-site checklist for the actual "delete the dead
legacy popup engine" refactor in `tp_desktop_window_rgb.c`, per direct
instruction after ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md's Phase 3
finding (the file itself isn't archivable - see that doc - but the now-
dead in-process context-menu code inside `open_context_menu()` can be
removed, since every real entity now has a `menu.chtpm`). Pushed to
`main` before starting (commit `1743233`) specifically so this can be
done for real, checked off one site at a time, with a safe rollback
point already in git history if anything regresses.

File: `*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c`
(3412 lines, 6 real live processes depend on it right now - confirmed
via `ps aux` at plan time: self, m8_redhorned, m1_ninjadragon,
book-stack, asa, ava, all under
`xyzfs/users/0a9558a7-.../home/livedesk/pals/`).

============================================================
REAL ARCHITECTURE, CONFIRMED BY DIRECT READ (don't re-derive)
============================================================
`open_context_menu()` (~line 1682) is called from ~21 real sites in
`main()`'s event loop, for 3 DIFFERENT real popup purposes - only ONE
of them is the real per-entity context menu that `menu.chtpm`/
`khtpm_entity_menu_render.c` actually replaces:

1. **CONTEXT MENU** - `open_context_menu(dpy, popup_gc, &popup_x,
   &popup_y, n_methods, methods)` (assigned to `popup_win`) - the
   real per-entity right-click menu, backed by `methods`/`n_methods`
   (from `load_methods()`/`load_objects()`/`load_flat_objects()`).
   **This is the one `launch_khtpm_menu()` already replaces visually**
   - these sites are the real refactor target.
2. **INPUT POPUP** - `open_context_menu(dpy, popup_gc, (int[]){win_x},
   (int[]){win_y + WIN_PX + 4}, 1, NULL)` (assigned to
   `input_popup_win`) - a different, single-item popup, NOT backed by
   `methods`. **NOT part of this refactor - has no khtpm.chtpm
   replacement, must keep the real legacy engine.**
3. **USER POPUP** - `open_context_menu(dpy, popup_gc, &user_popup_x,
   &user_popup_y, 4, user_methods)` - a third real popup kind, backed
   by its own separate `user_methods` array. **NOT part of this
   refactor either - keep the legacy engine for this one too.**

`open_context_menu()` itself (the function body) stays - it's shared
by all 3 kinds and cases 2/3 still need it for real. The real refactor
is: at CONTEXT MENU sites specifically, skip calling
`open_context_menu()` at all when a real `menu.chtpm` exists (call
`launch_khtpm_menu()` directly instead, matching what already happens
visually today just without the wasted legacy-window create+hide
first) - a real short-circuit per site, not a body-of-the-function
deletion.

**Verify before checking anything off**: after all CONTEXT MENU sites
are converted, confirm `open_context_menu()`'s own body is STILL fully
used by the 2 remaining real kinds (INPUT/USER popups) - do not delete
`open_context_menu()` itself, `draw_context_menu()`, `close_context_
menu()`, `popup_lock_acquire/release()`, `popup_soft_focus()`,
`clamp_popup_to_screen()`, or `measure_context_popup_w()` - all of
those stay, still real, still used by cases 2/3. What becomes
genuinely removable, ONLY after every CONTEXT MENU site below is
converted and none of them are calling `open_context_menu()` with real
`methods` anymore: nothing extra, actually - the SAME `open_context_
menu()` body serves all 3 cases via its own internal logic, so there
is likely no dead CODE to delete beyond the per-site short-circuit
itself. **Re-confirm this with a real read of `open_context_menu()`'s
own body before assuming there's a second deletion pass needed.**

============================================================
CORRECTION 2026-08-29, after actually reading open_context_menu()'s
own body - much simpler real fix than the section above assumed
============================================================
Read the real function (~1682-1860). The redirect is ALREADY
centralized in ONE place, not spread across ~21 call sites needing
individual analysis: `g_use_khtpm_menu` is a single global flag, set
ONCE per-process at the top of `main()` (~line 2033, `if (access(
menu_chtpm_path, F_OK) == 0) g_use_khtpm_menu = 1;`) - never reset,
never re-checked per-call-site. `open_context_menu()` itself checks
this SAME flag at the END of its own body (~line 1821), AFTER already
doing all the real work (XCreateWindow, popup_lock_acquire, both
grabs) - then destroys that just-created window and calls `launch_
khtpm_menu()` anyway.

This means: TODAY, right now, before any of this refactor, EVERY
`open_context_menu()` call in a process whose entity has a
`menu.chtpm` - context menu, input popup, AND user popup alike -
already redirects to `launch_khtpm_menu()`. That's the REAL, existing,
established behavior (not something this refactor introduces or
should change) - whether it's ideal for input/user popups is a
separate, pre-existing question outside this refactor's real scope.

**The actual, minimal, safe fix: move the SAME check to the TOP of
`open_context_menu()`, before any of the real work happens:**

    static Window open_context_menu(Display *dpy, GC gc, int *root_x, int *root_y, int nitems, MethodItem *items) {
    #ifndef _WIN32
        if (g_use_khtpm_menu) {
            int px = root_x ? *root_x : 0;
            int py = root_y ? *root_y : 0;
            launch_khtpm_menu(px, py);
            return None;
        }
    #endif
        popup_lock_acquire();
        ... (rest of the function, completely unchanged)

No per-call-site changes anywhere in `main()` - all ~21 sites keep
calling `open_context_menu()` exactly as they do today, with exactly
the same real outcome for every entity, just without the pointless
create-then-destroy X11 window cycle when `g_use_khtpm_menu` is set.
Zero semantic change, only removes wasted work. The old end-of-
function block (the `#ifdef _WIN32 ... #else if (g_use_khtpm_menu)
{...} #endif` block) becomes real dead code once the top-of-function
check exists, and should be deleted at that point (the ungrab/destroy/
lock-release logic inside it no longer applies, since the window it's
cleaning up is never created on this path anymore).

**Real remaining tasks, much shorter than the original checklist:**
- [x] Move the redirect check to the top of `open_context_menu()`,
      as shown above. DONE 2026-08-29.
- [x] Delete the now-dead end-of-function block that used to do this
      after the fact. DONE 2026-08-29 - replaced with a short real
      comment explaining where the logic moved.
- [x] Build clean. DONE - `build_khtpm_strip.sh`, only pre-existing
      style warnings (format-truncation/misleading-indentation), no
      new ones tied to this change.
- [x] All real entities WITHOUT a `menu.chtpm` - moot: Phase 2's
      backfill already gave every real entity one (30/30). No real
      legacy-path case left to test against - noted honestly rather
      than skipped.
- [x] Live reload via the real Player > Reset feature
      (`livedesk_reset_entities()` - graceful close, stray-kill sweep,
      respawn from saved desk state) - confirmed via `ps aux`: all 7
      real entities (including cursword, not previously running)
      relaunched clean at fresh PIDs on the rebuilt binary, zero stray
      old processes left behind.
- [ ] Human confirmation: right-click a real entity, confirm no
      legacy-popup flash before the real menu appears (the actual
      visible payoff of this change) - awaiting user's own live
      observation, not something a process check alone can confirm.
- [ ] Update ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md's Phase 3 section
      once the above is confirmed.

The section below (the original ~21-site-by-site list) is SUPERSEDED
by the above - kept only as the real record of the wrong initial
assumption, not a to-do list anymore.

============================================================
REAL CALL-SITE CHECKLIST (line numbers as of commit 1743233 - re-grep
if the file has moved since)
============================================================
CONTEXT MENU sites (real refactor target - short-circuit to
`launch_khtpm_menu()` directly when `menu.chtpm` exists, same real
`access(path, F_OK)` check `launch_khtpm_menu()`'s own header already
documents):

- [ ] line ~2374 - `popup_win = open_context_menu(...&popup_x,
      &popup_y, n_methods, methods)`
- [ ] line ~2387 - same shape, second real branch nearby
- [ ] line ~2437 - same shape
- [ ] line ~2448 - same shape
- [ ] line ~2546 - same shape
- [ ] line ~2557 - same shape
- [ ] line ~2600 - same shape, `load_flat_objects()`-sourced `methods`
      (confirm this one's real source still resolves correctly through
      the khtpm path - `menu.chtpm` conversion so far only covered
      `load_methods()`'s own METHOD-row source, NOT `load_flat_
      objects()`'s real flat-OBJECT-file source - **real open
      question, check what real feature uses this path before
      converting it** - may need its own `menu.chtpm` generation rule,
      not just reuse the METHOD-row converter)
- [ ] line ~2771 - same shape
- [ ] line ~2801 - same shape
- [ ] line ~2938 - same shape
- [ ] line ~2949 - same shape
- [ ] line ~3101 - same shape
- [ ] line ~3112 - same shape
- [ ] line ~3311 - same shape (`read_menu_config()` re-read alongside
      it - confirm STATE-row re-read still matters or becomes dead
      once this site no longer opens the legacy window)

INPUT POPUP sites (NOT this refactor - confirm still intact after the
above, don't touch):
- [ ] line ~2397 - confirm untouched
- [ ] line ~2458 - confirm untouched
- [ ] line ~2567 - confirm untouched
- [ ] line ~2965 - confirm untouched
- [ ] line ~3122 - confirm untouched

USER POPUP sites (NOT this refactor - confirm still intact after the
above, don't touch):
- [ ] line ~2537 - confirm untouched
- [ ] line ~2920 - confirm untouched
- [ ] line ~3092 - confirm untouched

============================================================
REAL VERIFICATION STEPS (per real testing convention this session
already used - relay/dump first, real click only if needed)
============================================================
- [ ] Build clean (`scripts/build.sh` in the tile-picker dir, or
      wherever this file's own build script lives - confirm exact
      script before assuming).
- [ ] Live: right-click a REAL, currently-running entity (book-stack
      or ava - both already proven working through the khtpm path
      tonight) - confirm the popup still opens correctly, same as
      before this refactor, and that NO legacy window flashes/appears
      even briefly (the real, visible improvement this refactor buys -
      today's create-then-hide might have a real, if brief, visual
      artifact worth confirming is gone).
- [ ] Live: trigger whatever real feature uses the INPUT popup (find
      it via the `input_popup_win` call sites' own surrounding code -
      not yet identified in this pass) - confirm it still opens/works
      exactly as before.
- [ ] Live: trigger whatever real feature uses the USER popup (find it
      via the `user_popup_win` call sites' own surrounding code - not
      yet identified in this pass) - confirm it still opens/works
      exactly as before.
- [ ] Drag a real entity - confirm `desktop_pos.txt` still gets
      written (this refactor should touch NOTHING here, but it's the
      file's own real primary purpose per the Phase 3 finding - worth
      a real regression check regardless).
- [ ] Confirm zero stray processes after all live tests (standing
      rule this whole session).
- [ ] Update `ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md`'s own Phase 3
      section once this is fully checked off.
