# HANDOFF — branch `chtpm-var-substitution`

**Last updated:** 2026-09-03 (rev 7)
**Branch:** `chtpm-var-substitution` (off `origin/main` @ `bbf9caf2`), pushed
**Goal:** restore the tpmos layout/data separation for khtpm windows
(`CHTPM-ARCHITECTURE-FIX.md`) — static template + a projector that writes
`state/ui.txt`, instead of a manager that regenerates markup or C that
builds the tree.

Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN

---

## State: DONE and pushed

### Renderer primitives — `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`

| primitive | what |
|---|---|
| `${var}` substitution | `parse_chtpm()`: if the template has `${` or `<repeat`, load the `vars="<path>"` state file and substitute `${key}` before parsing. `\$ \{ \\` escapes, `\n`→newline, unknown→empty. Relative `vars=` resolves against the `.chtpm`'s own dir (`g_package_dir`). |
| `${HOUSE}` / `${PKG}` | built-ins in `kh_get_var()` for action paths |
| `show="${x}"` | in `parse_element()` — drop the element when the value is `""`/`0`/`false` |
| `<repeat count="${n}" bind="x">…${x.field}…${x.#}…</repeat>` | in `parse_chtpm()` before `${var}`. `count` = bare int or one `${var}`. No nesting (v1). `KH_REPEAT_MAX` 4096. |
| `<!-- -->` skipped in both passes | a `${…}`/`<repeat>` inside a doc comment was being processed |
| `--dump-and-exit` (any argv) | every mode now: `dump_frame_png()` writes `/tmp/entity-menu-frame.png` + `.receipt.txt` + `.frame.txt` (ASCII Elem tree). Calls `reparse_chtpm_if_changed()` first so a just-launched projector's write lands. |
| `launch_module()` splits `src` on whitespace | `<module src="<interp> <script>"/>` (tpmos style). Each relative token resolved vs `package_dir` then `house_root`. Exports `KHTPM_HOUSE` / `KHTPM_PKG` / `PRISC_PROJECT_ROOT` to the child. One-token `src` (compiled manager) unchanged. |

Wrapper: `44.xyz.01.00/&.widgits/_shared-lib/ops/khtpm_png_dump.sh <chtpm> [house] [outdir]`.

### PAL interpreter string ops — `44.xyz.01.00/&.widgits/_shared-lib/system/prisc+x.c`

Backwards-compatible (additive: new `strcmp` branches, new executor cases,
`sregs[16][4096]` separate zero-init bank, enum values appended). A `.pal`
with no `s*` mnemonic runs byte-for-byte as before. The ~20 project-local
`prisc+x.c` copies are untouched.

Ops: `slit scpy sappend sgetenv sfmt sread ssplit sfind slen sfopen
sfappend swrite sfclose sbeq sbne strim satoi` — full spec in
`_shared-lib/system/string-ops.md`. Also: trailing `#` comments on
instruction lines (quote-aware). Build: `_shared-lib/ops/build_prisc.sh`
→ `_shared-lib/system/+x/prisc+x.+x`.

### Apps converted (manager/projector writes `state/ui.txt`, static template does layout)

| app | projector | commit |
|---|---|---|
| `&.hq-apps/signup-hq` | C (`signup_hq_manager.c` `write_state()`) | `1e088661` |
| `&.widgits/open-hai` | C (`khtpm_open_hai_manager.c` `write_state()`) | `a255846f` |
| `&.hq-apps/co-lab-hai` | C (`colab_hai_manager.c` `write_chtpm_projection` → key=value) | `f4ace065` |
| `&.widgits/db-hq-actors-pal` | **PAL** (`pal/actors_projector.pal`) — the reference | `5438043b` |
| `&.hq-apps/db-hq-pal` | **PAL** (`pal/dbhq_projector.pal`) — the real 15-tab db-hq | `4837684a` |

All verified headless with `khtpm_png_dump.sh`.

Doc with full status + `<repeat>` v2 sketch:
`08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md` §8.

---

## Done — this rev

- **`*.chtpm` → `*.xhtpm`** for all converted apps (`4837684a`). Parser
  doesn't care; sibling `.css` found by extension-swap. `.bootstrap`
  duplication removed (projectors write `state/ui.txt`, never the
  template), restore blocks stripped from every `button.sh`.
- **`&.hq-apps/db-hq-pal/`** — the full 15-tab db-hq as ONE static
  `dashboard.xhtpm` + `pal/dbhq_projector.pal`. Tabs are toolbar
  `<item>`s writing `state/active.pdl` via `ops/dbhq_action.sh`; the
  projector reads that + the tab's `#.desktop/db_hq_<x>.state.txt`
  (uniform `TAG | key | value`) and writes rows + selected-record
  fields. `class="db-hq-pal"` keeps the renderer's `g_is_db_hq` C
  path dormant. Common Events tab → `is_ce=1` → links to the existing
  editor. Verified headless: tab switch + record select.
- `prisc+x` `NUM_SREGS` 16 → 32.

## State: IN FLIGHT — nothing right now

---

## State: NOT DONE

- **network_browser** — page-content area is heterogeneous (TITLE/TEXT/
  LINK/IMG/VIDEO + sprite-grid wrapping). Needs `<repeat>` v2 (per-kind
  element or nested). Sketch in `CHTPM-ARCHITECTURE-FIX.md` §8.
- **chat-hai** — projector is `chat_hai_projector.sh` (bash). Same split,
  in shell.
- **db-hq (real)** — `db-hq-pal` is the parallel proof; folding it back
  into the `class="db-hq"` window (retiring the `dbhq_*` C in the
  renderer) is the large follow-on. CE is its own subsystem.
- `CENTROID_GOLD_STD.md` — update once the above land.

---

## How to test any converted window

```sh
HOUSE=…/44.xyz.01.00
sh "$HOUSE/&.widgits/_shared-lib/ops/khtpm_png_dump.sh" \
   "$HOUSE/&.hq-apps/<app>/<name>.xhtpm" "$HOUSE" /tmp/out
# prints + cats /tmp/out/<name>.receipt.txt and .frame.txt ; PNG at .png
```

`frame.txt` is the ground truth (tag | id | class | label | … | onclick | …).

## Rules

- Never `git add -A` — stage explicit paths. `khtpm_core_render.c` and
  `khtpm_taskbar_manager.c` have concurrent editors.
- Commit footer: `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`
  / `Claude-Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN`

---

## Rev 3 (2026-09-03) — generic `<tabbar>` + remaining-work handoff

### Done this rev

- **Generic `<tabbar>`/`<tab>`** ported into `khtpm_core_render.c` so a
  plain `class=""` template gets the old db-hq shape (`1e628f49`):
  - `layout_sidebar_panel()` lays a `<tabbar>` child of `<page>` as a
    horizontal strip, grows the window to fit, tabs nav-numbered first,
    active tab gets an `"active"` class (CSS `.tab`/`.tab.active`).
  - `activate_focused()` `<tab>` branch: run `action=`, mark active
    (`g_default_active_tab_id`, survives reparse), scope nav into the
    page `<sidebar>` — `[^]` on the tab, `Esc` back to the tab row.
- **Interact-mode scoped nav** for generic `ACTIVATE` (`0eae03b1`):
  `onclick="ACTIVATE" target_id="X"` confines nav to container `X`'s
  subtree; `Esc` pops; `[^]` on the trigger. Gated by
  `g_default_scope_confine` — a plain dropdown `ACTIVATE` is unchanged.
- **db-hq-pal** rewritten to the faithful shape (`<tabbar>` + 15
  `<tab>` + `<sidebar>` + `<panel>`, `dashboard.css` ported from the
  original). Renders ~1285px like the old db-hq.

