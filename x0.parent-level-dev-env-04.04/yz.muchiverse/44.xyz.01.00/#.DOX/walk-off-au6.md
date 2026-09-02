# walk-off-au6.md — livedesk / MUCHI_RANCHER / event-ez handoff
**Date:** 2026-08-06  
**Audience:** next human or agent picking this up cold  
**Also read:** prior walk-offs (`walk-off-au5.md` under MUCHI_RANCHER, event-ez, tile-picker, livedesk-taskbar), `MUCHI_RANCHER_DESIGN.md`, `&.widgits/event-ez/HOW2_USER_GUIDE.md`, `HOW2_event-ez_change_gold_k3.txt`, `TILE_PICKER_DESIGN.md` §9–§13.

---

## 1. One-line status

**Change Gold is end-to-end real** (author in event-ez → IR → wrapper `event.pal` → Play → `inventory.txt` qolq).  
**Menu RPG shell is real** (Level / XP / qolq / HP / MP pages).  
**Context menus size to content.**  
**Clear All Commands works.**  
**CPU idle is healthy** (fixed Events feedback loop + ≤30fps caps).  

**Not done:** Show Choices, trigger runtime, edit-in-place commands, multi-save hygiene polish, full dogfood of every path without agent help.

---

## 2. What we finished (2026-08-06 session arc)

### 2.1 event-ez — visual compiler (Change Gold)

| Piece | Status | Notes |
|---|---|---|
| Gallery → Page → Cmdpick → Change Gold | Working | Only working command type listed (no dead buttons) |
| KEY:5 Save Trigger | Working | Writes `condition.pdl` only |
| KEY:6 Save Change Gold | Working | Appends NODE; full recompile of `event.pal`; `cmd_N.sh` wrappers |
| KEY:7 Clear All Commands | **Fixed this session** | Was button-only; now wipes NODEs, empty pal+halt, deletes wrappers, keeps trigger, forces UI reparse via `piece_methods` |
| IR source of truth | Working | `event.ir.pdl` NODEs; pal always regenerated |
| prisc one-arg `exec` | Working | Wrappers bake entity_dir + amount |
| k3 dogfood Change Gold | **PASS** | See `@.apps/MUCHI_RANCHER/HOW2_event-ez_change_gold_k3.txt` |

**KEY encoding:** CHTPM injects digit **ASCII** (`'5'==53`, `'6'==54`, `'7'==55`). Handlers use `key == '5'` etc., not integer 5.

### 2.2 MUCHI_RANCHER — Play + Menu + qolq

| Piece | Path / notes |
|---|---|
| open Events (ez) | `@.apps/MUCHI_RANCHER/ops/open_event_ez.sh` — sets `EZ_PKG_*`, launches event-ez |
| open Menu (RPG) | `@.apps/MUCHI_RANCHER/ops/open_rp_menu.sh` — builds `objects.pdl` pages: main, activities, **menu** (stats), items, skills, status |
| Play event | `@.apps/MUCHI_RANCHER/ops/play_event.sh` — runs page `event.pal` via prisc |
| Change Gold op | `@.apps/MUCHI_RANCHER/ops/mr_change_gold.+x` — mutates entity `inventory.txt` `qolq=` |
| Stats labels | Menu page shows `Level`, `XP: n / next`, `qolq (gold): N`, HP/MP — refreshed when Menu reopens (script rewrites objects.pdl) |

Entity under active test: **`m6_golddeity`** (also **`m8_redhorned`** has event_pkg history).  
`m6` page_1 was cleared during Clear All verification — re-author commands as needed.

### 2.3 KHTPM context menus (`tp_desktop_window.c`)

| Piece | Status |
|---|---|
| Content-aware popup width | **Done** — measure longest row (header + labels + nav prefix); `g_popup_w`; floor 180 / ceiling 640; used in create/draw/hit-test/submenu offset |
| Fixed thin 160px menus | **Fixed** — was clipping XP / qolq lines |
| objects.pdl multi-page | Working — GOTO / BACK / Cancel; reload from disk on open (stale Events(ez) fix) |
| Shared nav claims | Working — `#.desktop/livedesk_nav_claims.txt` |
| Focus / XWayland grabs | Improved earlier — WM_CLASS `MuchiverseLivedesk`, soft focus, popup flock |
| Idle GL | **Done** — no swap when `need_redraw==0`; frame sleep remainder if under 30fps budget |

### 2.4 CPU / FPS throttle (critical)

**Symptom:** `chtpm_rgb_render` ~40%, `gl_mirror` ~20%, `chtpm_parser_pal` ~10% while Events sat idle.

