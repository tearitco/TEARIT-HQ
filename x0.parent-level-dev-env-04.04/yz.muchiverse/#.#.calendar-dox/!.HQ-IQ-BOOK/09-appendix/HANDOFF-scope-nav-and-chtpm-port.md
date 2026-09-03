# HANDOFF — chtpm projector port + interact-mode scoped nav

**Branch:** `chtpm-var-substitution` (NOT merged to `main`)
**Date:** 2026-09-03
**Read this whole doc before touching anything.** It is self-contained;
the older docs listed in §7 are background only.

---

## 0. TL;DR — what the next agent should do

1. **Fix scoped nav properly** (§3). The current implementation
   (`kh_apply_scope_confine()` in `khtpm_core_render.c`) is a hack — it
   *renumbers `g_nav[]` and zeroes `nav_index`* on out-of-scope rows.
   That is not how chtpm interact mode works. Port the real model from
   the reference parser (§2): keep `g_nav[]` immutable, gate everything
   through a predicate. Out-of-scope rows must stay on screen **with
   their numbers**, just inert, until `Esc`.
2. Then continue the projector port (§5): the remaining HQ windows off
   the per-app C dispatch.

---

## 1. The refactor in one paragraph

The khtpm renderer
`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
(~18.6k lines) is ONE generic engine with ~10 per-app modes gated by
`g_is_*` flags (`g_is_db_hq`, `g_is_events_hq`, `g_is_palettes`,
`g_is_bookmarks`, `g_is_stats_hq`, `g_is_swatch_picker`, …), ~1300+
lines of `dbhq_*`/`evhq_*` C. The goal: **retire all of it.** Each HQ
window becomes a static `<name>.xhtpm` template + a projector
(`.pal` via the shared `prisc+x` interpreter, OR compiled `.+x` C —
never bash) that writes `state/ui.txt` (key=value) ONLY when content
changes; the renderer stays generic. Template gets `${var}`
substitution + `<repeat>` + `show=` gating + `<tabbar>/<tab>`, all
already built and working. `db-hq-pal` is the first full example.

---

## 2. THE REFERENCE for scoped nav — copy this, do not reinvent

**File:** `44.xyz.01.00/101.ledger-player-npc-simple+3/system/chtpm_parser.c`
(3032 lines). Shared copy:
`44.xyz.01.00/&.widgits/_shared-lib/system/chtpm_parser_pal.c`
(≈20 per-package `chtpm_parser_pal.c` copies are the same logic).
This parser drives `pieces/apps/playrm/loader.chtpm` — the
"ACTIVATION SUBMENU" frames the user referenced.

### The model: 2 ints + the parse tree. The element array is NEVER mutated.

| symbol | line | meaning |
|---|---|---|
| `int focus_index` | 91 | index into `elements[]` — the `[>]` cursor |
| `int active_index = -1` | 91 | index of the activated scope root; `-1` = no scope |
| `elements[i].parent_index` | 84, set at 1858 | tree parent; drives `is_descendant()` |

### `is_navigable(int idx)` — **line 1750**. The entire gate. Pure, mutates nothing.

```
if !is_interactive(el) || !visible            -> false

if active_index != -1:                         # a scope is held
    if elements[active_index] is ACTIVATE + has children:
        idx == active_index                    -> true    # root stays navigable & numbered
        is_descendant(idx, active_index):
            # false if a folded ancestor sits between idx and the root
            -> true
        else                                   -> false
    else:   # active el is a cli_io / plain INTERACT target
        -> (idx == active_index)

else:                                          # global mode, no scope
    walk ancestors of idx:
        any ancestor .is_folded                -> false
        any ancestor .onClick == "ACTIVATE"    -> false   # submenu items hidden until activated
    -> true
