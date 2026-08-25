# AU10 Sonnet Handoff — Save/Load/Save-As Keyboard Access + Behavior-Tree Integration

**Date:** 2026-08-10  
**Status:** Implementation complete, keyboard harness integration needed  
**Owner (after handoff):** Sonnet 5  

---

## 1. What's Been Done ✅

### Code Implementation (COMPLETE)
- ✅ **Save-As Input Dialog** — `livedesk_save_as()` now opens cli-io modal, prompts for custom session name (lines 2729-2762)
- ✅ **Save-As Handler** — `livedesk_save_as_with_name()` creates session with user-provided name (lines 2702-2727)
- ✅ **Dispatch Wired** — dispatcher passes dpy/gc to save-as (line 3147)
- ✅ **Return Handler** — Return key dispatches to save-as-with-name when `g_cliio_op=="save-as"` (lines 3570-3580)
- ✅ **Dynamic Labels** — cliio_draw shows "new session:" for save-as, "rename desk:" for rename (lines 1436-1440)
- ✅ **Pals Migration** — entities running from pals registry, save-as clones layout-only (no entities/ copies)
- ✅ **Build** — EXIT=0, no errors

### Architecture Decisions
- Save uses current session name (no input needed)
- Save-As prompts via cli-io modal (matches rename-desk UX)
- Load shows session list (already working)
- Pals registry is canonical (entities reference, not copy)

---

## 2. Blocker: Keyboard Harness Access ❌

### Current Issue
- Harness key injection is **unreliable** — keys interpreted ambiguously by strip/popup system
- Example: `key '3'` opens Sessions menu instead of navigating to File nav=3
- **Agents/behavior-trees need predictable keyboard API**

### Root Cause
The livedesk taskbar uses index-based nav (nav=1..12 for strip cells), but key injection conflicts:
- Sending `key '3'` may select row 3 of open popup instead of navigating to nav 3
- Alt+F works (opens sessions) but Alt keys are not desired

### CHTPM Harness Pattern (Reference)
CHTPM uses **index-driven navigation** with clear priority:
1. Strip nav cells (top priority)
2. If no popup open: nav drives strip focus
3. If popup open: nav drives popup row selection
4. Commands are index-based: `nav 5` → navigate to strip cell 5

**Goal:** Livedesk taskbar should match this pattern for agent reliability.

---

## 3. Tasks for Sonnet

### 3.1 Keyboard Access Design (Research)
**Task:** Study CHTPM harness code and map exact behavior
- Read: `#.haiku+/tpmos-re-dox/fo-menu-sys.md` (reference design)
- Find: CHTPM's exact key dispatch logic (prioritize strip nav over popup rows)
- Document: How to adapt for livedesk taskbar

**Deliverable:** Short design doc (< 200 words) on how to fix key injection priority

