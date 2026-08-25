# 💾🎮 SV-LD-SES-STRAT — SAVE/LOAD/SESSION STRATEGY FOR THE `@.apps/` GAME FAMILY

> **STATUS:** Strategy doc, grounded in TWO real, proven precedents — `@.apps/text-editor-xyz` + `102.editor-📄️00.00`'s own widget-cmd-bus save/load (real, tested, jail-verified code, read directly this session), and `101.mutaclsym`'s own planned multi-file save-slot design (`%.harnesses/muta-zoo.md`, real design doc, partially built). **Nothing in this doc is built yet for `my-chara-txt`/`my-biotech`/`myne-qrypto`** — this is the plan, written before code, per this house's own "design-doc-first, always" standing preference.
>
> **Why this doc exists:** direct user correction, 2026-08-02 — `my-chara-txt`'s current persistence model (one global `config.txt`/`plots.txt`, seeded once, shared by EVERY session forever) is WRONG. *"it shouldn't be like that, each session should have its own data, thats how save load will/should work; like how @.apps/text-editor-xyz has save/load."* Read this whole doc before touching persistence code in ANY sibling app.

---

## 🚨 1. WHAT'S WRONG WITH THE CURRENT MODEL (all three built apps have this bug)

`my-chara-txt`, `my-biotech`, and `myne-qrypto/qtc` (to a lesser extent — `qtc` is pal-chain's own real login/wallet system, which DOES have per-user identity via wallet_id/password, but no explicit save-SLOT concept beyond "one wallet = one identity") all currently do this in `button.sh`:

```bash
# Fresh per-session config.txt seeded with starting state if the
# persistent one under the REAL project dir doesn't exist yet.
if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
    cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
    ...
EOCONFIG
fi
ln -sf "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"
```

**The bug:** `config.txt` (and `plots.txt`, `data/corpus/`, `data/research/`, `data/discovered_compounds.txt`, `data/master_ledger.txt`) live at the REAL PROJECT ROOT, seeded exactly ONCE, then symlinked into every session forever. There is no way to have two separate playthroughs, no way to name/choose a save, and no way to start a genuinely NEW game without manually deleting files by hand (which is what the `my-chara-txt/user-walkthru.txt` doc this session had to instruct as a workaround — a real, visible symptom that this is wrong).

**What this conflates, that must be split apart:** a **SESSION** (the ephemeral UI/process instance — keyboard history, interact_relay, screen_changed markers, one per `button.sh run`) is a completely different thing from a **SAVE** (a named, persistent snapshot of game state — day/health/money/plots/corpus/ledger — that should survive across many different sessions, and that the player should be able to have MULTIPLE of, name, switch between, and start fresh from).

---

## ✅ 2. THE REAL, PROVEN PRECEDENT — READ THESE TWO FILES DIRECTLY BEFORE BUILDING ANYTHING

### 2.1 `102.editor-📄️00.00/ops/editor_widget_cmds.c` — THE MECHANISM (single-buffer shape, but the path-safety code is directly reusable)

Real, working, tested code (not a stub). Key pieces, verbatim from that file:

- **Command verbs**, delivered via a plain-text inbox file (`pieces/system/widget_cmds/inbox.txt`, one command per line): `NEW`, `SAVE`, `SAVE_AS:<path>`, `LOAD:<path>`, `PING`. Results go to `pieces/system/widget_cmds/status.txt` (`last_cmd=`, `result=ok|error`, `message=`, `at=<timestamp>`).
- **Real per-user identity resolution chain** (`resolve_xyzfs_home()`): `pieces/system/house_root.txt` → `<house_root>/0.user-pal👤️/00.login-signup/current_login.txt`'s own `current_xyzfs` key → `<house_root>/<xyzfs>/home`. **This means saves are scoped to WHICHEVER USER IS CURRENTLY LOGGED IN via `0.user-pal👤️`**, not to a project-global file. Whoever builds this for `my-chara-txt`/`my-biotech` needs to check whether `0.user-pal👤️` login is a real prerequisite dependency or whether a simpler "default local user" stand-in is acceptable for now — **not yet decided, ask before building**.
- **Real, jail-verified path resolution** (`resolve_save_path()`): a bare filename resolves under `<xyzfs_home>/documents/<name>`; a leading `/` is STILL relative to `xyzfs_home` (there is NO way to express a host-absolute path through this input at all — a deliberate security choice, not an oversight); after building the candidate path, `mkdir -p`'s the parent, then `realpath()`s BOTH the parent and `xyzfs_home` and verifies the former is canonically inside the latter — this genuinely catches `../../../etc/passwd`-shaped escapes (via real path resolution, not string pattern-matching) and returns a hard failure (never "fall through to writing somewhere unverified") if the check fails. **This exact function should be copied near-verbatim into any sibling app's own save/load op** — it's the single most reusable, security-critical piece of this whole precedent.
- **`file_path` state tracking**: the editor's own `editor_state.txt` remembers which file is "currently open" (`file_path=`) so a bare `SAVE` (no args) knows where to write without re-asking; `SAVE_AS:<path>` sets a NEW target and switches the "currently open" pointer to it. **Direct analogue for a game**: a `current_save_slot=` key in some project-level state file, updated by SAVE_AS/LOAD, read by a bare SAVE.

### 2.2 `101.mutaclsym` (via `%.harnesses/muta-zoo.md`, read earlier this session) — THE DATA SHAPE for a MULTI-FILE game (closer to what `my-chara-txt` actually needs)

The text-editor's own save unit is ONE buffer (one file). A game's own save unit is MANY files (config.txt + plots.txt + master_ledger.txt + corpus/ + research/ + discovered_compounds.txt, for `my-biotech` specifically) — mutaclsym's own real design for this shape (partially built, not fully — see `muta-zoo.md`'s own status table) is the right precedent to follow instead of trying to force the single-buffer model to fit:

