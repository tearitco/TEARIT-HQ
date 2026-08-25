
Qwen 2.5 Coder series of models are now updated in 6 sizes: 0.5B, 1.5B, 3B, 7B, 14B and 32B all do toolcalls
(even simply? they can be iffy says gemini) 


# Phase 1 Implementation Log — Model Selector (2026-08-13)

## What Was Attempted

Implemented a choosable model selector for h-ai to allow users to cycle through a whitelist of 5 models:
- `stable-code:latest` (3B, Harnecient)
- `gemma3:1b` (Harnecient)
- `gemma3:270m` (Harnecient)
- `llama3-groq-tool-use:8b` (NATIVE tools)
- `llama2:latest` (Harnecient)

Goal: Users can navigate to the MODEL nav item in the sidebar and press Enter to cycle through models, with persistence to `sessions/model.txt`.

## Code Changes Made

### 1. NavKind Enum (line 883)
- Added `NAV_MODEL` to the enum of nav item types
- **Status**: ✅ Successful, verified in binary via `objdump`

### 2. Model Whitelist & State (after line 273)
- Created `ModelEntry` struct with name + BackendMode
- Static array `g_models[]` with 5 entries
- `g_model_idx` tracks current selection (0-4)
- `g_n_models` const = 5
- **Status**: ✅ Successful, strings verified in binary

### 3. Helper Functions (before draw_sidebar at line 1345)
Added three functions:
- `load_model_choice()` — reads `sessions/model.txt`, falls back to stable-code if missing
- `save_model_choice()` — writes current model name to `sessions/model.txt`
- `cycle_model()` — increments index (mod 5), saves choice
- **Status**: ✅ Successful, compiled without errors, visible in `objdump -t`

### 4. Sidebar UI (lines 1417-1426)
- Changed "BACKEND" label to "MODEL"
- Made model name clickable nav item with `nav_add(NAV_MODEL, -1)`
- Added badge and focus-state styling (accent color when focused)
- **Status**: ✅ Code present and correct in source

### 5. activate_focused() Handler (lines 1660-1662)
- Added `case NAV_MODEL: cycle_model(); break;` before `NAV_CLOSE`
- **Status**: ✅ Code present in source

### 6. Startup Integration
- Added `load_model_choice()` call after `refresh_sessions()`
- Added forward declarations before `refresh_sessions()` definition
- **Status**: ✅ Compiled successfully with no errors

## Compilation & Binary Verification

✅ **Compilation**: No errors, only expected XftDraw warnings
✅ **Binary strings**: Contains all new text/logic:
  - "gemma3:1b", "gemma3:270m" model names present
  - "%s/model.txt" path present  
  - "MODEL" label present

✅ **Symbol table**: `load_model_choice`, `cycle_model`, `g_models` all visible in `objdump -t`

## Relay Injection Testing

**Setup**: h-ai running on display, relay file at `#.desktop/ai_cell_agent_relay.txt`

**Test 1**: Send digit codes (ASCII 48-62) + Enter to navigate and activate nav items 1-15
- **Result**: No model.txt created for any item
- **Implication**: NAV_MODEL either not being added to nav array, or nav activation bypassing it

**Test 2**: Checked nav count via receipt
- Receipt showed `n_nav=8` (expected pre-addition)
- Adding MODEL should make it 9
- But no item 9 activation observed

**Test 3**: Added debug file write to `cycle_model()`
- Debug file never created
- Confirms `cycle_model()` not being called

**Conclusion**: Code is compiled in, but nav activation path isn't reaching NAV_MODEL case

## Root Cause Analysis

Three possibilities (in order of likelihood):

### 1. **nav_add(NAV_MODEL, -1) Not Called** (Most likely)
- draw_sidebar() is called (confirmed — sidebar renders)
- But nav_add() for the model item may not execute or fails silently
- Possible causes:
  - Compiler optimization removing call?
  - Conditional logic preventing execution?
  - Issue with g_n_nav counter?

