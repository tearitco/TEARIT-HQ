# 🧭 Taskbar & Sub-App Architecture — menus, dispatch, and building a new app from scratch

**Written:** 2026-08-15, after adding "Chat-h-ai" to cell 14 (h-ai)'s submenu took
~15 tool-call round trips of trial and error that should have taken 3, then
extended after a full session building chat-hai (a brand-new sub-app) end to
end. Covers both halves: wiring a menu item into an EXISTING cell, and the
real lifecycle/pitfalls of building a NEW `khtpm_*`-family app from scratch
(see the "Building a NEW sub-app from scratch" section below). This doc
exists so the next agent doesn't repeat the same dead ends.

**See also (2026-08-18)**: `taskbar-tpmos-parallel-refactor.md` and
`taskbar-history-txt-migration-investigation.md` go a level deeper than this doc's own two-layer
relay diagram below - they cover the real terminal ASCII mirror built on top of this same relay
(`khtpm_strip_render_ascii.+x`/`khtpm_strip_keyboard_ascii.+x`), two real relay-forwarding gaps
found+fixed in `dispatch_key_code()` (arrow-key nav and HQ-header-opening were NOT relayable before
that pass, despite this doc's own diagram implying the relay carries "raw keycodes" generally), and
the dispatch-mode migration this covered (2026-08-18 dual-path → **2026-08-19 cutover complete**:
`KHTPM_NEW_DISPATCH_MODE`/`g_new_dispatch_mode` and the old inline-dispatch path are now fully
DELETED — capture-then-`poll_captured_input()` is the only dispatch path, no flag, no fallback).
A frame-unification pass also landed the same day (`strip_frame.cells.pdl`, feeding the terminal
ASCII mirror) and a real bug it introduced (a filename collision that broke arrow-key submenu nav)
was found+fixed — see `taskbar-history-txt-migration-investigation.md` and
`au11-hq/TASKBAR-FRAME-UNIFICATION-HANDOFF.md` (under the `44.xyz.01.00/` subtree) for the
full story. Read those before assuming this doc's own diagram is the complete, current picture of
what the relay can carry.

---

## ⚡ TL;DR

- Cell 14 (h-ai)'s submenu is **hardcoded in C** (`livedesk_build_ai_menu()` in
  `khtpm_taskbar_manager.c`), NOT read from `#.desktop/livedesk_taskbar.pdl`.
- The PDL **does** have `strip_btn_14_menu_0/1_label`/`_cmd` rows that look
  live. **They are dead.** Editing them changes nothing. This is misleading
  by accident (leftover from an earlier, PDL-driven design) not by intent —
  see "How this happened" below.
- A menu item's `.command` field must be a **dispatch string**
  (`"livedesk:open-X"`), matched by a `strcmp` branch in `ktb_hq_activate()`
  that builds the real shell command using `s->house_root`. A raw shell
  command baked directly into the menu builder (e.g. `"setsid nohup bash
  '&.hq-apps/x/button.sh' run &"`) will compile fine, look right in
  frame-history, and then **silently no-op** when clicked — the relative
  path never resolves from `system()`'s unknown cwd.
- **This whole hardcoded-in-C pattern is itself the architecture violation.**
  See "Standing refactor debt" below — do the minimal, convention-matching
  fix to ship the feature, but don't extend the pattern believing it's
  correct.

---

## 🗺️ The two-layer relay/dispatch system (read this before touching taskbar C)

```
User click / nav.sh relay injection
        │
        ▼
#.desktop/livedesk_agent_relay.txt      (PARSER layer — raw keycodes)
        │  read by khtpm_strip_parser.c
        │  resolves hit-testing/focus/ACTIVATE locally
        ▼
#.desktop/strip_history.txt             (MANAGER layer — resolved codes)
        │  read by khtpm_taskbar_manager_main.c
        │  ktb_hq_open(which) → livedesk_build_<cell>_menu() builds HQMenuItem[]
        │  (cell 14 → livedesk_build_ai_menu(), C-hardcoded, NOT pdl-read)
        ▼
ktb_hq_activate(s, item_index)
        │  m->command is either:
        │    - a "livedesk:open-X" dispatch string  → strcmp branch, builds
        │      real shell cmd with s->house_root, system()
        │    - a raw shell command string           → falls through to a
        │      generic system() fallback (works ONLY if the string is a
        │      fully-resolvable absolute-path command already)
        ▼
Real process launched (or silently fails if the raw-command path had an
unresolvable relative path — no error surfaces anywhere in this chain)
```