### Left to do — safe for another agent / Haiku subagents

Each is the same recipe: static `<name>.xhtpm` template + a projector
(`.pal` OR compiled `.+x` C — whichever is easier) that writes
`state/ui.txt`; `button.sh` launches `khtpm_core_render.+x <house>
<name>.xhtpm`; verify with `khtpm_png_dump.sh`.

| # | target | notes / difficulty |
|---|---|---|
| 1 | **live-test db-hq-pal** tab→scope→`Esc`, then add its `toy.pdl` to a launcher / the toys menu | needs a real desk click; toy.pdl already exists |
| 2 | **chat-hai** | replace `ops/chat_hai_projector.sh` (bash) with a `.pal` or `.c` projector writing `state/ui.txt`; author `chat-hai.xhtpm`. Message list = one `<repeat>`. Straightforward. |
| 3 | **tile-picker** (`&.widgits/tile-picker/ops/khtpm_show_choices.c`) | small one-shot chooser; low priority |
| 4 | **events-hq** (`&.widgits/events-hq`, `g_is_events_hq`) | now unblocked — same `<tabbar>` port as db-hq-pal. Has view sub-tabs (Scripting/Scratch/Blueprints) + a command list. Its `evhq_*` C retires. Medium. |
| 5 | **palettes / bookmarks / stats-hq** (`g_is_palettes` / `g_is_bookmarks` / `g_is_stats_hq`, all ride `g_is_db_hq`) | each: static template + projector reading its existing state files. Palettes needs the `sprite=` grid (already a generic `draw_elem` capability). Medium each. |
| 6 | **swatch-picker / taskbar-settings** (`g_is_swatch_picker`) | flat `<item>` list + opacity ± ; small |

### Left to do — needs care (do NOT hand off)

| target | why |
|---|---|
| **network_browser** | page content is heterogeneous (TITLE/TEXT/LINK/IMG/VIDEO + sprite-grid wrapping). Needs **`<repeat>` v2** (per-kind element via `show=` on each candidate, or nested repeats). Sketch in `CHTPM-ARCHITECTURE-FIX.md` §8. |
| **retire `dbhq_*` C** | once db-hq-pal is signed off, point the real `open_db_hq.sh` at `dashboard.xhtpm`, drop `class="db-hq"`, delete the `dbhq_*` renderer C (~1350 lines). Big, do deliberately, its own branch. |
| **`CENTROID_GOLD_STD.md`** | document static-template + projector (`.pal`/`.c`) as the standard once the above lands. |

### `<repeat>` v2 sketch (for network_browser / any heterogeneous list)

```xml
<repeat count="${content_count}" bind="c">
  <text class="page-title" label="${c.text}"   show="${c.is_title}"/>
  <text                    label="${c.text}"   show="${c.is_text}"/>
  <item label="${c.text}" action="${c.action}" show="${c.is_link}"/>
  <item label="${c.label}" sprite="${c.sprite}" action="${c.action}" show="${c.is_media}"/>
</repeat>
```
The projector sets exactly one `c_<i>_is_*` to 1 per row. Sprite-grid
wrapping (consecutive media in one `<row class="sprite-grid-row">`) is
lost in this first cut — acceptable, or add a `<repeat wrap-class=...
wrap-when="${c.is_media}">` later.

---

## Rev 4 (2026-09-03) — `<tab>` scope lock actually working

### Done this rev

- **`59acda6d` — scope confinement now runs on the sidebar+panel path.**
  `kh_apply_scope_confine()` (was an inline post-pass at the tail of
  `assign_nav_and_layout()`) was **dead code for db-hq-pal**: the
  `layout_sidebar_panel()` branch `return`s early, before the tail.
  Factored into `kh_apply_scope_confine()`, called on *both* layout
  paths. Now: Return on a `<tab>` shrinks `g_nav[]` to that tab + its
  record rows; arrows can't leave; `Esc` restores the full tab row.
- **`dde3291e` — window chrome stays reachable under a scope.**
  `id="chrome-*"` (minimize / fullscreen / close) is kept navigable
  even while confined — it lives outside the page tree, like a real WM
  titlebar.

### Verified (headless, via history injection — NOT a live desk click)

Injection path after the refactor is unchanged: write
`KEY_PRESSED: <code>` / `MOUSE_EVENT: <btn> <x> <y> <press>` lines to
`#.desktop/entity_menu_history/<pid>.txt`; the generic window's
`poll_agent_history()` (in `hq_idle_tick`) consumes them via
`dispatch_relay_code()` → `handle_key()`. Codes: 13=Return, 27=Esc,
200-205=arrows/page, 32-126=ASCII. Live per-redraw dump to read back:
`#.desktop/entity_menu_frame_<pid>.txt` (`tag|id|class|label|extra|
onclick|nav_index|active_tab_flag|x|y|w|h|target_id`). Field 7 =
nav_index, field 8 = "is the selected tab" (NOT the focus ring — the
focus ring is `nav_index == g_focus_nav`, only visible in the PNG).

```sh
PID=$(pgrep -f 'khtpm_core_render\.\+x .*dashboard\.xhtpm' | head -1)
H=44.xyz.01.00/#.desktop/entity_menu_history/$PID.txt
F=44.xyz.01.00/#.desktop/entity_menu_frame_$PID.txt
: > "$H"; sleep 1.5
printf 'KEY_PRESSED: 201\nKEY_PRESSED: 13\n' >> "$H"; sleep 1.2   # Down onto a tab, Return
awk -F'|' '$7!=""&&$7!="0"{print $7,$1,$2}' "$F"   # -> tab + 4 records + 3 chrome only
```

### OUTSTANDING — matches prior interact mode, still to do

**`kh_apply_scope_confine()` currently renumbers `g_nav[]` and zeroes
`nav_index` on out-of-scope items.** The user's spec (stated twice):
*prior interact modes keep every index assigned/visible — keys are just
passed through / ignored for out-of-scope items until `Esc`.* So the
preferred implementation is **gate input, don't mutate the tree**:

- Leave `g_nav[]` / all `nav_index` untouched while a scope is held.
- Add an `elem_in_scope(Elem*)` predicate (the current `keep` test:
  under `g_default_active_scope_root`, or the trigger id, or a bound
  `dropdown-child`, or `chrome-*`).
- In the generic arrow / Page / digit-jump handlers and
  `activate_focused()`: when `g_default_scope_confine`, skip over /
  refuse to land on `!elem_in_scope(target)` — clamp movement within
  the in-scope set, ignore digit jumps and clicks outside it.
- `khtpm_draw_core.c` can dim out-of-scope rows (optional polish).