**Root cause:** `ez_compose_frame` **always** appended `ez_screen_changed.txt`. Pal loop recomposes when that file grows → self-feeding ~25 full frames/sec.

**Fixes:**

1. `ez_compose_frame.c` — only pulse `frame_changed` if `view.txt` fingerprint changes; **never** touch `ez_screen_changed` (only `ez_menu_input` bump).
2. `event-ez/pal/main_loop_chtpm.pal` — sleep **33333µs** (30fps max).
3. `101.mutaclsym*/system/chtpm_parser_pal.c` — usleep **33333** (was 16667 ≈ 60fps). **Live event-ez uses mutaclsym hardlinked system bins.**
4. `gl_mirror.c` idle_tick — usleep **33333** (was 16000).
5. Kill **zombie** event-ez sessions before retest (multiple prisc left running).

**Idle after fix (measured):** rgb ~0.2%, gl_mirror ~0.4%, parser ~0.4%, taskbar ~0.3%, each entity tile ~0.6–0.8%; frame pulses **0 B/2s**.

### 2.5 Other fixes this arc (pre-au6 / same day)

- Focus recovery, crypts restart-before-launch, toolbar nav claims  
- Events (ez) on context menu via objects.pdl (not only meta.pdl)  
- Back no longer wiping gallery rows (compose always rebuilds page list)  
- Multi-save debounce on Change Gold  
- open_rp_menu rewrite (broken heredoc / empty SCRIPT)  
- Session frame history cap / less spam  

---

## 3. What is NOT done (handoff backlog)

Priority order suggested:

### P0 — human / agent verify still

1. **Play → qolq → reopen Menu** on a clean entity: author Change Gold 25, Play, open Menu, confirm `qolq (gold)` label matches `inventory.txt`.  
2. **Clear All** from GL: list empties, status line shows “Cleared N…”, Play no longer applies old wrappers.  
3. **Wide menus** on long labels after respawn (already shipped; quick eyeball).

### P1 — product gaps (designed or partial)

| Item | Notes |
|---|---|
| **Show Choices** | Designed in event-editor visual-compiler doc §7; not listed in cmdpick; needs KHTPM `SHOW_PAGE` / branch pal call-return |
| **Trigger runtime** | `condition.pdl` saved; **not evaluated** at Play (no on_click / on_spawn / parallel enforcement) |
| **Edit / delete one command** | Save only appends; Clear All is bulk-only; no edit-in-place |
| **Single-command delete** | Not built |
| **Multi-save hygiene** | Debounce 2s same amount; still can spam different amounts; decoys / history noise possible |
| **Load IR into form** | No “open existing NODE into Change Gold field” |

### P2 — platform / polish

| Item | Notes |
|---|---|
| Stale `livedesk_open.txt` after SIGKILL | Tab can linger |
| `OPEN_USER` not remote-navable | Needs live popup geometry |
| `tp_range_grid` not KHTPM | No history/injection |
| Pets/asa/ava | Use same binary when relaunched; verify methods still correct |
| event-ez session isolation | Always one GL session; `button.sh kill` + check ps before retest |
| House-std name “KHTPM” | Informal; not formalized in `!.HOUSE_STDS.md` |

### P3 — further content

- Other monsters’ `event_pkg` once pattern is dogfooded  
- Items / Skills menu pages still placeholders  
- Feed / Train / etc. still void actions  

---

## 4. Key paths (house-relative)

```
@.apps/MUCHI_RANCHER/
  entities/m6_golddeity/          # active ranch entity + inventory.txt
  entities/*/event_pkg/pages/page_N/
    event.ir.pdl                  # IR truth
    event.pal                     # compiled
    cmd_*.sh                      # prisc wrappers
    condition.pdl                 # trigger (not runtime-enforced yet)
  ops/open_event_ez.sh
  ops/open_rp_menu.sh
  ops/play_event.sh
  ops/mr_change_gold.c → ops/+x/mr_change_gold.+x
  HOW2_event-ez_change_gold_k3.txt
  walk-off-au5.md                 # older pause; this file supersedes for 08-06

&.widgits/event-ez/
  ops/ez_compose_frame.c          # gallery/page compose + CPU ping fix
  ops/ez_menu_input.c             # KEY 5/6/7
  pal/main_loop_chtpm.pal         # 30fps sleep
  HOW2_USER_GUIDE.md
  button.sh                       # r | compile | kill

&.widgits/tile-picker/
  ops/tp_desktop_window.c         # KHTPM menus, width, idle GL
  TILE_PICKER_DESIGN.md

&.widgits/livedesk-taskbar/
  ops/tp_taskbar.c                # ~300ms poll; Nav terminal

#.desktop/
  livedesk_open.txt
  livedesk_nav_claims.txt
  livedesk_master_ledger.txt

101.mutaclsym*/system/            # event-ez runtime bins (hardlinked into sessions)
  chtpm_parser_pal.c/.bin
  gl_mirror.c/.bin
  chtpm_rgb_render
```

