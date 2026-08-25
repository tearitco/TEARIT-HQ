# khtpm strip parser — locked scope, 2026-08-11

This document is the concrete implementation scope for the ACTUAL missing piece of the strip refactor: a real declarative-layout parser, matching CHTPM's real architecture, not the process-split-only plumbing already built. See `khtpm-refactor-plan.md`'s "DECISION MADE" section and `!.aug-11-refactor-finish.md`'s top section for why this document exists — direct instruction: *"scope the real parser and document its necessitations."*

**Do not start implementation from this document alone without re-reading it once more immediately before writing code.** It is written to be precise enough to implement against directly, but the whole reason this document exists is that a prior pass skipped exactly this step and built plumbing instead of the point.

## What survives from today's work (do not rebuild these)

- `khtpm_taskbar_manager.c`/`.h` — the manager, unchanged. All `livedesk_*` business logic and the `ktb_*` API. This is what the parser's `<button onClick="...">` rows will ultimately call into (via the same relay mechanism already built).
- `khtpm_taskbar_manager_main.c` — the manager DRIVER binary (owns `KtbState`, polls `strip_history.txt`, writes `strip_state.txt`/`strip_frame_changed.txt`). Survives largely as-is, though its `serialize_state()` needs extending (see "Manager-side necessitations" below) to emit the markup fragments the layout format needs, alongside its existing scalar rows.
- The file-relay protocol shape: `strip_history.txt` (bare-decimal keycodes in), `strip_state.txt` (pipe-delimited state out), `strip_frame_changed.txt` (dirty-signal touch file). Unchanged.
- The fork/exec lifecycle (parser launches manager, `waitpid(WNOHANG)`, lazy restart). Unchanged.

## What gets replaced (do not keep patching)

- `khtpm_strip_parser.c`'s current rendering: `HQ_HEADER_LABELS[]` (hardcoded array), `draw_header_win()`/`draw_popup_win()`'s direct `XDrawString`/`XDrawLine`/`XMoveResizeWindow` calls, `g_header_x0[]`/`g_header_x1[]` manual hit-testing, `compute_header_width()`/`header_cell_width()`. All of this becomes the OUTPUT of a layout-driven render walk, not hand-written per-cell drawing code.
- The bare-decimal `KSC_HQ_HEADER_BASE`/`KSC_HQ_ITEM_BASE` code-range dispatch in `khtpm_strip_codes.h`/`dispatch_code()` — replaced by `onClick` attribute strings resolved through the tag tree, matching real CHTPM's `send_command()` model.

## Locked tag vocabulary (minimal, not CHTPM's full set)

Modeled directly on `chtpm_parser.c`'s real tags (confirmed by reading the actual source, not assumed), reduced to exactly what a taskbar strip needs — most of CHTPM's vocabulary (`<scroller>`, `<canvas>`, `<module>` used for game panels, `<link>`) is irrelevant here and explicitly OUT of scope:

| Tag | Purpose | Attributes |
|---|---|---|
| `<panel>` | Root wrapper, one per layout file | none required |
| `<text>` | Static or `${var}`-interpolated label, non-interactive | `label` |
| `<button>` | Interactive, clickable/focusable row or cell | `label`, `onClick` |
| `<row>` | Layout grouping only (no visual meaning of its own beyond containing children) — used to group a submenu's rows or the header's cells | none required |
| `<cli_io>` | Text-input field (save-as / rename-desk) | `target_id` (which cli-io op), `label` |

That's it — 5 tags. Real CHTPM has ~10-12; the strip doesn't need `<scroller>`, `<canvas>`, `<module>`, `<link>`, `<br/>` (use newline-in-`${var}` instead), or CHTPM's `visibility`/`time_reactive` panel attributes (the strip's popup-vs-header visibility is handled by the `ACTIVATE` scope mechanism below, not per-element visibility expressions — simpler, and matches how the strip's popup/header split already works structurally).

## `ACTIVATE` scope mechanism (ported from real CHTPM, not invented)

