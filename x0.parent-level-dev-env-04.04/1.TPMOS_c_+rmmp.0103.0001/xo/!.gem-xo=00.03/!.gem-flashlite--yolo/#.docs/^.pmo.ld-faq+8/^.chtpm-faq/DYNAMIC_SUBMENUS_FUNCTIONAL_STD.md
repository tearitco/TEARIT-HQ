# Functional Standard: Dynamic Activation Submenus (m19)
**Date:** 2026-05-19
**Context:** CHTPM / TPMOS Architectural Standards

## 1. The Core Conflict: Manager vs. Parser
The most common mistake when implementing submenus is trying to handle "UI Logic" (arrows, selection markers, focus) inside the **Manager**. This creates a "Dual Brain" conflict where the Manager and the CHTPM Parser fight over the same keyboard indices and state.

### The Functional Solution:
- **Manager (Muscle):** Always projects the available content.
- **Parser (Brain):** Manages the focus, navigation, and visibility.

---

## 2. The Pattern: Dynamic `ACTIVATE`
Use the `<button onClick="ACTIVATE">` pattern combined with a dynamic variable for the content.

### A. Layout Structure (`.chtpm`)
Place the dynamic variable *inside* the activation button. This ensures the Parser "hides" the content until the user presses Enter on the parent.

```xml
<button label="[3] Switch API" onClick="ACTIVATE">
    <br/>
    <button label="Back" onClick="BACK" /><br/>
    ${my_project_dynamic_menu}
</button>
```

### B. Manager Projection (`gui_state.txt`)
The Manager should **always** write the markup to `gui_state.txt`. Do NOT gate the writing of this variable behind a `g_menu_mode` flag.

```c
// Correct: Markup is always ready
void update_menu_markup(void) {
    char buf[4096] = "";
    for (int i = 0; i < count; i++) {
        char line[512];
        // Use SET_ prefix for command injection
        snprintf(line, sizeof(line), "    <button label=\"%s\" onClick=\"SET_VAL:%s\" /><br/>", name, value);
        strcat(buf, line);
    }
    set_gui_var("my_project_dynamic_menu", buf);
}
```

---

## 3. Communication: String Commands
Avoid `onClick="KEY:n"` for submenus, as indices will conflict with the main menu. Use **String Commands**.

### A. Prefixes
The Parser (as of 2026-05) recognizes these prefixes for injection into `history.txt`:
- `OP:` (e.g., `OP:scan`)
- `SET_` (e.g., `SET_API:url`)
- `MP3:`

**Note:** The Parser automatically prepends `COMMAND: ` to these strings in the history file.

### B. Robust Parsing
In the Manager's input loop, use `strstr` rather than `strncmp` to find your command, as timestamps may precede the text.

```c
while (fgets(line, sizeof(line), hf)) {
    char *cmd = strstr(line, "SET_API:");
    if (cmd) {
        // Skip the "SET_API:" (8 chars) and trim
        strncpy(g_target, trim_str(cmd + 8), 255);
        state_changed = 1;
    }
}
```

---

## 4. Pitfall Summary
1. **Manual Markers:** Do NOT write `[>]` or `[ ]` in dynamic buttons. The parser does this.
2. **Gated Markup:** Always publish the submenu markup. Let the parser handle visibility via `ACTIVATE`.
3. **Prefix Mismatch:** Using `COMMAND:MY_CMD` in `onClick` fails injection. Use `SET_MY_CMD` instead.
4. **Stale Processes:** Stale manager binaries are the #1 cause of "ghost" behavior during testing.

---
*Reference: projects/groq-ollama/manager/groq-ollama_manager.c*
