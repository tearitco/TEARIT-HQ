# AU24-oc-handon.md — Handoff for fresh agent ( virgin context )
**Date:** 2026-08-24
**Author:** Kilo (on behalf of user)
**Target agent:** Any agent picking this up cold. Read this entire file before doing anything else.

> **STATUS NOTE (2026-08-24, added during doc-compaction pass):** §4 (CURSword entity) is
> now PARTIALLY DONE — the spawn mechanism, entity template, and chat-HQ wiring described
> in §4.1/4.2/4.4 and most of §4.3 are DONE and relay-verified, see `CURSWORD-HQ-SPAWN.md`
> in this directory for the real current state (read that FIRST for §4, then come back here
> for what's still open in it — minimize/windows-list/voice input-output per §4.3.1 are
> NOT done yet). **§1, §2, §3, §5, §6, §7-10 are still live, open backlog, not superseded
> by anything** — this doc was reviewed in full during compaction and kept intact on
> purpose rather than shrunk to a stub.

---

## 0. WHERE WE ARE (do not overstate progress)

### Events system — what is PROVEN working
- **One event command proven end-to-end:** `Change Gold` (+35 on right-click Play) via `event-ez` authoring → `event.ir.pdl` → `cmd_N.sh` wrappers → `play_event.sh` → `prisc+x` → entity window.
  - Verified via pure relay injection (`RUN_METHOD:Play` into `interact_relay.txt`).
  - Verified persistence across desk switches.
- **Multi-page / multi-trigger dispatch** — `play_event.sh` scans `event_pkg/pages/page_*`, picks highest-numbered page whose `condition.pdl` trigger matches. Default trigger = `on-click`. Switches/variables as conditions NOT yet built.
- **Session common events** — same `event_pkg/pages/page_N/` shape rooted at `sessions/<session>/common_events/`. Verified via direct `play_event.sh` invocation (0→50 gold). No UI trigger point wired yet.
- **event-ez IS db-ez** — the existing `&.widgits/event-ez` GUI pointed at a session package (`EZ_PKG_DIR=sessions/s4/common_events/event_pkg`). No new code needed.
- **Ops are shared, events are session-private** — `mr_change_gold.+x` lives in `xyzfs/bin/muchi-pet/ops/+x/`. Events stay in `sessions/<user>/<session>/` until published to the **store** (taskbar cell 12, currently inert).

### What is NOT yet working / built
- No `Show Text`, `Show Choices`, `Input Number`, `Wait`, `Play SE`, etc. — only `Change Gold` has a working op binary.
- No message/choice UI rendering in entity windows (khtpm_show_text.+x exists but only writes to relay; no popup renderer wired yet).
- No FSM/page-condition evaluation beyond trigger-name matching.
- No store UI for publishing events.
- No CURSword entity.
- No idle animation system.
- No hum sound.

### Key files you must read before touching anything
| File | Why |
|---|---|
| `yz.muchiverse/#.#.calendar-dox/1.^V-hq/EVENTS_RUNTIME.md` | Full runtime architecture, bug history, verification recipes |
| `yz.muchiverse/#.#.calendar-dox/1.^V-hq/EVENT_AI_VISION.md` | Priority order, design constraints, RPG Maker trigger taxonomy |
| `&.widgits/event-ez/ops/ez_menu_input.c` | How event-ez generates `cmd_N.sh` wrappers (the compiler half) |
| `&.widgits/event-ez/ops/ez_compose_frame.c` | How event-ez renders the event editor GUI |
| `xyzfs/bin/muchi-pet/ops/play_event.sh` | The runtime dispatcher — multi-page, multi-trigger |
| `xyzfs/bin/muchi-pet/ops/+x/mr_change_gold.+x` | The ONLY proven event command op. Study its shape. |
| `.monads/*.livedesk-taskbar/ops/+x/khtpm_show_text.+x` | Show Text relay writer (already built, no build script) |
| `.monads/*.livedesk-taskbar/ops/+x/khtpm_show_choices.+x` | Show Choices picker (already built) |
| `.monads/*.book-stack/entities/book-stack/meta.pdl` | Example entity meta.pdl (METHOD dispatch format) |
| `#.ref/menu/event.commands.1.txt` | RPG Maker event command reference list |

---

## 1. PROVE SHOW TEXT WORKS (first task)

