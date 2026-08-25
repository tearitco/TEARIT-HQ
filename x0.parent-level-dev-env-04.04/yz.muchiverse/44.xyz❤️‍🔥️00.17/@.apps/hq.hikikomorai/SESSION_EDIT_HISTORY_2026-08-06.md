# 📜 hikikomorai / livedesk — session edit history (2026-08-06)

**Product name:** hikikomorai (company/house framing) · product surface = **livedesk**  
**This file:** chronological record of agent edits that touched the live desk stack this session.  
**Companion design:** `🏯️.LD.hikikomorai-design.md` · mouse research: `x11-mouse-2do.txt`  
**House standards:** `!.HOUSE_STDS.md` · prior handoffs: `#.grok…/14.Au6-CL0D-HOFF.txt`, `15.GROK_AU6-10.txt`

**Scope honesty:** code lives mainly under `&.widgits/tile-picker/`, `&.widgits/livedesk-taskbar/`, `$.crypts/`, and `#.desktop/` — not under this dir as a build tree. This dir is the **product/docs home** for hikikomorai; history is recorded here so the next agent does not re-litigate focus/nav.

---

## 0. What “most affected” means

| Area | Path | Role in hikikomorai |
|---|---|---|
| Entity windows + KHTPM menus | `&.widgits/tile-picker/ops/tp_desktop_window.c` | Live desk entities, context menus, relays |
| Taskbar | `&.widgits/livedesk-taskbar/ops/tp_taskbar.c` | Desk session wrangler (§2.2 design doc) |
| Autostart / `$` | `$.crypts/` | Mount + launch desk session |
| Shared state | `#.desktop/livedesk_*.txt` | open registry, nav claims, theme, shortcuts |

---

## 1. Timeline of changes (this session, Grok)

### 1.1 Pre-session advisory (docs only)
- Wrote `#.grok…/15.GROK_AU6-10.txt` (+ §11 focus-first pivot, §12–13 flag trials).
- Prioritized: fix human/KHTPM nav before rancher event-ez product work.

### 1.2 Focus recovery experiments (tp_desktop_window + tp_taskbar)
**Goal:** restore keyboard focus that “used to work” before locks/grabs fights.

| Experiment | Result |
|---|---|
| Feature flags: registry/popup locks OFF; X grabs OFF | Locks alone not enough; without grabs, menus lost keys |
| Option C: grabs ON + soft focus on **popup only** | **Context menus OK** |
| Taskbar sticky `XGrabKeyboard` while Nav active | **Broke context** (AlreadyGrabbed) — over-protective |
| Smart yield (ungrab when KIND=row) | Too many rules; user rejected |
| Full-width bar grab “like context” | Still not parity |
| Floating **Nav popup** (second window + pointer+keyboard grab) | **Double Nav UI**, blocked entity context — **removed** |
| Simplify: **no grab on taskbar** | Context free again |

**Standing rule learned:**  
Context menus = own small override_redirect + pointer/keyboard grab (keep).  
Taskbar = **no sticky grab**; do not invent a second Nav window.

### 1.3 `$.crypts` — quit before re-launch
**Files:** `$.crypts/ops/crypt_autostart.c`, `$.crypts/button.sh`

- Every successful `run` / `restart` / `$` shortcut now:
  1. `CLOSE` all `livedesk_open.txt` packages  
  2. SIGTERM taskbar + entity PIDs  
  3. `/proc` sweep for leftover `tp_taskbar` / `tp_desktop_window`  
  4. Clear open + nav claims  
  5. Then LAUNCH rows  
- Fixes dual-taskbar / stale-binary stacks after rebuilds.

### 1.4 Dual taskbar race
**File:** `tp_desktop_window.c` → `ensure_taskbar_running()`

- Pid-file race with crypts concurrent launch could spawn **two** bars.
- Fix: also scan `/proc` for existing `tp_taskbar` + house_root before spawn.

### 1.5 Toolbar UX (final shape this session)

**File:** `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`

| Removed | Kept / added |
|---|---|
| Middle **Nav >** typing box | Tabs + shortcuts + X only |
| Floating Nav popup | — |
| Sticky keyboard/pointer grab on bar | Soft focus only when armed |
| `#digits` next to tab names | Digits only in small right status `[NAV]` / `[12]` |

**Nav model (wraith-ish engage, not a type-in field):**

| Input | Behavior |
|---|---|
| **Right-click** taskbar | Arm nav (`nav_armed`) |
| **Left/Right/Up/Down** | Move tab `[>]` |
| **Digits** | Bounded accum (chtpm-style vs `max_claimed_nav`) → jump `[>]` immediately |
| **Enter** (no digits) | Activate focused tab → raise + **`OPEN_CONTEXT`** |
| **Enter** (digits) | Jump by shared `NAV=` (tab or open menu row) |
| **Esc** / left-click tab | Disarm; left-click tab **raise only** (no forced menu) |

**Digit accum (mirrors `chtpm_parser_pal`):**  
`new_val = accum*10+d` only if `1..total_nav`; else restart with `d` if valid; arrows clear accum; hard digit-length cap.

