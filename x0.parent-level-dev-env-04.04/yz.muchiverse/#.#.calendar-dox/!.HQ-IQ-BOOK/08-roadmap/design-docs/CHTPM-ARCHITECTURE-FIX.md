# `.chtpm` Architecture Fix — restore the tpmos separation of layout vs data

**Status:** spec / not started · **Date:** 2026-09-03
**Supersedes:** the runtime-generated-`.chtpm` pattern in `khtpm-generic-dispatch-design.md`
**Blocks:** `media-suite.md` (toys should get the right architecture from the start)

---

## 1. The problem, stated plainly

The original tpmos design (`chtpm_parser.c`) had a clean separation:

- **Human writes `.chtpm`** — a static layout template with `${var}` placeholders (like HTML)
- **Manager writes `gui_state.txt`** — plain key=value pairs (dynamic data)
- **Parser substitutes `${var}`** from `gui_state.txt` into the static template at render time

The current khtpm design (`khtpm_core_render.c`) inverted this:

- **Manager regenerates the entire `.chtpm` file every tick** — writing full XML markup, not just data
- **Renderer re-parses the `.chtpm` every tick** via `reparse_chtpm_if_changed()`
- **No `${var}` substitution exists** — it was bypassed, not solved

The result: `.chtpm` files are **not human-authorable**. The manager overwrites them immediately. There is no separation between layout and data. The manager has to know about rendering (generating `<sidebar><scrolllist><item>` markup) instead of just writing state.

**The user's correction:** `.chtpm` should be like HTML (human-writable), and the manager should be like JavaScript (called from `<module>`, handles behavior). The refactorer got this backwards.

---

## 2. What `${var}` does in tpmos (and what khtpm does instead)

### tpmos (`chtpm_parser.c`)

The parser has a `substitute_vars()` function. At render time, it reads `gui_state.txt` (a flat key=value file the manager wrote) and replaces every `${key}` token in the `.chtpm` template with the corresponding value.

**`gui_state.txt`:**
```
game_map=...map content here...
last_key=42
```

**`.chtpm` template (human-authored):**
```xml
<text label="${game_map}" />
<text label="KEY: ${last_key}" />
```

**After substitution:**
```xml
<text label="...map content here..." />
<text label="KEY: 42" />
```

The layout structure (buttons, panels, borders) is stable. Only the `${var}` values change tick to tick. The manager never touches the `.chtpm` file.

### khtpm (current — no `${var}` at all)

There is no `substitute_vars()`. There is no `gui_state.txt`. Instead, the manager's `write_chtpm_projection()` regenerates the **entire `.chtpm` file** from scratch every tick:

```c
// open_hai_manager.c — this is the current shape
void write_chtpm_projection(void) {
    FILE *f = fopen(g_out, "w");
    fprintf(f, "<window label=\"open-hai\">\n");
    fprintf(f, "  <page name=\"main\">\n");
    fprintf(f, "    <sidebar>\n");
    fprintf(f, "      <scrolllist>\n");
    // ... loops over sessions, writes <item> for each one
    fprintf(f, "      </scrolllist>\n");
    fprintf(f, "    </sidebar>\n");
    // ... writes panel, transcript, cli_io, etc.
    fprintf(f, "  </page>\n");
    fprintf(f, "</window>\n");
    fclose(f);
}
```

The manager is generating markup. It knows about `<sidebar>`, `<scrolllist>`, `<item>`, `<panel>`, `<cli_io>`. The `.chtpm` file is a transient artifact — the bootstrap is overwritten within one tick, and the live file is regenerated every ~150ms.

**What this means in practice:**
- You cannot hand-edit `open-hai.chtpm` — the manager overwrites it
- The `.chtpm` file is not a stable contract — it's whatever the manager last wrote
- The manager has to know the renderer's tag vocabulary — it's coupled to rendering
- There's no separation between "what the UI looks like" (layout) and "what data it shows" (state)

---

## 3. What needs to change

### 3.1 Add `${var}` substitution to `khtpm_render_core.c`

Port or rewrite a `substitute_vars()` function for the khtpm parser. The parser reads a state file (e.g. `<app>_state.txt` or `gui_state.txt`) and substitutes `${key}` tokens in `label=`, `action=`, and other attribute values.

The tpmos reference implementation is in `101.ledger-player-npc-simple+3/system/chtpm_parser.c` — the `substitute_vars()` function and its `load_vars()` / `get_var()` helpers. The khtpm version needs to work on the already-parsed Elem tree (post-parse substitution on attribute strings), not during tokenization.

**Key design decision:** when does substitution happen?
- Option A: at parse time (substitute before building the Elem tree) — simpler, but requires re-parsing on every state change
- Option B: post-parse on the Elem tree (substitute in `apply_attr()` or a new pass after layout) — can re-substitute without re-parsing, just re-layout + re-draw. **This is the better choice** — it avoids the full re-parse cost and keeps the `.chtpm` template stable in memory.

### 3.2 Have managers write state files, not `.chtpm`