Confirmed via direct read of `chtpm_parser.c` (`is_navigable()`, `is_descendant()`, `active_index`/`focus_index` handling, ~lines 1750-1770): a `<button onClick="ACTIVATE">` toggles into a submenu scope. While `active_index` points at it, only its own descendants are `is_navigable()` (focusable/clickable) and — critically for the strip — this is exactly the mechanism that already governs "is a submenu open." Port directly: `active_index`/`focus_index` are real, PARSER-OWNED runtime ints (not manager state, not embedded in any `${var}`) — same as CHTPM's actual source. Escape/close pops `active_index` back to its parent (`elements[active_index].parent_index`), matching CHTPM's real `BACK` handling.

This maps the strip's existing three states directly: `active_index == -1` → plain header, focus among header cells. `active_index == <header cell>` → that cell's row-children are shown/navigable (the current `popup_win` content). `cli_io` is a special-cased leaf, not a scope (matches CHTPM's own `is_interactive()` treating `cli_io` as directly interactive, not a container).

## Cursor/focus rendering: PARSER-OWNED, not manager-baked — a real design correction from today's hardcoded version

Confirmed via direct read: real CHTPM's `render_element()` computes the `[>]`/`[^]`/`[ ]` cursor prefix itself, by comparing the element's own index against `focus_index`/`active_index` — these are runtime ints the PARSER tracks, not something baked into a label string by the manager. **Today's hardcoded `khtpm_strip_parser.c` did it the other way** — the manager formatted `"[>] 1. HQ"` as one complete string and the parser just drew it verbatim. That was a real design shortcut, not a deliberate choice, and should NOT carry into the real parser: it couples rendering concerns (cursor position) into the manager's business-logic layer for no reason, and diverges from the actual reference architecture being matched.

**For the real parser**: `<button label="...">`'s `label` attribute is the PLAIN text only (no cursor prefix). The parser's own render walk prepends `[>]`/`[^]`/`[ ]` based on its own `focus_index`/`active_index`, exactly like real CHTPM. The manager does NOT need to know about cursor state for rendering purposes at all — though it still needs `strip_focus_cell`/`tab_focus_idx`/`hq_focus` published in `strip_state.txt` for the parser to seed its own `focus_index` from (see "Manager-side necessitations").

## Variable-length lists (tabs, shortcuts, submenu rows): manager pre-renders markup fragments

Confirmed via direct read: real CHTPM has NO foreach/repeat tag. The manager side builds a markup-fragment STRING (e.g. `<button label="tab1" onClick="TAB:0"/><button label="tab2" onClick="TAB:1"/>...`) and the layout references it as one `${var}`. Port this exactly — do not invent a loop construct the reference doesn't have.

**Manager-side necessitations** (extends `khtpm_taskbar_manager_main.c`'s `serialize_state()`/`publish_state()`, or a new sibling function): `strip_state.txt` needs new rows (alongside its existing `TAB`/`SHORTCUT`/`KEY` rows, which can stay for compatibility/debugging or be removed once the parser no longer needs raw rows — decide at implementation time) carrying pre-rendered markup fragments:
- `VAR | strip_tabs | <markup>` — one `<button label="N. entity" onClick="TAB:i"/>` per tab
- `VAR | strip_shortcuts | <markup>` — one per shortcut
- `VAR | strip_hq_items | <markup>` — one per currently-open submenu's rows (only populated while a submenu is `ACTIVATE`d)
- Scalar `VAR` rows for anything else `${...}`-interpolated directly in the static layout file (theme colors if the layout wants them as text, though color is more likely a parser-side rendering concern read from `livedesk_theme.pdl` directly, not something that needs to flow through the layout `${var}` system — decide at implementation time, lean toward "parser reads its own rendering config directly," matching how `load_theme_opacity()` already works today).

Multi-line values in a pipe-delimited `.pdl`-style row are awkward (embedded newlines break the row format). Two real options, pick one before implementing: (a) escape newlines within the `VAR` row's value (e.g. `\n` literal, unescaped by the parser after reading), or (b) write each `VAR`'s markup fragment to its OWN small file (`strip_state_tabs.txt` etc.) instead of a row in the shared file. Option (b) is simpler to implement correctly and matches the house's general preference for one-concern-per-file; lean toward it unless a concrete reason favors (a).

## The layout file itself

A NEW static file, e.g. `*.monads/*.livedesk-taskbar/khtpm_strip.chtpm` (or `.pdl` extension if `.chtpm` feels presumptuous outside the real CHTPM tree — house convention question, not a technical one, ask if unsure). Hand-authored, checked into the repo like any other config — this is the actual "users and devs create layouts" deliverable the whole refactor was for. Rough shape (illustrative, not final — write the real one once the tag vocabulary above is locked and reviewed):

```
<panel>
  <row>
    <button label="HQ" onClick="ACTIVATE"/>
    <row>${strip_hq_items}</row>
  </row>
  <button label="USER" onClick="strip:user"/>
  <button label="file" onClick="ACTIVATE"/>
  ... (remaining 9 header cells, same shape)
</panel>
```

(Bottom bar / tabs likely wants its OWN separate `<panel>` in a second file, or a second root element — the current architecture already has two persistent windows, `win` and `hq_win`; whether the layout format should describe both windows in one file or two is an open call, lean toward two files matching the two-window reality, unless a strong reason emerges to unify them.)

## Implementation plan — files and rough order

1. **Tokenizer** (`khtpm_strip_tokenize.c` or a function block in the parser): walk the file, emit tag-open/tag-close/text tokens. Model directly on `chtpm_parser.c`'s own `tokenize()` — read that function in full before writing this, don't reinvent the wheel or the edge-case handling (attribute quoting, self-closing tags) it already solved.
2. **Attribute parser**: `name="value"` pairs within a tag, generic (not per-tag-hardcoded) — model on `chtpm_parser.c`'s `parse_attributes()`.
3. **Tag tree builder**: tokens → `UIElement` tree with `parent_index`, matching CHTPM's own flat-array-with-parent-pointers approach (not a pointer-based tree) — simpler to reason about for focus/`is_descendant()` walks, and matches the reference exactly.
4. **Variable substitution**: `${var}` → looked-up value from the manager's published `VAR` rows/files. Model on `substitute_vars()`.
5. **`is_navigable()`/`is_descendant()`/`ACTIVATE` scope handling**: port directly from the real functions of the same name.
6. **Render walk**: replaces `draw_header_win()`/`draw_popup_win()` — walks the visible (per `is_navigable`/ACTIVATE-scope) elements, computes `[>]`/`[^]`/`[ ]` prefix from `focus_index`/`active_index`, draws via Xlib (the ACTUAL drawing primitives — `XDrawString` etc. — are fine to keep, it's the per-cell hardcoding around them that goes away).
7. **Click/key dispatch walk**: hit-test against the render walk's own computed positions (single source of truth — this was a real, separately-fixed bug in today's hardcoded version, don't reintroduce draw/hit-test drift in the new one either), resolve to an element, read its `onClick`, dispatch via `send_command()`-equivalent (either `KEY:n` → append to `strip_history.txt` for the manager, or a local `ACTIVATE`/`BACK` scope change handled entirely in the parser with no manager round-trip at all).

## Necessitations — must be true/decided before implementation starts

- [ ] Exact tag vocabulary above reviewed and confirmed (or amended) — don't start the tokenizer against a vocabulary that might still change.
- [ ] Multi-line `VAR` value strategy decided (embedded-file vs. escaped-newline row) — affects both the manager's serializer and the parser's reader.
- [ ] One-file-vs-two-file layout question decided (single `<panel>` covering both windows, or one file per window).
- [ ] Confirm whether `${var}` scalar values (theme colors etc.) flow through the layout system at all, or stay a parser-side direct config read (leaning toward the latter, per above).
- [ ] Read `chtpm_parser.c`'s `tokenize()`, `parse_attributes()`, `substitute_vars()`, `is_navigable()`, `is_descendant()`, `render_element()`, `send_command()` in FULL (not re-summarized from this doc or from memory of today's earlier research) immediately before implementing the corresponding piece — this doc describes what they do, not their exact code, and the whole lesson of this session is that "read the full function before porting" is the only method that actually worked.

---
*End khtpm-strip-parser-SCOPE.md*
