


# !.clone-clowning.md — Handoff: house "clone games" continuous improvement

| Field | Value |
|-------|--------|
| **Doc** | Clone-clowning handoff (agents & humans) |
| **Date** | 2026-07-28 |
| **House root** | this directory (`44.xyz…`) |
| **Status** | living document — update when a clone graduates or is demoted |
| **Primary law** | Mutaclysm / CHTPM / master_ledger / dual ASCII+GL — **not** freeglut-only product paths |

---

## 0. Why this document exists

We spawned a wave of "clone" games (craft, DF, civ, pokemon, RPG Maker MZ, Star Wars Battlefront, TTG-tactics). Some are freeglut vertical slices that look cool but **break house laws** (state in RAM, no ASCII frame, no harness key inject, no master ledger). Future agents must:

1. **Drive continuous quality** on the priority clones.
2. **Migrate architecture** toward mutaclysm standards (file-mediated I/O, dual render, human-readable maps, entity dirs, turn/tick frames).
3. **Ignore low-priority clones** unless explicitly asked.
4. **Use this file as the permanent handoff** for critique loops, harnesses, and PR slices.

If you are a future agent and context is compacted: **read this file first**, then the per-project `DESIGN.md` / `CRITIC_REPORT.md` / `UI_EXPECTATION.md`.

---

## 1. Architecture north star (mutaclysm)

**Canonical signal flow** (`CHTPM_ARCHITECTURE_GUIDE.txt`, `101.mutaclsym*/dox/05-file-mediated-io-architecture.md`):

```
keyboard_input  →  history.txt (decimal keycodes; harness/AI write same file)
       →  chtpm_parser_pal / interact_relay  (or TTG dispatch that honors same history)
       →  prisc+x / game loop ops
       →  data/master_ledger.txt  + entity state files under pieces/ or data/
       →  ops/compose_frame  →  pieces/display/current_frame.txt   [ASCII = truth]
       →  compose_rgb / chtpm_rgb_render  →  rgb_frame.raw
       →  system/renderer (terminal) + system/gl_mirror (GL blit only)
```

### Non-negotiables for "house standard" clones

| Law | Meaning |
|-----|---------|
| **L1 Dual view** | Always both **ASCII** (`current_frame.txt` + terminal renderer) and **GL** (`gl_mirror` on RGB). `NO_GL=1` only for headless harness. |
| **L2 File-mediated** | Process boundaries talk via plain files — not sockets/pipes for core loop. |
| **L3 Same input path** | Humans, AI, harnesses append keycodes to the **same** history file. |
| **L4 Master ledger** | Append-only (or hybrid + reconstruct) human-readable ledger of actions. |
| **L5 Human-readable maps** | Board/world as text (`map.txt`, grid in frame, PDL tables) agents can read/edit. |
| **L6 Entity directories** | Units/actors/buildings live under dirs with `state.txt` / small PDL files. |
| **L7 Tick / turn frames** | Update loop is tick or turn based; compose full frame; pulse file **size** growth wakes renderers. |
| **L8 Nav-friendly UI** | Continuous numbered nav or clear cursor; maps + side HUD; no GL-only menus. |
| **L9 Kill hygiene** | `button.sh kill` stops exact names; no orphan 60fps redraw zombies. |
| **L10 No freeglut product path** | Freeglut may exist as **A/B mock** or temporary viz, never as sole product brain. |

### Reference package (gold standard)

- **`101.mutaclsym*`** — dual render, orchestrator, fonts under `pieces/registry/fonts/ascii/`, ledger, maps, ops/+x.
- **`%.harnesses/*`** — workdir + proof + key inject + frame asserts.
- **`101.lpns+map*`** — multi-seat turns, ledger reconstruction patterns.
- **`&.widgits/event-editor`** — CHTPM→rgb→gl_mirror product path lessons (fonts, ASCII boxes).

---

## 2. Priority matrix (agents: respect this)

### P0 — active quality + architecture migration

| Project | Path | Today | Target |
|---------|------|-------|--------|
| **RPG Maker MZ clone** | `201.rpg-maker-clone/` | Freeglut one-page MZ editor; procedural tiles; maps/events on disk | Keep freeglut as **visual mock A** if needed, but add **muta-style dual path** (or package rewrite): human-readable maps already exist under `projects/demo/maps/` — strengthen ledger/events, ASCII overview mode, harness paint/play proofs, quality chrome vs `REFERENCE_rpg_maker_mz_expected.jpg` |
| **SW Battlefront** | `204.sw-battlefront/` | Freeglut + GLEW shaders; 3 modes; RAM state | **Highest arch debt.** Port product brain to file-mediated loop: match state files, ASCII radar/board HUD frame, RGB mirror, harness key fly/shoot. Keep shaders as **optional GL presentation** fed by RGB compose, not sole state. |
| **TTG-Tactics** | `205.ttg-tactics/` | **Design-only** (rev 2 approved) | **Build here first.** Full house standard from day one per `DESIGN.md`. |