---

## 5. Restart / hygiene recipes

### Livedesk entities (after rebuilding `tp_desktop_window`)

```bash
cd <house>
bash "&.widgits/tile-picker/button.sh" compile
# restart each entity on new binary (same package path from ps cmdline)
```

### event-ez

```bash
cd <house>/&.widgits/event-ez
sh button.sh compile
# kill ALL event-ez sessions first (zombies burn CPU)
sh button.sh kill          # or carefully kill by session id
export EZ_PKG_NAME=m6_golddeity
export EZ_PKG_DIR="$(pwd)/../../@.apps/MUCHI_RANCHER/entities/m6_golddeity/event_pkg"
# from house root, EZ_PKG_DIR absolute path preferred
sh button.sh r
```

### Preflight CPU safety (still mandatory)

```bash
ps -eo pid,pcpu,args | awk '/chtpm_parser_pal|gl_mirror|chtpm_rgb_render|event-ez.*prisc/ && !/awk/'
# ideally ONE event-ez stack only while testing Events
xwininfo -root -tree 2>/dev/null | grep -c "mutaclsym RGB mirror"   # 0 before, 1 after
```

### Rebuild mutaclsym system (parser / gl_mirror FPS)

event-ez sessions **hardlink** `system/*.c` from mutaclsym and **copy/run** mutaclsym binaries. After editing FPS:

```bash
cd 101.mutaclsym*/
# scripts/build.sh  OR
gcc -std=c11 -Wall -O2 -Wno-unused-result -Wno-stringop-truncation \
  -o system/chtpm_parser_pal system/chtpm_parser_pal.c
gcc -std=c11 -Wall -O2 -o system/gl_mirror system/gl_mirror.c \
  -lglut -lGL -lGLU -lX11
# then restart event-ez so session picks up new bins
```

---

## 6. Dogfood path (happy path for next agent)

1. Clean event-ez (one session).  
2. Open entity → **Events (ez)** → Page 1 → **Clear All** (if junk) → **New Event Command** → **Change Gold** → amount `25` → Save → Back.  
3. Confirm list shows `• Change Gold: 25`.  
4. Close event-ez cleanly.  
5. Entity menu → **Play** (or play_event.sh).  
6. Entity → **Menu** → confirm **qolq (gold)** matches `inventory.txt`.  
7. `top` / `ps`: no multi-session zombies; event stack idle &lt; few % when Events closed.

---

## 7. Design north star (unchanged)

MUCHI_RANCHER is the **proving ground** for RPG-Maker-style event authoring:

- Human (or k3) clicks event-ez GUI  
- Output is real executable `event.pal`  
- Desktop **Play** / context action runs it  
- Next hard milestone: **Show Choices** with real branch call/return via KHTPM  

---

## 8. Doc map

| Doc | Role |
|---|---|
| **This file** (`walk-off-au6.md`) | Current handoff — done / not done / recipes |
| `walk-off-au5.md` (per project) | 2026-08-05 pause state; still useful history |
| `HOW2_event-ez_change_gold_k3.txt` | Agent-proven Change Gold inject sequence |
| `&.widgits/event-ez/HOW2_USER_GUIDE.md` | Human-facing Events UX |
| `MUCHI_RANCHER_DESIGN.md` | Product design |
| `TILE_PICKER_DESIGN.md` §9–13 | KHTPM deep dive |
| `!.HOUSE_STDS.md` §H | prisc exec, naked vars, GL title collisions |
| `#.haiku+/!.xyzos-pitfalls+1.txt` **PITFALL 70** | Toolbar nightmare + context focus — **do not get fancy** |
| `103.media-studio/103.daw/walk-off-a6-daw.txt` | DAW Phase-1: what works, GarageBand roadmap, agent handoff |
| `103.media-studio/103.vid-edit/walk-off-a6-video.txt` | Video Phase-1: preview+timeline, ffmpeg export, next steps |

---

*End handoff 2026-08-06. Prefer updating this file (or a new walk-off-auN) rather than only burying status in chat.*