```

`is_descendant(child, parent)` — **line 1708** — walks `parent_index` upward.

### Arrow nav — **lines 2610-2617** (global) / **2760-2764** (in scope)

```c
int prev = focus_index;
do { focus_index += dir; wrap 0..element_count-1; }
while (focus_index != prev && !is_navigable(focus_index));
```

Pure skip-scan over the immutable array. Nothing is rebuilt.

### Numbers — derived every frame, never stored as "the nav order"

- `do_jump(n)` — **line 2456**: `for i: if (is_navigable(i)) { if (++cn == n) focus_index = i; }`
- `count_navigable()` — **line 2457**: same count.
- So the visible `1..N` is always contiguous **within the current
  scope** without renumbering anything — it's just "the k-th navigable
  element right now".

### Render — `render_element()` **line 2195**, two counters threaded down

- `p_global_counter` — bumped for every interactive row NOT in the
  active scope. That row still draws, prefix `[ ]`, with its number —
  **visible but inert**.
- `p_scoped_counter` — bumped for rows where
  `is_descendant(idx, active_index)`. Its own `1..N`.
- On entering the active root's children, `*p_scoped_counter = 0`
  (**line 2324**) so the submenu restarts at 1; restored after (2332).
- prefix (**line 2258**): `[^]` if `idx == active_index`, else `[>]` if
  `is_focused && (active_index == -1 || navigable)`, else `[ ]`.
- descendants get `"    "` × depth indent (2217-2229).
- nav prompt (**line 2384**):
  `active_index == -1` → `"Nav > %s_"` ; else → `"Active [^]: %s (ESC to exit)"`.

### ENTER — **lines 2701-2730**

```
focused el is cli_io                      -> active_index = focus_index
focused el onClick == "ACTIVATE" w/ kids  -> active_index = focus_index;
                                             focus_index = first navigable child
focused el onClick == "INTERACT"          -> active_index = focus_index
focused el has href                       -> layout switch, active_index = -1
else onClick                              -> send_command(onClick)
```

### ESC — **lines 2745-2754** — pops ONE level (supports nesting)

```c
int old_active = active_index;
int p = elements[active_index].parent_index;
while (p != -1 && strcmp(elements[p].onClick, "ACTIVATE") != 0)
    p = elements[p].parent_index;          // nearest ACTIVATE ancestor
