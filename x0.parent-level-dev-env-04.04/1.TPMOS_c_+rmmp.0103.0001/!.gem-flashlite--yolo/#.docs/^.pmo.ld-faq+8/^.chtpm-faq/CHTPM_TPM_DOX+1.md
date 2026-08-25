[2026-03-22 ACTIVE CHTPM/TPM REFERENCE]
**Purpose:** Quick reference cheat sheet (5-minute read before coding)
**Full Guide:** See `^.chtpmodop.txt` for complete implementation details

# CHTPM + TPM Reference (Concise)

## Design intent
The stack must remain modular and recursively composable so agents can modify one part while other parts keep running.
If behavior is hardcoded across layers, this design intent is broken.

## Layer contract
1. CHTPM (Theater/View)
- Layout composition
- Variable substitution
- Input routing
- No app-specific business ownership
- CHTPM support modules (player/orchestrator/render helpers) must stay infrastructure-level.
- They should not hardcode project-specific gameplay assumptions (piece IDs, app rules, world/map semantics).

2. Module (Orchestration)
- Reads routed input
- Calls ops/plugins
- Synchronizes piece state
- Triggers render/update artifacts
- Must prefer data-driven dispatch and shared contracts over hardcoded one-off logic.

3. Piece (Atomic state unit)
- File-backed state and behavior descriptors
- Container-capable (piece may contain child pieces)

## Canonical storage model
`projects/<project>/pieces/world_<id>/map_<id>/<piece_id>/`

Rules:
- world/map-first lookup
- temporary fallback compatibility for migration
- no mandatory `/pieces/` under map directories

## Runtime pipeline (simplified)
1. Input captured
2. Parser routes input to app history
3. Module handles key/event
4. Ops/plugins update piece state
5. Render op produces map/stage view artifact
6. Parser composes final frame

## Preferred UI Patterns

### Form-Based Input (RECOMMENDED for login/settings)

Instead of dynamic submenus that change screens, use **always-visible form fields**:

```xml
<!-- Layout: Always-visible fields with cli_io -->
<text label="Username: " /><cli_io id="username" label="${username_input}" /><br/>
<text label="Password: " /><cli_io id="password" label="${password_input}" /><br/>
<button label="Login" onClick="login" />
```

```c
// Module: State variables persist across renders
static char input_username[MAX_USERNAME] = "";
static char input_password[MAX_PASSWORD] = "";

// State writing (always update form fields)
fprintf(f, "username_input=%s\n", input_username);
fprintf(f, "password_input=%s\n", masked_password);

// Method handlers (no screen switching)
static void method_login(void) {
    // Process input_username and input_password
}

// ESC just deactivates - NEVER clears
if (key == 27) { return; }
```

**Benefits:**
- Fields never clear unless explicitly cleared
- ESC preserves all input
- Users see all fields at once (intuitive)
- Simple state management (no `auth_screen` switching)

### Navigation & Tree Views (Standardized)

For lists and hierarchical directory views, use standardized selection and expansion markers to maintain ecosystem consistency.

**Markers:**
- `[>]` : Currently selected item.
- `[ ]` : Unselected item.
- `[+]` : Collapsed directory or expandable node.
- `[-]` : Expanded directory or collapsible node.

**Pattern:**
```xml
<!-- Layout -->
<text label="${directory_listing}" />
```

**Implementation logic:**
```c
// Format: ║ [marker] [index]. [indent] [expansion] [name]
// Example: ║ [>] 1. [+] [Music]
// Example: ║ [ ] 2.      song.mp3

const char *marker = (i == selected_index) ? "[>]" : "[ ]";
const char *exp_marker = is_dir ? (is_expanded ? "[-] " : "[+] ") : "    ";
snprintf(line, sizeof(line), "║ %s %d. %s%s%s\\n", 
         marker, i + 1, indentation, exp_marker, item_name);
```

**UX Rules:**
- **Selection:** Arrow keys move the `[>]` marker.
- **Expansion:** ENTER toggles `[+]`/`[-]` for directories.
- **Execution:** ENTER on a leaf node (file) triggers its primary action.
- **Hierarchy:** Use consistent indentation (2-4 spaces) to indicate depth.

### Row-Break And Utility-Strip Contract

For dynamic text surfaces, row intent is part of the UI contract.

**Rules:**
- `<br/>` is the explicit row break.
- Multiple controls may intentionally share one visual row.
- Dynamic selectable items should remain one item per row unless a grouped utility strip is explicitly intended.
- Host/project bridges must not width-truncate markup rows before parse/render, or same-row control groups can collapse or corrupt.