`khtpm_show_text.+x` already exists and works as a relay writer (verified earlier this session). The gap is wiring it through the **event runtime pipeline** (event.pal → cmd_N.sh → play_event.sh → entity window) just like Change Gold.

### Step 1.1 — Create a Show Text op binary
`mr_change_gold.+x` is the template. Create `mr_show_text.+x` in the same `xyzfs/bin/muchi-pet/ops/+x/` directory.

**Contract:** `mr_show_text.+x <entity_package_dir> <text_file>` — reads the text file, writes it into the entity's `inventory.txt` under a `show_text=` key (or appends to `master_ledger.txt` with a SHOW_TEXT marker). Keep it simple. Study `mr_change_gold.+x` source first — it's tiny, pure C, reads `inventory.txt`, modifies `qolq=`, writes back.

**Verification:**
```bash
# 1. Build the op
gcc -Wall -O2 -o xyzfs/bin/muchi-pet/ops/+x/mr_show_text.+x \
    <source_path>/mr_show_text.c

# 2. Create a test event package
mkdir -p sessions/s4/test_showtext/event_pkg/pages/page_1
cat > sessions/s4/test_showtext/event_pkg/pages/page_1/condition.pdl << 'EOF'
COND | trigger | on-click
EOF
cat > sessions/s4/test_showtext/event_pkg/pages/page_1/event.ir.pdl << 'EOF'
NODE | 1 | SHOW_TEXT | /tmp/test_verse.txt
EOF
cat > sessions/s4/test_showtext/event_pkg/pages/page_1/event.pal << 'EOF'
exec cmd_1.sh
halt
EOF

# 3. Run play_event.sh directly
xyzfs/bin/muchi-pet/ops/play_event.sh sessions/s4/test_showtext/event_pkg

# 4. Verify: entity's interact_relay.txt should contain SHOW_TEXT_FILE:/tmp/test_verse.txt
#    AND the text should appear in the entity window as a popup.
```

### Step 1.2 — Wire it through event-ez
After the standalone op works, add `SHOW_TEXT` as a selectable command in `ez_menu_input.c`'s command palette. Follow the exact same pattern as `CHANGE_GOLD` — the only difference is the op binary name and the argument format.

Then rebuild event-ez:
```bash
cd &.widgits/event-ez && sh button.sh compile
```

### Step 1.3 — End-to-end relay test
```bash
# Reset entity state, inject Play via relay, confirm SHOW_TEXT_FILE appears in relay
# AND the text popup renders in the entity window.
echo "RUN_METHOD:Play" > xyzfs/users/<uuid>/home/livedesk/pals/<test_entity>/interact_relay.txt
```

**Success criterion:** `history.txt` shows `INJECTED: RUN_METHOD:Play` and `SHOW_TEXT_FILE:/tmp/...` appears in the entity's relay. The entity window displays the text popup.

---

## 2. ADD MORE RPG MAKER EVENT COMMANDS

The full RPG Maker command list is at `#.ref/menu/event.commands.1.txt`. Start with the highest-value, lowest-complexity ones:

### Recommended order
1. **Show Text** (in progress — see §1)
2. **Show Choices** — `khtpm_show_choices.+x` already exists. Wire it through event-ez + cmd_N.sh. The picker already opens near the entity window.
3. **Input Number** — new op needed. Reads a number from the user via a small khtpm popup. Store in `inventory.txt`.
4. **Wait** — simplest possible op. `mr_wait.+x <entity_package_dir> <frames>` — just `sleep <frames/60>`. Proves timing events work.
5. **Play SE** — `mr_play_se.+x <entity_package_dir> <se_file>` — shell out to `aplay`/`paplay`/`ffplay`. Platform-specific but trivial.
6. **Show Animation** — display an animation tile on the entity. Needs animation tile support (see §6 below).

### Pattern for every new op
1. Write the C source (keep it tiny — study `mr_change_gold.c`).
2. Compile to `xyzfs/bin/muchi-pet/ops/+x/mr_<name>.+x`.
3. Add the command type to `ez_menu_input.c`'s palette + save handler.
4. Rebuild event-ez (`sh button.sh compile`).
5. Test via `play_event.sh` directly, then via relay.

**Do NOT** put event ops inside individual projects. All event ops go in `xyzfs/bin/muchi-pet/ops/+x/` — that is the established shared-ops convention.

---

## 3. COMMON EVENTS IN THE DB