Each manager's `write_chtpm_projection()` becomes `write_state_file()`. Instead of generating `<sidebar><scrolllist><item label="...">`, it writes plain key=value:

```
sessions_count=3
session_0_label=General
session_0_id=sess_001
session_1_label=Code
session_1_id=sess_002
session_2_label=Music
session_2_id=sess_003
transcript_msg_0=Hello, how are you?
transcript_msg_1=I'm doing well, thanks!
active_session=sess_001
```

The manager is now a pure business-logic process. It reads input, resolves actions, writes state. No markup knowledge required.

### 3.3 Author static `.chtpm` templates

Each app gets a real, human-authored `.chtpm` that defines structure and uses `${var}` for dynamic content:

```xml
<window label="open-hai" class="">
  <module src="&.widgits/open-hai/ops/+x/khtpm_open_hai_manager.+x"/>
  <page name="main">
    <sidebar>
      <scrolllist>
        <item label="${session_0_label}" action="'...' 'select' '${session_0_id}'"/>
        <item label="${session_1_label}" action="'...' 'select' '${session_1_id}'"/>
        <item label="${session_2_label}" action="'...' 'select' '${session_2_id}'"/>
      </scrolllist>
    </sidebar>
    <panel>
      <scrolllist>
        <text label="${transcript_msg_0}"/>
        <text label="${transcript_msg_1}"/>
      </scrolllist>
      <cli_io target_id="composer" label="Send:" action="'...' 'send'"/>
    </panel>
  </page>
</window>
```

**Problem:** the template above has hardcoded session slots. For a dynamic list (unknown number of sessions), the tpmos approach uses a loop in the parser — `chtpm_parser_pal.c` has `elem_inject_loop()` / `reusable_slot()` for this. The khtpm parser needs an equivalent: a way for a template to declare "repeat this row for each entry in the state file."

This is the "content-injection genericity" problem flagged in `khtpm-generic-dispatch-design.md` §2c — "LARGE, NOT DESIGNED YET." The tpmos answer is `reusable_slot()` — a template row that gets cloned per data entry. The khtpm version needs the same concept applied to the Elem tree.

### 3.4 The renderer's tick loop changes

Current:
```
check .chtpm mtime → re-parse entire file → re-layout → re-draw
```

New:
```
check gui_state.txt mtime → re-substitute ${var} in Elem tree → re-layout → re-draw
```

No full re-parse. The `.chtpm` template is parsed once at startup. On each tick, only the `${var}` values change, and the layout + draw passes re-run with fresh data. This is cheaper than the current full-re-parse approach.

The `<module>` tag is still only checked at initial parse (not on reparse) — the manager's lifetime is tied to the renderer's, same as today.

---

## 4. What this fixes

| Issue | Current (broken) | Fixed |
|---|---|---|
| Can human write `.chtpm`? | No — manager overwrites it | Yes — it's a static template |
| Separation of concerns | Manager generates markup | Manager writes state, template defines layout |
| Manager knows rendering? | Yes — must generate `<sidebar>` etc. | No — pure business logic |
| `.chtpm` stable contract? | No — transient artifact | Yes — versionable, reviewable |
| Like HTML + JS? | No — like React virtual DOM | Yes — template + behavior |

---

## 5. Scope and risk

- **Parser-level change** to `khtpm_core_render.c` — the shared renderer every khtpm window uses
- Must be **backward-compatible** — existing apps that don't use `${var}` must still work (no substitution = no change in behavior)
- Must be **tested against real production data** — open-hai's 49+ sessions, real transcripts, real model responses
- Should be done **before media-suite migration** — so the toys get the right architecture from the start
- The `reusable_slot()` / dynamic list injection (§3.3) is the hardest part — the tpmos `elem_inject_loop()` is the reference, but it needs to work on the khtpm Elem tree

---

## 6. Reference files

| File | Why |
|---|---|
| `101.ledger-player-npc-simple+3/system/chtpm_parser.c` | Original `substitute_vars()` / `load_vars()` / `get_var()` — the reference implementation |
| `101.ledger-player-npc-simple+3/system/game_manager.c` | Canonical manager — writes `gui_state.txt`, pulses `frame_changed.txt`, no markup generation |
| `101.ledger-player-npc-simple+3/pieces/chtpm/layouts/lpns_word_menu.chtpm` | Example static `.chtpm` with `${var}` placeholders |
| `#.haiku+/tpmos-re-dox/fo-menu-sys.md` | "Thin Theater / Manager Projection" — the canonical description of the intended pattern |
| `&.widgits/open-hai/open-hai.chtpm.bootstrap` | Current bootstrap (shows the inverted pattern — manager overwrites this) |
| `&.widgits/open-hai/ops/khtpm_open_hai_manager.c` | Current manager with `write_chtpm_projection()` — the code that needs to become `write_state_file()` |
| `khtpm-generic-dispatch-design.md` §2c | Acknowledges this is "LARGE, NOT DESIGNED YET" — the gap this doc fills |
| `CENTROID_GOLD_STD.md` | The rendering architecture standard — needs updating once this lands |

