# muta-zoo.md — Harness & product notes (mutaclysm + widgets + zoo orbit)

**House:** `44.xyz…`  
**Date:** 2026-07-28  
**Status:** documentation only (no new harness implementation in this note)  
**Audience:** humans + agents before coding the next mutaclysm / widget / zoo slice  

---

## 1. What “muta-zoo” means here

**muta-zoo** is the *working name* for the multi-project slice that ties together:

| Piece | Role |
|--------|------|
| **mutaclysm** (`101.mutaclsym…`) | Live RPG world: `pieces/world_01/<map>/` maps + entities |
| **file-menu** widget | Focus-adaptive SAVE/LOAD (editor *or* mutaclysm save slots) |
| **tile-picker** widget | Paint emoji/glyph onto a mutaclysm cell |
| **map-picker** widget | List maps, switch map, teleport xlector/hero |
| **user xyzfs** | Durable per-user docs + save slots (not only install tree) |
| **zoo** (orbit) | Later: pets in/out of simplified zoo; same “visible, not open-the-code” maker vibe |

This file lives under **`%.harnesses/`** because the *proof* of these features is almost always **cross-project** (same placement rule as `file-menu+editor/` and `#.drag-drop-test/`). Unit chrome can still live under each package’s `test-harn-same/`.

---

## 2. Laws that apply (do not re-derive)

| Doc | Rule |
|-----|------|
| `#.haiku+/!.xyzos-standards+1.txt` **§35** | **GL is primary**; ASCII secondary; widgets = `run-widget` (GL on, ASCII off) |
| **§36** | Per-project harness vs **`%.harnesses/<pair>/`** for multi-project |
| Pitfall **48** | Idle must not spam frames |
| Pitfall **49** | Do not require a second human terminal for widgets |
| `&.widgits/WIDGETS_ROADMAP.txt` | Full product plan for the three widgets + user FS |
| `&.widgits/file-menu/widget+plan.txt` | file-menu + cmd bus; §12 mutaclysm extension |

---

## 3. Already proven (baseline — do not break)

### 3.1 Editor × file-menu cmd bus

| | |
|--|--|
| **Harness** | `%.harnesses/file-menu+editor/` |
| **Run** | `./button.sh demo` |
| **User reports** | that harness + `&.widgits/file-menu/USER_REPORT.txt` |

**Proven:**

1. Editor session publishes `widget_bridge.txt`  
2. file-menu `fm_set_focus` → editor session  
3. `fm_enqueue` **LOAD** / **SAVE_AS** / **NEW**  
4. Editor `editor_widget_cmds` drains inbox → buffer/disk update  

**Not proven yet:** GL file-menu UI, mutaclysm save slots, tile/map pickers.

### 3.2 Editor alone

`102.editor-…/test-harn-same/` — INTERACT canvas (type, backspace, arrows, CLEAR).

### 3.3 Mutaclysm live + legacy save (install tree)

Live world (example maps under install):

```text
101.mutaclsym…/pieces/world_01/
  map_start/ map_02/ map_gen_01/ building_01_gf/ building_01_f2/ …
    map.txt  furniture.txt  hero/  monsters/  items/  state.txt  …
```

Legacy in-game save (existing op):

```text
ops/save_game.c
  → cp -r pieces/world_01/ → pieces/saves/save_N/world_01/
```

New product path (planned) moves **named user slots** into **xyzfs**, not only `pieces/saves/`.

---

## 4. Target architecture (what we are building toward)

### 4.1 User filesystem (xyzfs)

```text
xyzfs/users/<user_uuid>/home/projects/
  agy-editor/
    docs/                 # editor SAVE/LOAD home
  mutaclysm/
    saves/
      demo-project/       # seeded from install world or template
        meta.pdl
        world_01/         # full map+entity tree
      <user-slot-name>/
        meta.pdl
        world_01/
```

Guest: same layout under guest-uuid (login-signup pattern).

