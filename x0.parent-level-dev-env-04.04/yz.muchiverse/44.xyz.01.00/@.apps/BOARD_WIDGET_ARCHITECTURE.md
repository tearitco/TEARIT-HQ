# 🪟 BOARD WIDGET ARCHITECTURE — Shared Map/Board Viewer for civ-txt & tactics-txt

**Status: PLAN ONLY — nothing in this document is built yet.** Written 2026-08-02, after a same-session correction: an earlier pass wrongly implemented civ-txt's "View Map" as a same-session CHTPM screen reached via `<button href>`. **That was architecturally wrong** and must be reverted (see §7). This document is the researched, correct replacement plan, written before any further code so a future agent (or this session, resuming) can implement it without re-deriving the house's real widget standard from scratch.

**Read `&.widgits/WIDGIT_BIBLE.md` in full before touching any of this** — this document summarizes and applies it, but the Bible is the canonical source and has pitfalls/debugging detail not repeated here.

---

## 0. THE MISTAKE, STATED PLAINLY (so it isn't repeated)

`@.apps/civ-txt/pieces/chtpm/layouts/map.chtpm` currently exists as a screen inside civ-txt's own CHTPM session, reached by a `<button href="pieces/chtpm/layouts/map.chtpm">` on `main.chtpm`. Clicking it **changes civ-txt's own view** — same process, same session, same GL window (if any), just a different rendered screen.

**That is not what a "map widget" means in this house.** A widget is a **separate program**, its own process, its own session, its own GL window — spawned by the host on a keypress, not navigated to via `href`. The user's own words: *"clicking map button for civ shouldn't change its view. it should open a completely new gl window by triggering widget and pairing data."*

§7 below has the concrete revert/fix plan.

---

## 1. THE REAL HOUSE STANDARD — Two-Program Widget Pattern

Summarizing `&.widgits/WIDGIT_BIBLE.md` §1 (read that file for full detail):

- A widget and its host are **two completely separate programs**: own session dir, own `system/` binaries, own `ops/`, own PAL loop. Never a subprocess/thread of the host, never a screen inside the host's own CHTPM session.
- **Communication is file-mediated only** — no sockets, pipes, shared memory, or D-Bus:
  - Host → Widget: key forwarding via `interact_relay.txt`
  - Widget → Host: command bus, `widget_cmds/inbox.txt` → `widget_cmds/status.txt`
  - Both: discovery via the **xyzfs runtime ledger**
- **Two launch profiles** every program should support: `app` (default `./button.sh run`, GL primary + ASCII secondary, owns TTY) and `widget` (`./button.sh run-widget` or `RUN_PROFILE=widget`, GL **required**, ASCII off, no TTY).
- **Session isolation**: widget sessions live under the widget project's own `pieces/sessions/<id>/` (NOT `/tmp` — that's only for `app`-profile host sessions), symlinking read-only assets, real files for mutable state.

### 1a. The `gl_mirror.c` version matters — a concrete, confirmed gotcha

Two incompatible copies of `gl_mirror.c` exist in this house:

