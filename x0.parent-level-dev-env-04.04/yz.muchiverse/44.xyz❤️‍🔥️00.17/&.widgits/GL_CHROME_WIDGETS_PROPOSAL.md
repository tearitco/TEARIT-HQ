# Proposal: GL Chrome for XYZOS Widgets  
### Date: 2026-07-28 · Status: PROPOSAL (not implemented)

---

## 1. Problem

Widget **ops + cmd bus** are harness-green (file-menu, map-picker, tile-picker,  
proc-monitor). Users still need a **window** to click:

- Today: agents/harnesses call `fm_*` / `mp_*` / `tp_*` / `runtime_*` directly.  
- Product law (§35): **GL is primary**; ASCII off for `run-widget`.  
- We must not invent a second UI toolkit — reuse mutaclysm/house RGB path.

---

## 2. Goals

| Goal | Detail |
|------|--------|
| One chrome pattern | All widgets share layout + RGB + optional input |
| Same session files | `view.txt` → chtpm → `current_frame.txt` → `rgb_frame.raw` → `gl_mirror` |
| Widget profile | `launch_profile=widget`: GL on, ASCII renderer skipped/null |
| Host unchanged | Host (mutaclysm/editor) keeps its own GL; widget is second window |
| Harness stays file-based | GL optional for CI; ops remain source of truth |

Non-goals for v1 chrome:

- Full desktop WM / z-order policy  
- Shared OpenGL context across processes  
- Mouse hit-testing on mutaclysm map from tile-picker (phase 2)

---

## 3. Proposed architecture

```text
  button.sh run-widget
       │
       ├─ session dir (throwaway, like editor/START_BUTTON)
       ├─ NO ascii system/renderer  (or stdout → /dev/null)
       ├─ chtpm_parser_pal  layouts/<widget>.chtpm
       ├─ prisc+x  pal/main_loop_chtpm.pal
       ├─ ops: compose_frame → view.txt
       ├─ chtpm_rgb_render  (or shared rgb compose)
       └─ gl_mirror  title="File Menu" | "Tile Picker" | …
              ▲
              └── optional: GLUT keys → keyboard history (widget-local)
```

**Compose content** is widget-specific (methods list, brush, process rows)  
but **pipeline is identical** to mutaclysm/editor.

### Config (`config/ui.pdl` or `pieces/system/ui_mode.pdl`)

```text
STATE | launch_profile   | widget
STATE | ui_primary       | gl
STATE | ascii_renderer   | 0
STATE | gl_window        | 1
STATE | gl_title         | File Menu
STATE | gl_w             | 640
STATE | gl_h             | 480
```

### Shared “widget shell” (optional extract later)

```text
&.widgits/_shell/          # future shared code
  system/                  # copy or symlink gl_mirror, rgb_render, chtpm
  pal/widget_loop.chtpm.pal
  ops/widget_compose_chrome.c   # box borders + status line helper
```

v1: each widget may still vendor copies (house tradition);  
v2: factor `_shell` once three widgets share code.

---

## 4. Per-widget chrome content (what to paint)

### file-menu
- Title, ACTIVE project, FILE/SLOT path  
- Numbered methods (NEW/SAVE/LOAD or NEW_GAME/SAVE_AS/LOAD_GAME)  
- Status from `status.txt`  
- Nav: KEY:n via chtpm (same as terminal menus)

### map-picker
- ACTIVE map  
- List of maps as methods → SWITCH_MAP  
- Status line

### tile-picker
- Palette grid of glyphs (ASCII first; emoji atlas later)  
- Selected brush highlight  
- Instructions: “click map in mutaclysm” (phase 2)  
- v1: keys 1–9 select brush; method PLACE uses xlector coords from host  
  (read hero xlector_pos from focus session) — **no cross-window mouse yet**

### proc-monitor
- Rows of processes (alive/dead, gl, kind)  
- FOCUS / KILL / SOFT QUIT / REFRESH methods  
- Poll registry every N seconds or on REFRESH only (no idle spam)

---

## 5. Input strategy

| Phase | Input |
|-------|--------|
| **A (now–next)** | chtpm keyboard nav only (works with harness inject + optional GL key forward) |
| **B** | gl_mirror key forward into widget interact_relay (like mutaclysm) |
| **C** | Mouse: hit-test widget UI buttons in GL |
| **D** | Cross-window: click mutaclysm map cell → place brush (Xdnd or shared focus + coords file) |

**Proposal: ship chrome with Phase A+B first.**  
Tile paint stays cmd-bus + xlector position until Phase D.

Xlector-assisted place (smart interim):

```text
tile-picker "PLACE AT XLECTOR"
  → read focus session hero xlector_pos_x/y + map_id
  → PLACE_TILE:map:x:y:brush
```

No need to click the game window for v1 maker loop.

---

## 6. Process / lifecycle

```text
Host app (mutaclysm)     Widget (tile-picker)
   register ──────────►  runtime/
   publish bridge
                            focus host
                            GL chrome
   still running ◄──────  PLACE_TILE inbox
   compose map
```

- Widget exit: unregister self from runtime; do not kill host  
- Host exit: proc-monitor GC; widgets show “focus dead”  

---

## 7. Implementation phases (chrome only)

| Phase | Deliverable | KPI |
|-------|-------------|-----|
| **G0** | Shared run-widget skeleton (one widget: file-menu chrome list) | Window title appears if DISPLAY set; files always |
| **G1** | All four widgets have `.chtpm` + compose + methods | KEY nav works headless via inject |
| **G2** | gl_mirror auto-start when `gl_window=1` | NO_GL=1 still works |
| **G3** | GL key → interact_relay | Type/nav in window |
| **G4** | Xlector place from tile-picker method | Place without mouse on map |
| **G5** | Mouse UI hit-test (optional) | Click method row in widget GL |

---

## 8. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| No DISPLAY in CI | Harness asserts files only; G0 window check optional |
| Two GL windows + one keyboard | Widget owns its focus; document click-to-focus |
| RGB format drift | Reuse mutaclysm `chtpm_rgb_render` / gl_mirror pair |
| Code duplication | Accept v1 copies; extract `_shell` after 2+ widgets |

---

## 9. Recommendation

1. **Keep ops/harnesses as contract** (already green for place/list/kill).  
2. **Next coding after this proposal approval:** G0–G1 for **file-menu** only (simplest chrome).  
3. Clone chrome to map-picker / tile-picker / proc-monitor.  
4. Defer cross-window mouse paint; use **xlector PLACE** for makers.  
5. Only then `@app` “Muta Maker Desk” recipe (see AFTER-widgets-apps-store).

---

## 10. Acceptance for “GL chrome done enough”

- [ ] `run-widget` opens one GL window per widget (when DISPLAY available)  
- [ ] ASCII not required for use  
- [ ] All existing harnesses still pass without DISPLAY  
- [ ] FILE / map / tile / proc methods reachable via KEY inject  
- [ ] Documented in XYZOS_README + muta-zoo  

---

*End GL_CHROME_WIDGETS_PROPOSAL.md*