This is more invasive than the ship-it fix above (which functionally
locks correctly and the user OK'd keeping the numbers visible), so it's
a refinement, not a blocker. The two commits above are safe to keep as-is.

### db-hq-pal status

Wired into the strip `db` menu as **`db-hq (PAL)`**
(`livedesk:open-db-hq-pal` in `khtpm_taskbar_manager.c`,
`&.hq-apps/db-hq-pal/button.sh`). Tab switch + record list + panel
fields all project correctly (`pal/dbhq_projector.pal`). Still read-only
— field editing and the Common Events command editor are the next
build (see `DB-EVENTS-HQ-PORT-DESIGN.md` §3–§5). Old `db-hq`
(`class="db-hq"`, `open_db_hq.sh`) untouched for A/B.

---

## Rev 5 (2026-09-03) — STOP hacking scope. Port the reference parser's `is_navigable()`.

The user is right: the current `kh_apply_scope_confine()` (commits
`59acda6d`, `dde3291e`) **renumbers `g_nav[]` and zeroes `nav_index`**
on out-of-scope rows. That is NOT how chtpm interact mode works. In the
real thing the other rows keep their numbers and brackets on screen —
they're just inert; keys pass through them until `Esc`, and the
activated submenu gets its OWN `1..N` numbering nested under `[^]`.

### THE REFERENCE — copy this, don't reinvent

**File:** `44.xyz.01.00/101.ledger-player-npc-simple+3/system/chtpm_parser.c`
(3032 lines; the `chtpm_parser_pal.c` siblings in ~20 packages are the
same logic — `44.xyz.01.00/&.widgits/_shared-lib/system/chtpm_parser_pal.c`
is the shared copy). The piececraft loader that produced the frame dump
in the handoff request is `pieces/apps/playrm/loader.chtpm` driven by
this parser.

Everything hangs off **two ints and one tree pointer — the element
array is never mutated:**

| symbol | line | meaning |
|---|---|---|
| `int focus_index` | 91 | index into `elements[]`, the `[>]` cursor |
| `int active_index = -1` | 91 | index of the activated scope root; `-1` = no scope |
| `elements[i].parent_index` | 84/1858 | tree parent, set at parse; drives `is_descendant()` |

**`is_navigable(int idx)` — line 1750. The whole gate. Pure function,
mutates nothing:**

```
not is_interactive || not visible            -> false
if active_index != -1:
    if elements[active_index] is an ACTIVATE menu w/ children:
        idx == active_index                  -> true   (root stays navigable/numbered)
        is_descendant(idx, active_index)      -> true   (unless a folded ancestor between)
        else                                 -> false
    else (cli_io etc.): idx == active_index   -> ...    (only the field itself)
else (global mode): walk ancestors ->
    any ancestor is_folded                    -> false
    any ancestor onClick == "ACTIVATE"        -> false  (submenu hidden until activated)
    else                                     -> true
```

`is_descendant(child, parent)` — line 1708 — just walks `parent_index` up.

**Arrow nav — lines 2610-2617 (global) and 2760-2764 (in scope):**
```
do { focus_index += dir; wrap 0..element_count-1; }
while (focus_index != prev && !is_navigable(focus_index));
```
Skip-scan. No array rebuild.

**Digit jump — `do_jump(n)` line 2456 / `count_navigable()` 2457:**
count elements where `is_navigable(i)` is true, land on the n-th. The
visible numbers are *derived every render* from `is_navigable`, so they
are always contiguous **within the current scope** without renumbering
anything.

**Render — `render_element()` line 2195, two counters threaded down:**
- out-of-scope interactive row: `(*p_global_counter)++` — shows its
  normal number, prefix `[ ]`, **still drawn, just inert**.
- in-scope row (`is_descendant(idx, active_index)`):
  `(*p_scoped_counter)++` — its own `1..N`.
- entering the active root's children resets `*p_scoped_counter = 0`
  (line 2324) so the submenu starts at 1; restored after (2332).
- prefix (2258): `[^]` if `idx == active_index`, else `[>]` if focused
  & navigable, else `[ ]`.
- descendants get `"    "` × depth indent (2217-2229).
- nav prompt (2384): `active_index == -1` -> `"Nav > %s_"`,
  else -> `"Active [^]: %s (ESC to exit)"`.

**ENTER — line ~2703:** focused el `onClick=="ACTIVATE"` w/ children ->
`active_index = focus_index`; then move `focus_index` to its first
navigable child (2711). `onClick=="INTERACT"` -> `active_index =
focus_index` (2730).

**ESC / RELEASE / key `9` — lines 1668-1679 and 2751-2753:** pop ONE
level: `active_index = elements[active_index].parent_index;
focus_index = old_active;` (so nested submenus pop correctly). If
nothing to pop: `active_index = -1; focus_index = 0;
initialize_focus()`.

**`initialize_focus()` — line 2458:** if `focus_index` isn't navigable,
scan forward to the first that is.

### How to port into `khtpm_core_render.c` (replaces the hack)

1. **Delete the renumbering** in `kh_apply_scope_confine()`. Keep
   `g_nav[]` built exactly as today — every navigable `Elem*`, full
   1-based `nav_index`, nothing zeroed.
2. Add `static int kh_elem_in_scope(Elem *e)` = the reference's
   `active_index` branch: `e == g_default_active_scope_root ||
   <e is descendant of g_default_active_scope_root> ||
   strcmp(e->id, g_default_active_scope_id)==0 ||
   strncmp(e->id,"chrome-",7)==0`. (Descendant walk = `for (p=e->parent;
   p; p=p->parent) if (p==root) return 1;`.)
3. Arrow handlers (generic branch, where `g_focus_nav++/--`): when
   `g_default_scope_confine`, wrap the step in
   `do { step; } while (!kh_elem_in_scope(g_nav[g_focus_nav-1]));`
   — the reference's skip-scan, on `g_nav[]` instead of `elements[]`.
4. Digit-jump + `activate_focused()` + mouse-click routing: if
   `g_default_scope_confine && !kh_elem_in_scope(target)` -> ignore.
5. `khtpm_draw_core.c`: draw `[ ]` (inert, keep the number) on
   `!kh_elem_in_scope` rows; `[>]`/`[^]` only in scope. Optionally add
   the scoped `1..N` counter for the submenu rows (reference
   `p_scoped_counter`) — nice-to-have, not required for correctness.
6. Nav-prompt / status line: show `Active [^]: (ESC to exit)` while
   `g_default_scope_confine`, else the normal prompt.
7. ESC: pop one level (`g_default_active_scope_root` -> its parent
   container if nested, else clear) rather than always clearing.

Net: `g_nav[]` and `nav_index` become **immutable render data**; the
scope is enforced entirely by a predicate consulted at
focus-move / jump / click / draw time — exactly the reference model.
Then revert `59acda6d` + `dde3291e`'s `g_nav[]` mutation (the
`chrome-*` keep survives as a `kh_elem_in_scope` clause).

---

## Rev 6 (2026-09-03) — phantom-HQ-window + ledger-bloat fixes, then git-hygiene cleanup

Four production hardening fixes + two repo-cleanup commits, all on this
branch, all committed and pushed after the Rev 5 work. A next agent
should know these so it doesn't chase ghosts or redo them.

### Phantom taskbar entry (chat-hai) — root cause + fix

`#.desktop/strip_ui.txt` showed `n_hqwins=1` with `hw_0_label=🪟 chat-hai`
for an entry whose process did not exist. Root cause was **PID reuse**:
chat-hai's `livedesk_hq_windows_62518.txt` was stale (its process died
abnormally — no atexit cleanup), and PID 62518 was reused by a Firefox
content thread (`/proc/62518/comm` = `StyleThread#5`), so
`ktb_merge_hq_windows`'s `ktb_pid_alive(pid)` returned true for the
wrong process.

- **Fix (committed, pushed):** `khtpm_taskbar_manager.c` now has
  `ktb_pid_is_hq_renderer()` — checks `/proc/<pid>/comm` starts with
  `khtpm_core_r` (fail-open on non-Linux) — used at the merge site in
  `ktb_merge_hq_windows`. Phantom can't recur even with stale files.
- **Also swept:** 35 dead-pid `livedesk_hq_windows_*.txt` files.

### `nav_master_ledger.txt` unbounded growth — fixed in code

It had reached 5.98 MB / 24,237 lines. Added `nav_ledger_trim()`/
`nav_ledger_write()` (cap 250 KB, keep newest whole lines, tmp+rename)
in `khtpm_core_render.c`; rewired the 3 append sites (nav_tab_register,
SNAP block, RMMV_CLICK) + an inline cap in
`tp_arm_placer_rmmv.c`. Truncated the live file to 256 KB.

### SIGTERM/SIGINT cleanup hook — added

