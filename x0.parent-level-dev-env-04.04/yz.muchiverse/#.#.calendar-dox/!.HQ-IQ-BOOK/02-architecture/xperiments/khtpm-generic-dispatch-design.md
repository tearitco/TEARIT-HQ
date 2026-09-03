# khtpm generic-dispatch design — making the shared renderer actually agnostic

## READ-HANDOFF, 2026-09-03 — current state + the real size of the `g_is_X` debt a next agent must own

**Read this block first. It is additively up to date; everything below a
given block is what that block's date thought was true.** This is a
living doc — an older section's "left in place / not deleted" claim can
go stale the day it was written. Verified-against-the-tree corrections
are in the two deltas at the end of this block.

### What this doc is FOR (a next agent doing the §2b dispatch refactor)

It specifies how to end the `g_is_<mode>` hardcoding in the shared
renderer. Per the house standard (`CENTROID_GOLD_STD.md` rule 7 / the
`khtpm-house-standards` skill): **no new `g_is_<project>` global or
per-project `strcmp` branch may be added to `khtpm_core_render.c`** —
the §2b `g_khtpm_modes[]` table (or, per the 2026-08-31 pivot, the
non-table generic default path) is how it should be done. That rule is
violation-classed the same as inline business logic. The §3 rollout
step "design → build first user → migrate one existing mode at a time →
verify each → lock into standards" is still the ordered plan.

### Current `g_is_*` inventory in `khtpm_core_render.c` (2026-09-03 count) — the real size of #2

`khtpm_core_render.c` is 18,847 lines. It carries **212 `g_is_*`
dispatch sites** across **9 mode globals**:

| global | live branch sites |
|---|---|
| `g_is_db_hq` | 48 |
| `g_is_palettes` | 35 |
| `g_is_events_hq` | 30 |
| `g_is_cursword` | 30 |
| `g_is_stats_hq` | 23 |
| `g_is_bookmarks` | 20 |
| `g_is_swatch_picker` | 18 |
| `g_is_pchq_board` | 5 |
| `g_is_chat_hai` | 3 |

These are **live runtime branches** (class-tagged windows), not dead
code. The §1 call-site inventory (1-14) still holds. The honest scope of
§2b is: replace the SELECTION mechanism (which function the ~10
scattered sites call), NOT each mode's own legitimate content logic —
see §2b's "Real, honest limits" paragraph. **This is the large, ordered
refactor — the "2k-line-ish" debt.** Do it per §3 (migrate one mode at
a time, smallest first — `swatch-picker` is the intended first real
row), verifying live between each mode.

### What 2026-09-03 already DID (this cleanup session) — don't redo it