active_index = p;                          // -1 if none -> back to global
focus_index  = old_active;
if (active_index != -1 && !is_navigable(focus_index)) initialize_focus();
```

`initialize_focus()` — **line 2458**: if `focus_index` isn't navigable,
scan forward to the first that is.

---

## 3. How to port §2 into `khtpm_core_render.c` (replaces the hack)

Renderer today builds `g_nav[]` = compacted array of navigable `Elem*`,
`g_focus_nav` = 1-based index into it. Scope state globals already
exist: `g_default_active_scope_root` (Elem*),
`g_default_active_scope_id[64]` (trigger id),
`g_default_scope_confine` (0/1), `g_default_active_tab_id[64]`.

**Steps:**

1. **Delete the renumbering** in `kh_apply_scope_confine()`
   (`khtpm_core_render.c`, ~line 8504). Build `g_nav[]` exactly as
   today — every navigable `Elem*`, full 1-based `nav_index`, nothing
   zeroed, nothing dropped.

2. Add the predicate (the §2 `active_index` branch, on `Elem*`):

   ```c
   static int kh_elem_in_scope(Elem *e) {
       if (!g_default_scope_confine || !g_default_active_scope_root) return 1;
       if (e == g_default_active_scope_root) return 1;
       for (Elem *p = e->parent; p; p = p->parent)
           if (p == g_default_active_scope_root) return 1;         /* descendant */
       if (e->id[0] && g_default_active_scope_id[0] &&
           strcmp(e->id, g_default_active_scope_id) == 0) return 1; /* the trigger row */
       if (e->id[0] && strncmp(e->id, "chrome-", 7) == 0) return 1; /* WM titlebar stays live */
       return 0;
   }
   ```

3. **Arrow / Page handlers** (generic branch, where `g_focus_nav++/--`):
   wrap the step in the §2 skip-scan —
   `do { step; wrap 1..g_n_nav; } while (!kh_elem_in_scope(g_nav[g_focus_nav-1]));`

4. **Digit-jump, `activate_focused()`, mouse-click routing**: if
   `g_default_scope_confine && !kh_elem_in_scope(target)` → ignore the
   input (don't move focus, don't fire).

5. **`khtpm_draw_core.c`**: on `!kh_elem_in_scope(e)` rows draw `[ ]`
   and **keep the existing number** (inert). `[>]`/`[^]` only for
   in-scope. Optional polish: a scoped `1..N` counter on the submenu
   rows (the §2 `p_scoped_counter`) — not required for correctness.
   The `[^]` marker already keys off `g_default_active_scope_id` in
   `draw_core` (line ~576) — leave that.

6. **Status/nav line**: show `Active [^]: (ESC to exit)` while
   `g_default_scope_confine`, else the normal prompt.

7. **ESC**: pop ONE level like §2 — set `g_default_active_scope_root`
   to its nearest ancestor container that is itself a scope trigger,
   else clear (`g_default_scope_confine = 0`,
   `g_default_active_scope_id[0] = '\0'`). Today's ESC (line ~9605)
   always fully clears — fine for one level, extend for nesting.

**Net:** `g_nav[]` / `nav_index` become immutable render data; the
scope is enforced only by `kh_elem_in_scope()` consulted at
focus-move / jump / click / draw time. Then the two hack commits
(`59acda6d`, `dde3291e`) are superseded — the `chrome-*` keep survives
as the clause in `kh_elem_in_scope()`.

### Where the `<tab>` scope is set today (keep this, it's correct)

- `activate_focused()` (`khtpm_core_render.c` ~line 8997): `<tab>`
  branch runs `dispatch(item->onclick)`, sets `g_default_active_tab_id`,
  resolves `item->target_id` → `<sidebar id="sidebar">`, sets
  `g_default_active_scope_root` + `g_default_scope_confine = 1` +
  `g_default_active_scope_id = item->id`.
- `reparse_chtpm_if_changed()` (~line 1418): re-resolves the scope
  across the projector's every-tick `state/ui.txt` rewrite (content-
  hash-gated so identical writes don't churn). `<tab>` → always locks
  onto the page `<sidebar>`.

---

## 4. HOW TO TEST (headless — no live desk click needed)

The renderer is `override_redirect`; xdotool / scrot can't see it.
Drive it with **history injection** and read back the **live frame
dump**.

### Launch db-hq-pal

```sh
cd .../yz.muchiverse
HOUSE="$PWD/44.xyz.01.00"
sh "44.xyz.01.00/&.hq-apps/db-hq-pal/button.sh" "$HOUSE"      # builds renderer+prisc, launches
PID=$(pgrep -f 'khtpm_core_render\.\+x .*dashboard\.xhtpm' | head -1)
```

### Inject keys

Write lines to `#.desktop/entity_menu_history/<pid>.txt`. The generic
window's `poll_agent_history()` (called from `hq_idle_tick`) consumes
them via `dispatch_relay_code()` → `handle_key()`.

```
KEY_PRESSED: 13     # Return        KEY_PRESSED: 27   # Esc
KEY_PRESSED: 200    # Up            KEY_PRESSED: 201  # Down
KEY_PRESSED: 202    # Left          KEY_PRESSED: 203  # Right
KEY_PRESSED: 204/205  # PgUp/PgDn   KEY_PRESSED: 32..126  # ASCII
MOUSE_EVENT: <button> <x> <y> <press>
```

**First** `stat` of the file sets the read cursor to its current EOF,
so: `: > "$H"` (create/truncate), `sleep 1.5`, then append and
`sleep ~0.5` per key.

### Read back the live frame

`#.desktop/entity_menu_frame_<pid>.txt` — rewritten every redraw,
serialized subtree. Pipe fields:

```
tag | id | class | label | extra | onclick | nav_index | active_tab_flag | x | y | w | h | target_id
```

Field 7 = `nav_index`. Field 8 = "is the selected tab" (NOT the focus
ring — the focus ring is `nav_index == g_focus_nav`, only in the PNG).

