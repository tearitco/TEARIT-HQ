# Fuzz-Op Dynamic Menu System (Trait Menus)

This document provides a technical guide for implementing dynamic, PDL-driven menus in TPMOS projects, modeled after the `fuzz-op` pet simulation.

## 1. Core Philosophy
The Fuzz-Op menu system follows the **Thin Theater / Manager Projection** philosophy. The UI layout is a hollow shell. The manager reads sovereign Piece state, resolves capabilities from Piece DNA (`piece.pdl`) through Ops, and publishes a flat projection to `projects/fuzz-op/manager/gui_state.txt`.

CHTPM renders that projection. It substitutes variables and injects keys; it does not recursively discover Piece state or synthesize project methods.

## 2. Architecture Overview

### A. The Layout (`.chtpm`)
The layout simply provides a slot for the methods using the `${piece_methods}` variable.
```xml
<text label="║  " />${piece_methods}<text label=" ║" /><br/>
```

### B. The Parser (`chtpm_parser.c`)
The parser is responsible for:
1.  **Projection Load**: Reading the manager-published `gui_state.txt`.
2.  **Substitution**: Replacing layout variables such as `${pet_name}` and `${piece_methods}`.
3.  **Navigation**: Rendering focus markers for `<button>` elements.
4.  **Injection**: Mapping button clicks to ASCII key injections (e.g., `KEY:2` -> `50` in `history.txt`).

The parser is not responsible for finding active Piece state, parsing project PDL methods, or deciding which Piece data belongs in the UI panel.

### C. The PDL (`piece.pdl`)
Methods are defined as `SECTION | KEY | VALUE` where `VALUE` is the command/Op to execute.
```pdl
METHOD | scan    | pieces/apps/playrm/ops/+x/scan_op.+x xlector
METHOD | feed    | projects/fuzz-op/ops/+x/feed_op.+x xlector
```

### D. The Manager (`_manager.c`)
The manager acts as the projector and dispatcher. It reads Piece state, chooses the display-facing values, asks Ops such as `get_piece_methods_op.+x` to build method markup, writes `gui_state.txt`, reads the history file, resolves method indices via PDL, and executes handlers.

## 3. The Lifecycle of an Action

1.  **Projection**: Manager reads active context and Piece state. It calls a method Op to turn the active Piece PDL into `${piece_methods}` markup, then writes `gui_state.txt`.
2.  **Render**: Parser reads `gui_state.txt`, substitutes `${piece_methods}`, and renders buttons such as `[2] feed`, `[3] play`, `[4] sleep`.
3.  **Input**: User presses `2` or clicks the `feed` button.
4.  **Injection**: Parser calls `send_command("KEY:2")` which writes ASCII `50` to `pieces/apps/player_app/history.txt`.
5.  **Dispatch**: Manager reads `50`, subtracts `'0'` to get index `2`.
6.  **Resolution**: Manager calls `pdl_reader.+x fuzzball list_methods`. It takes the 2nd item (e.g., `feed`).
7.  **Execution**: Manager calls `pdl_reader.+x fuzzball get_method feed` to get the handler Op. It then executes the handler.
8.  **Sync**: Manager updates state, refreshes `gui_state.txt`, calls `trigger_render()`, and pulses the relevant marker file.

## 4. Implementation Guide for Agents

### Step 1: Define PDL Methods
Ensure your piece has a `piece.pdl` with `METHOD` entries.
- Use `void` for internal methods.
- Use full paths for Ops.

### Step 2: Set Active Target
In your manager, ensure `active_target_id` is set to the piece you want to control.
```c
fprintf(state_file, "active_target_id=%s\n", target_id);
```

### Step 3: Publish the UI Projection
In your manager, write the layout-facing values to `projects/<id>/manager/gui_state.txt`.
```c
fprintf(gui, "active_target=%s\n", active_target_id);
fprintf(gui, "piece_methods=%s\n", methods_markup);
fprintf(gui, "pet_name=%s\n", display_name);
```

Use manager-side helpers or Ops for recursive project lookup. Do not add project-specific recursive lookup to `chtpm_parser.c`.

### Step 4: Implement Dynamic Dispatcher
Copy the dispatch pattern from `fuzz-op_manager.c`:
1.  Detect keys in range (2-9).
2.  Use `pdl_reader.+x` to find the method name at that index.
3.  Execute the handler string returned by `pdl_reader.+x`.

## 5. Critical Pitfalls (2026 Edition)

| Pitfall ID | Name | Description | Fix |
|:---|:---|:---|:---|
| **#80** | **ASCII Injection** | Parser injecting raw `2` instead of `'2'` (50). | Ensure `inject_raw_key('0' + k)` is used in `send_command`. |
| **#93** | **Stale Methods** | Methods don't refresh when `active_target_id` changes. | Manager MUST pulse `state_changed.txt` after updating target. |
| **#103** | **Parser Sovereignty Drift** | Parser starts resolving project Piece state or generating project methods. | Keep that logic in the manager or a manager-called Op; parser only substitutes `gui_state.txt`. |
| **#31** | **Project ID Drift** | App layouts loaded via `href` may have stale `project_id`. | Explicitly set `project_id` when launching app or loading layout. |
| **#82** | **Triple Render** | Calling `compose_frame()` multiple times per keypress. | Write to `frame_changed.txt` marker instead of setting `dirty=1`. |
| **#26** | **Button vs Text** | Writing `[2] Feed` as text instead of `<button>`. | Use `<button label="Feed" onClick="KEY:2" />`. |
| **#N/A** | **Index Desync** | Manager and Parser starting at different indices. | Standardize: Methods start at index 2 (1 is reserved for Map/Main). |
| **#2d** | **Path Drift** | Hardcoding `pdl_reader` paths. | Always resolve `project_root` from `location_kvp`. |

## 6. Pro-Tip: Response Conjugation
For better UX, the manager should conjugate method names in the `last_response` field.
- `feed` -> `fed`
- `scan` -> `scanned`
- `inventory` -> `inventoried`

---
*Documented by Gemini CLI for TPMOS Future Agents - 2026-04-21*
