# 🍱 Chapter 18: Dynamic Trait Menus (PDL-Driven UI)
TPMOS applications often need to adapt their UI based on the active Piece. We use the **Dynamic Trait Menu** system to turn Piece methods into interactive buttons automatically. 🧬🍱

---

## 1. Core Philosophy: The Thin Brain
The Dynamic Menu system follows the **Thin Brain** philosophy. The UI (CHTPM) is a hollow shell that reflects the state and capabilities (Methods) of the active Piece. Capabilities are defined in the Piece's DNA (`piece.pdl`) and are dynamically rendered as interactive buttons.

---

## 2. Architecture Overview

### A. The Layout (`.chtpm`)
The layout simply provides a slot for the methods using the `${piece_methods}` variable.
```xml
<text label="║  " />${piece_methods}<text label=" ║" /><br/>
```

### B. The Parser (`chtpm_parser.c`)
The parser is responsible for:
1.  **Resolution**: Finding the PDL for the `active_target_id`.
2.  **Extraction**: Parsing `METHOD` lines (skipping internal ones like `move`, `select`).
3.  **Generation**: Creating `<button>` elements with `onClick="KEY:n"` (indexing usually starts at 2).
4.  **Injection**: Mapping button clicks to ASCII key injections (e.g., `KEY:2` -> `50` in `history.txt`).

### C. The PDL (`piece.pdl`)
Methods are defined as `SECTION | KEY | VALUE` where `VALUE` is the command/Op to execute.
```pdl
METHOD | scan    | pieces/apps/playrm/ops/+x/scan_op.+x xlector
METHOD | feed    | projects/fuzz-op/ops/+x/feed_op.+x xlector
```

### D. The Manager (`_manager.c`)
The manager acts as the dispatcher. It reads the history file, identifies the method index, resolves the method name/handler via the PDL, and executes it.

---

## 3. The Lifecycle of an Action

1.  **Render**: Parser reads `active_target_id` (e.g., `fuzzball`). It loads `fuzzball/piece.pdl`, finds methods `feed`, `play`, `sleep`. It renders them as buttons `[2] feed`, `[3] play`, `[4] sleep`.
2.  **Input**: User presses `2` or clicks the `feed` button.
3.  **Injection**: Parser calls `send_command("KEY:2")` which writes ASCII `50` to `pieces/apps/player_app/history.txt`.
4.  **Dispatch**: Manager reads `50`, subtracts `'0'` to get index `2`. 
5.  **Resolution**: Manager calls `pdl_reader.+x fuzzball list_methods`. It takes the 2nd item (e.g., `feed`).
6.  **Execution**: Manager calls `pdl_reader.+x fuzzball get_method feed` to get the handler Op. It then calls `system()` or `fork()/exec()` on the handler.
7.  **Sync**: Manager updates state, calls `trigger_render()`, and pulses `state_changed.txt`.

---

## 4. Implementation Guide for Agents

### Step 1: Define PDL Methods
Ensure your piece has a `piece.pdl` with `METHOD` entries.
- Use `void` for internal methods.
- Use full paths for Ops.

### Step 2: Set Active Target
In your manager, ensure `active_target_id` is set to the piece you want to control in the state file.

### Step 3: Implement Dynamic Dispatcher
Copy the dispatch pattern from `fuzz-op_manager.c`:
1.  Detect keys in range (2-9).
2.  Use `pdl_reader.+x` to find the method name at that index.
3.  Execute the handler string returned by `pdl_reader.+x`.

---

## 5. Critical Pitfalls

| Pitfall | Symptom | Fix |
|:---|:---|:---|
| **ASCII Injection** | Parser injecting raw `2` instead of `'2'`. | Use `inject_raw_key('0' + k)` in parser. |
| **Stale Methods** | UI doesn't refresh on target change. | Manager MUST pulse `state_changed.txt` after update. |
| **Button vs Text** | Focus navigator doesn't work. | Use `<button>` tags, never hardcode `[>]`. |

---

## 🏛️ Scholar's Corner: The "Ghost in the Menu"
Before we standardized method indexing, some projects started at `1` and others at `2`. This caused a "Ghost in the Menu" bug where clicking `[1] Map` would actually trigger the `[2] scan` operation. We solved this by reserving Index 1 for the core navigation (Map/Grid) and starting all Dynamic Trait Menus at **Index 2**. 👻🍱

---

## 📝 Study Questions
1.  What is the "Thin Brain" philosophy in the context of UI?
2.  Why do we start dynamic method indexing at 2 instead of 1?
3.  Explain the role of `pdl_reader.+x` in the action lifecycle.
4.  **True or False:** The CHTPM parser executes the Op handler directly when a button is clicked.
