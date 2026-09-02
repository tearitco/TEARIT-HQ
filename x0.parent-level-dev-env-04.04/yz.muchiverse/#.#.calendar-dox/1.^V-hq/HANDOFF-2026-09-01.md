# Handoff — 2026-09-01 (session end, next pickup "next week")

Read this first if picking up cold. Full technical detail for
everything shipped today lives in
`xperiments/khtpm-generic-dispatch-design.md` (several dated status
sections) - this file is the map, not a duplicate of that content.

## What's DONE and live

**open-hai** is fully converted onto the shared `khtpm_core_render.+x`
pipeline. `button.sh` launches the shared renderer against a checked-in
bootstrap `open-hai.chtpm`; the real manager (`khtpm_open_hai_manager.+x`)
runs as its `<module>` child and regenerates the real projection every
tick. Real sidebar+panel layout, `cli_io` composer, backspace-to-delete,
chrome X/! buttons, CSS message coloring - all real, generic capabilities
in the shared renderer now, not open-hai-specific. Six-plus real bugs
found and fixed live this session (see the design doc's own dated
sections for each). The old hand-rolled renderer
(`khtpm_open_hai_render.c`) is untouched on disk as a rollback reference.

**Not done**: `chat_button.sh` (open-hai's own per-instance/`--data-root`
chat launcher) still uses the OLD renderer - `launch_module()` only
forwards one extra argv token today, not the two `chat_button.sh` needs
(`--data-root <dir>`). Real, scoped, low-risk follow-up: extend
`launch_module()`'s `extra_arg` to a real argv array, or add a second
optional param.

## chat-hai — NOT migrated, here's the real scoped plan

Direct question this session: "did u say u had to finish that?" - no,
it was never started as a full migration. What WAS done: a small, safe,
real dedup borrowed FROM chat-hai's own already-working mechanism (see
below) - chat-hai's own code was not touched.

**What chat-hai still owns, 100% hardcoded, separate from every generic
capability built today**: its own composer/key handling
(`chai_handle_key()`), its own sidebar+panel layout
(`chai_layout_pass()`), its own nav assignment
(`chai_assign_nav_indices()`), its own redraw/present
(`chai_redraw()`), its own module launch (`chai_launch_module()` - still
a real, separate `fork()+execl()`, NOT delegated to the generic
`launch_module()` the way `dbhq_launch_module()` already is).

**Real, borrowed-and-generalized win this session**: chat-hai's own
`chai_launch_module()` writes the renderer's own real pid to
`state/chat_hai_renderer.pid`; `chat_hai_loop.sh` polls it and self-exits
when that pid is gone - a real, working answer to "orphaned module
survives its dead parent" that predates and is DIFFERENT from (and more
robust than) the SIGTERM-handler approach tried and reverted for
open-hai today. That same real technique was generalized (not copied a
second time): `khtpm_core_render.c`'s own generic module-launch now
writes `module_parent.pid` next to ANY module-launching `.chtpm`, and
`khtpm_open_hai_manager.c` is its first other real consumer
(`parent_still_alive()`). Live-verified: kill -9 the renderer, the
manager self-exits within one loop tick. chat-hai's OWN mechanism was
never touched - it already worked.

**Real plan for an actual chat-hai migration, when there's a full
session's budget for it** (do NOT rush this - every one of today's new
generic capabilities surfaced a real, non-obvious bug even with careful
testing, and chat-hai is live/daily-used, unlike open-hai which had
zero prior users to disrupt):
1. Real, low-risk first step (matches db-hq's own already-done
   precedent): delegate `chai_launch_module()` to the generic
   `launch_module()`, verifying the pid-file write happens at the right
   point for chat-hai's own real orphan-prevention to keep working.
2. Prove the generic `<cli_io>` composer against a REAL COPY of
   chat-hai's own transcript data first (same "isolated `--data-root`
   test before touching the daily driver" discipline this session used
   for open-hai) - chat-hai's composer has real features open-hai's
   composer doesn't yet (word-wrap, multi-line growth, emoji tile
   inline rendering via `build_segs()`/`blit_emoji_tile()`) that the
   generic `cli_io` element does not support at all today - closing that
   gap is real, non-trivial work, not a drop-in swap.
