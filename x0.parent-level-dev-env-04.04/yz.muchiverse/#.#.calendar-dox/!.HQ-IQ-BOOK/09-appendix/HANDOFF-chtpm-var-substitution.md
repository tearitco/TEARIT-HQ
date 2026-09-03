# HANDOFF — branch `chtpm-var-substitution`

**Last updated:** 2026-09-03 (mid-session, rev 2)
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
