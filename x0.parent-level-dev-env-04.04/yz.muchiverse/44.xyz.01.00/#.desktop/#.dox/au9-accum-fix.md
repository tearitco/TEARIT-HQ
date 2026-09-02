# 🐛 Toolbar Digit Accumulation Bug: Investigation & Fix

## 🎯 Problem Statement

**Current Buggy Behavior**: When a digit key is pressed in the toolbar's armed navigation mode, the toolbar immediately consumes it and jumps/activates that nav target, opening a popup or activating a tab.

**Expected Behavior** (matching CHTPM parser): Digits should **accumulate** in a buffer, allowing multi-digit navigation jumps (e.g., type "1", "2" to navigate to nav 12, then press Enter to activate). Only Enter should trigger the actual activation/popup-open.

**User Impact**: Cannot use multi-digit nav numbers (10+) effectively; typing "1" jumps immediately instead of waiting for the second digit or Enter.

---

## 🔍 Root Cause Analysis

### Location: tp_taskbar.c, lines 2230-2279 (keyboard handler, digit-press branch)

**Current Code (WRONG)**:
```c
/* chtpm do_jump: move [>] immediately to that index */
if (digit_buf[0]) {
    int nav_n = atoi(digit_buf);
    char kind[8] = "", entity[128] = "", path[PATH_BUF] = "";
    if (lookup_nav(house_root, nav_n, kind, sizeof(kind), entity, sizeof(entity), path, sizeof(path))) {
        if (strcmp(kind, "tab") == 0) {
            for (int ti = 0; ti < n_tabs; ti++) {
                if (strcmp(tabs[ti].entity, entity) == 0) {
                    tab_focus_idx = ti;
                    strip_focus_cell = -1;
                    nav_focus = n_cells + ti;
                    break;
                }
            }
        } else if (strcmp(kind, "btn") == 0) {
            int ci = cell_for_nav(cells, n_cells, nav_n);
            if (ci >= 0) {
                strip_focus_cell = ci;
                nav_focus = ci;
                open_cell_popup(dpy, gc, house_root, cells, n_cells, ci, hq_menu, hq_n_menu);  // ❌ PROBLEM HERE
                draw_strip(dpy, strip_win, gc, strip_w, cells, n_cells, bg_pixel, nav_armed, digit_buf);
            }
        } else if (strcmp(kind, "row") == 0 && path[0]) {
            // ... row handling ...
            if (strcmp(path, house_root) == 0) {
                // Open local HQ/strip popup
            } else {
                // Send FOCUS_NAV relay (correct for digit phase)
            }
        }
    }
}
```

**The Bug**: Line 2249 calls `open_cell_popup()` immediately when a digit is pressed on a button (KIND=btn). This activates the popup before Enter is pressed, violating the digit-accumulation contract.

### Why This Is Wrong

1. **Violates Digit Accumulation Contract**: The whole point of digit accumulation is to defer action until Enter. Premature activation defeats this.

2. **Mismatches CHTPM Behavior**: CHTPM parser's `do_jump()` only moves focus within menus; it doesn't activate them. Activation happens on Enter (separate code path).

3. **Inconsistent Across NAV Types**:
   - For KIND=tab: Only moves focus (lines 2235-2243), does NOT activate ✅
   - For KIND=btn: Moves focus AND opens popup (lines 2244-2251) ❌
   - For KIND=row: Only moves focus OR sends FOCUS_NAV relay, does NOT activate ✅

### Evidence in Code

**Compare: Digit-press behavior vs Enter-press behavior**

Digit-press (line 2249): `open_cell_popup()` called immediately
```c
} else if (strcmp(kind, "btn") == 0) {
    int ci = cell_for_nav(cells, n_cells, nav_n);
    if (ci >= 0) {
        strip_focus_cell = ci;
        nav_focus = ci;
        open_cell_popup(dpy, gc, house_root, cells, n_cells, ci, hq_menu, hq_n_menu);  // Premature!
        draw_strip(dpy, strip_win, gc, strip_w, cells, n_cells, bg_pixel, nav_armed, digit_buf);
    }
}
```

