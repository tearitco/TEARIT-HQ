Plugin Architecture Scoping — "10.plugins" (2026-08-25)
==========================================================
Design/scoping pass only — NO CODE YET. Per direct instruction: this needs
a real decision on shape before any Haiku-ready tasks get carved out of it
(see EVENTS_ROADMAP_NEXT_STEPS.md step 2). This doc is the place to argue
about the shape; once settled, split it into bounded tasks the way H6/H7/H8
were.

THE ASK, VERBATIM (2026-08-25 conversation)
----------------------------------------------
- "10.plugins is in toolbar" — a real numbered taskbar cell, not just a
  bare folder convention.
- "it will work like rpg maker plugins" — per-project plugin list, each
  independently toggleable on/off, applied in a defined order — same
  mental model as RPG Maker MV/MZ's Plugin Manager.
- "but using .pal script" — the unit of a plugin is `.pal`, not JS (RPG
  Maker's own language) and not a raw compiled `.c` op.
- ".pal to trigger ops" — a plugin's `.pal` script is glue/orchestration
  that calls existing or new real compiled ops, same shell/manager-style
  separation used everywhere else in this house. A plugin does not
  reimplement algorithms in `.pal` itself.
- Toggling is scoped **per `file:project`** — i.e. a specific project
  (session/game) has its own enabled-plugins list, not a single global
  on/off switch.
- First candidate real plugins once the architecture exists: line-of-
  sight and pathfinding (smaller/mechanical) before a full tactics/
  targeting system (see EVENTS_ROADMAP_NEXT_STEPS.md's own ordering).

REAL PRECEDENT TO REUSE, NOT REINVENT
-----------------------------------------
1. **UI shape**: every `-hq`/khtpm window this house has built recently
   (db-hq, events-hq, stats-hq, bookmarks) is the SAME compiled
   `khtpm_entity_menu_render.c` binary, mode-selected by `<window
   class="...">`, using the real sidebar+panel layout
   (`find_by_tag(window,"sidebar")`/`"panel"`) — NOT a bespoke standalone
   renderer, and NOT a tabbar (stats-hq's old tabbar was the thing that
   got replaced BECAUSE it never worked). A plugins manager window should
   be `class="plugins"` on this SAME binary, following bookmarks' own
   real shell+manager split as the closest existing template (small,
   list-of-named-things-with-on/off-state — structurally very close to
   what a plugin list actually is).
2. **Shell + manager split, always**: the renderer draws/dispatches
   clicks; a separate compiled manager owns the real file I/O (scanning
   available plugins, reading/writing the enabled-list, whatever "load
   order" needs). They talk only through files (`action.txt` in →
   `*.state.txt` out), exactly like every other `-hq` app tonight.
3. **PDL is the house's universal registry format** — `SECTION | KEY |
   VALUE` pipe-delimited rows, same shape as `bookmarks.pdl`
   (`BOOKMARK | name | path`), `condition.pdl` (`COND | trigger |
   value`), `event.ir.pdl` (`NODE | id=... type=... | params`). A
   plugin's enable-state list and a plugin's own manifest should both be
   PDL, not a new format.
4. **`.pal` scripts trigger ops via compiled wrapper scripts, never
   inline logic** — `event.pal` just does `exec cmd_N.sh` / `halt`;
   `cmd_N.sh` is a tiny generated shell wrapper that resolves paths and
   execs a real compiled op (`mr_change_gold.+x`, etc.). A plugin's
   `.pal` should follow the exact same shape: trigger real ops, don't
   hand-roll LOS/pathfinding math in `.pal` itself.
5. **Taskbar cell dispatch**: adding a new numbered cell means following
   `TASKBAR-MENU-ARCHITECTURE.md`'s "correct recipe to add a menu item"
   (dispatch-string + `ktb_hq_activate()` branch, single-instance-guarded
   `button.sh`) — same mechanism cell 14 (h-ai)/cell 9 (db) already use.
   **Open question**: is cell 10 actually free? Not found reserved in any
   doc searched tonight (2, 3, 6, 9, 11, 12, 13, 14, 15 are all spoken
   for) — but confirm against the LIVE header-cell registry before
   building, not just doc greps, since docs can be stale (caught twice
   already this session for a different file).

OPEN DESIGN QUESTIONS (need a real decision before scoping tasks)
----------------------------------------------------------------------
These are the load-bearing decisions RPG Maker's own plugin system makes
for you implicitly — this house needs to make them explicitly since
there's no existing analog to copy verbatim (unlike the UI/manager-split
precedent above, which IS copyable as-is).

Q1. **RESOLVED (direct instruction, 2026-08-25).** "file" = "desk"
    terminology, mapped onto REAL, already-existing code structure (not
    documented under these names anywhere before now, confirmed by
    search):
    - **"file"** = a **session** — `xyzfs/users/<uuid>/home/livedesk/
      sessions/<id>/` — the whole RPG-Maker-style project. Already has
      its own `desks/`, `entities/`, `common_events/` (see step 1's own
      common-events wiring, which is ALREADY session-scoped this same
      way via `$HOUSE_ROOT/common_events/` — note: today that's actually
      HOUSE-root-scoped, not session-scoped, per khtpm_hq_manager.c's
      `publish_common_events()`. That's a pre-existing inconsistency
      worth flagging, not something this doc invents: common_events
      currently live under the house root globally, while `desks/` and
      `entities/` live under each session. Plugins should follow the
      session-scoped pattern per direct instruction below, not
      common_events' current house-global one.).
    - **"desk"** = an individual map — `sessions/<id>/desks/<name>.pdl`
      (e.g. `desk_01.pdl`, `office.pdl`).
    - **Plugin storage, per direct instruction**: `sessions/<id>/
      plugins/<plugin_name>/` — each plugin gets its own directory,
      INSIDE the session/file, not a shared house-wide catalog like RPG
      Maker's single `js/plugins/`. This is a real difference from RPG
      Maker worth being deliberate about: every session has its OWN
      independent plugin storage, so the "on/off per file:project"
      framing is really "plugins are installed AND enabled per-session"
      — there's no separate global-catalog-plus-opt-in-list split like
      Q1's original draft proposed. Simpler: a plugin's presence in
      `sessions/<id>/plugins/` IS it being installed for that session;
      an enabled/disabled flag (likely still a small PDL row per plugin,
      `PLUGIN | <name> | enabled=1`) handles on/off without needing a
      separate global registry at all.
    - **UI, per direct instruction**: a real HQ-style window (same
      khtpm_entity_menu_render.c sidebar+panel shell+manager shape
      proposed above) — confirms the UI approach in this doc's "REAL
      PRECEDENT" section rather than a plain file-only convention.

Q2. **RESOLVED (direct instruction, 2026-08-25): a hybrid of three hook
    KINDS, not one universal mechanism** — deliberately NOT RPG Maker's
    monkey-patch-anything model (that's the "N writers racing" anti-
    pattern this house has already moved away from once, see the
    taskbar's own capture-only-writer/single-dispatcher migration). Each
    kind maps onto a real, already-proven house mechanism instead of
    being invented fresh:

    **(a) Provider/Registration — for LOS, pathfinding, anything that's
    "ask a question, get an answer NOW".**
    A plugin manifest declares `PROVIDES | <query_name>` (e.g.
    `PROVIDES | line_of_sight`). Exactly one enabled plugin per session
    may provide a given query name (registration, not broadcast) - the
    manager should refuse/warn on a second plugin claiming the same
    query, not silently pick one. The CALLER (engine C code, or another
    plugin's `.pal`) execs the provider's real op DIRECTLY and
    synchronously - same shape `cmd_N.sh` already uses to exec
    `mr_change_gold.+x`, just read for a real return value (exit code
    and/or a small stdout/result-file contract) instead of fire-and-
    forget. This is NOT file-polling/async - a LOS query needed mid-frame
    can't wait on a ~150ms poll tick, it needs a direct synchronous
    subprocess call, same as any other op invocation already is.

    **(b) Event-driven Observer — for UI actions/state changes.**
    This is common_events' own step-1 shape, generalized: a plugin
    manifest declares `OBSERVES | <trigger_name>` (on-click,
    on-interact, item-picked-up, gold-changed, etc.). Whatever process
    already owns firing that trigger (`play_event.sh` for the event
    triggers that exist today) checks the session's enabled plugins for
    matching `OBSERVES` rows and execs each match's `.pal`, fire-and-
    forget, exactly like common events already do - ALL matches run
    independently, no return value expected, no override semantics
    (matches Q3's existing recommendation).

    **(c) Lifecycle hooks (tick/render) — for anything that must run
    every frame.**
    The most architecturally expensive of the three - do NOT spawn a
    subprocess per plugin per frame (a 60Hz spawn storm). A plugin
    declaring `ON_TICK`/`ON_RENDER` needs to run as its OWN persistent
    process (launched once, like any other manager), polling a real
    state file the owning renderer's main loop writes every tick/frame
    (same shape khtpm's own poll-loop-every-~150ms managers already use,
    just driven by the render loop's own tick instead of a relay file).
    **Recommend deferring (c) entirely past the first slice** - it's the
    one hook kind with no simpler existing precedent to copy, and
    neither of the two named first-real-plugins (LOS, pathfinding) needs
    it - they're both provider/(a)-shaped. Build (a) and (b) first, only
    design (c) once a real plugin actually needs a per-frame hook.

Q3. **Load order semantics** — RPG Maker runs plugins in array order,
    later plugins can override earlier ones. Does this house need
    override semantics at all for a first pass, or just "run all enabled
    plugins matching this hook, in list order, independently" (same
    shape `play_event.sh` already uses for common events — every match
    runs, not "last one wins")? Recommend starting with the simpler
    independent-run shape (matches precedent already built) and only
    adding override semantics if a real plugin actually needs it.

Q4. **Per-plugin parameters** — RPG Maker plugins declare typed params
    edited in the Plugin Manager UI. Does a first version need this, or
    can early plugins (LOS, pathfinding) hardcode their own tunables in
    their own `.pal`/ops for now? Recommend deferring — adds real UI
    complexity (per-param editing widgets) for zero proven need yet.

PROPOSED MINIMUM FIRST SLICE (once Q1/Q2 are answered)
----------------------------------------------------------
Small enough to be a real Haiku-ready task list once decided, mirroring
H6/H7/H8's shape:
1. `plugins_manager.c` (shell+manager split, bookmarks-shaped) — scans
   the CURRENT session's `sessions/<id>/plugins/*/` for installed
   plugins, shows enabled/disabled per each plugin's own manifest PDL
   row, nav-reachable toggle. No global catalog to scan — per-session
   only, per direct instruction.
2. Taskbar wiring: cell 10 (pending confirmation) launches it, following
   the existing recipe. Needs to resolve "current session" the same way
   any other session-scoped window would (check how `desks`/`entities`
   windows already resolve their own session context — reuse that, don't
   invent a new resolution mechanism).
3. TWO real trivial proof-case plugins, one per hook kind actually
   needed for the named first plugins (skip lifecycle hooks entirely
   for this slice, per Q2's recommendation to defer (c)):
   - A trivial (b)-shaped (Observer) plugin: appends a line to a file
     when `on-click` fires, proving `play_event.sh` correctly checks
     `OBSERVES` rows and execs it. Same lesson as common-events step 1 -
     prove the wiring with the simplest possible payload first.
   - A trivial (a)-shaped (Provider) plugin: `PROVIDES | ping`, a real
     op that just echoes its input back, proving the direct-synchronous-
     exec contract (not file-polling) actually works end to end before
     LOS has to be the one proving it.
4. Only after #1-3 are verified live: attempt line-of-sight as the first
   REAL (a)-shaped plugin, per the agreed roadmap order.

NOT IN SCOPE for this pass
------------------------------
- Actual LOS/pathfinding/tactics/targeting implementation.
- Per-plugin parameter editing UI (Q4).
- Plugin override/priority semantics beyond "all enabled matches run".
- Any changes to `play_event.sh` beyond what step 1 already did — the
  plugin hook dispatcher is a NEW, separate mechanism from the
  common-events wiring, not an extension of it (different hook shape,
  per Q2).
