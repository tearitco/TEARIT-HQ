# TODO: khtpm_ui_common.c Unfactor

**Status:** Deferred - Move action implementation complete (2026-09-27)  
**Priority:** Medium - eliminates text-include pattern, improves IPC architecture  
**Scope:** Affects khtpm_entity.c AND khtpm_core_render.c

## Problem

`khtpm_ui_common.c` (485 lines) is text-included by two binaries:
- **khtpm_entity.c** (desktop entity process)
- **khtpm_grid_jump.c** (shared-lib utility) 
- Possibly **khtpm_core_render.c** (main HQ renderer) - verify

This violates house standard: text-includes are **transitional**, not intended (see memory: text-includes-not-the-standard.md).

## Current Content

Shared globals:
- Theme colors: g_theme_bg, g_theme_fg, g_theme_accent
- UI scale settings: g_ui_scale_pct, g_ui_user_pct, g_ui_auto_pct, g_ui_screen_w/h, g_ui_ref_w/h
- UI config: g_override_redirect, g_click_two_step, g_ui_font_family, g_grid_cell_base
- Type definitions: MethodItem, MAX_METHODS

Per-process (should NOT be shared):
- g_package_dir, g_house_root (per-binary specific)
- g_frame_dirty (per-process dirty flag)
- g_shutdown_requested (per-process signal)
- g_khtpm_menu_* (per-process menu state)

## Refactor Pattern (Option A - house standard)

Follow the pattern already established in codebase (e.g., dock split, entity animation):

1. **Extract shared types** into header file (MethodItem, MAX_METHODS, TP_PATH_BUF)
2. **Extract shared state** into config files:
   - Theme: read from hq_ui.pdl (already has this pattern)
   - UI scale: read from hq_ui.pdl (already has this pattern)
   - Grid cell base: read from desk_grid.pdl or new ui_config.pdl
3. **Remove text-include** from both binaries
4. **Load shared state** at startup from .pdl files (same pattern as khtpm_ui_scale.c)
5. **Add khtpm_ui_manager.+x** if any runtime state changes need real IPC (unlikely - most is static config)

## Implementation Steps

- [ ] Audit all g_* globals: which are truly shared vs per-process
- [ ] Extract MethodItem + MAX_METHODS to `khtpm_ui_common.h`
- [ ] Move load_theme_colors() logic into hq_ui.pdl reader
- [ ] Move UI scale logic into hq_ui.pdl reader (already done?)
- [ ] Remove `#include "khtpm_ui_common.c"` from khtpm_entity.c
- [ ] Remove `#include "khtpm_ui_common.c"` from khtpm_grid_jump.c
- [ ] Verify khtpm_core_render.c doesn't use it (if it does, unfactor that too)
- [ ] Rebuild both binaries
- [ ] Test: entity menus still open, theme colors still apply, UI scale still works

## Risk Assessment

**High**: Both khtpm_entity.c and khtpm_core_render.c use this code
- If unfactored incorrectly, both HQ window and desktop entity windows will break
- Test must verify: context menus open, methods load, theme applied, UI scale correct

**Mitigation**:
- Unfactor one binary at a time (khtpm_entity.c first, then khtpm_core_render.c)
- Keep hq_ui.pdl reader as source of truth (use existing pattern)
- Test on real desktop before/after each step

## Related

- **memory:** [[text-includes-not-the-standard]] - why this matters
- **memory:** [[khtpm-shared-layout-caution]] - layout changes must be idempotent (applies to shared config loading)
- **patterns:** desk_grid.pdl, hq_ui.pdl - existing config file patterns

## Owner

Blocked on: next session or owner availability  
Triggered by: Move action implementation exposed text-include pattern (2026-09-27)