**Two harness layers exist for testing** (`#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh`):
- `nav <n>` / `row <n>` — PARSER layer, raw ASCII keycodes, matches real
  keyboard/mouse input exactly. Use this for end-to-end tests.
- `hqcell <n>` / `mgrcode <n>` — MANAGER layer, injects an already-resolved
  code straight into `strip_history.txt`. Use only when parser-layer has no
  path there (e.g. arrow-key nav has no PARSER-layer relay equivalent).

**Known nav.sh trap:** `nav.sh nav <n>` already sends the digits AND a
trailing Enter (see its own `cmd_nav_or_row`). Do **not** follow it with a
manual `key Return` — that's a second Enter, which selects/activates
whatever item currently has focus in the now-open submenu (bit us live:
double-Enter after `nav 14` launched "Open h-ai" — the item at focus 0 —
instead of testing the item we meant to). Open the submenu with `nav <n>`,
THEN issue a separate `row <n>` call for the actual submenu selection.

---

## ✅ The correct recipe to add a menu item to an EXISTING C-hardcoded cell

(Cells known to use this pattern: 14/h-ai via `livedesk_build_ai_menu()`.
Check `ktb_hq_open()`'s dispatch switch for other cells before assuming —
some may genuinely be PDL-driven already.)

1. **Add the menu row** in the cell's `livedesk_build_*_menu()` function:
   ```c
   if (n < max) {
       snprintf(menu[n].label, sizeof(menu[n].label), "Your Label");
       snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:open-yourthing");
       n++;
   }
   ```
2. **Add the dispatch branch** in `ktb_hq_activate()` — copy the shape of the
   existing `livedesk:open-open-hai` branch verbatim (renamed from
   `livedesk:open-ai-cell` since this doc was written — search for it, ~line
   2708 as of this writing) and adapt paths:
   ```c
   } else if (strcmp(m->command, "livedesk:open-yourthing") == 0) {
       char sh[KTB_PATH_BUF * 3];
       snprintf(sh, sizeof(sh),
           "setsid nohup sh -c 'sh \"%s/<app-dir>/button.sh\" \"%s\"' >/dev/null 2>&1 &",
           s->house_root, s->house_root);
       int rc = system(sh);
       (void)rc;
       ktb_hq_close(s);
   }
   ```
3. **The app's `button.sh` must accept `house_root` as `argv[1]`** (a
   directory path, not an action keyword like `"run"`). Copy
   `&.widgits/ai-cell/button.sh` wholesale and adapt binary/path names —
   it already has the correct single-instance guard (kill-existing via
   `pgrep -f '<binary>\.\+x'`, TERM then KILL escalation, confirm exactly
   1 PID after launch). Do not hand-roll this from scratch — every
   from-scratch attempt this session had a different bug.
4. **Check the render binary's own argv contract** by running it with zero
   args — most print a `usage: <bin> <house_root> [<extra_arg>]` line.
   Don't assume it matches ai-cell's shape; chat-hai's renderer needed a
   *second* argument (a `.chtpm` layout path) that ai-cell's didn't.
5. **Rebuild + fully restart, every time**, even for a comment-only C
   change during debugging — a stale binary + a live process is
   indistinguishable from a real bug and wastes the most time of anything
   in this whole recipe:
   ```sh
   cd "*.monads/*.livedesk-taskbar/ops"
   bash build_khtpm_strip.sh          # ~15s, warnings-only is a pass
   bash run_khtpm_strip.sh new        # kills old, rebuilds, launches, confirms PID
   ```
   Verify via `ps -p "$(cat '#.desktop/livedesk_taskbar.pid')"`, not a raw
   `pgrep -a khtpm` — the real binary name has a literal `.+x` suffix that
   `pgrep`'s regex needs escaped (`khtpm_strip_parser\.\+x`) or every check
   falsely reports "not running" even when it is.
6. **Test via `nav.sh`**, not hand-crafted relay codes (see the double-Enter
   trap above). Full walkthrough below.

---

## 🧪 Testing via relay injection — concrete `nav.sh` walkthrough

Never test a taskbar change by guessing raw ASCII codes into
`livedesk_agent_relay.txt` by hand — the harness already exists and
already got the timing/settle logic right. Use it.