Independent of the §2b migration, two house-maintenance changes landed
on branch `chtpm-var-substitution` that a next agent should know
(they're the current tree state, all committed + pushed):

1. **Runtime/transient file cleanup** (`#.desktop/.gitignore`, commit
   `f6e0a834`): untracked 527 runtime/state files (per-session
   `entity_menu_history/`, `taskbar_settings_history/`,
   `livedesk_hq_windows_*`, `*.state.txt`, `*_history.txt`, `*.pid`,
   `*.lock`, `colab_hai/`, `nb_tabs/`, `x/`, …) to stop git-history
   bloat. 107 legitimate files remain tracked (entities/, events/,
   sprites/, harnesses/, all `.chtpm/.pdl/.pal` templates). Files stay
   on disk; only their index tracking was removed.
2. **`khtpm_choice_picker.c` deleted** (commit `7b331459`) — see Delta
   A below; it reverses this doc's older §5 claim.

### Deltas correcting stale claims in this doc (noted, not silently changed)

- **Delta A — §5's `khtpm_choice_picker.c` "left in place, unused, as
  a rollback reference — not deleted" is now stale.** It **was deleted
  2026-09-03** (`7b331459`): confirmed via grep it had **zero runtime
  callers** (the live dispatcher `dispatch.sh` resolves
  `khtpm_show_choices.+x`, never choice_picker) and was simply a
  superseded 2026-08-16 fork. Removed: `&.widgits/tile-picker/ops/
  khtpm_choice_picker.c`, its `build.sh` gcc line, and the stale
  `+x` binary; `khtpm_show_choices.+x` still builds and is dispatched.
  If a later agent truly needs a rollback reference for that exact
  file, it's recoverable from git history.
- **Delta B — the branch's own operating handoff.** Environment,
  kill/relaunch discipline, relay-injection/history verify recipe, and
  the chtpm-var-substitution refactor state live in
  `HANDOFF-chtpm-var-substitution.md` (Rev 3-5). Read that alongside
  this doc before editing `khtpm_core_render.c`; the §3 §2b work does
  not restart there — it is the pre-existing `g_is_*` migration this
  doc specifies.

### First next action for the §2b dispatcher (#2)

1. Re-read this doc's §2b + §3 (§1 for the call-site inventory).
2. Confirm the current `g_is_*` count above hasn't grown (re-grep).
3. Rebuild/re-verify nothing active is broken first (`khtpm_png_dump.sh`
   smoke on db-hq-pal per the branch handoff).
4. Per §3 step 2-3: register `swatch-picker` as the FIRST
   `g_khtpm_modes[]` row (its own real functions, smallest surface),
   delete its 18 `g_is_swatch_picker` branches, live-verify, then
   proceed one mode at a time. Do NOT attempt the whole table in one
   pass.

---

## Status update, 2026-08-31 (later same day) — real pivot away from any per-mode table at all

Direct correction, mid-build of open-hai's own mode: "still hardcoding
project names. why?" - caught adding `g_is_open_hai` at ~14 dispatch
points, the exact same pattern as every existing mode, which is the
debt this whole doc exists to end, not extend. Reverted in full
(`git checkout`, confirmed clean) before landing.

Escalated through the real options and their real limits:
- A `g_khtpm_modes[]` table of function pointers INSIDE
  `khtpm_core_render.c` (§2b below) - still requires a rebuild of the
  shared file for every new project. Rejected.
- A `dlopen()`/`.so`-per-project plugin registry, real class-name to
  `.so`-path mapping in a `.pdl` - genuinely needs no rebuild ever, but
  real, direct correction: "we dont use .so or linking or anything...
  'if its not in file its a lie'... they should all use the same
  layout tags and standards. the renderer/parser should have no need
  to know the difference." Rejected - not this house's convention.

**The real, adopted answer**: don't register per-app C behavior at
all. Every app (existing or new) uses the SAME generic tag vocabulary
(`window`/`page`/`item`/`panel`/`button`/`text`) through the renderer's
own already-existing, fully generic default page/item path - the SAME
one taskbar-settings/entity-right-click-menus/choice-picker/the open-
hai sessions proof already use. A real, separate manager keeps
regenerating a real `.chtpm` file as its own live projection; real user
input already dispatches through the existing, proven `action="<shell
command>"` mechanism. This is the exact real philosophy `#.haiku+/
tpmos-re-dox/fo-menu-sys.md` already documents for the ASCII/
`chtpm_parser.c` family - direct instruction: "see existing chtpm
parser std format... use standards in here when possible instead of
hardcoding... can khtpm parser be more similar?" - the khtpm/X11 side
finally getting the real, equivalent capability, not a separate
design.

This needed exactly two real, missing, GENERIC (not project-specific)
engine capabilities, confirmed by direct read before either was built:

1. **Live `.chtpm` re-parse** - `parse_chtpm()` was called exactly
   once, at startup, nowhere else (confirmed via grep before writing
   anything). Added `reparse_chtpm_if_changed()` - a real, generic
   mtime-gated re-parse of `g_chtpm_path`, wired into the periodic tick
   for the default/popup family only (`!g_is_db_hq && !g_is_events_hq
   && !g_is_chat_hai` - those three own real, cached Elem pointers into
   their tree and manage their own real content refresh already;
   reparsing out from under them is a real, deliberate exclusion, not
   an oversight). Live-verified: edited a real `.chtpm` on disk while a
   real window was open, confirmed the content changed live, confirmed
   growing content (1 row -> 3 rows) resized the real window correctly.
   **Two real bugs found and fixed during that same live testing**,
   both now permanent, generic fixes benefiting every future consumer
   of this capability, not just open-hai:
   - `BadMatch` on `XGetImage` when content grew past the already-
     allocated Pixmap - the default mode's own final present path never
     had the resize-safety check db-hq/events-hq's own branches already
     have (never needed one before this capability existed). Ported the
     same real fix.
   - **Direct live report** ("blank black screen that flashes before
     load" on every entity context menu): the fix above depends on
     `g_buf_w`/`g_buf_h` correctly recording the real Pixmap size - the
     default mode's own window-creation code never set them (0/0
     default), so the NEW resize check misfired on literally every
     popup's first-ever frame, discarding real content as a spurious
     "grow". Fixed by recording the real size at Pixmap creation,
     matching what every other mode's own window-creation code already
     does.
2. **Generic text-input element** - real, confirmed gap: `<cli_io>`
   only exists inside db-hq's own code (`dbhq_cli_io_navigable()`), not
   the generic default path. Needed for anything (open-hai's composer,
   any future app) to accept typed text with zero per-app C. Design/
   build not started as of this status update - next real step.

**Backlog, recorded here per direct instruction, not started**: the
"1.hq hide" button currently closes/hides every HQ window and entity
outright; the real, intended behavior is a pure z-order toggle (send
behind other open windows/terminals, not close/hide) - separate, real,
later fix, unrelated to this doc's own subject.

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
  confirmed BIGGER lift than a simple registration: ~3,769 real lines
  across the render+manager files, its own hand-rolled text wrapping,
  emoji-tile blitter, flat nav array, drag/resize handling - none of it
  on the Elem/CSS model, comparable in real size to chat-hai's own
  original migration. Direct instruction, 2026-08-31, given this size:
  "scoped first slice, prove the pattern" rather than a full rewrite in
  one pass. **First slice DONE, live-verified**: a real, standalone,
  NOT-wired-into-the-live-app proof (`ohp_sessions_preview.c`, new file)
  reads the real, live `state/sessions.state.txt` (real session dir|
  label rows, khtpm_open_hai_render.c's own real, unmodified format)
  and generates a real temp `.chtpm` - one `<item>` per real session,
  same exact real pattern the choice-picker consolidation just proved
  - then launches the SAME shared `khtpm_core_render.+x`. Zero new C
  code in the shared renderer, again. Screenshot-verified: 22 real
  chat-session labels rendered correctly with real `[>]`/`[ ]` nav
  badges and a real Close item, real window auto-sized to the real
  item count. `button.sh`/`chat_button.sh` (the real, live app the user
  actually uses daily) are completely UNCHANGED - this proof runs
  standalone, zero risk to the real chat tool while the pattern is
  proven. Real, minor, non-blocking finding from this test: the shared
  parser does not XML-decode entities in attribute values (a label
  containing a literal `&`, escaped to `&amp;` per real XML rules,
  renders as the literal text `&amp;` instead of `&`) - noted, not
  fixed, doesn't block anything here since real session labels rarely
  contain `&`.
  **Real work still not done**: converting the actual LIVE app (real
  message send/receive, real transcript scrolling, real settings, real
  session switching) onto this pattern - this slice only proved the
  static/read-only rendering half. That remains a real, separate,
  larger task - author the full `open-hai.chtpm`, wire real send/
  receive through the manager's own real request/transcript files via
  `reusable_slot()`-injected content, migrate emoji-tile rendering onto
  the shared sprite mechanism `khtpm_css_parser.c`'s `sprite=` attribute
  already provides for palettes - THEN register/switch the real
  `button.sh` over, only once fully proven equivalent live.
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
  pick, real token on stdout). **UPDATE 2026-09-03: the standalone
  `khtpm_choice_picker.c` — which the "left in place as a rollback
  reference, not deleted" sentence below once described — has since
  been DELETED (commit `7b331459`)**. A 2026-09-03 grep confirmed zero
  runtime callers (the live dispatcher is `khtpm_show_choices.+x`);
  the source, its `build.sh` gcc line, and the stale `+x` binary were
  removed. Recoverable from git history if ever genuinely needed.
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

## Status update, 2026-08-31 (later same day) — capability #2 (generic `<cli_io>` text input) DONE

Real generic capability #2 (the second, and last, missing piece the
"real pivot away from any per-mode table at all" section above called
for) is built, live-tested end to end, and works with ZERO new
`g_is_<project>` branches. Same real reference this whole pivot was
built from - `chtpm_parser.c`'s own `UIElement.input_buffer`/
`target_id` fields and its real key-handling shape (append/backspace/
Enter-saves-and-clears-but-stays-armed) - ported as closely as the
X11/Xft side allows, not reinvented.