```sh
H="44.xyz.01.00/#.desktop/entity_menu_history/$PID.txt"
F="44.xyz.01.00/#.desktop/entity_menu_frame_$PID.txt"
navlist(){ awk -F'|' '$7!=""&&$7!="0"{printf "  nav%-3s %-6s %-16s %s\n",$7,$1,$2,$4}' "$F"; }

: > "$H"; sleep 1.5
printf 'KEY_PRESSED: 201\nKEY_PRESSED: 201\n' >> "$H"; sleep 1     # Down onto a tab
printf 'KEY_PRESSED: 13\n' >> "$H"; sleep 1.2                      # Return -> activate + scope
navlist
```

### Pass criteria for the CORRECT port (§3)

- After Return on a tab: **all 15 tabs + all rows still appear in the
  frame with their numbers**; only the activated tab + its record rows
  + `chrome-*` are *navigable* (respond to arrows / digits / Enter).
- `Left` / `Right` / `Down` cannot move focus onto another tab.
- Digit key for an out-of-scope number does nothing.
- `chrome-minimize` / `-fullscreen` / `-close` still reachable.
- `Esc` → focus back on the tab row, full nav restored.
- The status line reads `Active [^]: (ESC to exit)` while locked.

### Reset between runs

`44.xyz.01.00/&.hq-apps/db-hq-pal/state/active.pdl` persists the tab +
selection (written by `ops/dbhq_action.sh`). Rewrite it to Actors for a
clean start:

```
SECTION | KEY   | VALUE
FILE    | file  | db_hq_actors.state.txt
TAG     | tag   | ACTOR
TITLE   | title | Actors
SEL     | sel   | 0
```

### Rebuild after editing the renderer

```sh
cd "$(ls -d .../yz.muchiverse/44.xyz.01.00/*.monads/*.livedesk-taskbar/ops)"
sh build_core_render.sh          # dirs are literally named *.monads etc — cd needs absolute/globbed path
```
`button.sh` also rebuilds on each launch. `build_khtpm_strip.sh` builds
both the taskbar manager and the renderer.

---

## 5. Projector port — remaining work

Recipe per window: static `<name>.xhtpm` + projector writing
`state/ui.txt`; `button.sh` launches
`khtpm_core_render.+x <house> <name>.xhtpm`; verify with the frame dump
(§4) or `&.widgits/_shared-lib/ops/khtpm_png_dump.sh`.

### Done & working
`signup-hq`, `open-hai`, `co-lab-hai`, `db-hq-actors-pal` (reference
PAL app), `db-hq-pal` (15-tab, read-only — tab switch + list + panel
project correctly via `pal/dbhq_projector.pal`).

### Safe to hand off / parallelize

| target | notes |
|---|---|
| **db-hq-pal field editing + Common Events editor** | the real remaining db-hq work — see `DB-EVENTS-HQ-PORT-DESIGN.md` §3–§5. Needs `<cli_io>` in `<scrolllist>` (already ported, `38a3da4d`) + overlay for the CE command editor. |
| **chat-hai** | replace `ops/chat_hai_projector.sh` (bash — NOT allowed) with `.pal` or `.c`; author `chat-hai.xhtpm`. Message list = one `<repeat>`. |
| **events-hq** (`g_is_events_hq`) | same `<tabbar>` port as db-hq-pal; view sub-tabs + command list; `evhq_*` C retires. The `event.ir.pdl → event.pal → cmd_N.sh` compile chain stays ops-side (`&.widgits/events-hq/ops/khtpm_events_hq_manager.c` `compile_page()`); projector shells to it. |
| **palettes / bookmarks / stats-hq** (all ride `g_is_db_hq`) | each: static template + projector reading its existing state files. Palettes needs the `sprite=` grid (already a `draw_elem` capability). |
| **swatch-picker / taskbar-settings** (`g_is_swatch_picker`) | flat `<item>` list + opacity ± ; small. |

### Needs care — do NOT hand off