```sh
HOUSE="<absolute house root>"
NAV="$HOUSE/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh"

# 1. Clear the frame-history log so you only see THIS test's frames
> "$HOUSE/#.desktop/khtpm_strip_frame_history.txt"

# 2. Open cell 14's submenu (types "14", presses Enter — ONE call, ONE Enter)
HOUSE="$HOUSE" bash "$NAV" nav 14

# 3. Confirm the submenu actually opened before selecting anything —
#    look for header.active != -1 and the element_count you expect
#    (a bigger count than the cell's OWN button row confirms your new
#    item compiled in, even before you click it)
HOUSE="$HOUSE" bash "$NAV" frame 2

# 4. Select the Nth submenu row (1-based!) — THIS call sends its own
#    digits+Enter. Do NOT send an extra `nav.sh key Return` before or
#    after this — see the double-Enter trap above, it silently
#    activates whatever item currently has focus instead of the row
#    you actually typed.
HOUSE="$HOUSE" bash "$NAV" row 2

# 5. Verify the real side effect, not just the frame log — a menu
#    "select" can look identical in frame-history whether or not the
#    launched command actually worked (system() failures are silent).
#    Check the actual process:
pgrep -af "<your-binary-name>\.\+x"
#    ...and/or the app's own log file (button.sh should write one):
tail -20 "$HOUSE/<app-dir>/pieces/audit/<app>.log"
```

**Reading `frame` output** — one line per parser tick, format:
```
header.focus=<idx>[type=button label=<Label> onClick=<Action>] header.active=<idx or -1> bottom.focus=<n> ... element_count=<n>
```
`header.active=-1` means no submenu is open. A submenu open shows
`header.active=<parent cell's focus idx>` and `hq_focus=<selected row,
0-based>`. `element_count` jumping up when a submenu opens (e.g. 25 → 52)
is itself useful evidence that your new C-hardcoded row actually compiled
into the running binary — check this BEFORE spending time debugging the
dispatch/launch side if the count looks unchanged (stale binary is the
more common bug).

**If nothing happens after `row <n>`:** the bug is almost never the relay
itself — it's one of: (a) stale binary (see step 5 in the recipe above —
did you rebuild AND fully restart via `run_khtpm_strip.sh new`, not just
`build_khtpm_strip.sh` alone?), (b) the menu command is a raw shell string
with a relative path instead of a `"livedesk:open-X"` dispatch string, or
(c) the target app's own launcher script has an argv contract mismatch
(run it directly with the exact args the manager would pass, before
blaming the taskbar side at all).

---

## 🚧 Standing refactor debt — PDL externalization (not done, don't extend the C pattern)

