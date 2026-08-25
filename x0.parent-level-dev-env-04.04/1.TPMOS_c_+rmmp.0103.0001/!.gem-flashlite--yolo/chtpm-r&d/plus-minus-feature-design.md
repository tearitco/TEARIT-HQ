# CHTPM Native Expand/Collapse (+/-) Design

## Objective
Implement a native folding (expand/collapse) feature within the `chtpm` parser. This allows layouts to have hierarchical lists that can be toggled by the user without requiring a dynamic manager or module logic.

## Proposed Syntax

### 1. Label Markers
The parser will automatically detect `[+]` and `[-]` patterns within the `label` attribute of interactive elements (like `<button>`).

Example:
```xml
<button label="[-] My Directory" onClick="TOGGLE_FOLD:dir1" />
```

### 2. Grouping/Hierarchy
To know which elements to hide/show, we can use:
- **Option A: Indentation/Level attribute.** (Simple but brittle)
- **Option B: Parent/Child relationship via tags.** (Existing in `chtpm` via `<panel>` or nested tags)
- **Option C: ID-based targeting.** (Explicit but verbose)

Given the user's desire for simplicity and "native" feel, **Option B** combined with a `fold` state is preferred.

However, the user's example layout was flat:
```xml
<button label="[1][-] Dir" onClick="KEY:1" /><br/>
<button label="[2] file" onClick="KEY:2" /><br/>
<button label="[3] file" onClick="KEY:1" /><br/>
<button label="[4][+] subdir" onClick="KEY:2" /><br/>
```

In a flat layout, "folding" usually means hiding all subsequent lines until an element with the same or lower "level" is found.

### 3. Proposed Native Behavior
If a `<button>` has a label containing `[-]` or `[+]`:
1.  It is considered a "Fold Header".
2.  If clicked/entered:
    - If it contains `[-]`, it toggles to `[+]` and "folds" its content.
    - If it contains `[+]`, it toggles to `[-]` and "unfolds" its content.
3.  The "content" is determined by the next elements until another "Fold Header" of the same level (if we add levels) or just until the end of the current container.

To support levels without complex nesting, we can use a `level` attribute or detect it via leading spaces/dashes in the label.

## Implementation Details

### UIElement Changes
Add `bool folded` and `int level` to `UIElement`.

### Parsing
- Detect `[+]` or `[-]` in `label`.
- Detect level (e.g., number of leading spaces).
- Store fold state in a way that persists (e.g., `gui_state.txt`).

### Rendering
- In `render_element`, if a parent is folded, skip rendering children.
- If using flat layout with levels, `render_element` should track the current "fold depth".

### Interaction
- In `process_key`, if Enter is pressed on a Fold Header:
    - Toggle the state in `gui_state.txt`.
    - Reload/Reparse.

## Persistence
Fold states should be stored in `projects/<project_id>/manager/gui_state.txt` using a key like `fold_<id>`.
If no `id` is provided, we might need a way to generate one or use the label (less stable).

## Verification Plan
1. Update `+-demo.chtpm` to use a hierarchical structure or levels.
2. Modify `chtpm_parser.c`.
3. Recompile and run.
4. Verify that pressing Enter on `[-]` hides subsequent items and changes label to `[+]`.