| target | why |
|---|---|
| **network_browser** | heterogeneous page content (TITLE/TEXT/LINK/IMG/VIDEO + sprite-grid wrapping). Needs `<repeat>` v2 — one body with `show="${c.is_X}"` per candidate kind; projector sets exactly one `c_<i>_is_*` = 1. Sprite-grid row wrapping lost in first cut (acceptable). |
| **retire `dbhq_*` C** | once db-hq-pal is signed off: point `*.monads/*.muchi-pet/ops/open_db_hq.sh` at `dashboard.xhtpm`, drop `class="db-hq"`, delete `dbhq_*` renderer C (~1350 lines). Big — its own branch. |

---

## 6. Rules / gotchas

- **Never `git add -A` / `-u`.** Stage explicit paths.
  `khtpm_core_render.c` and `khtpm_taskbar_manager.c` have concurrent
  editors (other agents). Never stage runtime churn: `#.desktop/*`,
  `state/*.txt`, `*.pid`, `module_parent.pid`, `*.ledger`.
- `+x` binaries are **not** tracked — rebuild after any pull.
- Dirs are literally named `*.monads`, `*.livedesk-taskbar`,
  `&.hq-apps`, `&.widgits` — `cd` to an unexpanded glob path fails; use
  absolute paths. A bare `&` in a shell line = background operator
  (has broken menu wiring before).
- `pkill -f '<pattern that matches pkill's own argv>'` self-kills (exit
  144). Use explicit PIDs.
- Old `db-hq` (`class="db-hq"`, `open_db_hq.sh`) stays UNTOUCHED for
  A/B comparison until the PAL one is signed off.
- Projectors are `.pal` (shared `prisc+x`, comment every line "for
  newbs") or compiled `.+x` C. **Never bash.**
- Renderer only re-renders on a marker/state change (content-hash-gated
  reparse) — keep projectors writing `state/ui.txt` only when content
  actually changed.
- Commit footer:
  ```
  Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN
  ```

---

## 7. Other docs (background only — this doc supersedes them for scope nav)

All under `#.#.calendar-dox/!.HQ-IQ-BOOK/`:

- `09-appendix/HANDOFF-chtpm-var-substitution.md` — the incremental
  session log, revs 1–5 (rev 5 = the §2/§3 material, condensed here).
- `08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md` — original spec:
  static template + `${var}` + projector-writes-state.
- `08-roadmap/design-docs/DB-EVENTS-HQ-PORT-DESIGN.md` — full port
  design: debt inventory, HTML/CSS/JS reframe (multiple `<module>`
  tags), per-app projector table, migration procedure.
- `08-roadmap/design-docs/EVENT-COMMAND-REGISTRY-ARCHITECTURE.md` — the
  `event.ir.pdl → event.pal → cmd_N.sh` chain.
- `08-roadmap/design-docs/media-suite.md` — `103.media-studio` → x11-hq
  toys migration (Grok).
- `08-roadmap/design-docs/OPENCODE-HANDOFF-events-6-rung-ladder.md` —
  events "6-rung ladder" (OpenCode).
- `&.widgits/_shared-lib/system/string-ops.md` — `prisc+x` string
  opcodes (`slit scpy sappend sgetenv sfmt sread ssplit sfind slen
  sfopen sfappend swrite sfclose sbeq sbne strim satoi`) + projector
  idiom.

---

## 8. Commit state (branch `chtpm-var-substitution`, HEAD)

```
9aac1edb handoff: rev 5 - port is_navigable() from the reference chtpm parser
5354a326 handoff: rev 4 - <tab> scope lock working, input-gating refinement noted
dde3291e taskbar: keep the window chrome bar reachable while a scope is held    <- HACK, supersede
59acda6d taskbar: apply scope confinement on the sidebar+panel layout path       <- HACK, supersede
189ee325 design: renderer capabilities status + Haiku task shape
38a3da4d chtpm: <cli_io> rows in a <scrolllist>
2394fd0d design: full db-hq/events-hq port - shell module + per-tab modules
4b6250b6 taskbar: db-hq (PAL) via livedesk:open-db-hq-pal verb
```

`59acda6d` + `dde3291e` do functionally lock the scope, but via the
wrong mechanism (mutating `g_nav[]`). Replace with §3. Nothing is
broken to pick up from.