The C-hardcoded-menu pattern used above is a real spec-drift violation of
this house's own external-config standard (every other subsystem's
behavior lives in a `.pdl`/`.chtpm`, editable without a recompile — see
`!.HOUSE_STDS.md`). **How this happened:** the PDL rows
(`strip_btn_14_menu_0/1_label`/`_cmd`) were very likely written FIRST, as
the intended real config surface, then a later session added
`livedesk_build_ai_menu()` in C as a quicker path to get cell 14 working —
and the PDL rows were never wired up OR removed, so they sat there looking
authoritative while actually being dead weight. This is exactly the kind
of quiet architecture drift the standing INDEX.md rule ("check local chtpm
usage... before inventing new shape") is meant to catch, except in this
case the drift was in the OPPOSITE direction — a config-driven design
regressed into hardcoded C, not the other way around.

**Real fix (deferred, not done this session):** make the manager read
`strip_btn_<n>_menu_<m>_label`/`_cmd` from the PDL at cell-open time
(pattern already proven for the top-left `hq_menu_*` rows — see
`ktb_hq_open()`'s HQ-cell branch, which DOES read the PDL correctly; cell
14 should follow that same code path, not `livedesk_build_ai_menu()`),
either deleting `livedesk_build_ai_menu()` entirely or keeping it only as a
documented fallback for `.pdl`-cache-miss. Until this is done: **any new
agent editing `#.desktop/livedesk_taskbar.pdl`'s `strip_btn_14_*` rows is
wasting their time** for this specific cell — always check
`ktb_hq_open()`'s dispatch switch in the manager C first to see which
cells are genuinely PDL-driven vs. C-hardcoded before touching either
file.

**UPDATE 2026-08-24 (direct instruction):** the debt is NOT cell-14-only.
User confirmed **none** of the cells' builders are supposed to be
C-hardcoded — ALL of them must eventually follow the hq PDL-driven
pattern (`livedesk_build_hq_menu()` reading `hq_menu_N_label/cmd`, and
now `livedesk_build_palettes_menu()` reading `palettes_menu_N_label/cmd`
— both read live from `#.desktop/livedesk_taskbar.pdl` at cell-open
time, no recompile). Convert when touching each cell:
`livedesk_build_user_menu()`, `_file_menu()`, `_desk_menu()`,
 `_player_menu()`, `_db_menu()`, `_pals_menu()`, `_toys_menu()`,
 `_clock_menu()`, `livedesk_build_ai_menu()` (cell 14). Each conversion =
define `<cellname>_menu_N_label/_cmd` PDL rows + swap the builder body
for the hq-style read loop; directory-scanning builders keep the scan,
but emit its results into the same PDL rows (or keep scan-in-C as
documented fallback only). Until converted, edit the C builder for that
cell — its PDL rows are dead weight, exactly like cell 14's.

---

## 🏗️ Building a NEW sub-app from scratch (not just adding a menu item)

Everything above assumes an app already exists and you're wiring it into
a menu. This section is the other half — the real lifecycle of building a
brand-new `khtpm_*`-family app (a persistent tb-launched window like
db-hq/ai-cell/events-hq/chat-hai), generalized from building chat-hai
end-to-end this session. **You are in the `khtpm_*` parser family, not
`chtpm_parser_pal.c`'s** — see `!.HOUSE_STDS.md` §J before assuming
anything about CSS/layout support carries over from the legacy widget
family.

### The file skeleton (copy an existing app's shape, don't invent one)
```
&.hq-apps/<yourapp>/
  button.sh                  launcher - see the "correct recipe" section above,
                              argv[1]=house_root, single-instance guard
  <yourapp>.chtpm             layout: <window><sidebar>...<panel>...</panel></window>
  <yourapp>.css                colors/fonts/padding/border ONLY - see §J, no
                                real flex/box-model, don't write layout CSS
                                expecting it to do anything
  <yourapp>_config.pdl         REAL runtime-tunable settings (window geometry,
                                timing, anything a user might want to adjust
                                without a rebuild - see §A.7 of HOUSE_STDS)
  ops/
    <yourapp>_hq_render.c      the renderer - copy khtpm_ai_cell_render.c's
                                (or khtpm_hq_render.c's) overall shape: parse_chtpm(),
                                layout_pass(), redraw(), handle_key(),
                                main()'s X11 window-standard setup
                                (!.HOUSE_STDS.md #21 - real WM-managed window,
                                _MOTIF_WM_HINTS decorations=0, NOT
                                override_redirect)
    build_<yourapp>.sh         compiles the renderer to ops/+x/<binary>.+x
    khtpm_css_parser.c/.h      COPY, don't symlink (matches this house's own
                                "copied not symlinked" convention for shared
                                engine pieces, §A of HOUSE_STDS - even though
                                that section is about the OTHER parser family,
                                the copy-not-symlink convention itself applies
                                here too)
  state/                       whatever the app actually needs to persist -
                                see the elem-pool/geometry pitfalls below
                                before assuming a naive design is safe
```

### Real pitfalls found building chat-hai, generalized (check for these in ANY new khtpm_* app)
1. **Elem pool exhaustion, not just for chat-hai** — if your renderer
   dynamically injects Elems every redraw (a live feed, a refreshing
   list, anything not 100% static from the `.chtpm` file), the pool
   bump-allocator (`g_n_elems`/`elem_new()`) will silently return NULL
   once exhausted, and an unchecked `item->parent = ...` write on that
   NULL will segfault - NOT tied to any specific user action, just
   whichever redraw happens to be the one that finally fills the pool.
   **Any app with dynamic content needs the `g_n_elems_static` rewind
   pattern** (capture the baseline right after `parse_chtpm()`, rewind to
   it at the top of every `layout_pass()`, before any injection) - copy
   this from `chat_hai_hq_render.c` directly, don't rediscover the bug.
   (2026-08-16: chat-hai's own standalone renderer is now archived to
   `_.ARCHIVED-pre-merge-legacy.zip` — the live equivalent logic is the
   `chai_`-prefixed functions in `khtpm_entity_menu_render.c`, see
   `khtpm-merge-how2.md` §5d.12.)
2. **`apply_css(window, 0)` clobbers any one-time `window->style`
   override, every single redraw** — if your app needs runtime-computed
   geometry (screen-relative size/position, anything not a fixed CSS
   value), a one-time mutation of `window->style.width/height` in
   `main()` gets silently reverted the next time `layout_pass()` runs
   (which calls `apply_css()` at its own top). Use dedicated
   `g_forced_win_w`/`g_forced_win_h`-style globals, applied INSIDE
   `layout_pass()` AFTER its own `apply_css()` call, every time - not a
   pre-loop one-shot.
3. **CSS descendant selectors (`.a .b`) silently match nothing** — see
   §J. If a class-based color/font rule isn't applying and there's no
   error anywhere, check whether the selector has a space in it before
   assuming anything else.
4. **All window/layout dimensions should be `.pdl`-driven from day one**,
   not hardcoded C constants — this session hand-edited raw pixel
   constants and rebuilt 3 separate times chasing "wrong position/size"
   reports that a single `<yourapp>_config.pdl` read at startup would
   have made a one-line edit instead. See `chat_hai_config.pdl` for the
   real, working pattern (unscaled base pixels, `font_scale` still
   multiplies them, `SECTION | key | value` pdl rows, a small dedicated
   loader function called once in `main()`).
5. **A ledger/ever-growing-log-file feed must read the TAIL, not the
   head** — if your app shows "the last N items" from an append-only
   file, reading top-to-bottom and stopping at N items shows the OLDEST
   N once the file exceeds that cap, not the newest - looks exactly like
   "data isn't updating" from the outside (a real, hard-to-spot bug:
   the reload WAS firing correctly on every file change, it just kept
   re-parsing the same stale head every time). Count total lines first,
   skip to `(total - N)`, then parse.
6. **Verification**: add a text `#.desktop/<yourapp>_frame_history.txt`
   log (one line per `redraw()`, whatever state fields matter for your
   app - session/counts/flags/position, NOT a screenshot) from day one.
   Use it for any question about DATA (is X updating, what's the current
   value of Y) via relay injection + `tail`; reserve an actual PNG dump
   (`dump_frame_png()` bound to a debug key, relay-injectable) for
   genuine VISUAL/layout questions only. Building the text log early
   saves far more time than it costs.
7. **Reliable start/stop for any backgrounded work your app kicks off**
   (a companion loop script, a long-running task) should gate the
   actual slow operation itself (e.g. right before a network call) via a
   plain state file the background script checks in a wait-loop -
   `pkill -STOP/-CONT` on the script's own process is NOT reliable if
   that script has async children (curl in a subshell, etc.) already in
   flight when the signal arrives.

### Wiring the new app into the taskbar
Once the app itself works standalone (`sh button.sh <house_root>` launches
it, confirmed via `pgrep`), follow the "correct recipe to add a menu
item" section above to wire it into whichever cell should launch it -
same dispatch-string + `ktb_hq_activate()` branch pattern, same
single-instance-guarded `button.sh` contract, same rebuild+restart
discipline.

---

## Chat-hai integration — final working state (2026-08-15)

**(2026-08-16 forward pointer, historical section below unchanged):**
`button.sh` now launches the shared `khtpm_entity_menu_render.+x`
binary (chat-hai mode), not the standalone `chat_hai_hq_render.+x`
named below — see `khtpm-merge-how2.md` §5d.12.

- Menu: cell 14 (h-ai) → "1. Open h-ai | 2. Chat-h-ai | 3. Cancel" ✅
- `livedesk_build_ai_menu()` (khtpm_taskbar_manager.c) — item 1 command is
  `"livedesk:open-chat-hai"`
- `ktb_hq_activate()` has a matching `livedesk:open-chat-hai` branch calling
  `&.hq-apps/chat-hai/button.sh "<house_root>"`
- `&.hq-apps/chat-hai/button.sh` rewritten to match `&.widgits/ai-cell/button.sh`'s
  exact contract: `argv[1]` = house_root, single-instance guard, launches
  `ops/+x/chat_hai_hq_render.+x "<house_root>" "<chtpm_path>"`
- Verified: renderer binary launches with a real, single, confirmed PID when
  `button.sh <house_root>` is run directly. Full end-to-end via taskbar click
  reached the same code path after the fixes above (see chat-hai-design.md's
  own status section for current confirmed-working state going forward).

**Next real task (per direct instruction, not yet started):** reformat
chat-hai's layout to match ai-cell's real layout — sessions list on the
LEFT, chat feed on the RIGHT, composer/input pinned to the BOTTOM
(vertically stacked under the feed, not beside it). Read ai-cell's actual
render code (`khtpm_ai_cell_render.c`) for the real geometry before
changing chat-hai's renderer — don't assume from the label "sidebar" alone
(see [[Gap sweeps: check geometry, not just presence]] memory entry).