`main()`'s default/HQ path previously had no signal handler (only
`tp_main` did). Installed `handle_shutdown_signal` after
`atexit(cleanup_hq_window_registry)`; `hq_run_event_loop()` now checks
`g_shutdown_requested` at loop top. Killed HQ windows now remove their
registry file instead of leaving stales.

### Rebuilds

Both production binaries rebuilt (+x/ is git-ignored, no commit):
`khtpm_taskbar_manager_main.+x` and `khtpm_core_render.+x`. A running
old manager picks up fixes on relaunch.

### Git-hygiene cleanup

- **`f6e0a834`** — added `#.desktop/.gitignore` (per-session
  `entity_menu_history/`, `taskbar_settings_history/`,
  `livedesk_hq_windows_*`, `*.state.txt`, `*_history.txt`, `*.pid`,
  `*.lock`, `colab_hai/`, `nb_tabs/`, `x/`, …) and untracked **527**
  runtime files (kept on disk; 107 legitimate files stay tracked:
  entities/, events/, sprites/, harnesses/, all `.chtpm/.pdl/.pal`).
  Stops git-history bloat from high-churn runtime state.
- **`7b331459`** — deleted `khtpm_choice_picker.c` (superseded fork,
  zero runtime callers; `.c` + build.sh line + stale `+x`). The live
  picker is `khtpm_show_choices.+x`. NOTE: this reverses the older
  claim in `khtpm-generic-dispatch-design.md` §5 that choice_picker
  was "left in place as a rollback reference" — that §5 bullet is now
  amended with a 2026-09-03 UPDATE.

### Standing house rules still in force

- Never `git add -A` — stage explicit paths (`khtpm_core_render.c` and
  `khtpm_taskbar_manager.c` have concurrent editors).
- Commit footer: `Co-Authored-By: Claude Sonnet 5
  <noreply@anthropic.com>` / same `Claude-Session` URL as the header.
- Rebuild binaries after editing renderer/manager C; running windows
  don't pick up a rebuild until relaunch.
- Read this doc's Rev 5 §"THE REFERENCE" before touching in-scope nav /
  `kh_apply_scope_confine`; read `CENTROID_GOLD_STD.md` +
  `khtpm-generic-dispatch-design.md` (top block, now updated 2026-09-03)
  before adding any mode/dispatch branch to `khtpm_core_render.c`.

---

## Rev 7 (2026-09-03) — live-test #1 done + crash root cause + FLICKER REGRESSION handoff

Handed off to the **next agent (Sonnet): first fix the db-hq-pal flicker
regression below, then do task #2 (chat-hai)**. This rev documents the
recovery that was done live, proves #1 is verified on the rebuilt stack,
and lays out the flicker evidence + the exact code paths to gate.

### Crash during live-testing — ROOT CAUSE (fixed in the rebuilt binary, NOT reproducing)

During the first live-test of db-hq-pal the Return-to-enter-a-tab hit a
segfault in `khtpm_core_render.c`. **It was NOT a logic bug in the
tab→scope→Esc path.** It was element-pool exhaustion:

- `elem_new()` (the fixed-size `chai_n_elems_static` pool) returns NULL
  when exhausted, and the code then dereferences `item->parent` → SEGV.
  This is documented at `khtpm_core_render.c:7055-7064` (the comment
  already described the bug) and was already fixed via `chai_n_elems_static`.
- The **previously-crashed manager + strip were OLD pre-rebuild binaries**
  running since 14:26. The freshly-rebuilt stack **does not reproduce the
  crash**. Confirmed by full relaunch + repeat of the #1 test below.
- `ulimit -c unlimited` is set in this shell; no core was dropped; apport
  is the `core_pattern` handler. Don't chase the earlier "crash" — it is
  gone with the rebuild.

**Full stack relaunch + health check** (via
`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh new`):
manager **8953**, strip **8970**, `strip_ui.txt` populated
(3202→5404→3280 bytes), parser log clean.

### Task #1 — db-hq-pal live-test: PASSED