Enter-press (line 2131-2159): Only opens popup when Enter is pressed
```c
if (ks == XK_Return || ks == XK_KP_Enter) {
    if (digit_buf[0] == '\0') {
        /* Activate focused button or tab */
        if (strip_focus_cell >= 0) {
            /* Focused on a strip button */
            open_cell_popup(dpy, gc, house_root, cells, n_cells, strip_focus_cell, hq_menu, hq_n_menu);  // Correct!
        } else if (n_tabs > 0) {
            if (tab_focus_idx < 0) tab_focus_idx = 0;
            if (tab_focus_idx >= n_tabs) tab_focus_idx = n_tabs - 1;
            taskbar_activate_tab(dpy, tabs, n_tabs, tab_focus_idx);
        }
    } else {
        int nav_n = atoi(digit_buf);
        // ... lookup and activate via nav_n ...
    }
}
```

The Enter handler correctly defers activation until the Enter key.

---

## ✅ Prescription (Fix)

### Change Required

**File**: `/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/livedesk-taskbar/ops/tp_taskbar.c`

**Lines**: 2244-2251 (the KIND=btn branch of the digit-press handler)

**Current Code**:
```c
} else if (strcmp(kind, "btn") == 0) {
    int ci = cell_for_nav(cells, n_cells, nav_n);
    if (ci >= 0) {
        strip_focus_cell = ci;
        nav_focus = ci;
        open_cell_popup(dpy, gc, house_root, cells, n_cells, ci, hq_menu, hq_n_menu);
        draw_strip(dpy, strip_win, gc, strip_w, cells, n_cells, bg_pixel, nav_armed, digit_buf);
    }
}
```

**Fixed Code**:
```c
} else if (strcmp(kind, "btn") == 0) {
    int ci = cell_for_nav(cells, n_cells, nav_n);
    if (ci >= 0) {
        strip_focus_cell = ci;
        nav_focus = ci;
        /* Do NOT open popup here - defer to Enter key */
        mark_strip_frame_changed(house_root, "digit-btn-focus");
        draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, cells, n_cells, bg_pixel, nav_armed, digit_buf);
    }
}
```

### Why This Fix Works

1. **Defers Activation**: Only moves focus to the button; popup opens on Enter (line 2133).

2. **Matches Tab/Row Behavior**: Consistent with KIND=tab (lines 2235-2243) and KIND=row (lines 2252-2277), which also defer activation.

3. **Maintains Visual Feedback**: Still updates the strip display via `draw_strip_if_marked()` so user sees the cursor move to the target button.

4. **Follows CHTPM Contract**: `do_jump()` focuses; `apply_action()` activates (these are separate phases).

### Behavioral After Fix