### What's already done
- Session-level `common_events` packages work. `play_event.sh` runs against `sessions/<session>/common_events/event_pkg` exactly like an entity.
- `event-ez` already works pointed at a session package (`EZ_PKG_DIR=sessions/s4/common_events/event_pkg`).

### What's missing
- **UI trigger point** — no taskbar cell / menu row launches common events yet. The `db` cell (currently inert) or a new per-session menu are candidates.
- **Autorun/Parallel triggers** for common events (fire on session load, fire every tick).

### Recommended approach
1. Add a "Common Events" row to the entity right-click menu (`objects.pdl` → `PAGE|main`) that runs `play_event.sh` against the current session's `common_events/` instead of the entity's own `event_pkg/`.
2. Or: add a new taskbar cell / submenu entry. The taskbar cell route is more visible but requires C code in `khtpm_taskbar_manager.c`. The entity-menu route is faster to prototype.

---

## 4. CURSWORD ENTITY (do this BEFORE animation/hum)

The user wants a "cursed sword" personal-assistant entity spawned from the HQ's new "cursword" option.

### 4.1 — RPG Maker tileset location
Confirmed: the sword sprite is in the RMMV tileset on the mounted drive at:
```
/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/📲️📵️.GTFO.flxbx.📦️arcbit♏️/#.⛷️LOOSE-TANGENT]2wild]🎄️⛷️/🪆️.a0/👻️.🖋️.pix+-/^.sp-rmmv.pngs]100=24x24/
```
Look for a **3 rows × 4 columns** animation tile strip (12 frames total, **24×24 px each**, total strip 96×72 px) in the bottom-left of the tileset PNG. Extract the 12-frame sword animation as `atlas.png` for the entity.

> **DONE 2026-08-24** (actuals differ from the guess above — see
> CURSWORD-HQ-SPAWN.md): file is `!.RMMV-pix+pyCODE/48+char]b1]SHIP/
> characters_48x48/!Other1.png` (576×384); sword block = sheet cols 10–12 ×
> rows 5–8, cells 48×48; static sprite = row 5 col 10 (upside-down sword,
> user-pinned); all 12 frames in `anim/frame_01..12.png`; atlas/sprite.csv
> regenerated at 64×64 and synced to template + owned pal.

### 4.2 — Entity template
Copy an existing simple entity (e.g., `book-stack` under `*.monads/*.book-stack/entities/book-stack/`) as your starting template. It needs:
- `atlas.png` — your sword sprite sheet
- `desktop_pos.txt` — starting position
- `glyph.txt` — optional, for the strip tile
- `instance_id.txt` — unique ID
- `interact_relay.txt` — starts empty
- `livedesk_index.txt` — desk slot
- `meta.pdl` — **THIS IS CRITICAL** — defines the entity's METHODS (context menu actions)
- `pal.pdl` — basic identity
- `sprite.csv` — sprite animation config
- `history.txt` — starts empty

### 4.3 — meta.pdl context menu
The user wants these actions:
| Label | Action | Status |
|---|---|---|
| dir | shell command that opens the entity's session dir in a file browser | ✅ `Dir \| xdg-open` |
| chat | launches a khtpm-based chat HQ-like window (see below) | ✅ **DONE 2026-08-24** — open-hai shared binary w/ `--data-root` per-instance redirect (see §4.3.1 amendment + CURSWORD-HQ-SPAWN.md) |
| harnesses | opens a test runner UI populated from a list of .pdl locations | pending (events work next) |
| add | user-defined action placeholder | pending |
| cancel | CLOSE (kill the entity) | ✅ implemented as `Cancel \| void` (house convention — CLOSE kills whole window; see CURSWORD-HQ-SPAWN.md) |

Study `book-stack/meta.pdl` for the exact format. `METHOD` lines define right-click menu items. `Close` maps to `CLOSE`. `Cancel` can also map to `CLOSE`. `dir` can be `xdg-open <session_dir>`.

### 4.3.1 — CURSword chat HQ (independent of events system)
**AMENDED 2026-08-24 (user directive, corrected to open-hai, see CURSWORD-HQ-SPAWN.md):**
CURSword's chat uses the **same interface/binary as open-hai** (`&.widgits/open-hai/`,
renderer `khtpm_open_hai_render.c`), just with a different (CURSword-specific) session
history. New features (voice, minimize/windows list, etc.) are added to that SHARED
binary so open-hai gets them too — do not build a bespoke CURSword chat renderer.
The spawn flow itself is DONE and relay-verified: see `CURSWORD-HQ-SPAWN.md` in this directory.