```
xyzfs/users/<user_uuid>/home/projects/
  my-biotech/                    # ← this app's own save namespace
    saves/
      demo-project/               # a named save slot = a whole directory
        meta.pdl                  # slot metadata (created_at, last_played, etc.)
        config.txt                # this slot's own copy of game state
        plots.txt
        data/
          corpus/player.txt
          research/<compound>/dossier.txt
          discovered_compounds.txt
          master_ledger.txt
      <user-chosen-slot-name>/
        meta.pdl
        config.txt
        ...
```

**Verb mapping** (same four verbs as §2.1, reinterpreted for a multi-file directory instead of one buffer):
- **NEW GAME** — wipe/reset the LIVE working directory's own state files (`pieces/system/config.txt`, `plots.txt`, `data/*`) back to a fresh-start template — NOT the same as deleting a save slot; this only resets what's currently "in play," same as mutaclsym's own real op (`ops/save_game.c`'s counterpart for NEW, not yet built there either per `muta-zoo.md`'s own status table — a real, shared gap across both precedent projects, not something to assume is solved elsewhere).
- **SAVE** — snapshot the LIVE working files → the CURRENT save slot (tracked via a `current_save_slot=` key, same pattern as `file_path=` in §2.1).
- **SAVE AS `<name>`** — snapshot LIVE → a NEW named slot under `saves/<name>/`, then switch `current_save_slot=<name>`.
- **LOAD `<name>`** — copy `saves/<name>/*` → the LIVE working files, overwriting whatever's currently in play (should probably warn/confirm before overwriting unsaved progress — a real UX question, not decided here).