User Types | Result
-----------|--------
Press "1" | digit_buf = "1", focus moves to nav 1, strip display updates, popup stays closed
Press "2" after "1" | digit_buf = "12", focus moves to nav 12, strip display updates, popup stays closed
Press "0" (invalid, total_nav=13) | digit_buf clears or resets, depends on bounds logic
Press "Backspace" | digit_buf = "1", focus moves back to nav 1
Press "Enter" on "12" | Looks up nav 12, activates it (popup opens if it's a button)
Press "Escape" | digit_buf cleared, nav_armed = 0, returns to unarmed state

---

## 🧪 Testing Strategy (After Fix)

### Unit Test: Multi-Digit Navigation

1. **Setup**: Taskbar running with 13+ nav numbers (7 buttons + 7+ tabs)
2. **Step 1**: Right-click to arm navigation
3. **Step 2**: Press "1" → Observe cursor moves to nav 1, display shows "[1]", NO popup opens
4. **Step 3**: Press "0" → Observe digit_buf becomes "10", cursor moves to nav 10, display shows "[10]", NO popup opens
5. **Step 4**: Press "Backspace" → Observe digit_buf becomes "1", cursor moves back to nav 1
6. **Step 5**: Press "2" → Observe digit_buf becomes "12", cursor moves to nav 12, display shows "[12]"
7. **Step 6**: Press "Enter" → Popup opens or tab activates (depending on what nav 12 is)

### Regression Test: Single-Digit Navigation

1. **Setup**: Same, with 13+ nav numbers
2. **Step 1**: Right-click to arm
3. **Step 2**: Press "5" → Cursor moves to nav 5, display shows "[5]", NO popup opens ✅
4. **Step 3**: Press "Enter" → Popup opens (if nav 5 is a button) ✅

### Regression Test: Arrow Keys Still Work

1. **Setup**: Same
2. **Step 1**: Right-click to arm
3. **Step 2**: Press "Right" three times → Cursor advances 3 positions, digit_buf clears (per line 2125)
4. **Step 3**: Press "1" → Cursor moves to nav 1, digit_buf = "1"
5. **Step 4**: Press "Right" → Cursor advances, digit_buf clears, cursor at nav 2 (unified focus at work)

### Regression Test: Old Single-Digit Jump (No Enter Needed For Single)

**Question**: Should "5" alone activate nav 5, or require Enter?

**Answer**: Based on CHTPM behavior and the digit-accumulation design, **always require Enter**. This is consistent and removes ambiguity (is "5" complete or am I about to type "50"?). User presses "5" then "Enter", not "5" alone.

**However**, if the house standards say "single digit activates immediately," this design decision should be made explicit in !.HOUSE_STDS.md. For now, fix assumes **all activations require Enter**.

---

## 📊 Impact Summary

| Aspect | Before | After |
|--------|--------|-------|
| **Multi-Digit Nav** | ❌ Broken (activates on first digit) | ✅ Works (accumulates, activates on Enter) |
| **Single-Digit Nav** | ✅ Works (but inconsistent—why?) | ✅ Consistent (Enter required for all) |
| **Popup Opening** | Happens on digit press | Happens only on Enter (or click) |
| **Visual Feedback** | Cursor moves + popup opens | Cursor moves, popup deferred |
| **CHTPM Parity** | ❌ Digit handler inconsistent | ✅ Matches parser's do_jump/activate split |
| **Code Consistency** | ❌ KIND=btn differs from KIND=tab | ✅ All nav kinds defer to Enter |

---

## 🔧 Implementation Checklist

- [ ] **Edit tp_taskbar.c** line 2244-2251: Remove `open_cell_popup()` call from KIND=btn digit branch
- [ ] **Replace with**: `mark_strip_frame_changed()` and `draw_strip_if_marked()` for visual feedback
- [ ] **Rebuild**: `gcc -std=c11 -Wall -O2 tp_taskbar.c -o +x/tp_taskbar.+x -lX11`
- [ ] **Test**: Multi-digit (1→0→Enter), single-digit (5→Enter), Backspace, Escape, arrows
- [ ] **Harness**: Update livedesk-taskbar harness scenario if it assumes old behavior
- [ ] **Document**: Add note to !.HOUSE_STDS.md if single-digit immediate activation ever becomes required

---

## 🏛️ Architectural Note

This bug exists because the digit-accumulation logic was partially ported from CHTPM's `chtpm_parser_pal` (where `do_jump()` only focuses and `apply_action()` activates separately), but one branch (KIND=btn) didn't complete the port and mixed both phases into one. The fix is simply **completing the port**: all nav kinds now defer activation to Enter, matching CHTPM.

**Related Code**:
- CHTPM: `pieces/chtpm/plugins/*.c` in parser, look for `digit_accum`, `do_jump()`, `apply_action()`
- XWindow: tp_taskbar.c lines 2128-2279 (unified Enter handler handles activation for all nav types)

After this fix, tp_taskbar.c's digit-accum behavior will be **identical** to CHTPM's across all nav target types (buttons, tabs, menu rows).