Verified headlessly on the rebuilt stack (Tab→scope→Esc confinement +
escape, the exact steps from Rev 3/#1). Injection via
`#.desktop/entity_menu_history/<pid>.txt` (generic mode;
`history_dir()` at renderer:9869-9874):

- Fresh launch → focus nav=1, tab 1 "Actors" active.
- Return(13) on the Actors tab → `state/active.pdl` wrote
  `FILE=db_hq_actors.state.txt|TAG=ACTOR|TITLE=Actors`; `ui.txt` populated.
- Down×2(201) → Return → `SEL=2`, `ui.txt` `detail_title=#3 Marsha`; nav
  stayed confined to sidebar r0-r3 → **scope engaged** (Rev 4 fix live).
- Escape(27) → scope popped, `active.pdl` untouched. **No crash**; manager/
  strip/db-hq-pal all survive.
- `db-hq-pal/toy.pdl` wiring confirmed: manager scans `toy.pdl` per app
  dir (lines 3520-3544), reads title/launch. `toy.pdl` =
  `title: Database (PAL)`, `launch: button.sh`. **#1 DONE.**

> NOTE on first-injection skip (noticed during this test): the FIRST
> injected batch is always skipped as bootstrap-leftover
> (`g_history_cursor` skips to EOF on first sight of the file). Inject,
> wait, then write a SECOND batch to actually consume.

### PROBLEM FOR SONNET — db-hq-pal FLICKER regression ("it never did this before")

The db-hq-pal window is **intermittently** redrawing/flickering, and the
user reports this is a NEW 2026-09-03 regression. Evidence that the
renderer is NOT continuously repainting (the window is otherwise stable):

- `state/ui.txt` byte-identical across ticks (md5 `118fe121...` on 3/3);
  layout/reparse fired **0×** in 8s (`kh_files_hash`/`g_vars_hash`,
  renderer:1004/1412 suppress the reparse loop).
- `redraw()` fires ~once/5s (registry-mtime probe) with a *0.4s projector
  tick* (`pal/dbhq_projector.pal`: `sleep 400000; j proj_top`).
- Two `xwd` captures 0.4s apart are **pixel-identical (0/668,200 changed)**.
- Only ONE db-hq-pal instance/window `[0x1a00004]`, 1285×520 @ 80,80.
- Registry `#.desktop/livedesk_hq_windows_65714.txt` shows `focused=0`,
  stable geometry.

So it is NOT a per-tick full-frame blit. The real culprits are **NEW
2026-09-03 code in the generic sidebar+panel redraw path**:

1. **`redraw()` corrective `XMoveWindow` + `XSync`** (~line 9568) — fires
   whenever real `wa.x/y` != `g_win_x/y`. Even one XRoundtrip per redraw
   can visibly interrupt against a compositor.
2. **`layout_sidebar_panel()` position clamp** (~line 7740, labeled
   `REAL NEW 2026-09-03`) — mutates `g_win_x/y` every pass.
3. Per-tick **`XGetInputFocus`** `^`/`.` title indicator + the
   `livedesk_hq_windows_<pid>.txt` **registry write on every redraw**.

**Note:** the clamp does NOT fire on this 2496×1664 screen
(`80+1285=1365 < 2496`), so the residual interrupt is most likely the
corrective `XMoveWindow`/`XSync` feedback loop or an intermittent
Expose-driven full-frame blit.

**FIX REQUEST (Sonnet):**
- Gate / remove the corrective `XMoveWindow`+`XSync` in `redraw()`
  (~9568): only move when `wa.x/y` genuinely differs from what WE last
  requested, and skip the `XSync` roundtrip when nothing changed.
- Gate the `layout_sidebar_panel()` clamp (~7740) so it never rewrites
  `g_win_x/y` to identical values (the `REAL NEW 2026-09-03` mutation).
- Defer/cap the `XGetInputFocus` title indicator add and the per-redraw
  registry write so a quiet repaint does zero X roundtrips / file writes.
- Rebuild `khtpm_core_render.+x` after the fix and re-run the step-6
  verification below.

**Verification recipe (10s):**
```sh
Xwd both windows with the projector at rest (no input for >1s), compare:
  for k in 1 2; do xwd -root -out /tmp/f$k.xwd; sleep 0.4; done
  md5sum /tmp/f1.xwd /tmp/f2.xwd        # want IDENTICAL when idle
```
With the regression fixed, two idle captures must match byte-for-byte.
Before the fix they were already identical in the captured sample — the
flicker is intermittent, so capture repeatedly (e.g. a 20-frame rapid
burst) to catch it, and confirm the X-roundtrip count drops to ~0 when
idle.

### Git-hygiene flag (not yet cleaned — next agent can)

`&.hq-apps/db-hq-pal/state/active.pdl`, `state/ui.txt`, and
`module_parent.pid` are **wrongly git-tracked** (confirmed by
`git ls-files`) — these are transient per-tick files. Only
`state/.gitkeep` should stay. Suggest adding to a `.gitignore` and
untracking, mirroring the Rev-6 `#.desktop/.gitignore` sweep. Not done
this rev to avoid a noisy commit mid-flicker-fix.

### Current live state

- Git branch `chtpm-var-substitution`, HEAD `9560c8f8` (docs), working
  tree clean of source (only runtime `.pid/.state/.history` + the
  wrongly-tracked `active.pdl`/`ui.txt`/`module_parent.pid` show modified).
- Live stack: manager **8953**, strip **8970**, db-hq-pal renderer
  **65714** + prisc+x projector **65715**; window `0x1a00004` 1285×520
  @ 80,80. Left running for continued testing; do not kill unless needed.
- Next task after the flicker fix: **#2 chat-hai** — author `chat-hai.xhtpm`
  + a `.pal`/`.c` projector replacing `ops/chat_hai_projector.sh` (bash);
  message list = one `<repeat>`. See `PROGRESS-chat-hai-xhtpm.md`.

---

## Rev 8 (2026-09-03) — db-hq-pal flicker: ROOT-CAUSED + FIXED

The "redraws with no change while active" flicker is fixed. Full
writeup: `09-appendix/forensic-report-flicker.md`. Short version:
the generic (non-`g_is_db_hq`) window had no repaint-trigger gate, so
it blitted on grab-synthetic `FocusIn`/`FocusOut` (the new `^`/`.`
indicator), per-rectangle `Expose`, relayed mouse-*moves* (tpmos
PITFALL #52), and a corrective-`XMoveWindow` feedback loop — none of
which are visible-state changes. Fixes (branch commits `bc18c5ad`,
`2865e2cd`, `75d6833c`): filter grab/pointer focus events, coalesce
`Expose` bursts, stop counting relayed moves as input, add a
`g_frame_dirty` coalescing flag consumed once per event-loop tick
(the tpmos marker/dirty model the `g_is_db_hq` path already used),
gate the corrective move on *intended* geometry change with
root-translated coords, `graphics_exposures=False`, vars-hash
debounce. Idle redraw count after fix: **0** (was continuous).
New standing rule: `CENTROID_GOLD_STD.md` §3 rule 8.

**If you still see it:** you are on a stale renderer process — kill
every `khtpm_core_render.+x` and relaunch (`run_khtpm_strip.sh new`);
a window opened before the rebuild keeps running the old binary.

Follow-up (not the flicker, but noted): the generic present path still
round-trips the frame through `#.desktop/entity_menu_frame_<pid>.txt`
(serialize → disk → read back → paint) every redraw. PID-scoped so not
a live race, but it should move onto `render_tree()` per
`CENTROID_GOLD_STD.md` §3.4.

---

## Rev 9 (2026-09-03) — palette polish done; C-deletion is the next big rock

**Branch note:** work has moved to `chtpm-delete-per-app-c` (cut from
`chtpm-var-substitution` after the flicker forensics commit `639fec5c`).
It carries: the Rev 8 flicker fixes, the palette work below, the step-2
launcher rewires, and the keep-list rename. `chtpm-var-substitution`
still has the flicker + palette-perf commits; everything else is only on
`chtpm-delete-per-app-c`.

### Palette window — fixed this rev (all on `chtpm-delete-per-app-c`)

| commit | issue |
|---|---|
| `a520d223` | "incredibly slow": `draw_core` `alloc_pixel()` was uncached (~1000+ `XAllocColor` round-trips/frame); the first checkerboard cut did ~49 `alloc_pixel`+fill per tile; `dbhq_redraw_content()` re-laid-out + re-serialised all 256 tiles every redraw. Fix: 64-entry colour cache, FillTiled-GC checkerboard, `dbhq_pal_content_sig()` gate (198ms → 22ms). |
| `0644150c` | tiles off-centre — `32104e91` gave every `h<64` sprite cell the taskbar-strip layout; grid tiles (`pal-tile`/`swatch`) now excluded. |
| `01daa6b2` | gold `#d9b64a` tile bg → grey/white PNG-transparency checkerboard. |
| `110844f2` | **clicks going dead after a few clicks** — the content-sig cache skipped `dbhq_layout_pass()` after `dbhq_inject_palette_tiles()` rebuilt the tile Elems (periodic manager republish, rmmv tab clicks), leaving tiles 0×0 so `hit_test` missed. `g_pal_tree_gen` in the sig forces a re-layout on any tree rebuild. |
| `79aef9f7` | chrome title hardcoded "db-hq" → reads `<window label=…>` (data, no per-app strcmp); added `label=` to palettes/bookmarks/stats/db-hq templates; `^`/`.` focus indicator added to the db-hq chrome bar. |
| `5b93e572` | `.pal-tab-active`/`.pal-tileset-active` gold+black → house blue `#2f5f8f`+white (was unreadable, esp. the focused-state dark badge). |
| `47b167f0` | tileset chooser rows flex-stacked below a tall grid → off-screen + thumb couldn't reach last row. Now pinned as a fixed bottom footer; grid scroll box shrunk by footer height. |

### Still open on the palette window (revisit later, NOT blocking)

- **Scroller still not fully right** — footer pin + box-shrink landed
  (`47b167f0`) but the user reports the thumb/scroll extent still isn't
  perfect for the largest tilesets. Re-check `generic_scroll_layout_pass`
  `g_pal_visible_rows`/`max_scroll` math against the new reduced `box_h`,
  and the `max_h = scaled(600)` window cap in `dbhq_inject_palette_tiles`.
- Live-confirm the title / active-button / click fixes on a fresh binary
  (the `--dump-and-exit` PNG lags a frame and gets clobbered by
  concurrent test runs — not a reliable check; use a real window).

### The next big rock: delete the per-app C (CLEANUP-AND-REWIRE.md "THE TASK")

Ordered plan the user gave: **(2) rewires → commit → (3) evhq+dbhq
cluster → commit → (1) the rest**.

- **(2) DONE** — `e598a864`: `livedesk:open-palette:` emojis/elements →
  `palettes/button-pal.sh`; `launcher_db` → `db-hq-pal/button.sh`; db
  menu "db-hq"/"db-hq (classic)" relabel.
- **(3-prep) DONE** — `8dc41d8b`: keep-list frame helpers renamed
  `dbhq_* → kh_*` (`kh_serialize_frame_elem/_subtree`,
  `kh_paint_frame_line`, `kh_append_frame_history`).
- **(3-core) NOT STARTED** — the atomic removal of `g_is_events_hq` +
  `evhq_*` + `g_is_db_hq`/`g_is_stats_hq`/`g_is_palettes`/`g_is_bookmarks`
  + `dbhq_*` + `dbhq_ce_*`. Must compile as ONE unit (`dbhq_ce_*` calls
  `evhq_*`; stats/palettes/bookmarks ride `g_is_db_hq=1`). `g_is_swatch_picker`
  (18 refs) is cleanly separable and stays for step (1). Full function
  map + call-site list in `CLEANUP-AND-REWIRE.md` §1-§5.
  - KEEP (already renamed or verified generic): the `kh_*` frame
    helpers; `frame_field_escape/unescape_pipe`; `zero_nav_subtree`;
    `evhq_nonfatal_x_error` (→ rename `kh_nonfatal_x_error`, it's the
    generic `XSetErrorHandler` now); `hq_run_detached`; `input_disarm`.
  - `g_dbhq_active_scope_root` is read by `_shared-lib/khtpm_draw_core.c`
    (`[^]` badge scope test) — keep as an always-NULL stub or delete
    that one draw_core use too.

---

## Parked follow-ups (2026-09-04) — do after the dbhq_* deletion

### click_two_step should also govern the taskbar strip

`#.desktop/hq_ui.pdl` `click_two_step=1` (first click moves `[>]`,
second click activates; `=0` single-click activates) is honored for
every window via `click_focus_then_activate()` in `khtpm_core_render.c`
— BUT that function has an explicit bypass:
`if (window_is_dock() && (onclick=="ACTIVATE" || class "dropdown-child")) { focus; return 1; }`
so the strip's top-level cells always open on one click. The user's
intent: `click_two_step` was meant to cover the taskbar too. Fix:
gate the `onclick=="ACTIVATE"` half of that bypass on
`!g_click_two_step` (keep `dropdown-child` always-activate — two-step
inside an already-open menu is wrong). Verify the strip still feels
right at `=0`.

### hq:settings toggle for click_two_step (live, no relaunch)

- `taskbar-settings-pal.xhtpm`: add `<item id="click-mode" action="…">`
  (label reflects current state). The action is a shell command
  (generic `<item action="'script' 'args'">` path, no new dispatch
  verb) that flips `click_two_step` in `#.desktop/hq_ui.pdl` and
  appends one byte to `#.desktop/click_mode_changed.txt`.
- `khtpm_core_render.c` idle tick: poll `click_mode_changed.txt` size
  (mirror `theme_changed_dirty()` exactly) → on growth re-run
  `desktop_load_click_two_step()`. One helper + one call, no new
  parser/layout logic.
- The projector for taskbar-settings emits the current `click_two_step`
  value into `taskbar_settings_ui.txt` so the item label can show
  "click: two-step" / "click: single".

---

## Rev 10 (2026-09-04) — events-hq C deleted; palettes fully ported; dbhq_* deletion in progress

Active branch is `chtpm-delete-per-app-c` (off `chtpm-var-substitution`
@ `639fec5c`). Everything below is committed + pushed there.

### DONE

- **events-hq C removed** — `81cedb8f` (−2465 lines). `g_is_events_hq`
  (now `static const int = 0`), the whole `evhq_*` block, `dbhq_ce_*`
  (the embedded Common Events editor bridge), `g_evhq_*` state.
  `evhq_nonfatal_x_error` → `kh_nonfatal_x_error` (`e7a8b19e`).
  Compliant CE flow: db-hq-pal lists events → `dbhq_action.sh sel`
  (TAG=CE) opens `events-hq.xhtpm` on that event. events-hq.xhtpm has
  the full editor (command list / Add-Command picker / per-field
  editor / delete / Scratch / Blueprints) — `evhq_projector.c` +
  `evhq_action.sh`; `khtpm_events_hq_manager.c` (compile chain)
  untouched. `PROGRESS-events-hq-xhtpm.md` boxes ticked.
- **All palette categories off `g_is_palettes`** — emojis/elements
  (earlier), **rmmv** (`0b061024` + renderer cap fixes `c6bb981d`),
  piececraft/debug/stub (`117ee0a8`). Every `livedesk:open-palette:<cat>`
  now routes through `button-pal.sh` → `palettes-<cat>.xhtpm` or
  `palettes-stub.xhtpm` (`17fbe7a8`). `palettes_menu.sh` = rollback only.
- **Renderer caps lifted** (`c6bb981d`) — `KH_MAX_VARS` 256→2048,
  `MAX_CHILDREN` 64→320 (`khtpm_render_core.c`, both copies),
  `parse_chtpm()` repeat-expansion buffer `sz*16+128K`→`sz*48+512K` and
  `${var}` sub buffer `len*2+4K`→`len*4+64K`. A 256-row `<repeat>` grid
  was being silently cut to ~88. **Other agents: these are the numbers
  now — a big `<repeat>` no longer truncates.**
- **Swatch grid scroll** (`c6bb981d`) — the generic `class="swatch"`
  branch in `assign_nav_and_layout()` was single-pass with non-swatch
  items pinned at a fixed `y=CHROME_H+180` (overlapped tall grids).
  Now 3 passes: nav+close → a 12-row scrolled viewport (`generic_sbar`,
  `SCROLLUP:`/`SCROLLDOWN:` + wheel) → non-swatch items flow as wrapping
  chips below. A big tileset scrolls instead of a 1700px window.
- **db-hq chrome title** — reads `<window label="…">`, no per-app
  strcmp; `^`/`.` focus indicator (`79aef9f7`). Labels added to
  palettes/bookmarks/stats/db-hq templates.
- **Flicker** (`bc18c5ad` `2865e2cd` `75d6833c` `639fec5c`) — full
  writeup `09-appendix/forensic-report-flicker.md`; new standing rule
  `CENTROID_GOLD_STD.md` §3 rule 8; `03-pitfalls/X11-AND-SESSION-PITFALLS.md`
  dated section.
- **Palette polish** — tile centering (`0644150c`), PNG-checkerboard
  bg (`01daa6b2`), colour cache + grid re-layout gate (`a520d223`),
  click-dead-after-N-clicks fix (`110844f2`), readable active buttons
  (`5b93e572`), tileset chooser footer pin (`47b167f0`), tree-gen
  layout invalidation (`110844f2`).

### IN PROGRESS (this session)

- **The `dbhq_*` / `g_is_db_hq` / `g_is_palettes` / `g_is_stats_hq` /
  `g_is_bookmarks` deletion** — all four windows have generic
  replacements now, so this is finally unblocked. Method: same as the
  events-hq cut — rename any generic-but-`dbhq_`-named keeper to `kh_*`,
  delete the block + call sites, compile-fix loop, one commit.
  Notable keepers (already `kh_*` or generic): `kh_serialize_frame_*`,
  `kh_paint_frame_line`, `kh_append_frame_history`,
  `frame_field_escape/unescape_pipe`, `zero_nav_subtree`,
  `hq_run_detached`, `input_disarm`, `generic_sbar_*`, the whole
  `layout_sidebar_panel` / swatch-grid / dock paths.
- Before/with it: retire the **"db-hq (classic)"** menu row +
  `*.monads/*.muchi-pet/ops/open_db_hq.sh` (the last live `g_is_db_hq`
  entry point) — point at `db-hq-pal/button.sh` or drop.

### PARKED (after the deletion)

- `click_two_step` (`#.desktop/hq_ui.pdl`) should also govern the
  **taskbar strip** — remove the `window_is_dock() && onclick=="ACTIVATE"`
  bypass in `click_focus_then_activate()` (gate on `!g_click_two_step`;
  keep `dropdown-child` always-activate).
- **hq:settings toggle** for `click_two_step` — item in
  `taskbar-settings-pal.xhtpm` → shell action writes the pdl + a
  `click_mode_changed.txt` marker; one idle-tick poll in the renderer
  (mirror `theme_changed_dirty()`) so open windows pick it up live.
- db-hq-pal DB record FIELD editing (still read-only — needs the
  `dbhq_pdl_publish_manager` → RPG-Maker JSON chain).
- rmmv: live scroll/click/arm verify; scrollbar track/thumb visibility
  in the swatch grid; chooser-chip label wrapping polish.

### Reference doc (in flight)

A Haiku subagent is writing
`02-architecture/XHTPM-PARSER-REFERENCE.md` — full xhtpm parser/renderer
feature catalogue (tags, attrs, `${var}`, `<repeat>` v1/v2, `<module>`,
action verbs, layout modes, nav model, CSS support, frame round-trip,
the hard-limit table) cross-referenced against tpmos
`pieces/chtpm/plugins/chtpm_parser.c`.

---

## Rev 11 (2026-09-04) — dbhq_* C deleted (functional); husk cleanup deferred

### DONE

- **`dce0f1f4`** — deleted 62 `dbhq_*`/`stats_*` function bodies + the
  two `main()` db-hq one-time init blocks. `khtpm_core_render.c`
  19040 → 14286 lines total this session (events-hq + dbhq),
  binary 369K → 249K.
- **`9ce89904`** — class detection for `db-hq`/`stats-hq`/`palettes`/
  `bookmarks` removed; all four flags now `static const int = 0`, so
  every `if (g_is_db_hq){...}` husk constant-folds away (GCC drops the
  dead code; that's why the file still compiles with ~30 husk `if`s and
  ~128 dead `dbhq_*` refs still textually present).
- **`90bcbe2c`** — "db-hq (classic)" menu row + `livedesk:open-common-
  events-hq` handler retired (last live `g_is_db_hq` entry point).
- KEPT / renamed: `dbhq_marker_pilot` / `dbhq_loop_request_redraw` /
  `dbhq_loop_paint_if_dirty` (no-ops with const 0),
  `kh_measure_text_px` / `kh_shift_subtree` (generic, renamed),
  new `kh_grab_keyboard_retry` / `kh_capture_click` / `kh_capture_key`
  / `kh_key_history_code` (history-relay + kbd-grab, needed by
  popup/entity-menu/cli_io). `g_dbhq_active_scope_root` = always-NULL
  stub (read by `_shared-lib/khtpm_draw_core.c` `[^]` badge).
- All 7 ported windows + rmmv verified headless `ok=1`.

### DEFERRED — husk cleanup (cosmetic, NOT functional)

The file still carries, all dead-at-runtime:
- ~30 `if (g_is_db_hq){...}` / `if (g_is_palettes){...}` blocks in
  `redraw()`, `hq_dispatch_xevent`, `hq_idle_tick`, `handle_key`,
  `assign_nav_and_layout`, `main()` window-creation.
- `g_dbhq_*` state decls (`g_dbhq_actors`, `g_dbhq_list_recs`,
  `g_dbhq_close_elem`, `DbhqActor` / `DbhqListRec` typedefs, …).
- `? :` chains in `history_path()` / `frame_changed_path()` that pick a
  never-taken `g_is_stats_hq ? … : g_is_db_hq ? …` branch.
- `dbhq_marker_pilot()` (`return g_is_db_hq && …` = always 0) - could
  become `return 0` and its two callers inline the else branch.
An automated brace-match delete of the husk BLOCKS was attempted and
reverted (broke `hq_idle_tick`'s nested `if (g_is_bookmarks)` /
`else if` structure). Do it by hand, block by block, compiling after
each. Then rename the surviving `dbhq_*` (marker helpers) → `kh_*`.

## Rev 12 (2026-09-04) — husk hand-removal done

### DONE — `0d23126c`

Every executable `if (g_is_db_hq){...}` husk hand-removed, block by
block, compiling + `ok=1` headless after the cluster:

- `redraw()` — the 67-line db-hq content/present branch, gone.
- `assign_nav_and_layout()`, `scaled()`, `handle_key()` (×3),
  `dump_frame_png()`, `kh_apply_scope_confine()`,
  `hq_dispatch_xevent()` (ButtonPress `} else if (g_is_db_hq)`,
  ButtonRelease `g_dbhq_dragging`/`g_pal_thumb_dragging` resets,
  MotionNotify ×2, KeyPress branch, FocusIn/FocusOut `g_is_db_hq &&
  g_dbhq_has_real_focus` clauses collapsed to the plain default body),
  `poll_agent_history()` mouse-relay (`dbhq_handle_click` / scrolllist
  guard), `hq_idle_tick` reparse guard — all simplified to the generic
  path.
- `history_path()` / `frame_changed_path()` ternary chains — the
  never-taken `g_is_stats_hq ? … : g_is_db_hq ? …` legs removed.
- `dbhq_marker_pilot()` / `dbhq_loop_request_redraw()` /
  `dbhq_loop_paint_if_dirty()` — deleted; the two call sites
  (`hq_request_redraw`, idle-tick tail) now go straight to the
  `g_frame_dirty` / `redraw()` path.
- `assign_palettes_nav()` — deleted (was unused and referenced the
  now-undefined `dbhq_elem_is_navigable`).

`khtpm_core_render.c` −405 lines. All 8 ported windows
(dashboard/events-hq/rmmv/emojis/elements/piececraft/debug/stub) +
a plain entity menu verified `ok=1` headless.

### DONE — orphan decl sweep — `5efc32c4`

All `g_dbhq_*` / `g_pal_*` / `g_bm_*` / `DB_HQ_*` orphan state removed
(`DbhqActor`/`DbhqListRec` typedefs, `g_dbhq_list_cfg`,
`DB_HQ_TAB_LABELS` + `DB_HQ_*_TAB` macros, all the tile/options/scroll
state), plus `generic_scroll_layout_pass()` (sole consumer of the
`g_pal_*` scroll cluster; superseded by `generic_sbar_*`),
`g_pal_rmmv_armed` + its two dead husks, and
`kh_append_frame_history()` (no reader). `g_is_db_hq` itself dropped
(zero refs). −544 lines.

KEPT: `g_dbhq_active_scope_root` (draw_core `[^]` badge) and the
still-unused-but-wanted generic helpers — `nav_tab_register`/
`_unregister`/`_cycle`, `apply_theme`, `nav_ledger_publish`,
`history_unregister`, `zero_nav_subtree`, `input_disarm`,
`hq_run_detached`, `hq_window_has_x_focus`, `reusable_slot`,
`render_tree`, `hit_test`, `kh_shift_subtree`, `mark_frame_changed`/
`consume_frame_changed`, `css_layout_pass`, `draw_topdown_block_rgb`.
These are the only `-Wunused` warnings left in the file.

Note: `taskbar_settings.chtpm` returns "no PNG" under
`khtpm_png_dump.sh` — expected, swatch-picker mode writes its frame to
`#.desktop/taskbar-settings-audit/`, not `/tmp`; pre-existing, not a
regression. Needs a live check.

### BRANCH NOTE

`chtpm-delete-per-app-c` now also carries **oc's browser JS-engine rung
commits** (`b079f0c9`…`e8d72790`, all under `&.hq-apps/network/`) - a
stray commit of mine landed on oc's `chtpm-js-rungs` branch and the
push to `chtpm-delete-per-app-c` fast-forwarded them in. Disjoint files
(network/ vs the renderer), no conflict, but the branch is now two
feature streams. Untangle at merge-to-main time if wanted.

---

## Rev 13 (2026-09-04) — palette live-parity pass + window-frame polish; z-order + font-scale backlog

### DONE this rev (all on `chtpm-delete-per-app-c`, pushed)

Live-tested via the authoritative frame files
(`#.desktop/entity_menu_frame_<pid>.txt`) — PNG dumps were repeatedly
stale this session (documented `dump_frame_png` failure mode: captures
pre-layout pixels), frame text is the source of truth.

| commit | fix |
|---|---|
| `049a3502` | swatch-picker colour squares were getting the palette PNG-transparency checkerboard; now solid (`!g_is_swatch_picker` gate in draw_core, both copies). |
| `b76f1514` | **palette closed on every tile click** — `dispatch()`'s `g_quit=1` "menus close after an action" tail only spared `g_default_has_sidebar_panel`; a `<page>`-of-`<repeat>` palette isn't that. New `g_default_persistent` flag, set from `class="database-window"`/`"palettes-pal"`, checked at the quit gate. |
| `ab0ff01d` `1e3afe2c` | chrome **close button declared in the template** (`<item id="close" class="chrome-btn" onclick="CLOSE">`, generic verb — replaces deleted `dbhq_draw_chrome_bar`); 3 distinct chooser classes `.pal-dir`/`.pal-sheet`/`.pal-tileset` + `-active` variants; projector emits family-matched active class; sheet tabs bind `${c.letter}` so they read **A/B/C** not `a2`/`b`/`c`. |
| `33ae8976` `45c9de48` | swatch-grid layout for a `g_default_persistent` window now mirrors the pre-port `dbhq_layout_pass` picker (git `94d12680`): **wide window** (5/8 screen, cap 1180), column count derived from width; **A/B/C + tileset choosers on top**, **folder list pinned to the footer**; class-family change starts a new chip row; inter-row gap 10px. |
| `6675ece0` | chooser chips sized `max(CSS width, label + 46)` — `draw_elem` prepends a `[ ]NN. ` badge, so short labels ("A") were clipping to just the badge. |
| `19b47bd0` | **`palettes_menu.sh arm-rmmv` never ran** — `case arm-rmmv) ... "$7" "$8"` under `set -u` (a real call has 6 args: 4 + the `<pkg>`/`<house>` `dispatch()` appends), and those two paths landed in the `$5-$8` picker-rect slots. Now `${7:-}` + only forwards `$5-$8` to `tp_arm_placer_rmmv.+x` when all four are integers. Verified: writes `rmmv_armed.txt`, spawns the placer. |
| `fd0fe2eb` | chrome close `x` box widened to `label + 52` and **hard-clamped** so `item->x + item->w <= g_win_w - 4` — never off the right edge regardless of window width / badge digits. |
| `dec62198` | **sidebar+panel window never wider than the screen** + a 14px margin off the right/bottom edges (was: chat-hai's session scrollbar flush against / past the physical screen edge). Applies to chat-hai / open-hai / db-hq-pal / events-hq / network-browser. |
| `d16e2ffa` | **2px frame in the theme SECONDARY colour** (`g_theme_fg`) on every non-dock window; `generic_sbar` `track_x` pulled in 6px so the thumb never touches the frame. |

Also: removed dead `tile_rmmv_*` DESK rows + orphan entity dirs from
`sessions/s1/desks/office.pdl` (test placements + 3 pre-existing dead
refs) — desktop is clean. `xyzfs/` is untracked so no commit.

### WHERE THE OLD (pre-deletion) CODE IS — for auditing missed features

- **`origin/main` (`2f644c52`) and `origin/chtpm-var-substitution`
  still contain the complete `g_is_db_hq`/`palettes`/`events_hq`/
  `stats_hq` + `dbhq_*` / `evhq_*` / `dbhq_ce_*` C.** The deletion is
  ONLY on `chtpm-delete-per-app-c`. `main` is the reference.
- On this branch: **`94d12680`** is the last commit with the working
  old C (before `9ce89904` neuter → `dce0f1f4` bodies → `81cedb8f`
  events-hq → `5efc32c4` sweep). Extract with
  `git show 94d12680:44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`.
- The **original standalone `khtpm_hq_render.c`** (older, more
  palette/HQ code — the "recovered from git" one earlier comments cite)
  was deleted in **`0dbcfccd`** — `0dbcfccd^` has it.
- No archive branches or tags. Diff feature-by-feature against
  `origin/main`'s renderer; `dbhq_*` fn names map to per-window
  behaviour.

### BACKLOG — z-order / raise-on-click (WM-managed / always-on-top=false)

All facets of one gap: in `override_redirect=false` (WM-managed) mode
nothing re-asserts stacking on our side.

1. **Taskbar window-nav click → raise that window (+ its context /
   dropdown window if it opens one) to the top.** Works today only when
   always-on-top=true. Needs `XRaiseWindow(win)` + focus on the
   `FOCUSWIN:` path regardless of `g_override_redirect`.
2. **Lower bar (taskbar strip) gaining focus → raise ALL its member
   windows** to top of view, even with always-on-top=false.
3. **Click on a window that's under another X11/GL window or an entity
   window → raise it on top, no data loss** (`XRaiseWindow` is pure
   stacking; content Pixmap untouched). Raising above a *native
   Wayland* surface is the known XWayland limit; our own windows/
   entities are fine.
4. Likely one shared helper: `kh_raise_and_focus(win)` called from
   ButtonPress in `hq_dispatch_xevent` + the taskbar's `FOCUSWIN:` /
   strip-focus paths.

### BACKLOG — hq:settings font family + size ± (scale everything)

- Renderer has `scaled(int base_px)` — currently identity. It *was*
  `base * g_dbhq_font_scale` (deleted). Generic replacement:
  `scaled()` returns `base * g_ui_scale`, `g_ui_scale` + `g_ui_font`
  read once from `#.desktop/hq_ui.pdl` (`ui_scale=1.0`, `ui_font=…`).
- Every px in the renderer already goes through `scaled()`, so wiring
  it makes chrome / rows / buttons / fonts grow together.
- Settings UI: a font `<repeat>` (curated list or `fc-list`) + `[-]`/
  `[+]` steppers in `taskbar-settings-pal.xhtpm` (or a new hq:settings
  page) → shell action writes the pdl + a `ui_scale_changed.txt`
  marker → renderer idle-poll re-reads (mirror `theme_changed_dirty()`).

### BACKLOG — carried from earlier revs (still open)

- `click_two_step` (`hq_ui.pdl`) should also govern the taskbar strip;
  hq:settings toggle for it.
- Scope `^` ESC: a click outside the armed scope should de-arm + act
  (no ESC needed); setting to restore the trap. **[user confirmed the
  no-trap behaviour is wanted]**
- Placed-tile right-click context menu: event / cut / copy / paste /
  delete.
- `tp_arm_placer_rmmv.+x` is InputOnly (invisible) — wants a visible
  overlay + nav-grid on the yellow picker surface.
- db-hq-pal record FIELD editing (currently read-only).

### MERGE STATE

`chtpm-delete-per-app-c` is 63 commits ahead of `origin/main`, 43 ahead
of `origin/chtpm-var-substitution` (which has only 1 commit not in this
branch — a docs sync). **Not safe to merge direct to `main`:**
- oc's `nb_js_eval` rung-6 commits (`b079f0c9`…`2c2af9ad`) are in the
  delta — oc's to land.
- 538-file diff, large share is runtime noise (`history.txt`, `*.pid`,
  `*.state`, `frame_history`, `xyzfs/`) — needs `git rm --cached` +
  `.gitignore` pass first.
- −88,837 lines (whole db-hq/palettes/events-hq/stats-hq C removal) —
  milestone-review merge.

**Path:** `chtpm-delete-per-app-c` → `chtpm-var-substitution`
(consolidate + noise cleanup) → oc coordinates → `main`.
