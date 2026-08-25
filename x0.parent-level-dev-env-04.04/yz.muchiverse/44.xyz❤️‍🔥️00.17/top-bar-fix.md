# Top-Bar Label Bug Fix

## Summary
The taskbar displays submenu labels ("load", "reset") instead of button labels ("file", "player").

## Root Cause
In `&.widgits/livedesk-taskbar/ops/tp_taskbar.c:load_strip_config()`, the sscanf pattern matching order is wrong.

When parsing `"strip_btn_0_menu_3_label"` with value `"load"`:
1. The string becomes `"0_menu_3_label"` after `key + 10`
2. `sscanf("0_menu_3_label", "%d_label", &n)` matches:
   - `%d` successfully parses "0"
   - `_label` fails to match "_menu_...", but sscanf returns 1 (items matched)
   - The condition `if (...== 1)` is TRUE (wrong!)
3. Code incorrectly executes `btns[0].label = "load"` instead of `btns[0].menu[3].label = "load"`

## The Problem Code
Lines 455-471 check simpler patterns BEFORE more specific ones:
```c
if (sscanf(key + 10, "%d_label", &n) == 1 && ...) {
    // Partial match! "0_menu_3_label" matches %d part
    snprintf(btns[n].label, ...);  // WRONG for menu items
} else if (..."%d_menu_%d_label"...) {  // Never reached for menu items
    snprintf(btns[n].menu[m].label, ...);
}
```

## Fix Applied
Reordered the sscanf checks to test **more specific patterns first**:

**Before (WRONG ORDER):**
```c
if (sscanf(key + 10, "%d_label", &n) == 1 && ...) {          // Line 455
    // Catches "0_menu_3_label" incorrectly!
} else if (sscanf(key + 10, "%d_cmd", &n) == 1 && ...) {      // Line 458
    // ...
} else if (sscanf(key + 10, "%d_menu_%d_label", &n, &m) == 2 && ...) { // Line 461
    // Never reached for menu items
} else if (sscanf(key + 10, "%d_menu_%d_cmd", &n, &m) == 2 && ...) {   // Line 466
    // ...
}
```

**After (CORRECT ORDER):**
```c
if (sscanf(key + 10, "%d_menu_%d_label", &n, &m) == 2 && ...) {        // Line 455 ✓
    // Catches "0_menu_3_label" correctly
} else if (sscanf(key + 10, "%d_menu_%d_cmd", &n, &m) == 2 && ...) {   // Line 460 ✓
    // ...
} else if (sscanf(key + 10, "%d_label", &n) == 1 && ...) {             // Line 465
    // Now only matches plain "N_label", not "N_menu_M_label"
} else if (sscanf(key + 10, "%d_cmd", &n) == 1 && ...) {               // Line 468
    // ...
}
```

## Impact
- Button 0 ("file") will now display correctly (was showing "load")
- Button 2 ("player") will now display correctly (was showing "reset")
- All other button/menu assignments now parse correctly

## Status
✅ **FIXED** - tp_taskbar.c:454-471 reordered to check menu patterns before button patterns

## File Affected
`&.widgits/livedesk-taskbar/ops/tp_taskbar.c` lines 454-471