---

## 7. Migration order

1. **Build `substitute_vars()` for khtpm** — parser extension, generic, no app changes yet
2. **Build `reusable_slot()` / dynamic list injection for khtpm** — the hardest piece, needs its own design
3. **Pilot on open-hai** — convert `write_chtpm_projection()` → `write_state_file()`, author real `open-hai.chtpm` template, verify against production data
4. **Migrate media-suite toys** — they get the right architecture from the start
5. **Migrate remaining khtpm apps** — co-lab-hai, stats-hq, bookmarks, palettes, etc.
6. **Update `CENTROID_GOLD_STD.md`** — document the restored layout/data separation as the standard

---

## 8. Implementation status (2026-09-03, branch `chtpm-var-substitution`)

### Done - renderer primitives (`khtpm_core_render.c`)

| primitive | commit | notes |
|---|---|---|
| `${var}` substitution + `vars="<path>"` state file | `3eb73450` | ported from tpmos `chtpm_parser.c`; `\$ \{ \\` escapes, `\n`->newline, unknown var -> empty. Skipped entirely if the template has no `${` and no `<repeat>`. |
| `${HOUSE}` / `${PKG}` built-ins | `e3025d8b` | `${PKG}` = the `.chtpm`'s own dir (what the renderer passes as `$1` to every action). |
| `show="${x}"` element gating | `e3025d8b` | value `""` / `"0"` / `"false"` -> element dropped from the tree at parse time. No `show=` -> always shown. |
| headless dump: `--dump-and-exit` for every mode | `e3025d8b` | writes `/tmp/entity-menu-frame.png` + `.receipt.txt` + `.frame.txt` (ASCII Elem-tree = the tpmos "current_frame.txt" check). Wrapper: `&.widgits/_shared-lib/ops/khtpm_png_dump.sh`. |
| `<repeat count="${n}" bind="x">` dynamic lists | `3846660c` | runs before `${var}`: emits body `count` times, `${x.field}` -> `${x_<i>_field}`, `${x.#}` -> index. `count` is a bare int or one `${var}`. Cap `KH_REPEAT_MAX` 4096. No nesting (v1). |
| relative `vars=` resolves against `${PKG}` | `7f165ad2` | so a per-instance template copy (open-hai `--data-root`) finds its own state file. |
| `<!-- -->` skipped in both passes | `a255846f` | a `${...}` or `<repeat>` inside the template's own doc comment was being processed. |

### Done - apps converted (manager writes `state/ui.txt`, static template does layout)

| app | commit | lists as `<repeat>` | gates as `show=` |
|---|---|---|---|
| signup-hq | `1e088661` | - | hint / id-line / field / err / rules / button per stage |
| open-hai | `a255846f` | session list, transcript tail | tool-approval gate |
| co-lab-hai | `f4ace065` | participants, sessions, conversation | Pending count + PENDING text + Approve/Reject row |

All three verified headless (`khtpm_png_dump.sh`) - `${var}` filled, `<repeat>` rows
correct, `show=` gates drop, `&` / `<` / emoji / apostrophes round-trip through
`decode_entities()`.

### Not done

- **network_browser** - the page-content area is a heterogeneous list (TITLE / TEXT /
  LINK / IMG / VIDEO), with consecutive IMG/VIDEO runs wrapped in
  `<row class="sprite-grid-row">` and per-row `sprite=` / conditional `action=`.
  `<repeat>` v1 clones ONE fixed body - it can't select an element per row-kind or
  wrap a sub-run. **Needs `<repeat>` v2**: either a per-item element chosen by a
  `kind` field, or nested repeats. This app is the concrete motivation for v2.
  (The bookmarks/history sidebar lists are plain and would convert fine on their own.)
- **chat-hai** - projector is `chat_hai_projector.sh` (bash), not a C manager. Same
  `write_projection -> write_state` split, just in shell. Straightforward, not yet done.
- **Phase 2 - the C-dispatch apps** (db-hq / events-hq / palettes / swatch-picker /
  bookmarks): these build their Elem tree entirely in renderer C and have no
  author-facing `.chtpm` at all. Converting them is a separate project with its own
  design pass - NOT folded into this work.
- `CENTROID_GOLD_STD.md` update - after network_browser + chat-hai land.

### `<repeat>` v2 - sketch (for network_browser)

Option A (per-kind element, no nesting):
```xml
<repeat count="${content_count}" bind="c">
  <text  class="page-title" label="${c.text}" show="${c.is_title}"/>
  <text                    label="${c.text}" show="${c.is_text}"/>
  <item  label="${c.text}" action="${c.action}" show="${c.is_link}"/>
  <item  label="${c.label}" sprite="${c.sprite}" action="${c.action}" show="${c.is_media}"/>
</repeat>
```
The manager sets exactly one `c_<i>_is_*` to 1. Loses the sprite-grid-row wrapping
(all media become individual items) - acceptable first cut, or add a
`<repeat wrap-class="sprite-grid-row" wrap-when="${c.is_media}">` later.