CURSword is **not just an event-driven entity**. It is a long-term OS assistant that must function even when the events system is not running. Its chat UI is a **khtpm-based window** (similar to db-hq / open-hai / events-hq) with these requirements:

- **Chat window** — a khtpm popup/renderer that displays a conversation log and accepts text input.
- **Minimize button** — the window must have a minimize button that hides it to a tray/list.
- **Windows list** — when minimized, CURSword appears in a "windows" list accessible from its context menu or a dedicated button. This list should show ALL "window" programs (khtpm-based apps) currently running on the desktop, not just CURSword. Clicking an entry focuses / unminimizes that window.
- **Voice input** — the chat window should accept voice input (microphone). Use the same TTS/STT pipeline as book-stack's `bible_tts` branch (`7.tts.sh` on the mount).
- **Voice output** — CURSword should speak responses aloud using the same TTS engine. The hum sound (§6) also uses this TTS pipeline.

**Architecture note:** CURSword's chat logic is **separate from the event runtime**. The event system (§1-3) is for in-game event commands (Change Gold, Show Text, etc.). CURSword's chat/OS-assistant functionality is a **parallel system** that happens to live inside an entity window. Do not conflate them. The chat window should be a khtpm renderer (like `khtpm_hq_manager.c` / `khtpm_open_hai_render.c`) with its own input loop, not triggered by `play_event.sh`.

### 4.4 — Spawn mechanism
Add a "cursword" option to the HQ menu. When selected:
1. Create a new session dir under `sessions/<new_session_id>/`.
2. Copy the CURSword entity template into `sessions/<session>/entities/cursord/`.
3. Add a LAUNCH row to `autostart.pdl` (or spawn directly via `Start-Process` equivalent).
4. The entity should auto-assign itself a unique `instance_id` and `livedesk_index`.

The HQ menu itself is driven by `khtpm_hq_manager.c` — find where rows are defined and add a "cursword" row that triggers the spawn flow.

---

## 5. TEST ARTIFACTS (PNG dumps + VIDEO presentations)

The user wants CURSword to:
- Capture PNG screenshots of test runs
- Generate a **video presentation** per session explaining results (TTS narration + frame slides, like a PowerPoint — **preferred over PDF**)

### Recommended approach
1. **PNG dumps:** `khtpm_show_text.+x` already uses `dump_frame_png_op.+x` (in `&.widgits/_shared-lib/ops/+x/`). Call it from a test harness script: `dump_frame_png_op.+x <entity_package_dir> <output.png>`.
2. **Video report:** Write a Python script (`presentation_gen.py`) that takes a session dir and produces a video. Use the existing screen-rec code at `151.screen-rec+01.02/` as a reference for frame capture + audio pipeline. The script should:
   - Take a list of PNG frames + per-frame narration text
   - Use the same TTS engine as book-stack's `bible_tts` (`7.tts.sh` on the mount) to generate narration audio per frame
   - Composite frames + audio into a video (ffmpeg)
   - Store it in `sessions/<session>/reports/<timestamp>_presentation.mp4`
3. CURSword's `harnesses` context menu action should launch a test runner that:
   - Runs a sequence of relay injections (Play, Show Text, etc.)
   - Calls `dump_frame_png_op.+x` after each step
   - Calls the Python presentation generator at the end

**Reference code:** `151.screen-rec+01.02/` — study this for frame capture and audio pipeline patterns. The agent should reuse what it can rather than building from scratch.

---

## 6. HUM SOUND + IDLE ANIMATION (do AFTER CURSword is stable)

### Hum sound
- Periodic (every 30–60s) while CURSword is alive.
- Implement as a background thread in `tp_desktop_window_rgb.c` OR as a separate lightweight process spawned by CURSword's launch script.
- Use `aplay` / `paplay` / `ffplay` for cross-platform sound. The hum file itself can be a small OGG/WAV generated by the existing audio pipeline (`#.ref/3.diffusion.mp3...` — flag, that one is immature) or a hand-crafted tone.
- Alternative: write a tiny C program that uses ALSA/OSS directly, avoiding external deps.