**Thumb-scroll standard:**
- For long lists, prefer a compact utility strip above the main items:
  - `[^_UP]`
  - `[v_DOWN]`
  - `Thumb:[#-------]start-end/total`
- Keep the scroll controls and thumb indicator on the same row.
- Keep the actual content items on their own rows below the utility strip.
- Use the same row contract across ASCII mirrors, CHTPM layouts, and Wraith scene bridges so frame-debug output matches the live surface closely.

### Dynamic Submenus (ONLY when truly needed)

Use `${piece_methods}` with PDL method dispatch when:
- Options change based on context (e.g., pet actions vs selector actions)
- Screen space is limited
- Different entities have different method sets

**Pattern:**
```xml
<text label="║  " />${piece_methods}<text label=" ║" /><br/>
```

```c
// Module: Dispatch by method name, not key number
static MethodMapping methods[] = {
    {2, "feed"}, {3, "play"}, {4, "sleep"}, {0, NULL}
};
dispatch_method(get_method_name(key));
```

### Activation Submenus (Standardized for sub-lists)

Use `<button label="Menu" onClick="ACTIVATE" />` for hierarchical navigation within a single layout.

**UX Rules:**
- **Activation:** Pressing ENTER on an `ACTIVATE` menu makes its children visible and focuses the first child.
- **Indentation:** Submenus are automatically indented (4 spaces per level) when active.
- **Numbering:** The active root retains its global index; children start a new `1., 2., ...` count.
- **Navigation:** Only the scoped children (and the root anchor) are jumpable while active.
- **Back/Exit:** Pressing ESC or a button with `onClick="BACK"` traverses up the menu hierarchy or exits to global mode, preserving focus on the menu root.

**Pattern:**
```xml
<button label="Settings" onClick="ACTIVATE" id="settings">
    <button label="[Back]" onClick="BACK" />
    <button label="Audio" onClick="ACTIVATE" id="audio">
        <button label="[Back]" onClick="BACK" />
        <button label="Volume: ${vol}" onClick="toggle_vol" />
    </button>
</button>
```

### Navigation Pattern Comparison: Folding vs. Activation

| Pattern | Mechanism | Best For | UX Behavior |
| :--- | :--- | :--- | :--- |
| **Folding** | `[+]`/`[-]` in label. | **Directory Traversal**, deep trees, file explorers. | Persistent state. Parent and children stay in the global list. Global jumping (1-99) remains active. |
| **Activation** | `onClick="ACTIVATE"`. | **Scoped Menus**, context actions, settings. | Temporary focus scope. Children hidden until active. Numbering restarts (1., 2.) for faster keyboard access. |
| **Switching** | `href="layout.chtpm"`. | **Major Mode Changes**, isolated screens (Shop, Map). | Full context swap. Clears previous navigation state. Best for distinct functional silos. |

**Rule of Thumb:** Use **Folding** if the user needs to see the parent while browsing children (like a file tree). Use **Activation** if the user should "dive into" a task and ignore the rest of the screen.

### Guidance: Submenus vs. Layout Switching

| Approach | When to use | Pros | Cons |
| :--- | :--- | :--- | :--- |
| **Activation Submenus** | Small sub-lists, localized settings, tree views. | Stays in context, fast, no layout reload. | Limited screen space, complex hierarchy can get cluttered. |
| **Layout Switching (`href`)** | major screen changes, completely different modes (e.g., Shop vs. World). | Full screen for content, clean state separation. | Reload overhead, loses previous scroll/focus context (unless managed). |

**Recommendation:** Prefer `href` to a new layout + a `[Back]` button (also using `href` to return) for major functional areas. Use `ACTIVATE` submenus for localized tree structures or nested options within a persistent screen.

---

## Current roadmap alignment
- Step 1 emphasis: op-ed save/load/local-play + no-code event flow.
- Later phases: AI, pet expansion, voice, simulation, network, gl-os, install/versioning, kernel.

## Implementation caution
If old docs conflict with this file, prefer this file and Mar 18 roadmap/monolith docs.
Also prefer modular composition over hardcoded shortcuts even if shortcuts appear faster in the moment.
Special caution: avoid copying legacy hardcoded path snippets from old CHTPM support code into new module logic.
Do not resolve missing metadata (`entry_layout`, `player_piece`) with static guesses when scan/registry derivation is available.
