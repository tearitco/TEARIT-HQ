# OP-ED: The Sovereign RPG Editor

## 1. Overview
OP-ED is a "Thin Engine" RPG Maker for TPMOS. It allows users to design worlds, pieces, and events that are immediately playable via the PAL runtime.

### The Sovereign Architecture
Unlike traditional editors that use a single project file, OP-ED treats each game as a **Sovereign Context**.
-   **Context Switch:** Loading a game copies the entire directory structure into the Editor's working RAM (Disk Buffer).
-   **Encapsulation:** Maps, Pieces, and Scripts are all contained within the game's unique folder in `projects/op-ed/games/`.
-   **Baseline:** Standardized on the `agy-text-editor` file interaction model for Save As / Load.

## 2. Current State (m24)
- [x] **Milestone 1: File Baseline.** Standardized browser integrated. Context switching works.
- [x] **Slice 1: Mirror Lab.** `fuzz-op-mirror` and `test-game-01` projects created for sanity testing.
- [x] **Slice 2: Thin Engine Player.** Minimal PAL-based player (`player_loop.asm`) handles realtime movement (Mode 1) and PRISC interactions (Mode 2).
- [ ] **Slice 3: Event Editor (In Progress).** Moving from raw PAL editing to high-level "Block" building.

## 3. Road to Alpha (Roadmap)

### Phase 1: Behavioral Logic (Current)
-   **High-Level Event Editor:** Implement "Op Selection" builder that generates `.asm` scripts automatically.
-   **Logic Injection:** Connect the Editor's "EVENT" button to the script writing pipeline.

### Phase 2: Standalone Exporter
-   **Project Bundler:** Tool to package a game project with a minimal `prisc+x` and `parser+x` runtime for distribution.
-   **Manifest Generation:** Auto-generate `boot.asm` to point the system root to the local game directory.

### Phase 3: Advanced Assets
-   **Z-Level Editing:** Full support for multi-layered maps.
-   **Custom Traits:** No-code trait binder for pieces (e.g., adding "Inventory" or "Health" to any entity).

## 4. Engineering Standards
- **Mantra:** "If it's not in a file, it's a lie."
- **One Writer Rule:** `current_frame.txt` is owned by the Parser. Daemons trigger renders via `frame_changed.txt`.
- **Mode 1 vs Mode 2:** WASD movement is realtime (Direct C); Interaction is scripted (PRISC).

---
*Status: Behavioral Scaffolding. Next: High-Level Event Builder.*