### 3.2 Fix Key Injection Priority in Taskbar
**Task:** Update `tp_taskbar.c` KeyPress handling to match CHTPM pattern
- **File:** `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
- **Current code:** Line ~3465-3650 (KeyPress handling in event loop)
- **Problem:** When popup is open, numeric keys select popup rows BEFORE checking strip nav priority
- **Fix needed:** 
  - If key is '1'..'9': check strip nav first
  - Only route to popup row selection if no strip nav matches
  - Ensure `key '3'` (File nav) navigates strip even when popup open

**Affected code sections:**
- Line 3537: `else if (xev.type == KeyPress && g_cliio_active...` — cli-io key handling (OK as-is, modal takes priority)
- Line 3540-3650: Popup KeyPress handling — needs priority fix
- May need refactor of key dispatch logic (consider extracting to helper function)

### 3.3 Test Save/Load Keyboard Access
**Task:** Verify keyboard access end-to-end
- Launch taskbar, ensure no entities open
- Test: `key '3'` → open File menu
- Test: `key '2'` in File menu → select "save-as"
- Test: Type session name via `key a`, `key b`, etc. (cli-io input)
- Test: `key Return` → create session with custom name
- Test: `key '3'`, `key '1'` (or row selection) → load different session
- Test: `key '3'`, `key '1'` → save current state

**Expected behavior:**
- File menu should open via `key '3'` reliably
- Session name input should accept typed characters
- Return should commit; Escape should cancel
- Load should show session list with nav rows

**Test harness commands to verify:**
```bash
bash /tmp/opencode/nav.sh key 3          # File menu
bash /tmp/opencode/nav.sh key 3          # Save-As
bash /tmp/opencode/nav.sh key a          # Type 'a'
bash /tmp/opencode/nav.sh key Return     # Commit
bash /tmp/opencode/nav.sh popups 2>&1    # Check state
```

### 3.4 Update Resume & Design Docs
**Task:** Document as-built state
- Update `/AU10-claude-resume.md`:
  - Mark save-as input as ✅ COMPLETE
  - Note: keyboard access refactored by Sonnet for behavior-tree compatibility
- Update `#.livedesk/livedesk-editor-design.md` §11:
  - Add note on keyboard access pattern (index-driven nav injection)
  - Reference CHTPM pattern as model

### 3.5 Behavior-Tree Integration (Optional Future)
**Task:** Ensure agents can script save/load operations
- Example agent command sequence:
  ```
  key 3          # open File
  key 3          # select Save-As (or key 2 for Save, key 1 for New)
  type "my-session"
  key Return     # commit
  ```
- Verify this works reliably 10x in a row (race condition test)
- Add to agent SDK if working

---

## 4. Critical Context

### File Locations
- **Main code:** `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
  - Save-as impl: lines 2702-2762
  - Dispatch: line 3147
  - Key handler: lines 3537-3650
  - cliio functions: lines 1411-1486
  - cliio_draw: lines 1428-1446

- **Harness:** `/tmp/opencode/nav.sh` (read-only reference; don't modify)
- **Design refs:** 
  - `#.livedesk/livedesk-editor-design.md` §11 (KHTPM refactor notes)
  - `#.haiku+/tpmos-re-dox/fo-menu-sys.md` (Fuzz-Op manager projection pattern)

### Test Sessions
- **Active:** s1 (pre-design, 6 entities from pals registry)
- **Created during testing:** s2, s3, s4, s5 (safe to delete if needed)
- **Pals registry:** `/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/` (6 pals: asa, ava, book-stack, m1_ninjadragon, m8_redhorned, self)

### Key Globals (for reference)
```c
static char g_cliio_op[32] = "";      // "rename-desk" or "save-as"
static char g_cliio_buffer[256] = ""; // user input
static int g_cliio_active = 0;        // modal is open
static int g_cliio_typing = 0;        // in input mode
```

### Known Limitations
- ⚠️ Glyph rendering broken (emoji showing as weird chars) — UI polish only, not blocking
- ⚠️ desk_01.pdl empty (migration artifact) — users can copy office→desk_01 via future UI
- ⚠️ Hardcoded strip buttons (design debt) — documented in §11, defer to khtpm port

---

## 5. Success Criteria

### Must Have
- ✅ `key '3'` reliably opens File menu (no matter what popup was open)
- ✅ File menu row selection works via numeric keys
- ✅ Save-As input accepts typed characters
- ✅ Return commits; Escape cancels
- ✅ Load shows session list for selection

### Nice to Have
- 📝 Keyboard access documented in design doc
- 📝 Example agent commands in worklog or README
- 📝 Behavior-tree integration tested

---

## 6. Questions for Sonnet

1. **Key Priority:** Should `key '3'` on an open popup close the popup and navigate strip, or should it only navigate if no popup? (Recommend: prioritize strip nav, close popup if needed)

2. **Config:** Should keyboard hotkeys be configurable (e.g., via taskbar.pdl), or hardcoded for now?

3. **Agent API:** After fix, should we add a formal API to the harness for "do File→Save-As with name" or keep it as raw key injection?

---

## 7. References

- **Build:** `gcc -std=c11 -Wall -O2 "&.widgits/livedesk-taskbar/ops/tp_taskbar.c" -o "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x" -lX11`
- **Restart recipe:** `kill $(cat "#.desktop/livedesk_taskbar.pid"); sleep 1; setsid nohup "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x" "$PWD" </dev/null >/dev/null 2>&1 & disown`
- **Test harness:** `/tmp/opencode/nav.sh {nav,key,row,esc,popups,cells,wait}`

---

## 8. Sign-Off

**Work by:** Claude Haiku 4.5 (2026-08-10)  
**Handoff to:** Sonnet 5  
**Ready for:** Keyboard access refactor + behavior-tree integration

Implementation is solid. Blocking issue is harness/UI key dispatch priority. Sonnet should focus on fixing key injection order to match CHTPM pattern, then verify end-to-end via harness.

---

*End AU10-sonnet-handoff.md*
