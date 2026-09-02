# Handoff: piececraft-xyz — P1 Clone Phase (restored 2026-08-03)

**From:** Claude Haiku 4.5
**To:** Next agent picking up piececraft-xyz — assume ZERO context, read this whole doc first.

## ⚠️ Clone Restoration Complete — Mutation Removed

piececraft-xyz was previously in a broken state: the agent tried to build Minecraft mechanics (gravity, 3D position, custom keybinds) INSIDE the clone phase, violating the design's explicit "clone first, diverge later" order. This created a hybrid that was neither a working clone nor a working game.

**Fixed 2026-08-03:** piececraft-xyz is now a **true clone of civ-txt's P1 skeleton**, not a mutant. The clone phase is complete but UNVERIFIED — the next session must verify it.

---

## 1. WHAT'S BUILT AND CURRENT STATE

### 1a. piececraft-xyz itself (P1 clone phase only)
- **Exact replica of civ-txt's P1 structure** (from CIV_TXT_DESIGN.md's own proof-of-concept phase)
  - Real setup screen (Victory/Map/Combat, independently selectable) → `CONFIRM_START` → real navigation to main screen → real `END_TURN` loop with ledger logging
  - Board generation on `CONFIRM_START` (same random terrain glyphs as civ-txt: `.`=plains, `f`=forest, `^`=hills, `~`=water, `C`=capital)
- `OPEN_BOARD_WIDGET` command (in `pc_menu_input.c`) has the full ledger-discovery + spawn/refocus logic ported from civ-txt
- **All identifiers adapted for piececraft-xyz only** (`pc_menu_input`, `pc_compose_frame`, `piececraft-xyz` in paths/strings, etc.) — zero new game logic

### 1b. What the clone looks like right now
- `pieces/system/config.txt` — generic game config (game_id, turn, victory_condition, map_scale, combat_resolution, etc.) **NOT** player position or game-specific state
- `pieces/system/board.txt` — ASCII terrain grid (same format as civ-txt)
- `ops/pc_menu_input.c` and `ops/pc_compose_frame.c` — word-for-word adapted from civ-txt's equivalents, only cosmetic renames
- `default_op.txt` — references the pc_* ops only
- Ledger posting on END_TURN works (same as civ-txt)

---

## 2. WHAT'S NOT BUILT (and must NOT be started yet)

**NOTHING Minecraft-specific.** Per the design (PIECECRAFT_XYZ_DESIGN.md §11 Phase 1):
- No 3D storage or chunking — storage is still P2+ work
- No terrain generation beyond the P1 one-time board fill
- No place/break/inventory mechanics
- No gravity, Z-axis position state, or voxel-specific config
- No custom keybinds (board-viewer's existing camera modes 1–4 are reused, that's all)
- No terrain_legend.txt (board-viewer's Phase 0 work, not piececraft-xyz's)

---

## 3. CRITICAL: NEXT STEP IS VERIFICATION, NOT DIVERGENCE

**Do NOT start Phase 2 (Minecraft features) yet.** The clone must be verified first:

1. **Build and run piececraft-xyz** — does it compile? Does the button.sh work? Does it launch?
2. **Walk through the clone manually** — setup screen working? Board generated? END_TURN works? Ledger appends?
3. **Gold-file diff against civ-txt** — both projects' `CONFIRM_START` should generate identical board.txt (seeded same way). Both `END_TURN` should produce identical ledger entries.
4. **Update/create test-harn-same/** — copy civ-txt's proof/test fixtures, adapt them for piececraft-xyz, verify regression tests pass identically.

**Only once the clone is verified to be byte-identical to civ-txt (modulo game_id) should Phase 1 divergence begin** (§11 Phase 1 in the design: "world/chunk storage, terrain gen for ONE BIOME, camera+xlector wired for 3D voxel nav including Y, NO compression yet").

---

## 4. Real Bugs / Known Issues

None reported in the clone restoration — the ops are faithful copies of civ-txt's, only renamed.

---

## 5. Recommended next steps (in order)

1. **Build the project** (`button.sh build` or equivalent)
2. **Run it interactively** — test setup screen, confirm board generates, test END_TURN
3. **Compare ledger output** against civ-txt's — should be identical except for game_id/project_id
4. **Create proof/ and test-harn-same/ if missing** — copy structure from civ-txt, adapt paths
5. **Write user-walkthru.txt** — a step-by-step manual verification guide (copy civ-txt's as a template)
6. **Once verified, update PIECECRAFT_XYZ_DESIGN.md** to reflect "Clone phase VERIFIED 2026-08-XX" before moving to Phase 1 divergence

---

*End of handoff. Game state: fresh setup screen, no options picked — check pieces/system/config.txt. The clone is now structurally sound; the next session must verify it works.*