### P1 — park / ignore unless user re-opens

| Project | Path | Note |
|---------|------|------|
| glut-craft | `200.glut-craft/` | Voxel sandbox freeglut; park |
| dwarf-fortress | `201.dwarf-fortress/` | Park |
| snes-civ | `202.snes-civ/` | Park |
| gb-pokemon | `203.gb-pokemon/` | Park (was GBC amend; freeglut) |

### Related house (not "clones" but shared DNA)

- `101.mutaclsym*` — standard to copy
- `101.lpns+map*` — turn seats / ledger
- `&.widgits/*` — widgets, event-editor dual path
- `%.harnesses/*` — k3-style testing

---

## 3. Per-project agent briefings

### 3.1 `205.ttg-tactics` — BUILD (canonical new work)

- **Spec:** `205.ttg-tactics/DESIGN.md` (rev 2, implementable; Key Decisions §15; PR Plan §16).
- **Summary:** `205.ttg-tactics/DESIGN_SUMMARY.md`
- **Product:** 12×12 community-piece tactics, clocks 2/5/10/30, ante pot, Elo, AI via keycodes, dual render 48×22 frame.
- **MVP = PR0–PR7** in DESIGN.md (skeleton → move → attack → clock → pot → AI keys → dual render → Elo).
- **Harnesses:** design §10 scenarios `01`–`08` under `%.harnesses/ttg-tactics/` (create when implementing).
- **Do not** introduce freeglut main. Optional later viz only if RGB path already works.

**Definition of done (MVP ship tag):**
- `sh button.sh compile && sh button.sh run` shows title + match frame in **terminal and GL**
- Human moves unit; AI answers via history inject
- Regicide ends match; pot settles; Elo file updates
- Harnesses 01–07 green; 08 dual render receipt green

### 3.2 `201.rpg-maker-clone` — QUALITY + ARCH BRIDGE

**Already good for data:**
- `projects/demo/maps/<id>/map.txt`, `map_obj.txt`, `events/ev_x_y/`
- Map tree, tools, procedural A–R tileset

**Push quality:**
- Visual parity with `REFERENCE_rpg_maker_mz_expected.jpg` / `UI_EXPECTATION.md`
- Event editor depth; transfer maps; play camera; less "CS homework" HUD (see old `CRITIC_REPORT.md`)
- Animation (water/lights already tick-based) + denser factory demos

**Push architecture (incremental):**
1. **Export mode:** write `current_frame.txt` ASCII overview of map+cursor each save/tick (even if freeglut still draws).
2. **Ledger:** append paint/event actions to `data/master_ledger.txt` under project.
3. **Harness:** `%.harnesses/rpg-maker-clone/` inject keys or write map cells + assert map.txt.
4. Long-term: optional CHTPM shell that edits same project files as freeglut (single source of truth = files).

**Ignore for now:** full PNG autotile atlas unless quality pass needs it.

### 3.3 `204.sw-battlefront` — QUALITY + ARCH MIGRATION

**Already good for feel:**
- Supremacy / Deathmatch / Freeplay, ships, AI bots, shaders

**Arch debt:** state in `Game` struct in RAM; no ASCII frame; no ledger; kill/run freeglut-only.

**Migration ladder (do in order):**
1. **Snapshot files every tick or event:** `data/match_state.txt`, `pieces/units/*/state.txt` (ships/infantry), `data/master_ledger.txt` (kills, caps, deaths).
2. **compose_frame:** top-down ASCII radar (posts, ships as glyphs, tickets HUD) → dual renderer package.
3. **Input bridge:** freeglut keys also append history; or replace input with keyboard_input and drive sim from files.
4. **Quality:** better ship silhouettes in RGB glyphs, readable supremacy minimap in ASCII, pot-free tickets clarity, FPS/dirty draw (no thrash).

**Product quality bar:** still "engrossing dogfight" but **agents can watch `current_frame.txt` and inject keys**.

---

## 4. Continuous critique / playthrough loop

Future agents should run a **standing loop** (subagents or workflow):

```
for project in [205, 201, 204]:  # priority order
  1. Read this handoff + project DESIGN/UI/CRITIC
  2. Build agent: implement next PR slice
  3. Critic agent: harsh visual + arch laws (L1–L10) + fun
  4. Harness agent: k3-style key inject + frame/ledger asserts
  5. Update project CRITIC_REPORT.md + this file "Status board"
  6. Kill all stray procs (exact names only)
```

### Critic axes (score 1–10)

| Axis | Question |
|------|----------|
| **Arch law** | Dual view? Ledger? Entity dirs? File I/O? |
| **Visual** | Readable, beautiful, map nav friendly? |
| **Gameplay** | Addictive loop in 2 minutes? |
| **Harness** | Can AI play without GUI driver? |
| **Kill safety** | No CPU thrash orphans? |

