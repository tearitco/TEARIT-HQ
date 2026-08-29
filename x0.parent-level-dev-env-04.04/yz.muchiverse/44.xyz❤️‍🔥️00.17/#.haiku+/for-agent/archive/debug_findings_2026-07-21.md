---
name: strategy_a_debug_findings
description: Root cause of missing debug outputs in real terminal — main_loop ordering issue
metadata:
  type: project
---

## Finding: Why No Debug Outputs in Real Terminal?

**Date:** 2026-07-21
**Status:** ROOT CAUSE IDENTIFIED

### Layer 1 Testing Results ✅

File injection testing CONFIRMS the infrastructure works:
- ✅ gemma_strategy.c detects tools correctly, sets `sys_msg=[Strategy A] Tool: list_dir`
- ✅ strategy_execute_a.c executes tools, appends to context_log.txt, sets `sys_msg=[Tool: list_dir] 398 bytes`
- ✅ compose_frame.c reads sys_msg from state.txt
- ✅ compose_frame writes `[SYS]: [Strategy A] Tool: list_dir` to pieces/apps/player_app/view.txt
- ✅ All ops complete quickly (no blocking)

**Conclusion:** Data/logic layer is CORRECT. The problem is timing, not logic.

---

## Root Cause: Execution Order in main_loop_chtpm.pal

**File:** 1.muchi-pal-agent🤖️/pal/main_loop_chtpm.pal (lines 35-48)

Current order:
```pal
do_submit:
gemma_strategy       # ← Sets sys_msg=[Strategy X] Tool: Y
strategy_execute_a   # ← Executes tool, updates sys_msg
send_message         # ← BLOCKS HERE waiting for Gemma
j render             # ← NEVER REACHED while blocked

render:
irc_agent_poll
compose_frame        # ← Would display sys_msg, but never called until send_message returns
```

**The Problem:**
1. User types, presses Enter
2. gemma_strategy + strategy_execute_a run (very fast, <10ms)
3. send_message is called and blocks waiting for Gemma response
4. compose_frame is NEVER called (because we're blocked in send_message)
5. sys_msg updates are written to state.txt but never displayed
6. Terminal shows nothing except "Thinking..." while Gemma hangs

**Why user sees no debug output:** The main loop never reaches compose_frame while waiting for the Gemma response.

---

## Solution: Render Debug Info BEFORE Sending

Change main_loop_chtpm.pal to call compose_frame between strategy ops and send_message:

```pal
do_submit:
gemma_strategy       # Sets sys_msg=[Strategy X] Tool: Y
strategy_execute_a   # Executes tool, updates sys_msg and context_log
compose_frame        # ← RENDER NOW so user sees debug info
hit_frame            # ← Ping the frame marker to chtpm
send_message         # Now it's okay to block here - user already sees what's happening
j render

render:
irc_agent_poll
compose_frame
hit_frame
j loop
```

**Result:** User will see:
- `[SYS]: [Strategy A] Tool: list_dir` immediately when tool is detected
- Tool execution result in context log
- THEN "Thinking..." message while waiting for Gemma
- User understands the system is working and waiting, not broken

---

## Why This Fix is Safe

- ✅ compose_frame is already proven to work (Layer 1 tests pass)
- ✅ render gating already exists (chat_screen_changed.txt marker)
- ✅ No new ops needed
- ✅ No changes to data flow or state management
- ✅ Just reorders when rendering happens (still before send_message blocks)

---

## Next Steps

1. Update pal/main_loop_chtpm.pal lines 35-48
2. Re-compile (bash scripts/build.sh)
3. Run Layer 1 file injection test to verify still works
4. Run Layer 3 real terminal test to verify debug outputs appear
5. If Gemma still hangs, that's a separate issue (API timeout, model not responding)
