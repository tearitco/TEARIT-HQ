# WRAITH-ALPHA DEVELOPMENT HANDOFF - 2026-06-07

## CURRENT STATE
- **Startup**: Fixed. `run_orchestrator.sh` now clears stale focus and navigation state on launch, preventing "Index 2" desync.
- **Architecture**: Refactored to "Thin Theater" / Manager Projection pattern.
- **Window Management**: Implemented a dynamic Manager-owned window registry (`g_windows`). 
- **Toolbar**: The toolbar is now dynamically generated markup (`desktop_toolbar_markup`) projected by the Manager, moving away from hardcoded layout buttons.
- **Restore Bug**: Addressed by allowing the Input Router to resolve "Restore" actions via the window registry.

## NEXT STEPS FOR AGENT
1. **Multi-Instance Refinement**: The current registry supports multiple windows, but the `SET_ACTIVE:6` action is still hardcoded for a single slot. The next agent should make the `onClick` actions in `generate_toolbar_markup()` dynamic based on the `Window.id` and registry index.
2. **Tab-Reopen / Restore Logic**: Finalize the logic to handle reopening specific instances when more than one terminal is minimized.
3. **Mouse Support**: With the registry and dynamic toolbar in place, introduce mouse hit-testing for the taskbar buttons.

## RELEVANT FILES
- `projects/wraith-alpha/wraith-std.txt` (SOP)
- `projects/wraith-alpha/wraith-mod.txt` (Refactor Plan)
- `projects/wraith-alpha/manager/wraith-alpha_manager.c` (Registry implementation)
- `projects/wraith-alpha/layouts/alpha-shell.chtpm` (Dynamic toolbar layout)