### Idle animation
- CURSword's `atlas.png` is a 3×4 (or 4×2) tile strip. The entity renderer (`tp_desktop_window_rgb.c`) already reads `sprite.csv` for animation config.
- Study an existing entity's `sprite.csv` (e.g., `m8_redhorned/sprite.csv`) for the format.
- Add an `idle` animation cycle that loops through the sword's idle frames.
- The animation should play when no relay commands are pending (idle state).

---

## 7. EXECUTION PRIORITY (user's intent, confirmed)

1. **Prove Show Text works** through the event pipeline (§1)
2. **Add more RPG Maker event commands** — Show Choices, Input Number, Wait (§2)
3. **Common events in db** — UI trigger point + Autorun/Parallel (§3)
4. **Create CURSword entity** with dir/chat/harnesses/add/cancel context menu (§4)
5. **Test artifacts** — PNG dumps + PDF report generation (§5)
6. **Hum sound + idle animation** (§6)

---

## 8. CRITICAL RULES (do not violate)

1. **Linux is canonical.** All new code must work on Linux first. Windows `.ps1` twins can come later (auto-generated from bash originals).
2. **Ops are shared.** Event command binaries go in `xyzfs/bin/<game>/ops/+x/`, never inside an entity or project dir.
3. **Events are session-private.** Event content stays in `sessions/<user>/<session>/` until explicitly published.
4. **Relay-only testing.** Test via `interact_relay.txt` injection, never by calling binaries directly from your test harness. This is the standing rule.
5. **No new absolute `/home/no/...` paths.** Use house-relative paths everywhere. The `anchor-search` pattern (`find` upward for `101.mutaclsym*/system`) is the correct way to locate shared tools from variable-depth callers.
6. **Check before inventing.** Before designing new UI/state patterns, check whether `khtpm_*` already has a parser/layout/navigation system you can reuse (§2 in EVENT_AI_VISION.md).
7. **Scope prompts tightly.** When delegating to subagents, give them a checklist with research already done — not an open-ended "understand X, then design Y" prompt. The palette-picker failure mode is burning the entire context budget on research with zero deliverable. The "ai" cell task succeeded at 50K tokens / 4.5 min because the prompt was a tight checklist.

---

## 9. QUICK-REFERENCE: HOW THE EVENT RUNTIME WORKS

```
User right-clicks entity → selects "Play"
    OR
Relay injection: echo "RUN_METHOD:Play" > interact_relay.txt
    ↓
tp_desktop_window_rgb.c (entity process) polls relay, dispatches
    ↓
dispatch_action() in tp_desktop_window_rgb.c
    ↓
play_event.sh <entity_package_dir> [house_root] [trigger]
    ↓
Scans event_pkg/pages/page_N/condition.pdl for highest-numbered
page whose trigger matches (default: on-click)
    ↓
Runs event.pal via prisc+x
    ↓
event.pal = compiled output: exec cmd_1.sh, exec cmd_2.sh, halt
    ↓
cmd_N.sh = wrapper that calls the actual op binary
    (e.g., mr_change_gold.+x <entity_package_dir> <amount>)
    ↓
Op binary modifies entity state (inventory.txt, master_ledger.txt)
    ↓
Result visible in entity window / file system
```

**Key constraint:** `prisc+x`'s `exec` opcode only supports ONE literal argument. Multi-arg calls need a `cmd_N.sh` wrapper. This is non-negotiable — every event command that takes more than one arg must have a wrapper.

---

## 10. QUESTIONS TO ASK THE USER BEFORE STARTING

1. **RPG Maker tileset path** — the exact path to the tileset PNG containing the sword sprite (I found candidates on the mount but want you to confirm which one).
2. **Sword sprite coordinates** — "bottom left of 4 rows 2 columns" — is it 4 rows × 2 columns (8 frames) or 3 rows × 4 columns (12 frames)? And is it 24×24 px per tile or 48×48?
3. **Chat UI for CURSword** — do you want a full khtpm popup chat, or is a simple text log in the entity window sufficient for v1?
4. **Harnesses menu action** — should this run a predefined test suite, or should it let the user pick which tests to run?
5. **Hum sound** — do you have a specific hum audio file, or should I generate one programmatically?
6. **Video presentation format** — any specific template/layout for the frame slides, or "just sequence the test screenshots with TTS narration explaining each result"? Any preferred video resolution/framerate?

Ask these before writing any code. Do not guess.

---

