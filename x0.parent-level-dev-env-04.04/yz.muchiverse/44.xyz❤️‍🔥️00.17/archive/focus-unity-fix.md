# Unified Focus Navigation Fix

## The Real Problem
Entity context menus (e.g., book-stack right-click) don't receive keyboard nav focus.

**Symptoms:**
- Taskbar popup menus work fine (toolbar has focus, arrows/digits work)
- Entity window context menus open but don't get keyboard nav
- Second context menu on an entity gets no nav focus
- Keyboard input doesn't reach entity context menu rows

## Root Cause
The taskbar's nav focus system is isolated from entity window context menus:

**Current architecture:**
```
taskbar manages:  [strip buttons][tabs][taskbar popups]
entity windows manage: [entity context menus] — SEPARATE, no nav connection

Result: Entity context menus can't receive keyboard nav
        Taskbar doesn't know entity menus are open
        No unified keyboard routing
```

The taskbar DOES hide its own "[>]" when its popups are open (fixed in Aug 9 18:01:38 build), but it has no way to give nav focus to entity window context menus.

## The Real Solution
Connect entity context menu focus to taskbar nav system:

1. Detect when an entity context menu is open (check `livedesk_nav_claims.txt` for `KIND=row` entries)
2. When entity menu is open, disable taskbar nav and route keyboard to the entity
3. Entity context menu gets keyboard nav (arrows, Enter) directly
4. Taskbar releases focus while entity menu is active

## Implementation Path
- Monitor `#.desktop/livedesk_nav_claims.txt` for `KIND=row` entries indicating open entity menus
- When an entity menu has claimed a nav row, inhibit taskbar keyboard handling
- Let `tp_desktop_window.c` handle its own context menu keyboard (it already does)
- Restore taskbar nav when entity menu closes (KIND=row claims released)

## Files Involved
- `&.widgits/livedesk-taskbar/ops/tp_taskbar.c` — add entity menu detection
- `&.widgits/tile-picker/ops/tp_desktop_window.c` — already manages entity menu focus

## Status
✅ **IMPLEMENTED** (Aug 9 18:28:38):
- Toolbar popups show unified [>] (toolbar hides when popup open)  
- Entity context menus now receive keyboard focus
- Taskbar detects remote entity menus via livedesk_nav_claims.txt
- Taskbar yields keyboard to entity process when foreign menu is open

## Implementation Details

**New function: `remote_entity_menu_open()`**
- Reads `#.desktop/livedesk_nav_claims.txt`
- Looks for `KIND=row` entries (context menu rows)
- Returns TRUE if any row has a different PID than taskbar
- Returns FALSE if all rows (or no rows) are from taskbar or empty

**Modified: Main event loop KeyPress handling**
```c
} else if (xev.type == KeyPress && remote_entity_menu_open(house_root)) {
    /* Foreign entity menu is open - don't consume the KeyPress.
     * Let entity's own process (different PID) handle it via X server.
     */
    /* Simply ignore; keyboard naturally flows to focused window */
} else if (xev.type == KeyPress && g_hq_popup_open) {
    /* Taskbar's own popups - process as before */
```

**Flow:**
1. Entity window opens, registers PID in livedesk_open.txt
2. User right-clicks → context menu appears in entity window
3. Entity claims nav rows in livedesk_nav_claims.txt with its own PID
4. Taskbar receives KeyPress, calls remote_entity_menu_open()
5. ✅ Function detects foreign PID in KIND=row entries
6. ✅ Taskbar skips KeyPress handling
7. ✅ X server routes keyboard to entity window (already has focus)
8. ✅ Entity context menu receives arrows/Enter/digits

## Why This Matters
- First entity context menu works because it gets focus naturally (user right-clicked)
- Second/subsequent entity menus now also work—taskbar yields keyboard
- Unified keyboard routing: input goes to whoever has menu open