### 4.2 file-menu when focus = mutaclysm

| Verb | Meaning |
|------|---------|
| **NEW GAME** | Wipe/reset **live** `pieces/world_01` (default plan: from `world_01_template`) |
| **SAVE** | Snapshot live → current user slot |
| **SAVE AS…** | Browser of `…/mutaclysm/saves/` dirs + new name |
| **LOAD GAME…** | Copy `saves/<name>/world_01` → live `pieces/world_01` |
| **CHANGE FOCUS** | Other running processes |

When focus = **editor**, browser stays **docs/** text files (existing cmd-bus verbs).

### 4.3 demo-project seed

On first need (missing demo slot):

1. Prefer populated install `pieces/world_01/`  
2. Else `pieces/world_01_template/`  
3. Copy → `xyzfs/.../mutaclysm/saves/demo-project/world_01/`  
4. Idempotent: do not clobber if demo already exists (unless force)

So **NEW / wipe live** never destroys the only playable content.

### 4.4 tile-picker (`&.widgits/tile-picker/`)

1. Click emoji in **widget GL**  
2. Click cell on **mutaclysm** level  
3. Glyph appears (UTF-8-safe `map.txt` / place_tile-class op)  
4. Cmd first: `PLACE_TILE:<map>:<x>:<y>:<glyph>`; GL hit-test second  

### 4.5 map-picker (`&.widgits/map-picker/`)

1. List map dirs under live `world_01/*`  
2. User picks map → `SWITCH_MAP:<id>`  
3. Session current map updates  
4. **Teleport hero + xlector/selector** onto that map  

### 4.6 Zoo orbit (later, not this harness’s first green bar)

House already has zoo packages (`002.zoo…`) and notes about pets in/out of a simplified zoo, then “RPG maker / scratch-visible events.” **muta-zoo** is the umbrella name so harness docs and later `#.zoo-save-load` / pet-exchange work stay under one mental model: **visible maker surfaces + mutaclysm as the beautiful runtime**, without forcing agents to open C to understand save/load/map/paint.

---

## 5. Planned harness packages under `%.harnesses/`

| Path | Proves | Status |
|------|--------|--------|
| `file-menu+editor/` | Widget cmd bus × editor buffer | **GREEN** (ALL PASS 2026-07-28) |
| `file-menu+mutaclysm/` | User save slots, NEW/SAVE/LOAD world, demo-project survives wipe | **GREEN** (ALL PASS 2026-07-28) |
| `map-picker+mutaclysm/` | List maps, SWITCH_MAP, hero/xlector on target map | **GREEN** (ALL PASS 2026-07-28) |
| `tile-picker+mutaclysm/` | Brush + PLACE_TILE mutates map.txt | **GREEN** (ALL PASS 2026-07-28) |
| `proc-monitor/` | register / list / focus / soft / kill / gc | **GREEN** (ALL PASS 2026-07-28) |
| (optional later) `muta-zoo-pets/` | Pets in/out of simplified zoo × mutaclysm | **TODO** (out of scope) |

### Suggested first green bar: `file-menu+mutaclysm`

Scenario sketch (mirrors `file-menu+editor`):

1. Compile mutaclysm + file-menu ops + muta_save/load/new/seed ops  
2. Seed **demo-project** into a harness-controlled user FS root (or guest xyzfs)  
3. Boot mutaclysm session (or headless session root with live world)  
4. file-menu focus → mutaclysm  
5. **SAVE_AS** `t1` → assert `saves/t1/world_01/map_start/map.txt` exists  
6. Mutate live map.txt (or run a tiny op)  
7. **LOAD** `t1` → assert live map.txt matches saved  
8. **NEW** → live changes; **LOAD demo-project** still works  
9. Dump proof/ under this harness  

Unit mutaclysm tests (if any) stay under `101.mutaclsym…/`; cross-story stays here.

---

## 6. KPI cheat sheet

### User FS / demo
- **UFS1** editor docs path under xyzfs  
- **UFS2** mutaclysm SAVE creates user `saves/<name>/world_01/`  
- **UFS3** guest tree works  
- **D1–D3** demo-project seed, load after wipe, idempotent seed  

### file-menu × mutaclysm
- **M1** demo listed in mutaclysm browser mode  
- **M2** SAVE_AS creates slot  
- **M3** LOAD restores prior map bytes  
- **M4** NEW does not destroy demo-project  
- **M5** editor focus still text-browser (no regression vs file-menu+editor)  

### map-picker
- **P1** list matches live map dirs  
- **P2** SWITCH_MAP updates current map  
- **P3** hero + xlector under target map after switch  

### tile-picker
- **T1** brush state file updates  
- **T2** PLACE_TILE changes map cell  
- **T3** wrong focus → error, no crash  

---

## 7. Implementation order (before coding)

1. User FS provision + **demo-project** seed  
2. `muta_save_slot` / `muta_load_slot` / `muta_new_game` (+ seed op)  
3. file-menu mutaclysm profile (save-dir browser + cmds)  
4. **`%.harnesses/file-menu+mutaclysm/`** green  
5. map-picker list + switch + teleport  
6. tile-picker brush + PLACE_TILE (cmd first)  
7. GL `run-widget` chrome for widgets  
8. Zoo pet orbit when muta save/load is stable  

**Do not start GL-first polish until save/load + demo-project are harness-green.**

---

## 8. Agent / human checklist

- [ ] Read this file + `&.widgits/WIDGETS_ROADMAP.txt`  
- [ ] Do not put multi-project mutaclysm×widget demos only under one package  
- [ ] Keep editor cmd-bus harness green when touching file-menu  
- [ ] Seed demo before any NEW that wipes live world  
- [ ] Prefer file-mediated cmds over inventing sockets  
- [ ] GL primary for user-facing widgets; harness may assert files without display  

---

## 9. Pointers

| Path | What |
|------|------|
| `%.harnesses/file-menu+editor/` | Working multi-project harness |
| `%.harnesses/muta-zoo.md` | **This document** |
| `&.widgits/WIDGETS_ROADMAP.txt` | Product phases for three widgets |
| `&.widgits/file-menu/` | Widget ops + plan + USER_REPORT |
| `&.widgits/tile-picker/README.txt` | Stub |
| `&.widgits/map-picker/README.txt` | Stub |
| `101.mutaclsym…/ops/save_game.c` | Legacy install-tree save (cp -r) |
| `0.user-pal/…/xyzfs/README.txt` | User tree layout |
| `#.drag-drop-test/` | Cross-project harness shape reference |

---

## 10. One-line status

**Editor × file-menu cmd bus is proven.**  
**Mutaclysm user save slots, demo-project, tile-picker, map-picker, and muta-zoo harnesses are planned here and in WIDGETS_ROADMAP — not coded yet.**

---

*End muta-zoo.md*

---

## 11. After widgets work well — @apps (do not build yet)

See **`#.notes/AFTER-widgets-apps-store.txt`**.

Summary: a project + widgets can ship as one **@app** (optional toolbar).
Only register in **@app-store** / install to user profile **after** you have
run that combo manually and want a faster open. Pretend-prod: profile
remembers installs. Cloud/billing/AI/premium/support chat later under user —
not blocking maker work.

Also: **`%.harnesses/file-menu+mutaclysm/`** harness for save-slot proof.

---

## 12. proc-monitor widget

**`&.widgits/proc-monitor/`** — task list for house processes (headless or GL):
FOCUS for other widgets, SOFT QUIT / KILL, refresh/GC registry.

Details: `&.widgits/WIDGETS_ROADMAP.txt` §5b (PROC-MONITOR).
Depends on xyzfs runtime process registration (same as CHANGE FOCUS).