Write results to:
- `201.rpg-maker-clone/CRITIC_REPORT.md` (exists — append dated section)
- `204.sw-battlefront/CRITIC_REPORT.md` (create if missing)
- `205.ttg-tactics/CRITIC_REPORT.md` (create after first build)
- Optional rollup section at bottom of **this** file

### Harness pattern (k3 / house)

Mirror `%.harnesses/file-menu+mutaclysm/`:

```
%.harnesses/<clone-name>/
  button.sh          # compile | demo | kill
  scenarios/*.sh     # append keys, assert files
  proof/             # expected frame snippets / receipts
  workdir/           # isolated tree if needed
  README.txt
```

**Always:** `printf '<keycode>\n' >> pieces/apps/player_app/history.txt` (or project-equivalent path).  
**Never:** "test by only reading freeglut framebuffer" as sole proof.

---

## 5. Status board (update every session)

| Project | Arch | Quality | Harness | Next PR / task | Owner note |
|---------|------|---------|---------|----------------|------------|
| **205.ttg-tactics** | Dual-render loop live | Early playable | 01–03 green | PR3 clocks / Elo / AI keys | Active |
| **201.rpg-maker-clone** | Freeglut+files | Mid (v4 MZ) | Weak | ASCII frame export + ledger paint | Quality+bridge |
| **204.sw-battlefront** | Freeglut RAM | Mid (shaders) | None | Snapshot+ASCII radar | Migrate carefully |
| 200 craft | Freeglut | Mid | None | **PARK** | — |
| 201 DF | Freeglut | Mid | None | **PARK** | — |
| 202 civ | Freeglut | ? | None | **PARK** | — |
| 203 pokemon | Freeglut | ? | None | **PARK** | — |

---

## 6. Process hygiene (CPU / orphans)

From prior thrash incidents:

```sh
# Prefer exact names
pkill -x rpg_clone 2>/dev/null || true
pkill -x sw_battlefront 2>/dev/null || true
pkill -x gl_mirror 2>/dev/null || true
pkill -x chtpm_rgb_render 2>/dev/null || true
# project button
sh 201.rpg-maker-clone/button.sh kill
sh 204.sw-battlefront/button.sh kill
sh 205.ttg-tactics/button.sh kill   # once exists
# muta
# sh 101.mutaclsym*/button.sh kill  if needed
```

Do **not** `pkill -f` patterns that match the agent shell command line.

Clones that use `glutTimerFunc(16)` will burn CPU while the window lives — **kill when not testing**.

---

## 7. Suggested agent prompts (copy-paste)

### Build TTG PR slice
```
Read !.clone-clowning.md and 205.ttg-tactics/DESIGN.md §15–16.
Implement next open PR (start PR0 if no button.sh). House dual-render law.
No freeglut product. Harness scenario for the PR.
```

### Critic RPG / Battlefront
```
Read !.clone-clowning.md and project UI/CRITIC docs.
Harsh review: arch L1–L10 + visual + fun. Write CRITIC_REPORT dated section.
Propose smallest bridge PR toward muta dual path.
```

### Harness pass
```
Create/extend %.harnesses/<project>/. Key inject only. Assert current_frame / ledger / map files.
```

---

## 8. Session log (append-only)

### 2026-07-28
- Designed `205.ttg-tactics` (rev 2, reviewer **approve**, 0 open issues).
- Built freeglut experiments 200–204; **P0 active: 205, 201, 204**; park 200/202/203/DF.
- Created this handoff.
- Implemented TTG PR0–PR2 slice: `ttg_loop`, dual compose, unit dirs, ledger, harness 01–03 PASS.
- Vendored muta gl_mirror/renderer/keyboard_input via scripts/vendor_system.sh.
- Next: clocks polish, Elo file, richer key-only AI, then RPG/BF quality bridges.

---

## 9. Quick file index

| Path | Role |
|------|------|
| `!.clone-clowning.md` | **This handoff** |
| `CHTPM_ARCHITECTURE_GUIDE.txt` | Runtime law |
| `ARCHI_TEST_SUM-J28.txt` | Test status notes |
| `101.mutaclsym*` | Gold runtime |
| `205.ttg-tactics/DESIGN.md` | TTG full spec |
| `201.rpg-maker-clone/UI_EXPECTATION.md` | MZ UI bar |
| `201.rpg-maker-clone/REFERENCE_rpg_maker_mz_expected.jpg` | Visual target |
| `204.sw-battlefront/README.md` | BF modes/controls |
| `%.harnesses/` | k3 harnesses |

---

*End of handoff. Agents: update §5 Status board and §8 Session log when you ship something.*

### 2026-07-28 (later)
- Quick freeglut clone: `007-goldeye-clysim/` — voxel GoldenEye 2–4p split, seed gen, AI panes. **Not** dual-render standard; park under "fun freeglut" unless user promotes.


!. user note. those that aren't in "pal/tpmos/ xyzos standard, arent meant to be ,and are uses for reference and
or slated for future conversion