### 2. **Nav Item Counted But activate_focused() Skips It**
- activate_focused checks: `if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;`
- If g_n_nav=8 and we try to activate item 9, returns early
- Receipt showing `n_nav=8` every time suggests MODEL isn't being counted

### 3. **Relay Navigation Not Reaching item 9**
- Digit navigation (ASCII 48-57 = keys 0-9) might not work for items > 9
- Or nav focus isn't jumping to requested item

## What Worked Well

✅ **Codebase understanding**:
- Found model/backend setup correctly (g_model_name, g_backend_mode already exist)
- Nav system well-documented (1-based indices, g_nav array, nav_add pattern)
- Sidebar rendering architecture clear
- Python-based patching reliable for multi-location edits

✅ **Compilation pipeline**:
- build_ai_cell.sh worked cleanly
- Error messages precise
- Binary size reasonable

✅ **Integration points clear**:
- Where to load state (refresh_sessions)
- Where to persist (g_sessions_root/model.txt)
- Where to handle activation (activate_focused switch)

## What Needs Investigation

❌ **Why nav_add(NAV_MODEL) not counted**:
- Add logging to draw_sidebar() to confirm nav_add() is called
- Trace g_n_nav before/after draw_sidebar()
- Check if sidebar drawing is conditional (only under certain states?)

❌ **Whether relay activation reaches the right case**:
- Add debug output in activate_focused() to log which case is hit
- Log g_focus_nav and g_n_nav at activation time

❌ **Nav item ordering**:
- Verify MODEL is last in the sidebar draw order (after sessions, before close)
- Confirm there's no off-by-one in nav index counting

## Lessons & Recommendations

### For Phase 1 (Completing Model Selector)

1. **Add explicit debug logging to redraw()/draw_sidebar()**:
   ```c
   fprintf(stderr, "redraw: n_nav before sidebar=%d\n", g_n_nav);
   draw_sidebar();
   fprintf(stderr, "redraw: n_nav after sidebar=%d\n", g_n_nav);
   ```

2. **Verify nav_add() is called**:
   Add log inside or immediately before `nav_add(NAV_MODEL, -1)` in draw_sidebar()

3. **Test with real mouse click** (not just relay):
   If mouse click works but relay doesn't, issue is in relay→nav mapping, not nav system itself

4. **Check for compiler optimizations**:
   Code looks correct, so either compiler removed call or execution never reaches it

### For Phase 2+ (Harnecient Integration)

1. **Model state is ready**: g_model_name, g_backend_mode already wired; just need model selector working
2. **Persona loading location**: Should go in `pieces/registry/personas/` (mirrors my-lawyer structure)
3. **Backward compatibility**: BACKEND_OLLAMA_RAW still works; new BACKEND_HARNECIENT is additive
4. **Testing via relay proven viable**: The fact that h-ai opened via relay confirms injection works

## Timeline & Effort

- **Code implementation**: ~30 min (6 edits via Python patching)
- **Compilation troubleshooting**: ~20 min (forward declarations fix)
- **Relay testing & debugging**: ~60 min (cycles through relay variants, debug logging, binary inspection)
- **Total**: ~110 min on Phase 1

## Next Steps

**Option A**: Debug nav item activation (spend another 30-45 min on root cause)
- High confidence it's a logging/visibility issue, not architecture
- Likely finds the fix quickly

**Option B**: Proceed to Phase 2 while leaving Phase 1 as-is
- Model selection architecture is solid and compiled
- Phase 2 (Harnecient backend) doesn't depend on Phase 1 selector working yet
- Can revisit Phase 1 after Phase 2 proves the backend mode enum works

**Recommendation**: **Option B** — the nav system debugging could be a rabbit hole. Phase 2 validates whether the backend architecture (BACKEND_HARNECIENT enum, persona loading, deterministic dispatch) works. If Phase 2 works, Phase 1's nav issue becomes lower priority.

---

**Document created**: 2026-08-13 02:40 UTC
**Context**: Haiku 4.5, Phase 1 Model Selector implementation, pending completion