**Demo-slot survival rule** (direct quote from `muta-zoo.md`, worth preserving here since it's a real, previously-learned lesson): *"So NEW / wipe live never destroys the only playable content"* — i.e., NEW GAME should reset to a TEMPLATE, never accidentally wipe a player's only save with no way back. Seed a `demo-project`/template save once, idempotently, and NEW GAME re-copies from that template rather than hand-writing a blank state inline (matches `muta-zoo.md`'s own §4.3 "demo-project seed" logic — check for existing demo before creating, never clobber).

---

## 🏗️ 3. CONCRETE PLAN FOR `my-chara-txt` (build this first, same "smallest sibling proves the pattern" precedent this whole family already follows)

### 3.1 THE REAL FILE-MENU WIDGET POPS OPEN — not a reinvented in-game screen

**Direct user correction, 2026-08-02: *"the widget should just pop open if user chooses file-system option."*** This settles what looked like an open question — `my-chara-txt` does NOT get its own hand-rolled Save/Load `.chtpm` screen. It gets the SAME real, already-built, already-tested `&.widgits/file-menu` widget `@.apps/text-editor-xyz` already uses, launched the exact same way: a real, separate GL-window process, talking to `my-chara-txt` only via the widget cmd bus (`pieces/system/widget_cmds/inbox.txt`/`status.txt`, exactly `editor_widget_cmds.c`'s own real, working shape from §2.1) — not a fake "picker screen" living inside `my-chara-txt`'s own single window.

**Concretely:**
- `my-chara-txt`'s own `main.chtpm` gets a new action (e.g. `"File" -> href/onClick triggering the widget spawn`, matching `102.editor-📄️00.00/ops/editor_menu_input.c`'s own `do_fm()` — read that function directly, §3.1 of `EDITOR-WIDGET-INTEGRATION-HANDOFF.txt` has the exact real code shape, including the still-open "auto-spawn on demand" work item worth checking current status of before assuming it's done).
- On trigger: fork+exec `&.widgits/file-menu/button.sh run-widget` (NOT `system()`/`popen()` — `PITFALL 1`'s own "system() causes zombies" applies directly, same as the editor's own real implementation notes), same real spawn mechanism, not reinvented.
- `my-chara-txt` needs its OWN version of `editor_widget_cmds.c` (a NEW op, e.g. `mychara_widget_cmds.c`) — same `NEW`/`SAVE`/`SAVE_AS:<name>`/`LOAD:<name>`/`PING` verb set, same inbox/status file convention, same `resolve_save_path()` logic copied near-verbatim (§2.1) — but adapted for the MULTI-FILE snapshot shape (§2.2/§3.2 below) instead of the editor's own single-buffer read/write.
- file-menu itself needs zero changes — per PART 0 of `EDITOR-WIDGET-INTEGRATION-HANDOFF.txt`, it's already a real, generic, project-agnostic widget that talks to whichever project spawned it via the SAME cmd-bus contract; `my-chara-txt` just needs to be a second real project speaking that contract correctly, matching what `text-editor-xyz` already proved works.

**A snapshot/restore helper** (`cp -r`-shaped, matching mutaclsym's own real `ops/save_game.c` precedent) that copies the specific set of files `my-chara-txt` actually needs to persist (`pieces/system/config.txt`, `pieces/system/plots.txt`, `data/master_ledger.txt`) to/from a save slot directory — called from inside `mychara_widget_cmds.c`'s own SAVE/SAVE_AS/LOAD handlers, same place `editor_widget_cmds.c`'s own `do_save_to()`/`do_load()` live.

### 3.2 `button.sh` changes

Remove the current "seed once at project root, symlink into every session forever" logic entirely. Replace with:
- On `run`, auto-load the MOST RECENTLY PLAYED slot if one exists, falling back to auto-creating+loading a fresh default slot on true first-ever launch (mirrors `muta-zoo.md`'s own demo-project auto-seed logic) — the game should just open into wherever the player last left off, same as any normal game, WITHOUT forcing a picker screen on every single launch. The file-menu widget (§3.1/§3.3) is how the player EXPLICITLY chooses a different slot/starts fresh/saves-as — not something shown by default on every boot.
- The chosen slot's own files get copied (not symlinked) into the session's own live working paths (`pieces/system/config.txt` etc.) at session start, and copied back out on an explicit SAVE action (or possibly on clean exit too — auto-save-on-quit is a real, common game UX pattern worth considering, not decided).

### 3.3 NO new CHTPM screen inside `my-chara-txt` itself — the widget IS the picker

Per §3.1's own correction: there is no `pieces/chtpm/layouts/save_load.chtpm` inside `my-chara-txt`. **file-menu's own real, existing screens ARE the save/load picker** — it already has a real slot-listing UI (proven working for the editor's own `documents/` — same mechanism lists whatever's under `my-chara-txt`'s own `games/my-chara-txt/saves/` once `mychara_widget_cmds.c` publishes the right bridge info) and its own real `<cli_io>` text field for typing a new SAVE AS name. `my-chara-txt` only needs the ONE new trigger action (§3.1's "File" button) to spawn it — everything past that point is file-menu's own already-built, already-tested UI, not new UI work for this project at all.

### 3.4 Real, live-verified test to prove this works (before calling it done)

1. Create save "Playthrough A", play a few turns (plant something, end a turn or two).
2. Save As "Playthrough A".
3. Start a genuinely NEW game (NEW_GAME action) — confirm the live state resets to Day 1/fresh plots, NOT still showing Playthrough A's own progress.
4. Play differently for a turn or two under this new, unsaved state.
5. LOAD "Playthrough A" — confirm the live state reverts to EXACTLY what was saved in step 2 (day/health/money/plot states match), NOT the step-4 unsaved progress.
6. Confirm via `ls <xyzfs_home>/games/my-chara-txt/saves/` that "Playthrough A" is a real directory with real files in it — not just an in-memory illusion.

This is the same rigor every other feature in this house's own family got this session (real key injection through the real entry point, real file evidence, not an op-level shortcut) — no exceptions for save/load just because it's "just persistence."

---

## 🔗 4. HOW THIS EXTENDS TO THE OTHER SIBLINGS

- **`my-biotech`**: identical shape to `my-chara-txt`, plus `data/corpus/player.txt`, `data/research/<compound>/dossier.txt` (ALL compound subdirectories, not just one file — a save-slot copy needs to `cp -r data/research/` wholesale), and `data/discovered_compounds.txt` in the snapshot set.
- **`myne-qrypto/qtc`**: already has a real, different-but-related persistence model — wallet_id + password IS the real per-player identity (not `0.user-pal👤️` login), and `wallets/<id>/wallet.txt` + `data/blockchain.txt` are already real, persistent, un-symlinked-away files (this is closer to "real" than the other siblings' current bug, since a wallet already survives across sessions correctly by its own design — no immediate fix needed here, though whether it should ALSO gain the same `0.user-pal👤️`-scoped save-slot layer on top, for multiple separate blockchain instances per user, is a real open question, not decided).
- **`my-lawyer`** (design-only, zero code yet): should be designed with this save-slot model in mind from the START (same lesson as the judge-mechanic correction, §14 of `#.haiku+/HANDOFF_NEXT_SESSION.md`) — `data/cases/<case_id>/` (the real, player-visible case documents) are a natural fit for the same multi-file snapshot approach, not a new problem.
- **`genesis-txt`/`genesis-zr`** (multiplayer, design-only): a genuinely harder version of this problem — MULTIPLE players' own save state in one shared game session. Not scoped in this doc at all; worth its own follow-up strategy doc once `my-chara-txt`'s own single-player version is built and proven.

---

## 🤔 5. OPEN QUESTIONS (ask the user before building §3)

**RESOLVED, 2026-08-02** (direct user correction, §3.1/§3.3): file-menu is the real picker UI, popped open on demand — no in-game picker screen, no forced picker-on-every-launch. `my-chara-txt` auto-loads the last-played slot by default.

1. **Does this require `0.user-pal👤️` login to be a real, working prerequisite** (i.e., must a player be logged in via that system before any sibling game's save/load works at all), or should a simpler "default local user, no login required" fallback exist for now, with real `xyzfs`-scoped saves as a later upgrade? The text-editor precedent HARD-REQUIRES a real login chain (`resolve_xyzfs_home()` returns failure with no fallback if `current_login.txt` doesn't resolve) — copying that as-is means `my-chara-txt` would suddenly gain a real login dependency it doesn't have today.
2. **Auto-save-on-quit**, in addition to explicit SAVE — not decided.
3. **Should `NEW GAME` require confirmation** if it would discard unsaved live progress — not decided, but a real, likely-necessary UX safeguard given mutaclsym's own "never destroy the only playable content" lesson (§2.2).
4. **Where exactly under `xyzfs_home` do game saves live** — `games/<app>/saves/<name>/` (this doc's own working guess) vs. some other convention — not confirmed against any existing precedent for GAMES specifically (the text-editor precedent uses `documents/` because it's an editor; no game in this house has actually built the xyzfs-scoped version yet, only the OLD install-tree-relative `pieces/saves/save_N/` version mutaclsym's own legacy `ops/save_game.c` uses).

---

## 🏁 6. TL;DR

- **Session ≠ Save.** A session is ephemeral UI plumbing (one per launch). A save is a named, persistent, multi-file snapshot the player can have many of, choose between, and start fresh from.
- **The real file-menu widget IS the save/load UI** — it pops open as its own real GL window when the player picks a "File" action, exactly like `text-editor-xyz` already does. No in-game picker screen gets built inside any sibling app.
- **The real, proven MECHANISM to copy**: `102.editor-📄️00.00/ops/editor_widget_cmds.c`'s own `resolve_save_path()` — real per-user `xyzfs`-scoped, jail-verified (`realpath()`-checked) path resolution. Copy this logic near-verbatim.
- **The real, closer-fit DATA SHAPE to copy**: `mutaclsym`'s own planned save-slot design (`muta-zoo.md`) — a named save slot is a WHOLE DIRECTORY snapshot (multiple files), not one buffer, matching what `my-chara-txt`/`my-biotech` actually need to persist.
- **Build `my-chara-txt`'s version first** — smallest sibling, same "prove the pattern on the simplest case first" precedent this whole family already follows for everything else.
- **Four real open questions, not decided** — most load-bearing: does this require real `0.user-pal👤️` login, or does a simpler default-user fallback make sense for now?

💾 Ready to make saves real? 🎮
