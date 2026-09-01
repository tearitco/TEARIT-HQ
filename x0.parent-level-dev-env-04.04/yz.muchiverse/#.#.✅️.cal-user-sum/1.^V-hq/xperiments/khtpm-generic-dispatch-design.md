# khtpm generic-dispatch design — making the shared renderer actually agnostic

## Status update, 2026-08-31 (same day) — §2a built and proven live

The generic `launch_module()` described in §2a below is now real,
written, and live-tested: `dbhq_launch_module()` no longer forks/execs
itself, it delegates to the new generic function (same exact argv:
`house_root`, `package_dir`, optional `extra_arg`). Tested by launching
a real db-hq window end to end (`open_db_hq.sh`) — the manager self-
spawned correctly via the generic path (confirmed via `ps` showing the
real argv), published real Common Events data
(`db_hq_common_events.state.txt` populated with real rows), and a real
screenshot (`dump_frame_png_op.+x`) showed the window rendering
perfectly normally — tabs, sidebar, panel, Actor 0001's real fields,
zero visible or functional regression. This is the real proof the
mechanism works before events-hq/chat-hai/network-browser are migrated
onto it too (not yet done).

**Real, honest side-finding from reading `history_dir()`/
`frame_changed_path()` closely while designing this**: these two
ternary chains are already inconsistent with each other -
`frame_changed_path()` gives palettes and bookmarks their own distinct
filenames, but `history_dir()` does not (both fall through to
`"db_hq_history"`, since neither has its own `g_is_palettes`/
`g_is_bookmarks` arm in that specific chain). Not fixed here — real,
pre-existing behavior, out of scope for this pass — but worth carrying
into whichever future step unifies these two functions onto a shared
per-mode config table, so the fix (give palettes/bookmarks their own
real history dir, or confirm sharing db-hq's is intentional) happens
deliberately, not silently changed as a side effect of the refactor.

**Status: design only, 2026-08-31. No implementation in this doc.**
Triggered by a real, caught-in-time mistake: building the network-
browser-hq mode by adding `g_is_network_browser` branches at ~15
dispatch points in `khtpm_entity_menu_render.c`, following the exact
shape every existing mode already uses. **Direct correction**: "that's
still hardcoding... why cant u use existing conventions? we shouldn't
hardcode... the parser/renderer should have no knowledge of new
projects and be completely agnostic." Caught before any of it was
committed or compiled — reverted in full (`git checkout` on the file)
before this doc was written.

## 1. The real, confirmed problem — not just network-browser's

Every existing khtpm mode has the SAME shape: a `g_is_<mode>` global,
set by a `strcmp(class, "...")` in one big detection loop, then checked
again at every one of these real call sites (confirmed by direct read,
not assumed):

1. Class detection loop (`main()`, ~11115-11175 in the pre-this-session
   line numbering)
2. `assign_nav_and_layout()` — one `if (g_is_X) { X_layout_pass(...);
   X_assign_nav_indices(...); return; }` per mode
3. `redraw()` — `if (g_is_db_hq || g_is_events_hq) { ... }` (chat-hai
   short-circuits earlier, fully self-contained)
4. `handle_key()` — one early-return branch per mode (events-hq/chat-
   hai/db-hq's own `g_input_elem` case), ordered carefully around the
   shared `'p'` frame-dump shortcut
5. `poll_agent_history()`'s MOUSE_EVENT dispatch — one `else if` per
   mode
6. The real X11 `ButtonPress`/`KeyPress`/`FocusIn`/`FocusOut` handlers
   inside `hq_dispatch_xevent()` — one branch per mode, each with its
   own drag-state globals
7. `history_dir()` / `frame_changed_path()` — one ternary arm per mode
8. CSS path derivation condition (`if (g_is_db_hq || g_is_events_hq ||
   g_is_chat_hai)`)
9. `hq_window_has_x_focus()` — one branch per mode
10. `nav_tab_poll_active()`'s own gating condition
11. `nav_tab_register()`'s own ternary
12. Module launch — `dbhq_launch_module()` / `evhq_launch_module()` /
    `chai_launch_module()`, three near-identical fork+execl functions,
    called from three near-identical `find_by_tag(g_window,
    "module")` + launch call sites in `main()`
13. Each mode's own hand-written `<mode>_layout_pass()` in C (never the
    generic `css_layout_pass()` flex engine — that function's own
    header comment in `khtpm_render_core.c` says it is "NOT YET LIVE-
    TESTED against a real app," confirmed still true)
14. Each mode's own hand-written state-file parser/content-injection
    function (`dbhq_load_common_events()`, `evhq_load_pages()`, etc.)

**This is real, self-acknowledged debt, not a surprise.**
`khtpm_render_core.c`'s own header (Stage 2a of the original merge)
says outright: "Real single-binary end goal... wraith_parser_alpha.c
IS the standard: one generic binary, argv[1] = .chtpm path,
project.pdl resolved from convention, ZERO per-app hardcoded C logic:
db-hq/events-hq/chat-hai each still have real, different, hardcoded
business logic in C today... none of that is data-driven yet." That
end goal was written 2026-08-16 and never finished for ANY mode.
Adding network-browser via the same pattern would have been the 8th
real instance of not finishing it — this design exists so it becomes
the last one instead.

## 2. Real scope split — three genuinely different genericity problems

Conflating these three would make the refactor too large to do safely
in one pass. They are ordered here by real, confirmed risk/size, not
importance:

### 2a. Module launch (SMALL, LOW RISK — do first)

`dbhq_launch_module()`/`evhq_launch_module()`/`chai_launch_module()`
are structurally identical: `fork()`, then `execl(full_path, full_path,
house_root, ...)`. They differ only in how many extra argv slots they
pass (db-hq: `house_root, package_dir, extra_arg`; events-hq:
`house_root, evhq_pkg_dir, evhq_entity_label`; chat-hai: fewer). A
single real function:

```c
static pid_t launch_module(const char *src, const char *house_root,
                            const char *package_dir, const char *extra_arg);
```

replaces all three, called identically from every mode's own
`find_by_tag(g_window, "module")` site. Zero project knowledge in the
function itself — every argument comes from either the real, already-
parsed `<module>` Elem (`label` = src, `id` = extra_arg, matching the
already-existing real convention) or from generic context
(`g_house_root`, `g_package_dir`). This is real, mechanical
de-duplication — no behavior change for any existing mode, verifiable
by diffing the exact argv each call site used to pass.

### 2b. Class/key/click/layout DISPATCH (MEDIUM — the real design this doc is mostly about)

Replace every `g_is_X` global + its ~10 scattered `if`/`else if`
branches with one real, small registration table:

```c
typedef struct {
    const char *class_name;         /* matches <window class="...">    */
    void (*layout_pass)(Elem *window);
    void (*assign_nav_indices)(Elem *window);
    void (*redraw_content)(void);   /* NULL = mode is fully self-
                                        contained, own redraw() early-
                                        return (chat-hai's own shape) */
    void (*handle_key)(KeySym ks, char ch);
    void (*handle_click)(int mx, int my);
    int  has_input_field_hook;      /* real, per-mode "p is a literal
                                        character while a field is
                                        armed" exception - db-hq/events-
                                        hq/chat-hai/network-browser all
                                        need this; a generic bool covers
                                        it without inventing anything */
} KhtpmModeVTable;

static const KhtpmModeVTable g_khtpm_modes[] = {
    { "db-hq",               dbhq_layout_pass, dbhq_assign_nav_indices, dbhq_redraw_content, dbhq_handle_key, dbhq_handle_click, 1 },
    { "events-hq-window",    evhq_layout_pass, evhq_assign_nav_indices, evhq_redraw_content, evhq_handle_key, evhq_handle_click, 1 },
    { "chat-window",         chai_layout_pass, chai_assign_nav_indices, NULL,                 chai_handle_key, chai_handle_click, 1 },
    /* stats-hq/palettes/bookmarks: real, separate rows here (their own
       history/frame-changed filenames still need distinguishing - see
       §2c) once ported off riding g_is_db_hq's own C-level aliasing */
};
static const KhtpmModeVTable *g_active_mode = NULL; /* NULL = legacy popup/page mode, unchanged */
```

Class detection becomes one real, generic loop (`for each class token,
linear-search g_khtpm_modes, set g_active_mode`) instead of a growing
`strcmp` chain. Every one of the ~10 scattered dispatch sites becomes
`if (g_active_mode) g_active_mode->handle_key(ks, ch);` — **the
renderer file itself never again needs to know a mode's name to add
support for it**; adding a new app means adding one row to this table
(plus, obviously, writing that mode's own real `<mode>_layout_pass()`/
`_handle_key()`/etc. functions elsewhere in the file or, longer-term,
not needing to write those at all — see §2c).

**Real, honest limits of this phase**: this does NOT make the
renderer's business logic agnostic — `dbhq_layout_pass()` still knows
about tabs/actors/common-events, `evhq_layout_pass()` still knows about
event pages. It only removes the SELECTION mechanism's hardcoding
(the "which function do I call" part), not each mode's own real,
legitimate content logic. That's the correct, honest scope for this
phase - see §2c for what full content-agnosticism would need and why
it's a separate, larger, NOT-yet-designed problem.

### 2c. Layout + content-injection genericity (LARGE, NOT DESIGNED YET — explicitly deferred)

The real wraith-alpha end-state (`.chtpm` declares everything, zero
per-app C) needs two things nobody has built or proven yet:
- **Generic layout**: `css_layout_pass()` (the shared flex engine,
  already written) actually driving a real mode's geometry instead of
  hand-computed x/y/w/h in a per-mode `_layout_pass()`. Real, stated
  blocker in its own header: never live-tested against a real app.
- **Generic content injection**: a declarative way for a `.chtpm` panel
  to say "my children come from this state file, in this row format"
  so the renderer can inject `reusable_slot()` rows WITHOUT a per-mode
  C function knowing that file's schema. No such declarative contract
  exists anywhere in this house today.

**Not designed in this doc.** Real estimate: this needs its own,
separate design pass, likely after §2a/§2b are live and proven on at
least 2-3 real modes. Flagging it here so it isn't forgotten, not
promising a timeline.

## 3. Real, ordered rollout plan (per direct instruction: design → build network-browser as first real user → migrate existing apps → verify each → lock into standards)

1. **Build §2a (generic `launch_module()`) + §2b (mode vtable table +
   dispatch sites)** — pure mechanism, zero mode content yet.
2. **network-browser-hq becomes the FIRST real row in
   `g_khtpm_modes[]`** — its own `nb_layout_pass()`/`nb_handle_key()`/
   etc. (the real, mode-specific functions designed in the previous,
   reverted attempt are still the right SHAPE, they just get
   registered via the table instead of a new `g_is_network_browser`
   global) written and wired ONLY through the new generic mechanism -
   proving the pattern works end to end (build, real X11 window, real
   relay-injection test) before touching any existing mode.
3. **Migrate ONE existing mode at a time, smallest/lowest-risk first**
   (`swatch-picker` is the smallest real candidate - single page, no
   tabs, no manager) - port its existing real functions into a
   `g_khtpm_modes[]` row, delete its own scattered `g_is_swatch_picker`
   branches, then **live-verify it still works exactly as before**
   (real window open, real click/key test, real frame/history check)
   before moving to the next mode.
4. Repeat step 3 for chat-hai, events-hq, db-hq (and db-hq's own
   riders - stats-hq/palettes/bookmarks each need their own real vtable
   row once they're not just C-level aliases of `g_is_db_hq` anymore -
   real, separate sub-task, not assumed trivial).
5. **Only once every mode is migrated and verified**: delete the old
   `g_is_X` globals and every leftover `if (g_is_X)` branch this doc's
   §1 inventory lists - a real, final cleanup pass, not left half-done.
6. **Update standards** (per direct instruction, "write to standards
   and index that this should never happen again, this is std drift"):
   - `CENTROID_GOLD_STD.md` gets a new rule: the shared renderer file
     must never gain a new `g_is_<project>` global or a new per-project
     `strcmp` branch at a dispatch site again - a new mode registers
     via `g_khtpm_modes[]` (or, once §2c exists, needs no C registration
     at all). Violating this is the same severity class as inline
     business logic in the shared file (`dbhq_load_actors()`'s own
     condemnation).
   - `TPMOS-COMPLIANCE-DEBT.md` gets a real, named entry for this
     exact near-miss (§6) - caught before landing, not after, but real
     "std drift" in the sense the direct instruction means: the
     assistant defaulted to copying an already-known-bad existing
     pattern instead of checking whether it was still the standard.
   - `INDEX.md` Tier 1 gets a pointer to this doc + the eventual
     `g_khtpm_modes[]` implementation, read-before-adding-any-new-mode.

## 4. What this doc is NOT

Not a promise that §2c ships soon. Not a claim that today's 7 existing
modes are broken - they work, live, right now; this is about how the
NEXT one (and the one after that) gets added without adding an 8th,
9th, 10th copy of the same already-known-imperfect pattern. Not
implemented anywhere yet beyond §2a - `khtpm_core_render.c` (renamed
2026-08-31 from `khtpm_entity_menu_render.c`, same file, see
`CENTROID_GOLD_STD.md`'s own cross-reference) has zero diff from
`origin/main` on the mode-dispatch question specifically (the in-
progress network-browser hardcoding was reverted via `git checkout`
before this doc existed; the rename itself is real, committed, and
purely cosmetic - see that commit's own message for the full verified-
zero-behavior-change writeup).

## 5. Broader consolidation — real, separate renderer systems this design should eventually absorb

Direct instruction, 2026-08-31, same day: "open_hai need 2 use same
layout renderer as everyone else. we dont want more than one
renderer." Real, confirmed count as of this writing: at least **four**
separate real khtpm parser/renderer systems exist in the house
(`khtpm_core_render.c` itself, `khtpm_strip_parser.c` for the taskbar,
`khtpm_choice_picker.c`, and open-hai's own `khtpm_open_hai_render.c`),
plus real `.chtpm` files (`chain-hq.chtpm`/`forum-hq.chtpm`/`irc-chat-
hq.chtpm`/`network-browser-hq.chtpm`) nothing parses yet at all. This
section records each as a real, later, explicitly-not-started
consolidation target - sequenced AFTER the 7-mode `g_khtpm_modes[]`
migration in §3, not competing with it:

- **`khtpm_open_hai_render.c`** (`&.widgits/open-hai/ops/`) - real,
  confirmed BIGGER lift than a simple registration: its own header
  comment states it uses a "flat (non-tag-tree) layout" and does NOT
  parse a real `.chtpm` into an `Elem` tree at all - it hand-computes
  x/y/w/h directly, same category of gap as network-browser's own
  original (corrected) attempt. Real work needed: author a real
  `open-hai.chtpm`, convert its flat layout to the tag-tree model,
  THEN register it in `g_khtpm_modes[]` - three real steps, not one.
- **`khtpm_choice_picker.c`** (`&.widgits/tile-picker/ops/`) - **DONE,
  2026-08-31** (`TPMOS-COMPLIANCE-DEBT.md` §5, RESOLVED). Turned out to
  need NO `g_khtpm_modes[]` registration at all, unlike open-hai below -
  its real caller (`khtpm_show_choices.c`) now generates a real,
  temporary `.chtpm` (one `<item action="...">` per real choice) and
  launches the already-shared `khtpm_core_render.+x` directly, whose
  ALREADY-EXISTING generic default page/item path (used by swatch-
  picker/entity-menus) already treats an unrecognized `action=` as a
  real shell command and quits after - zero new C code in the shared
  renderer. Live-verified end to end (real window, real relay-injected
  pick, real token on stdout). The standalone `khtpm_choice_picker.c`
  itself is left in place, unused, as a real rollback reference - not
  deleted, not registered in any dispatch table (there wasn't a need).
  Real lesson for open-hai below: not every consolidation needs the
  `g_khtpm_modes[]` machinery - check whether the existing generic
  default path already covers the real need before building new
  registration plumbing.
- **`khtpm_strip_parser.c`** (the taskbar itself) - a genuinely
  different real case: it already parses real `.chtpm` files
  (`khtpm_strip_header.chtpm`/`khtpm_strip_bottom.chtpm`) through its
  own real, working parser/layout/manager split
  (`khtpm_taskbar_manager.c`/`khtpm_taskbar_manager_main.c`), and
  already has its own real ASCII mirror
  (`khtpm_strip_render_ascii.c`) - CENTROID_GOLD_STD.md's own dual-
  renderer rule, achieved independently, years before this design
  doc existed. Real open question, NOT answered here: does merging the
  taskbar into the SAME binary as every HQ window make sense at all
  (the taskbar is a fundamentally different kind of window - always-
  on, screen-edge-anchored, singleton), or should `g_khtpm_modes[]`
  become a real, SHARED table both binaries include (via the same
  real text-`#include` convention `khtpm_render_core.c` already uses)
  rather than forcing a literal one-binary merger. Flagged for a real,
  separate design decision later - not assumed either way here.

**Real, deliberate non-goal for now**: no work has started on any of
these three. This section exists so the real scope is written down
before someone reaches for "just merge it in" as an offhand line item
under an already-large migration.