**End of handoff. Start at §1 (prove Show Text works) after confirming the questions in §10.**


ANSWERS : 


1. thats the one, on the mount. 2 : yes 8 frames , of 3 rox 4 columsn of sprites? 3. it will have its own chat hq like window, but it should beable to chat with window minimized (we will add minimize button, which should add window to a list of curswords "windows" option (which should actually show all 'window' programs on desktop , and focus / unminimize on click , voice input, and voice output using same tts as "book-stack' bibleverse :tts uses . i know it seems kind of complicated and tangent but we will do it anyways. it will function as os assitant long term and be useful for users and myself for random tasks and functions w/o relying on the events system that we will want all entities and "toys(programs/apps) to use going forward.(pls be explaining all this in handoff) 4. we will populate it from a list of harndess .pdl locations that have populare harnesses user or agent may like to run.  5. hum is programmatically generated, using tts (a variable lenght "hmmmmmmm" or "mmmmmm" or "mhmmmmmm" 6. human reader friendly report relevant but std frame; we will do many of these so reusable script is best; record this in doc pls



also later i will want to create video of the presentation , using tts plus frames created of frame + explantion . like a powerpoint. i will actually prefer reports in this format instead of pdf , so lets note that instead .the agent may wanna use code from : /home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/151.screen-rec+01.02




## ADDENDUM 2026-08-24 (late session) - standards + status
- HOUSE RULE pinned: **no hardcoded UIs ever** - layouts are generated
  artifacts of stores (ir.pdl->pal, bookmarks.pdl->chtpm, stats->dashboard.chtpm).
  Permanent home for the full write-up (old-vs-new context windows, generic hq_render
  mechanisms + honest port-status caveat, SHOW_PAGE chooser contract, bookmarks spec):
  **`44.xyz…17/!.HOUSE_STDS.md` §K** (a copy that briefly landed in the k9 doc was
  removed same day; INDEX.md now carries this as Standing Rule 7).
- Context-window standard = khtpm_entity_menu_render (chtpm), opt-in via
  `<package_dir>/menu.chtpm`. VERIFIED 2026-08-24: live db-hq launches the MERGED
  binary (`open_db_hq.sh` -> `khtpm_entity_menu_render.+x`); standalone
  `khtpm_hq_render.+x` remains only for stats-hq - so this session's generic
  onClick/live-reload upgrades currently live ONLY in the stats-hq binary; porting
  to the merged binary is open work.
- Event-ez Show Text (KEY:8) fixed end-to-end (3 bugs: op-contract mismatch,
  speaker field separator, screen classification); verified via relay pipeline.
- mr_show_choices.+x exists but is STALE vs SHOW_PAGE convention - ladder #2 is
  an alignment job, not a from-scratch op.
- Remaining-events reference written to `44.xyz…17/#.ref/menu/event.commands.remaining.txt`.
- Bookmarks: store/mirror/compose wired + tested headlessly on cursword;
  live db-hq-style window (USER-PINNED correction — "the old window it was
  fine", NOT the entity-menu popup; DnD declined). Generic renderer upgrades
  landed in khtpm_hq_render.c for it: onClick→nav-row pass (incl. real
  stale-index bug fix) + native `input:` fields ("new+ should allow input
  from <cli-io>", zenity retired; consumed-newplus verb). Full spec: §K.3/K.5.
- Bookmarks final fixes: rows projected as <button> per fo-menu-sys.md #26
  (user pointed at 1.TPMOS…/!.gem-flashlite--yolo/fuzz-op-r&d/fo-menu-sys.md);
  REAL BUG: apply_attr() matched "onclick" case-SENSITIVELY vs house camelCase
  onClick= → attr silently dropped, clicks dead; fixed via attr_ci_eq().
  Symlink mirror REMOVED (user rule: symlinks disallowed for windows); rows
  open real dirs like entity Dir button; bm-bookmark = black-on-yellow.
- Plans-after-events pinned: pallets categories registered in
  #.ref/menu/palletes/pallets-help.txt (chemistry-palette, tiling-rmmv,
  minecraft-blocks, cdda-tiles, df-tiles from tiling-palettes-chemistry.txt);
  db-hq↔events-hq event-op parity + db-coupling notes added to
  event.commands.remaining.txt / db-tabs-remaining.txt; INDEX.md 🎯 section.
  Next work item: events ladder #1 Wait op.