| Version | `interact_relay.txt` forwarding | Use for |
|---|---|---|
| `014.wsr-pal💸️📌️+2/system/gl_mirror.c` | ✅ Yes — real widget keyboard control | **Widgets** (this feature) |
| `101.mutaclsym🧟‍♂️️+18.01/system/gl_mirror.c` | ❌ No — mirror-only, no key forwarding | Plain read-only text mirrors (already wired into my-chara-txt/civ-txt/tactics-txt's own basic GL windows this session) |

**The board widget MUST use the 014.wsr-pal version.** The mutaclysm version already ported into the three game projects' own `system/gl_mirror.c` this session is correctly left as-is — those are intentionally passive text mirrors of each game's own main UI, not meant to be controlled from the GL window. The board widget is a new, separate binary and gets its own copy of the wsr-pal version.

### 1b. The widget's ASCII output still exists even though it's not shown by default — real headless testability

`ascii_renderer` (§1's own launch-profile table) is a genuinely independent flag, not a structural GL-only limitation. `chtpm_parser_pal` (pipeline stage 2, `WIDGIT_BIBLE.md` §3) **always** writes `pieces/display/current_frame.txt` — a real, plain ASCII character grid — regardless of whether anything GL-related is running; `chtpm_rgb_render` (stage 3) just reads that same file to rasterize it for the GL path. In `widget` profile, `system/renderer` (the binary that polls `current_frame.txt` and prints it to a terminal) simply **isn't started** (`ascii_renderer=0`) — that's a launch choice, confirmed independently swappable per `WIDGIT_BIBLE.md`'s own headless/CI section (§10: `chtpm_parser_pal` ✅ works headless, `renderer` (ASCII) ✅ works headless, only `gl_mirror` ❌ needs a real `$DISPLAY`).

**Practical implication, worth using**: the board widget can be verified **without a real X server** — either read `current_frame.txt` directly (matching every other harness scenario built this session), or launch the widget with `ascii_renderer=1` (or just run `system/renderer` manually against its session dir) to see its exact content live in a plain terminal. This is the same headless-testability property every other project in this house already relies on, and should be the board widget's own primary test path (GL-window verification stays a manual/visual spot-check, same division already used for my-chara-txt/civ-txt/tactics-txt's own basic GL mirrors this session).

---

## 2. WIDGET SCOPE — One Shared, Focus-Adaptive Widget

Per direct instruction: **one shared widget**, not two per-project ones — "shared widgets are more modular... data driven and call ops depending on the paired game's needs." This matches `file-menu`'s own proven pattern exactly (`&.widgits/WIDGETS_ROADMAP.txt` §0: file-menu adapts its browser root and verbs to whichever project has focus — editor vs. mutaclysm).

**Proposed location:** `&.widgits/board-viewer/` (name open to bikeshedding, not load-bearing).

**Focus-adaptive behavior**, modeled directly on file-menu's own `focus = agy-editor` / `focus = mutaclysm` branching:

| Focus (`project_id`) | Board content source | Verbs |
|---|---|---|
| `civ-txt` | `pieces/registry/...`, civs/cities/units (once real, per `CIV_TXT_DESIGN.md` P4+) | pan/select tile, (later) issue `MOVE_ARMY`/`INVADE_CITY` etc. via cmd bus |
| `tactics-txt` | `pieces/battle_01/board/`, `units/` (once real, per `TACTICS_TXT_DESIGN.md` P2+) | pan/select tile, (later) issue `MOVE:<unit>:<x>:<y>` etc. via cmd bus |

The widget's own compose op reads a `focused_project_id` field from its own state, and branches its board-rendering logic exactly the way `civ_compose_frame.c`/`tactics_compose_frame.c` already branch on `active_piece` — same pattern, one more axis.

---

## 3. TRIGGERING — Keypress Spawns/Focuses the Widget, Not an `href`

Concrete flow, replacing the wrong `href` approach:

1. Player presses the "View Map" key/button on `main.chtpm` (civ-txt) or `main.chtpm` (tactics-txt).
2. That keypress is handled by `civ_menu_input.c`/`tactics_menu_input.c` as a new command, e.g. `OPEN_BOARD_WIDGET`.
3. The handler does **not** navigate screens. It either:
   - **Spawns** `&.widgits/board-viewer/button.sh run-widget <own_session_root>` as a detached background process (if no live board-viewer widget is registered in the ledger for this `project_id` yet), or
   - **Focuses** the already-running widget (writes a `SET_FOCUS:<project_id>:<session_root>` command to its cmd-bus inbox, per §4) if one is already alive.
4. The host's own screen **does not change** — civ-txt/tactics-txt keeps showing whatever screen it was on. The board widget opens as a **second, separate GL window**.

This mirrors `WIDGIT_BIBLE.md`'s own "Editor FILE menu: spawn `&.widgits/file-menu/button.sh run-widget`, not: open a second terminal and run file-menu" guidance exactly, with civ-txt/tactics-txt in the "editor" role and board-viewer in the "file-menu" role.

---

## 4. REAL-TIME SYNC — the mechanism, worked through in full (per direct instruction: "document this arch thoroughly before implementation")

**The key realization: this does not need continuous IPC traffic, because everything is already file-mediated.** The widget and the host both have filesystem access to the SAME real files — the widget doesn't need a live feed pushed to it; it can simply **read the host project's real state files directly**, the same way a second `button.sh run` session of the same project already shares `data/master_ledger.txt`/`pieces/system/config.txt` with a first session (already proven, this exact sharing pattern is how my-chara-txt/civ-txt/tactics-txt's own session isolation already works — session dirs symlink read-only assets but share real mutable-state files).

Concretely:

1. When the host spawns/focuses the widget (§3), it passes its own **real project root** (not a copy) — e.g. `/home/.../@.apps/civ-txt` (the actual project directory, not a `pieces/sessions/<id>/` ephemeral session dir, since THAT gets deleted on host exit but the real project root persists) — as the `focused_project_root` value, written into the widget's own session state and/or the ledger entry itself.
2. The widget's own compose op (`bv_compose_frame.c`, say) reads `focused_project_root`, then reads that root's own **real** `pieces/system/config.txt`, `pieces/registry/...`, city/unit piece directories, etc. — directly, by absolute path, exactly like `civ_compose_frame.c` already reads its own project's `pieces/system/config.txt` via `PRISC_PROJECT_ROOT`. The widget just points that same read logic at a **different** project's root instead of its own.
3. The widget's own PAL loop already polls-and-recomposes on some interval (same `no_key: sleep 30000 / j loop` shape every project in this house uses) — since it's now reading the HOST's real files each cycle, **it naturally reflects the host's real-time state with no additional plumbing**, the same idle-poll cadence already proven safe (0-2% CPU) in every other project this session.
4. **Writes stay one-directional and command-bus-mediated** (§5) — the widget never writes directly into the host's `config.txt`/piece directories. All host-state mutation still goes through the host's own `civ_menu_input.c`/`tactics_menu_input.c`, keeping the "one writer per file" discipline this whole house already enforces (Pitfall 14/15/18/19 lineage). The widget only *reads* host state directly; it *requests* changes via the cmd bus.

This is real, buildable, and requires no new IPC primitive — it's the direct consequence of "the widget knows the host's real project root and reads its files like any other reader would." The only genuinely new plumbing is **passing that root across** (§3's spawn/focus step) and the **write-path command bus** (§5).

---

## 5. COMMAND BUS — Widget → Host (control actions)

Same shape as file-menu's own (`WIDGIT_BIBLE.md` §7), reused directly:

```
pieces/system/widget_cmds/inbox.txt    (host reads; widget writes)
pieces/system/widget_cmds/status.txt   (widget reads; host writes ACK/NACK)
```

Command format, civ-txt/tactics-txt-specific verbs added to the same one-line-per-command shape file-menu already uses (`NEW`/`SAVE`/`LOAD:<path>`/`PING`):

- `SELECT_TILE:<x>:<y>` — widget's own xlector-equivalent cursor selected a tile; host may use this for context (e.g. "what's here" readout) without it being a full action.
- Future, once real gameplay exists: `MOVE_UNIT:<unit_id>:<x>:<y>`, `ATTACK:<unit_id>:<target_id>`, etc. — issued from the widget's own action menu when a unit/tile is selected, drained by the host's own existing dispatcher (`civ_menu_input.c`/`tactics_menu_input.c`) exactly like a real keypress would be, just arriving via the inbox instead of `interact_relay.txt`.

**Not building the actual verb set yet** — P1 scope for this widget is VIEW-ONLY (pan, cursor, view modes, emoji toggle). Control verbs are named here so the command-bus shape is future-proofed, not because they're in scope now.

---

## 6. VIEW MODES / CAMERA / EXTRUSION — SUPERSEDED, moved to its own document

**This section originally proposed a "5 numbered view modes" design (4 ported from mutaclysm + emoji collapsed in as a 5th mode). That proposal was WRONG and has been fully superseded** after direct correction and further investigation confirmed: (1) the widget genuinely gets a real 3D raymarch mode with 2D content **extruded** into it (not a flat-only widget as originally assumed), and (2) `render_mode` (2D/3D) and `emoji_mode` (flat/textured) are **two separate, orthogonal flags**, exactly matching mutaclysm's own real, working code — not collapsible into one 5-way selector.

**The full, corrected, code-grounded design lives in `&.widgits/5-pov-widgit.md` — read that document for the camera/POV/extrusion/emoji system in full.** It covers: the confirmed `render_mode`/`camera_mode`/`emoji_mode` semantics (with exact file:line citations), the real extrusion precedent (`extrude-emoji.md`, mutaclysm's own documented-but-unfixed entity-extrusion gap, which this widget will build for the first time), the terrain-selective extrusion decision, and the open questions still blocking implementation.

The camera-clamp + selector-cursor material originally in this section (§6a) is still accurate and portable as described — mutaclysm's own `ops/compose_frame.c` camera-clamp math and `ops/move_player.c`/`ops/choice.c` xlector-cursor logic — but is now covered in `5-pov-widgit.md` §6a's own restatement alongside the corrected view-mode system, to keep the camera/rendering material in one place rather than split across two docs.

---

## 7. THE FIX FOR CIV-TXT'S CURRENT (WRONG) MAP SCREEN

Concrete revert/replace plan, not yet executed (planning-first per direct instruction):

1. **Remove** the `href`-based navigation: `main.chtpm`'s `<button label="View Map" href="pieces/chtpm/layouts/map.chtpm" />` becomes a `piece.pdl` METHOD row instead (`OPEN_BOARD_WIDGET`), dispatched by `civ_menu_input.c` per §3 — no more same-session screen switch.
2. **Delete or repurpose** `pieces/chtpm/layouts/map.chtpm`, `pal/map_module.pal`, and the `civ_compose_frame.c` "map" branch — none of this belongs in civ-txt's own project once the real widget exists. (The placeholder-grid rendering logic they contain is NOT wasted — it's the right starting content for the WIDGET's own board-render branch, just needs to move to the widget's own project, reading civ-txt's root per §4 instead of being civ-txt's own screen.)
3. civ-txt's own `button.sh` gains the spawn/focus logic from §3 in its `civ_menu_input.c`'s `OPEN_BOARD_WIDGET` handler (or a small dedicated op, `civ_open_widget.c`, matching the "every verb = one op" convention).

**Not yet done.** Flagging clearly rather than silently leaving the wrong implementation in place unexplained — the current `map.chtpm`/`View Map` button in the live civ-txt tree as of this document's writing is the OLD, WRONG approach and should be treated as superseded by this document, not as working functionality.

---

## 8. DRAG-AND-DROP RE-PAIRING (future, explicitly out of scope for v1 — documented per direct instruction as "a very pivotal moment in architecture")

### 8a. What actually exists in this house (researched this session, not guessed)

`101.drag-drop-test=ON🀄️` is a **test harness**, not the implementation — it drives two real apps (`101.mutaclsym🧟‍♂️️+18.01/system/gl_mirror.c` as target, `01.muchi-pals-🥚️-13.01/system/egg_window.c` as source) via `xdotool`-simulated mouse events, then asserts on resulting files.

**Real XDND (the actual X11 drag-and-drop protocol) was tried and explicitly abandoned.** Both `xdnd_target.c`/`.h` (mutaclysm) and `xdnd_source.c`/`.h` (muchi-pals) still exist in-tree but are **not compiled/linked into any binary** — confirmed by their own header comments and by `button.sh` not building them. Six real protocol bugs were fixed (wrong X display connection, missing `XSetSelectionOwner`, etc., documented in `#.DOX/temp/drag-drop-bugs.txt`), and it **still never worked end-to-end** afterward, for two further reasons: (1) window manager reparenting broke the self-window-lookup the fgDisplay-based approach depended on, and (2) — **directly relevant given this session's own CPU-safety concerns** — the idle poll driving XDND event checking had **no throttle**, "the exact CPU-spin bug that crashed the machine once already" in an earlier prototype.

**What ships instead, and works**: a manual **"coordinate+file handoff."** Source tracks its own drag via plain `ButtonPress`/`MotionNotify`/`ButtonRelease` (not XDND messages). On release, it finds the target window by title (recursive `XQueryTree` + `XFetchName`, survives WM reparenting), computes whether the release point is inside the target's on-screen rect (`XTranslateCoordinates`), and if so, writes a plain text file (write-to-tmp-then-`rename()`, never a direct write — avoids the receiver reading a half-written file) into the **target's own project root**. The target's own GLUT idle callback polls for that file **with an explicit throttle** (`usleep(16000)` — the fix for the exact crash-causing pattern above) and consumes it.

### 8b. Why this doesn't already solve "re-pair the widget to different data"

What's proven is **content import into an already-fixed data root** (a pet gets imported into mutaclysm's existing world — mutaclysm's own `PRISC_PROJECT_ROOT`/data source never changes as a result of the drop). **Re-pairing** — dropping a "civ-txt" icon onto the already-open board widget so it switches from showing civ-txt's board to showing, say, tactics-txt's board, live, without restarting the widget process — has **no existing precedent anywhere in this house**. This is genuinely new design, though it follows the exact same proven shape:

**Proposed mechanism (design only, not built):**
1. Some future "game icon" source (a launcher tray, or each game's own window acting as its own drag source per the existing `egg_window.c` pattern) computes a drop onto the board widget's window (found by title, same recursive lookup).
2. On a valid drop, it writes a file into the **board widget's own project root** (`&.widgits/board-viewer/incoming_focus_change.txt` or similar) containing the new `project_id` + real project root path to re-target.
3. The widget's own idle poll (same throttled shape as §8a, reusing the exact `usleep()` fix that resolved the one documented crash-causing version of this pattern) detects the file, updates its own `focused_project_root` state (§4 step 1-2), and its very next compose cycle starts reading the NEW project's real files instead — no restart needed, since §4's read mechanism was already designed to be root-parameterized, not hardcoded to one project at launch.
4. The widget re-registers its ledger entry's `project_id` field to reflect the new focus (so other programs querying the ledger see the current pairing correctly).

**This is deliberately not being built in v1.** Named here in full because the user flagged it as architecturally pivotal — a future session should be able to build exactly this without re-deriving the drag-drop research from scratch, and should reuse the throttled-idle-poll pattern verbatim given its own documented history of causing a real crash when unthrottled.

---

## 9. OPEN QUESTIONS — genuinely undecided, ask before building

1. **Mode 3 (free pan) key scheme**: if the camera detaches from the selector in free-pan mode, what moves the camera vs. what moves the selector? Mutaclysm's own mode 3 uses `wasd` for pan — but this widget's selector likely also wants arrow-key movement. Need to either share keys contextually (arrows = selector always; wasd = camera pan only in mode 3) or pick a different scheme. Not resolved by anything ported from mutaclysm since mutaclysm's own hero-vs-camera split doesn't map cleanly (no "hero" here).
2. **Widget project name/location**: `&.widgits/board-viewer/` is a placeholder name in this doc — confirm before creating the directory.
3. **Ledger entry `type` field**: `WIDGIT_BIBLE.md`'s ledger schema has a `type` column (`editor`/`widget`/`app`/`daemon`) — should the board widget's own `project_id` be a single fixed value (`board-viewer`) regardless of which game it's currently focused on, or should it change its own registered `project_id` to match its current focus target? (Affects how OTHER future widgets/tools would discover "the board viewer," vs. discovering "whatever's currently showing civ-txt.")
4. **civ-txt's/tactics-txt's own text-mirror GL windows** (the mutaclysm-sourced, non-interactive ones already wired this session) — should these be REMOVED once the real interactive board widget exists (avoiding window clutter / redundant GL windows per game), or kept as a lighter-weight always-on mirror separate from the on-demand widget? Not decided.

---

## 10. BUILD ORDER (once the above questions are answered)

| Phase | What | Depends on |
|---|---|---|
| P1 | Revert civ-txt's wrong `map.chtpm` (§7) | — |
| P2 | Scaffold `&.widgits/board-viewer/` per file-menu's own proven structure (button.sh with app/widget profiles, ops/, pal/, own `pieces/registry/fonts/`, own copy of 014.wsr-pal's real `gl_mirror.c`) | §9 Q2 |
| P3 | Spawn/focus trigger in civ-txt (`OPEN_BOARD_WIDGET` command, §3) | P2 |
| P4 | Real-time read of civ-txt's root from the widget (§4), rendering the SAME placeholder grid civ-txt's own (now-deleted) map.chtpm had | P3 |
| P5 | Camera clamp + selector cursor (§6a) | P4 |
| P6 | 5 view modes (§6b) | P5 |
| P7 | Cmd bus write-path (§5), starting with `SELECT_TILE` only | P6 |
| P8 | Port the whole widget to also focus-support tactics-txt (§2), once `tactics-txt` has real board data (its own P2, movement) to show | P7, tactics-txt's own P2 |
| P9 | Drag-and-drop re-pairing (§8) | P8, all open questions resolved |

---

*End of plan. Nothing above is implemented. Read `&.widgits/WIDGIT_BIBLE.md` and `&.widgits/file-menu/widget+plan.txt` directly before starting P1 — this document summarizes them but is not a substitute for the source docs' own pitfalls sections.*