3. Real, separate design decision, not yet made: does chat-hai's own
   real tabbar (multiple real data sources under one window, unlike
   open-hai's single-session-at-a-time shape) fit the generic
   sidebar+panel shape at all, or does it need a third real region type
   this session never built.
4. Only switch `chat_button.sh`'s own real launch line once ALL of the
   above is proven equivalent, live-tested, on a copy - the same order
   open-hai's own conversion followed.

## events-hq — status as of today (not touched this session)

`evhq_layout_pass()`/`evhq_handle_key()`/`evhq_assign_nav_indices()`
remain its own real, separate implementation (own picker overlay, own
real drag/focus logic) - genuinely NOT just "another db-hq copy",
confirmed by direct code comparison earlier in this house's own history,
not assumed. **Real, low-risk, ready-to-do dedup, same shape as db-hq's
own already-completed one**: `evhq_launch_module()`
(khtpm_core_render.c, ~line 3788) still has its own separate
`fork()+execl()`, unlike `dbhq_launch_module()` which already delegates
to the generic `launch_module()`. Swapping it is the exact same real,
already-proven substitution (same argv shape: house_root, package_dir,
one extra arg) - lowest-risk real cleanup available in this file today,
recommended as the first thing to do next session if "reduce
redundancy" work continues.

## db-hq — status as of today (not touched this session)

Already the most consolidated of the three real "legacy" modes -
`dbhq_layout_pass()`/`dbhq_handle_key()`/nav machinery is SHARED across
db-hq itself, bookmarks, palettes, and stats-hq (all real, distinct
apps gated by `g_is_bookmarks`/`g_is_palettes`/`g_is_stats_hq` flags off
the same underlying real code, not four separate copies).
`dbhq_launch_module()` already delegates to the generic `launch_module()`
(the pattern today's open-hai/events-hq work is following). Its own
real keyboard-grab mechanism (`dbhq_grab_keyboard_retry()`,
`g_dbhq_focus_grab_enabled`) is currently OFF by default (a 2026-08-12
"KISS hail-mary" decision, direct instruction at the time) - given
today's real, confirmed focus-follows-mouse bug (found via open-hai's
own cli_io composer, root-caused via a real `XGetInputFocus` check
returning `0x0`/None), db-hq's own bookmarks "New+" input field almost
certainly has the identical bug, UNCONFIRMED - flagged, not fixed,
per the standing "check in first before touching other live windows"
instruction. `xperiments/khtpm-generic-dispatch-design.md`'s own
earlier section also flags a real, still-open question about whether
`khtpm_strip_parser.c` (the taskbar itself) should literalize-merge into
this same shared binary or share dispatch via a text `#include` across
two binaries - not decided, not started.

## Real generic capabilities now available to ANY future khtpm app
(all in the shared renderer, zero per-project C needed)

- Live `.chtpm` re-parse (a manager can regenerate content forever)
- `<cli_io>` real text-input element (armed/typed/synced to
  `cli_io_state.txt`, real keyboard-grab immunity to focus-follows-mouse
  while armed)
- `<sidebar>`+`<panel>` dual-region layout, `<scrolllist>` nested inside
  either one, `Page_Up`/`Page_Down` scroll
- `backspace_action=` on any `<item>` (a real second action, distinct
  from `onclick=`)
- Generic `<module>` launch for the default/popup mode (was db-hq/
  events-hq/chat-hai only before today), now with a real, generic
  orphan-safety file (`module_parent.pid`) any module can poll
- `.classname { color: ... }` CSS coloring (already existed for other
  modes, confirmed working end to end for open-hai's own transcript)

## Meta: installing this house fresh on another computer

Real, existing installer: `x0.parent-level-dev-env-04.04/xyz-installer-dev/
xyzos-starter-install.sh` (confirmed present, not something to build from
scratch). Real, existing test harness for it:
`44.xyz.01.00/%.harnesses/install-xyzos/button.sh` - `compile` builds
its own `tk_*` key-injection test ops, `kpi4` proves a clean install
boots to the signup screen, `kpi5` proves signup→logout→login→whoami→
real xyzfs tree, `all` runs both, `kill` reaps installed processes. Real
install logs/proofs from past runs already exist under
`%.harnesses/install-xyzos/proof/` - read one of those before assuming
the installer's own current real behavior rather than re-deriving it
from scratch. Did not touch or re-verify either this session - flagging
their existence and location, not their current pass/fail state.

## Windows/Mac compatibility

Real, existing house rule: `44.xyz.01.00/#.WIN-COMPAT-RULE.md` -
core doctrine is "one shared C sourceset, thin `#ifdef _WIN32` shims
(window/event/GL, process spawn, path separators), never a growing
parallel `*_win.c` that reimplements real menus/nav." Real status
tracking: `44.xyz.01.00/#.WIN-CONVERSION-STATUS.md` (not read this
session - check its own real current state before assuming anything
about Windows readiness). Real user-facing doc:
`44.xyz.01.00/windows-house-guide.md`. No Mac-specific rule file was
found under a quick search this session - if Mac support is a real,
current goal, that real gap (no dedicated Mac-compat doc/rule
equivalent to the Windows one) is itself worth flagging to whoever picks
this up, not assumed to already be covered by the Windows rule's own
"POSIX shim" half.

## One real, standing safety instruction, unchanged

"Check in before editing any legacy project" (db-hq/events-hq/chat-hai/
the taskbar itself) still applies - none of today's real fixes touched
their own hardcoded logic, only reused/generalized pieces of it where
genuinely safe to do so (db-hq's already-shared launch_module()
delegation pattern, chat-hai's own pid-liveness idea).