**What was added, all in the shared renderer/shared-lib, none of it
project-specific:**
- `Elem.input_buffer[256]` / `Elem.target_id[64]` - two new generic
  fields on the one shared `Elem` struct (`&.widgits/_shared-lib/
  khtpm_render_core.c`), same real "any tag can carry these, most
  never populate them" shape every other optional Elem field already
  uses.
- `target_id="..."` attribute parsing in `apply_attr()` - a real
  cli_io element's own key into `cli_io_state.txt`, defaulting to its
  `id` when omitted (mirrors the reference's own fallback).
- `g_default_input_elem` + `default_cli_io_state_path()`/
  `default_cli_io_save()`/`default_cli_io_run_action()`/
  `default_cli_io_handle_key()` (`khtpm_core_render.c`, right before
  `activate_focused()`) - a real, generic "one armed field at a time"
  mechanism: Enter/click on a `<cli_io>` arms it instead of dispatching
  its `onclick` immediately (`activate_focused()`'s own new tag check);
  while armed, printable keys append and backspace trims, each real
  edit immediately synced to `<package_dir>/cli_io_state.txt` (a real
  `key=value` line file, one line per real `target_id`, same "always
  in a file, never only in memory" house rule this whole design
  answers to); Enter saves once more, fires the element's own
  `onclick` as a real shell action via the SAME `action="<cmd>"`
  dispatch every other khtpm element already uses, then clears the
  buffer (stays armed, matching the reference's exact real behavior -
  not chosen arbitrarily); Escape disarms without saving.
- `draw_elem()` (`&.widgits/_shared-lib/khtpm_draw_core.c`, the real
  shared master - the ops-dir copy was edited first by mistake, then
  the SAME edit was ported to this master so the next build doesn't
  silently discard it) - a `cli_io` tag now shows its own live
  `input_buffer` appended after its static label, with a real trailing
  `_` cursor glyph while armed. The existing generic
  `nav_index == g_focus_nav` focus-outline box (already tag-agnostic)
  needed no change to also frame a `cli_io` field.
- **Real bug found and fixed along the way**: `assign_nav_and_layout()`'s
  own default (non-db-hq/events-hq/chat-hai) page/item layout loop only
  ever checked `strcmp(item->tag, "item") == 0` in both its grid and
  list branches - a `<cli_io>` element was silently skipped: never
  given real `x/y/w/h`, never added to `g_nav[]`, so it could never be
  focused, clicked, or even seen. Fixed by accepting `"item"` OR
  `"cli_io"` in both loops - the fully generic fix, not a `cli_io`-
  specific branch.

**Live-tested** (fresh `/tmp/*.chtpm` with one real `<cli_io
target_id="test_field">` + a close item, launched against the freshly
built `khtpm_core_render.+x`, driven entirely through the real
`#.desktop/entity_menu_history/<pid>.txt` relay convention - same
verification standard as every other change this session):
Enter arms the field; typing `h` then `i` shows `test_field=h` then
`test_field=hi` in `cli_io_state.txt` after each real keystroke;
Backspace shows `test_field=h`; Escape disarms and leaves the last
saved value untouched (no further write). A separate run confirmed
Enter after typing runs the real action and clears+re-saves the field
to empty, staying armed. All exactly as designed, no surprises.

Not yet started: actually rebuilding open-hai's own real `.chtpm`
projection (a real manager-side generator, likely extending
`khtpm_open_hai_manager.c`) to use this plus capability #1 - that is
the real, standing task both of these capabilities exist to unblock,
and it has not been restarted yet.

## Status update, 2026-08-31 (later same day) — open-hai's real conversion built, `<cli_io>` proven end-to-end on real human hardware

`khtpm_open_hai_manager.c` now owns a real `write_chtpm_projection()`,
called every main-loop tick: regenerates `&.widgits/open-hai/open-hai.
chtpm` from real state (sessions.state.txt, active_session.txt,
transcript.txt tail, pending_tool.state.txt, model.txt, settings.pdl),
using ONLY `item`/`cli_io`/`text` tags - zero new renderer C. Real
`CYCLEMODEL`/`TOGGLESOUND` request tokens added to the manager's own
`request.txt` protocol (model-cycling/sound-toggle moved manager-side,
since the old per-app hand-rolled renderer that owned them is being
replaced). Two new real action scripts, `oh_write_request.sh` (plain
`<item>` actions - real argv shape documented in its own header) and
`oh_write_send.sh` (the composer's own `<cli_io>` action - a DIFFERENT
real argv shape, also documented in its own header) - both embed the
manager's own live `g_state_dir` as a literal argv so this stays
correct under the manager's existing `--data-root` per-persona-pal
feature, not just plain open-hai. A content-unchanged guard skips the
write (and so the mtime bump, and so a needless reparse) when nothing
real actually changed.

**Real bugs found and fixed along the way (all via direct, repeated,
live relay-driven + real-hardware testing, not guessed):**
1. `assign_nav_and_layout()`'s default list-layout loop only ever laid
   out `item`/`cli_io` tags - a plain `<text>` row (status line,
   transcript message, tool banner) was left at its parse-time default
   `x/y/w/h` (0,0,0,0), garbled at the origin AND silently shifting
   every later item/cli_io up one row from its visual document
   position. Fixed generically: `text` now advances layout exactly
   like `item` but is never added to `g_nav[]` (not interactive).
2. `reparse_chtpm_if_changed()` resets `g_n_elems = 0` and rebuilds
   the ENTIRE `g_pool[MAX_ELEMS]` in place (never frees/reallocates) -
   `g_default_input_elem`, if left pointing into the old tree, becomes
   a dangling/aliased pointer into whatever the new parse happens to
   write at that same pool slot. A live-regenerating manager reparsing
   mid-arm isn't a rare edge case for this feature, it's the normal
   case. Fixed: disarm (`g_default_input_elem = NULL`, releasing any
   real keyboard grab - see #4) on every reparse.
3. The default/popup mode's real content draw does NOT call
   `render_tree()`/`draw_elem()` directly - it serializes the visible
   subtree to a text frame file (`dbhq_serialize_frame_subtree()`) and
   repaints from a SEPARATE parser (`dbhq_paint_frame_line()`) that
   builds a fresh, memset-zeroed temp `Elem` per line and calls
   `draw_elem()` on THAT. This is why a live-typed `input_buffer` never
   showed on screen through this path even though the underlying state
   was genuinely correct (confirmed via `cli_io_state.txt`) - the
   temp `Elem` never carried `target_id`/`input_buffer` at all, and the
   armed-indicator's own pointer-equality check (`e ==
   g_default_input_elem`) could never be true against a fresh local.
   Fixed on both ends: `dbhq_serialize_frame_elem()` now appends
   `target_id`/`input_buffer` as two more pipe-escaped trailing fields
   (a literal `|` in typed text is real and plausible - a naive split
   would misparse it exactly like onclick's own pipes once did, see
   that fix's own 2026-08-28 comment); `dbhq_paint_frame_line()` parses
   and populates them; the armed-indicator check now compares by `id`
   string instead of pointer (the one thing the round trip already
   carries faithfully).
4. **Real human-hardware bug, not reproducible via relay-file testing
   at all**: armed correctly (real "^" showed after a real double-
   click), but real physical keystrokes typed nothing. Root-caused
   live: `XGetInputFocus` returned `0x0` (None) with the real mouse
   pointer far from the popup window - this desktop's WM uses
   focus-follows-mouse, and moving the hand from mouse to keyboard
   silently took real X keyboard focus away from an `override_redirect`
   popup whose only focus mechanism was a plain `XSetInputFocus` retry
   at map time (mouse-position-dependent by construction). Studied
   both existing real precedents in this file before fixing: chat-hai's
   own conclusion (`chai_focus_grab_enabled` defaults OFF - a plain
   `override_redirect` + `XMapWindow` with zero focus calls was found
   to be the MORE reliable real behavior for its own window shape) and
   db-hq's own real, already-built, currently-gated-off
   `dbhq_grab_keyboard_retry()` (an `XGrabKeyboard` retry loop - a real
   exclusive grab is immune to focus-follows-mouse by construction,
   since the server routes KeyPress to the grabbing window regardless
   of pointer position or WM focus policy). Reused db-hq's function
   verbatim, scoped tightly to exactly a `cli_io` field's own armed
   lifetime (grabbed in `activate_focused()` on arm, released in
   `default_cli_io_handle_key()`'s Escape branch AND in
   `reparse_chtpm_if_changed()`'s own new disarm-on-reparse safety net
   from bug #2 above - an exclusive grab surviving a silent mid-type
   disarm would lock ALL keyboard input house-wide to one non-typing
   window until it closed, a real, much worse bug than the one being
   fixed). Made unconditional (no `.pdl` gate) since no existing popup
   uses `cli_io` yet - can't regress anything already working.
   **Confirmed fixed on real human hardware** (double-click to arm,
   move mouse away, type - characters now appear).

**Real, house-wide takeaway for any future khtpm app needing text
input**: use `<cli_io>` - as of this fix, it's the only real input
mechanism in this house proven immune to the focus-follows-mouse class
of bug. `db-hq`'s own armed-field input (bookmarks' New+ entry) and
`events-hq`'s own picker fields likely have the identical bug (same
plain-`XSetInputFocus`-only mechanism, `g_dbhq_focus_grab_enabled`
defaults OFF) - not yet checked or fixed, flagged as real follow-up
work, NOT to be touched without checking in first (these are live,
daily-used windows, not a fresh capability with nothing to regress).

Still not started: switching the real daily-driver
`button.sh`/`chat_button.sh` over to this new mechanism - the isolated
`--data-root`-scoped test proved the whole pipeline end-to-end, but
the live app is untouched. Do not touch it without checking in first
(direct instruction, 2026-08-31: "check in with me before we edit any
legacy projects so i can do my own safety checks").

## Status update, 2026-09-01 — generic `<module>` launch for default mode, real sidebar+panel dual-region layout, "lets start" on the switch

Direct instruction ("lets start") after a real, full `.7z` backup of
the whole dev-env was made (excluding image assets) - proceeded with
the FIRST real piece of the button.sh switch: a checked-in bootstrap
`open-hai.chtpm` (a `<module>` tag + a one-line placeholder page) plus
a real, generic, NEW capability in `khtpm_core_render.c`: the default/
popup mode never had ANY `<module>` launch support before now (db-hq/
events-hq/chat-hai each carry their own separate copy). Reused the
already-generic `launch_module()` (§2a) and db-hq's own
`g_dbhq_module_pid`/`dbhq_cleanup_module()` (neither actually has a
`g_is_db_hq` check inside despite the name - just never wired up for
this mode) - checked ONLY once, at initial parse, never inside
`reparse_chtpm_if_changed()` (which fires on every real content change
for this mode - re-checking there would fork a new manager every
tick). Live-verified against a fresh isolated `--data-root`: the
manager launches as a real child of the renderer, immediately
overwrites the bootstrap with its own live projection, capability #1
picks it up within one tick. Zero regression risk to db-hq/events-hq/
chat-hai (their own module-launch code, in their own `g_is_X` branches,
untouched).

**Real bug found+fixed along the way**: `label=` attribute values never
got XML-entity-decoded (only `action=`/`onclick=` did) - a real session
snippet containing `&.widgits` showed as the literal text `&amp;.
widgits` on screen. Ported the existing `decode_entities()` call to
`label=` too.

**Real UX findings from live testing against actual production
sessions data** (49 real sessions + full transcripts) forced a bigger,
real design change mid-stream, by direct instruction ("full sidebar
redesign now" - not a quick scroll cap): the flat single-column list
(every session AND every transcript message as one long numbered
column) does not scale - window grew to 1472px tall, and the user's
own real correction ("we actually didn't number every message, but we
did number a sidebar with different chat sessions to resume") called
for real structural separation, not just a scroll cap on the existing
flat shape.

**Real, generic sidebar+panel dual-region layout - a genuinely new
shared capability, not open-hai-specific** (`khtpm_core_render.c`'s own
new "generic sidebar+panel scroll" section, tag-based only, zero
project knowledge - any future default-mode consumer with a long list
+ composer can use this):
- `<sidebar>` and `<panel>` as direct children of `<page>` triggers the
  new `layout_sidebar_panel()` path; a page with neither still gets the
  exact same flat-list behavior as before (zero regression for swatch-
  picker/choice-picker/taskbar-settings/network-browser's own current
  `.chtpm`, none of which use these tags).
- Fixed window size (`window`'s own CSS width/height, else a real
  700x520 default) - the actual fix for the unbounded-growth bug.
- `<sidebar>`'s own children scroll independently (`layout_scroll_
  region()`, a new, real, generic helper: only `h/ROW_H` rows are ever
  given a real position/nav_index, the rest pushed off-canvas until a
  real Page_Up/Page_Down - newly wired into `handle_key()`, which had
  NO Page_Up/Down case at all before now for this mode - brings them
  into view; the SAME helper also lays out a `<scrolllist>` nested
  inside `<panel>`).
- `<panel>`'s own direct item/text children (NOT inside its own
  `<scrolllist>`) are fixed, always-visible rows (status/controls stay
  reachable without scrolling past a long transcript - real, deliberate
  UX requirement, not incidental); a nested `<scrolllist>` gets
  whatever vertical space is left after those fixed rows and a pinned
  `<cli_io>` composer (a generic composer is NEVER part of any scroll
  flow, tag-based rule, not ID-based).
- `<scrolllist>` auto-follows new content by default (real chat UX) -
  `reparse_chtpm_if_changed()` resets its scroll to a huge sentinel,
  clamped to the real max on the very next layout pass. Sidebar scroll
  is deliberately untouched by reparse (a session list has no "newest
  at the bottom" convention to auto-follow).

**Second real bug found+fixed, live, on a genuinely fresh `--data-root`
with zero prior sessions**: a brand-new session never appeared in the
sidebar at all. Root cause: `start_new_session()`'s own real callers
(`NEWSESSION`/`DELETESESSION` handlers) already remembered to call
`publish_sessions()` right after, but main()'s own startup bootstrap
(no sessions exist yet → start one) never did - invisible on any
`--data-root` with prior real data (this session's own earlier testing
never hit it), but exactly what a genuinely first-ever run would hit.
Fixed inside `start_new_session()` itself so no future caller can
forget - the two existing call sites' own `publish_sessions()` calls
are now harmless, cheap redundant republishes.

**Third real thing found+fixed, live**: an early attempt at real visual
separation ("no separation elements" - direct live report) set
`has_bg_color`/`has_border_color` directly on the live `sidebar`/
`cli_io` Elem objects inside `layout_sidebar_panel()`. Never once
painted - the default/popup mode's real content draw round-trips every
frame through a text frame file (`dbhq_serialize_frame_subtree()`/
`dbhq_paint_frame_line()`, the SAME indirection already responsible for
generic capability #2's own `input_buffer`/`target_id` bug earlier this
session) which does not carry style fields at all; the paint side
always recomputes style fresh from CSS. Real fix: `sidebar { ... }` /
`cli_io { ... }` rules added to `entity_menu_default.css` (the ONE
shared CSS file every default-mode consumer already loads, not a per-
app file - db-hq/events-hq/chat-hai are the only modes with their own
extension-swapped `<name>.css`) - a real CSS rule DOES survive the
round trip (the paint side calls `css_compute_style()` itself, using
the same real sheet), a live-set style field does not. Removed the
dead programmatic style code entirely rather than leaving it as
misleading dead weight.

Live-verified end to end on a fresh `--data-root`: real session
creation + publish + sidebar display, real fixed 700x520 window, real
visual separation, real send → real model response → real transcript
display inside the auto-following scrolllist, all together, no
regressions from the earlier isolated flat-list testing.

Still not done (same "check in first" instruction as the section
above, unchanged): `button.sh`/`chat_button.sh` themselves have not
been edited yet - only the bootstrap `.chtpm` + the renderer's own new
generic module-launch capability are in place so far. Also not yet
done: `launch_module()` only forwards ONE extra argv token, which is
enough for plain `button.sh` (no `--data-root` needed) but not for
`chat_button.sh`'s own per-instance `--data-root <dir>` (two separate
tokens) - flagged as real follow-up, not blocking plain `button.sh`.

## Status update, 2026-09-01 (later same day) — `button.sh` SWITCHED, real production data live-verified, DONE

Direct instruction ("yes, pls finish completely this phase") - `button.sh`
now launches `khtpm_core_render.+x` (pointed at the checked-in
bootstrap `open-hai.chtpm`) instead of the old `khtpm_open_hai_render.+x`.
Real, house-standard kill/relaunch discipline kept (pgrep -f full-
cmdline match, TERM-then-KILL, confirm exactly one PID after launch) -
now ALSO kills any leftover instance of the OLD renderer as a one-time
transition safeguard, real `--data-root` exclusion preserved for both.
This script only ever launches ONE process now (the manager is the
renderer's own real `<module>` child, tied to its lifetime - closing
the window stops both, no separate PID to track). The old render
binary/its own build script are untouched on disk, a real rollback
reference.

**Live-verified against REAL production data** (real `button.sh`,
real house_root, real 49-session history, real ongoing conversations) -
not just the isolated `--data-root` test: real launch succeeded, real
kill-existing-then-relaunch succeeded on a second run, sidebar shows
every real session, panel shows real transcript, composer visually
distinct, fixed 700x520 window.

**One more real bug found+fixed, this time only visible against real
data** (the isolated test's session labels/messages were all short
enough to never trigger it): a session label or message longer than
its own element's real box width drew straight past that box into
whatever was next to it, with zero clipping - looked exactly like a
garbled double-render until traced back to plain text overflow via the
raw frame-file data (which was byte-for-byte correct - x/y/w/h all
right, this was purely a DRAW-time gap). Real, generic fix in the
shared `draw_elem()` itself (`khtpm_draw_core.c`, not open-hai-
specific): any element with a real `w>0` now gets its own label
truncated to fit, with a real UTF-8-safe `"..."` ellipsis (real message
text contains multi-byte emoji - truncation backs up over continuation
bytes, never cuts mid-codepoint). A harmless no-op for anything that
already fits, so nothing currently working across ANY mode can
regress - but this is the actual reason the isolated `--data-root`
tests all looked clean while the first real-data run didn't: short
test strings never exceeded any box, so this exact gap was invisible
until real, long, real-world content hit it.

**A real, worth-remembering quirk for future agents**: `open-hai.chtpm`
itself is git-tracked with ONLY its bootstrap content (the `<module>`
tag + a one-line placeholder) - the moment the real manager runs (which
`button.sh` triggers immediately), it overwrites that same path with
its own live projection, same as it does every ~200ms after. A `git
status`/`git diff` run while the real app is live will show this file
as "modified" with real, current session/transcript content - this is
expected, not a bug to fix or a change to stage. `git checkout -- 
open-hai.chtpm` restores the real bootstrap before any commit that
touches this file - do not `git add` whatever the live manager most
recently wrote.

This phase is complete: open-hai's real daily driver now runs on the
shared khtpm_core_render.+x pipeline, zero new renderer C specific to
open-hai, the same generic capabilities (#1 live reparse, #2 cli_io,
the new sidebar+panel scroll, the new default-mode module launch) any
future khtpm app can reuse. Remaining real follow-up, explicitly NOT
done yet (flagged, not blocking): `chat_button.sh`'s own switch (needs
`launch_module()` extended for a 2-token extra arg to forward
`--data-root <dir>`); applying the same real keyboard-grab fix to
db-hq/events-hq's own armed-input fields (check in first, per direct
instruction - these are other live, daily-used windows).