### 1.6 Context menu digit → `[>]` (chtpm parity)

**File:** `tp_desktop_window.c`

- `popup_digit_accum` + on-digit **move `popup_focus_row`** (global nav index in this menu’s range).
- Enter still activates focused row.
- Arrows clear digit accum.
- Relay **`FOCUS_NAV:<N>`**: move `[>]` only (taskbar uses this for open-menu addresses).
- Relay **`OPEN_CONTEXT`**: same path as right-click (reload methods, open menu at entity). Used when toolbar nav activates a **tab**.

### 1.7 Popup on-screen clamp (earlier same session)

**File:** `tp_desktop_window.c` → `clamp_popup_to_screen()`

- Context / text popups stay on-screen above taskbar band; position writeback for submenus.

### 1.8 Feature flags still in desktop (locks default OFF)

Top of `tp_desktop_window.c` (approx):

```
LIVEDESK_USE_REGISTRY_LOCK   = 0
LIVEDESK_USE_POPUP_LOCK      = 0
LIVEDESK_USE_XGRAB_POINTER   = 1
LIVEDESK_USE_XGRAB_KEYBOARD  = 1
LIVEDESK_POPUP_SOFT_FOCUS    = 1
```

Taskbar: **no** sticky grab flags in final form (soft focus only).

---

## 2. Relay vocabulary (livedesk / KHTPM) — agent + UI

Written to `<package_dir>/interact_relay.txt` (consumed once):

| Command | Effect |
|---|---|
| `OPEN_CONTEXT` | Open base context menu (as right-click) |
| `FOCUS_NAV:<N>` | Move menu `[>]` to global nav N |
| `ACTIVATE_NAV:<N>` | Activate row N if menu open |
| `NAV_KEY:Up\|Down\|Enter` | Remote cursor (still present; toolbar no longer primary) |
| `RUN_METHOD:<Label>` | Dispatch method by label |
| `SHOW_PAGE:` / `SHOW_TEXT_FILE:` | Book-stack style (prior session) |
| `CLOSE` | Exit entity window |

Shared addressing: `#.desktop/livedesk_nav_claims.txt` (`KIND=tab` / `KIND=row`).

---

## 3. How to rebuild / restart

```bash
# Taskbar
gcc -Wall -O2 -o "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x" \
  "&.widgits/livedesk-taskbar/ops/tp_taskbar.c" -lX11

# Entity window
gcc -Wall -O2 -o "&.widgits/tile-picker/ops/+x/tp_desktop_window.+x" \
  "&.widgits/tile-picker/ops/tp_desktop_window.c" -lX11 -lXext -lGL -lGLX

# Crypts (quit-then-launch)
gcc -Wall -O2 -o "$.crypts/ops/+x/crypt_autostart.+x" "$.crypts/ops/crypt_autostart.c"

# Live restart
sh $.crypts/button.sh run   # or click $ on taskbar
```

Expect **one** `tp_taskbar` after clean `$`.

---

## 4. Explicitly NOT finished (next sessions)

From original rancher / events track (not done this session after nav pivot):

1. Change Gold `prisc+x` truth check on m8 / wrapper `event.pal`  
2. Wire **event-ez** into MUCHI_RANCHER `meta.pdl`  
3. Show Choices in ez + KHTPM  
4. Feed as real event page  
5. Media studio Phase-1 (parallel)

When entering those dirs, add a same-day `SESSION_EDIT_HISTORY_*.md` (or walk-off) there too.

---

## 5. Pitfalls for the next agent

1. **Do not** re-add sticky taskbar keyboard grab without a plan — it kills entity menus.  
2. **Do not** re-add a second floating Nav popup.  
3. Trust **files over walk-offs** for `event.pal` wrapper state (already disagreed once).  
4. After rebuilds, always **`$` / crypt quit** — otherwise dual taskbars + old binaries.  
5. `OPEN_CONTEXT` requires **new** `tp_desktop_window` processes (restart entities).  
6. Wayland: soft focus on the bar can still be flaky for keys; right-click arm + digit jump is the intended path; context menus use real grabs.

---

## 6. File checklist (touched this session)

| File | Nature of change |
|---|---|
| `&.widgits/tile-picker/ops/tp_desktop_window.c` | Locks flags, soft focus, clamp, OPEN_CONTEXT, FOCUS_NAV, digit→[>], ensure_taskbar /proc, OPEN_CONTEXT |
| `&.widgits/livedesk-taskbar/ops/tp_taskbar.c` | Full nav UX rewrite; final: no middle box, right-click arm, bounded digits, activate→OPEN_CONTEXT |
| `$.crypts/ops/crypt_autostart.c` | quit_current_livedesk before LAUNCH |
| `$.crypts/button.sh` | `restart` alias; help text |
| `#.desktop/*` | Runtime state only (claims/open cleared on quit) |
| `#.grok…/15.GROK_AU6-10.txt` | Advisory + addenda |
| `@.apps/hikikomorai/SESSION_EDIT_HISTORY_2026-08-06.md` | **This file** |

---

*End of 2026-08-06 livedesk/hikikomorai edit history.*
