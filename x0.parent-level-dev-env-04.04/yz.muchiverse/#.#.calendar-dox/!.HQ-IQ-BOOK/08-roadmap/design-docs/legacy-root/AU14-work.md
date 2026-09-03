# AU14-work — 2026-08-14 plan

> ## ⏱️ PROGRESS LOG (periodic — survives disconnect; read this first)
>
> ### 2026-08-14 session state
> - **Task 1 (tb cleanup) — consolidation DONE + verified.** The full
>   single-folder consolidation + flexible-path work is COMPLETE (2026-08-14
>   late session): entity renderer + helper set moved OUT of
>   `&.widgits/tile-picker/ops/` INTO `*.monads/*.livedesk-taskbar/ops/`
>   (user-confirmed target: "should be in ld-tb ops probably"). Canonical
>   state re-verified after `button.sh reset` + Desktop launcher run: exactly
>   1 parser + 1 manager + 6 entities, ALL from the new path, no stale
>   tile-picker entity processes, no dups. Details in Task 1 results below.
> - **Task 1 gate (user):** before we say tb is done, live-test h-ai chats via
>   the `14.hai` relay (`#.desktop/ai_cell_agent_relay.txt`) on the real
>   livedesk so the user can WATCH. PASSED earlier this session (see below) —
>   no re-test needed for the consolidation, it doesn't touch the ai-cell path.
> - **Task 2 (qwen LAN) — PAUSED per user ("finish task 1 before task 2").**
>   State: all 5 tiers pulled on Mac (0.5b/1.5b/3b/7b/codeqwen:7b-code);
>   `net/qwen.sh` rewritten to full house stack (sh escaper + connect_op +
>   json_parser, NO python/jq), ask/chat/ladder/status smoke-tested OK.
>   Known open: `fim` fails — codeqwen:7b-code doesn't support ollama `suffix`
>   insert param; needs CodeQwen1.5's native `<|fim_prefix|>...<|fim_middle|>`
>   token approach (NOT done — paused). `net/TOOLING-MAP.md` written.
> - **Task 2 testground DECISION (user, 2026-08-14):** chat-hai
>   (`&.hq-apps/chat-hai`, a selectable option under 14.h-ai — slender
>   side-bar GUI with a constantly scrolling chat feed) is the designated
>   TESTGROUND for the qwen ladder + Harnecient harness (app decides
>   who speaks/acts next, no model-driven control flow). Proof-of-concept:
>   get 4 smol models (gemma3:270M class) always chatting on the Mac,
>   recording/truncating memories, with 1-2 bigger models moderating
>   (piecing from the smols, reviewing memories, tuning) — then dev can
>   jump in with more ops/challenges, and eventually real tasks. Chat-hai
>   ladder or h-ai cell 14 = the first app onboarded onto the shared
>   `net/qwen.sh` wrapper as proof case. Task 2 resumes against this after
>   Task 1.
> - **Last actions:** (1) h-ai relay gate PASS (see below). (2) TB consolidation
>   EXECUTED + verified — see "Task 1 results" for the full change list
>   (moves, marker-walk, build scripts, autostart.pdl, manager C, harnesses,
>   launcher rewrite, reset verify). Next for Task 1: nothing pending; Task 2
>   qwen LAN can resume.
>
> _Update this section at every checkpoint._

---

## Task 1 — livedesk x11 khtpm: cleanup recommendations (skim level)

Start script: `44.xyz.01.00/$.crypts/button.sh`

### Where the code actually lives (the "scattered" map)

| Concern | Location |
|---|---|
| House-wide launcher | `$.crypts/button.sh` + `ops/crypt_autostart.c` + `autostart.pdl` |
| Taskbar runtime (C) | `*.monads/*.livedesk-taskbar/ops/*.c` (8.2k lines) |
| Taskbar layout | `*.monads/*.livedesk-taskbar/*.chtpm` (header 120, bottom 14) |
| State/config (.pdl) | `#.desktop/` — livedesk_taskbar.pdl, theme.pdl, launchers.pdl, shortcuts.pdl, hq_ui.pdl, live state files |
| Entity window binary | `*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x` (moved OUT of tile-picker 2026-08-14 — see results) |

### Standards we already hold (reference files skimmed)

- `1.TPMOS_c_+rmmp.0103.0001/button.sh` — thin dispatcher: every action just
  calls a scoped script/binary in `#.dev-storage/#.tools` or `pieces/buttons/linux`.
  No logic inline in button.sh.
- `@.apps/piececraft-xyz/button.sh` — real session-isolation pattern:
  `HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"`, per-session roots, scoped
  kill helpers, `kill` action + trap, `check|verify` listing every binary.
- `@.apps/asa-&-ava/button.sh` — thin compose: `run` just calls the two
  `pieces/<name>/button.sh` scripts; no duplicated logic.

### Recommendations (10, best first)

1. **One entity/launch list.** `button.sh reset` hardcodes
   `ENTITIES="self m8_redhorned ..."` + a full PALS path, while `run` reads the
   same list from `autostart.pdl` LAUNCH rows. Make `reset` (and `quit`) parse
   the pdl (or reuse `crypt_autostart`) so there is exactly one source of truth.
2. **One shared kill helper.** The pgrep regex
   `khtpm_strip_parser\.\+x|khtpm_taskbar_manager_main\.\+x|tp_desktop_window\.\+x`
   (+ `khtpm_hq_render\.\+x` in some spots, missing in others) is duplicated in
   button.sh (quit/reset/status), run_khtpm_strip.sh, EMERGENCY_CLOSE.sh and C.
   Extract `ops/+x/khtpm_kill.sh` and call it everywhere. Today: `quit` kills
   hq_render, `reset`/`status` don't — drift.
3. **Stop hardcoding colors in C.** ~20 hex literals in `khtpm_hq_render.c`,
   hardcoded palette in `khtpm_taskbar_settings_render.c`, while a real CSS
   parser (`khtpm_css_parser.c`) and `livedesk_theme.pdl` already exist. Route
   defaults through theme.pdl — no recompile, same win `hq_ui.pdl` font_scale
   already gives.
4. **One label source for strip cells.** Header .chtpm mixes hardcoded labels
   (`pals`, `palettes`, `edit`, `db`, ...), `${var}` labels, and
   `livedesk_taskbar.pdl` `strip_btn_*_label` rows. Pick the pdl as canonical;
   manager publishes all 15 cell labels.
5. **Standardize house-root discovery.** `button.sh` does `HOUSE="$SCRIPT_DIR/.."`,
   `run_khtpm_strip.sh` does `../../..`, parser takes it as argv. Pick one
   marker-file convention (like piececraft's `HOUSE_DIR`) so scripts survive
   relocation.
6. **Centralize log/pid paths.** `khtpm_strip_parser.log` is hardcoded in
   run_khtpm_strip.sh, button.sh reset, and C; pid in
   `#.desktop/livedesk_taskbar.pid`. One vars file so paths never drift.
7. **De-dup run vs reset.** `run`→crypt_autostart, `reset`→inline
   kill+build+launch+entities loop. Make `reset` = stop + build + crypt_autostart.
8. **Document/consolidate the .pdl sprawl.** 6+ config .pdl files; at minimum
   document the canonical set in `#.desktop/README.txt`.
9. **Reuse status logic.** `button.sh status` hand-greps "enabled"; reuse the
   shared kill/status helper. (button.ps1 = Windows port, keep, don't drift.)
10. **Split the monoliths** (`khtpm_taskbar_manager.c` 3125, `khtpm_strip_parser.c`
    1878) — long-term, low priority today.

### Basics to actually do today (safe, low-risk)
- Unify the kill regex (add `khtpm_hq_render\.\+x` to `reset` + `status`).
- Standardize log path via a shared var inside button.sh.
- `reset` derives entity list from autostart.pdl instead of the hardcoded
  ENTITIES/PALS list (recommendation #1).

### Task 1 results (verified 2026-08-14)
- **R1+R7 done:** `reset` now delegates to `crypt_autostart` + `autostart.pdl`
  (single source of truth); `quit`/`reset`/`status` all share `khtpm_pids()` +
  one `KHTPM_PAT`.
- **R6 done:** `khtpm_vars.sh` (KHTPM_HOUSE/KHTPM_LOG/KHTPM_PID) created and
  sourced by `run_khtpm_strip.sh`; log path no longer duplicated.
- **R8 done:** canonical .pdl set + runtime files documented in
  `#.desktop/README.txt`.
- **Bug found + fixed:** `autostart.pdl` LAUNCH rows pointed at
  `tp_desktop_window.+x` (does not exist). Real binary is
  `tp_desktop_window_rgb.+x` — all 6 rows sed-fixed.
- **Bug found + fixed:** `KHTPM_PAT` did not match `tp_desktop_window_rgb.+x`,
  so 7 entity processes survived `reset` (PIDs 46447+). Added to pattern.
- **End-to-end verified:** `sh button.sh reset` now (a) kills khtpm AND all
  entities, (b) rebuilds taskbar, (c) relaunches exactly the 6 pdl pals
  (self, m8_redhorned, m1_ninjadragon, book-stack, asa, ava). Canonical state
   after reset = khtpm (parser+manager) + 6 entities, no survivors, no dups.
   NOTE: entities take ~5s to appear after reset (background spawn) — don't
   check too early.
- **Consolidation DONE (2026-08-14, late session) — entity renderer + helper
  set moved OUT of tile-picker INTO the livedesk-taskbar runtime folder** (the
  entity window is a livedesk-taskbar concern, not a tile-picker one — user:
  "tile picker? thats not good they have nothing to do with that . should be
  in ld-tb ops probably"):
  - **Moved** `tp_desktop_window_rgb.c`, `tp_asset_to_sprite.c`,
    `tp_range_grid.c`, `stb_image.h` + binaries `tp_desktop_window_rgb.+x`,
    `tp_asset_to_sprite.+x`, `tp_range_grid.+x`, `emoji_gen_atlas.+x`,
    `emoji_xtract.+x` from `&.widgits/tile-picker/ops/{,+x/}` to
    `*.monads/*.livedesk-taskbar/ops/{,+x/}`. Tile-picker keeps ONLY its own
    widget tools (tp_set_brush/place/place_desktop/import_from_desktop/etc.)
    + the Windows `.exe` port artifacts.
  - **Flexible path discovery (no hardcoded climbs):** `resolve_livedesk_paths()`
    in `tp_desktop_window_rgb.c` + `tp_place_desktop.c` (both the emoji
    invocation and the spawn block) now walk UP from `/proc/self/exe` until a
    dir holding BOTH `#.desktop/` and `&.widgits/` is found — same
    marker-walk `khtpm_vars.sh` uses. Survives any relocation.
  - **Build scripts:** `build_khtpm_strip.sh` now also builds the entity
    (+helpers, incl. wsr-pal emoji-tool copy-backup); removed the entity +
    emoji builds from `tile-picker/scripts/build.sh` (keeps only widget tools).
  - **Consumers updated:** `autostart.pdl` 6 LAUNCH rows →
    `'*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x'`;
    `khtpm_taskbar_manager.c` both spawn sites (livedesk_spawn_desk +
    livedesk_place_pal, the latter now rgb not legacy GLX name);
    harnesses `$.crypts/scrypts/openall/run.sh`, `*.hard-vvar-agent-Q0000/button.sh`,
    `*.book-stack/button.sh`, `@.apps/asa-&-ava/pieces/{asa,ava}/button.sh`,
    `@.apps/pets/pieces/{dog,cat,chicken}/button.sh` (all pointed at the
    RETIRED `tp_desktop_window.+x` GLX name + old tile-picker dir).
  - **Launcher rewrite:** `$.crypts/ops/button_launcher.c` (globbed a
    hardcoded `NNEST-*/yz.muchiverse/*` path) → `livedesk-start-button.c` with
    the same marker-walk house-root discovery. `~/Desktop/🔐-Livedesk-Start`
    is now a symlink to the in-house binary (so /proc/self/exe resolves inside
    the house). Old `button_launcher` source+binary removed.
  - **Bug caught + fixed during verify:** first marker-walk used glibc
    `dirname()` in-place, which always returned the same buffer → loop broke
    immediately ("Could not find house root"). Fixed to `strrchr`-truncate
    walk in BOTH `livedesk-start-button.c` and `tp_place_desktop.c`
    (`tp_desktop_window_rgb.c` already used the correct truncate pattern).
  - **End-to-end verified:** `sh button.sh reset` AND
    `~/Desktop/🔐-Livedesk-Start` both rebuild + relaunch the canonical state
    (1 parser + 1 manager + 6 entities) — every process launched from the NEW
    `*.monads/*.livedesk-taskbar/ops/+x/` path; zero stale tile-picker entity
    procs; emoji→sprite pipeline re-tested OK from the new location;
    `livedesk_open.txt` registry repopulated (self, m8_redhorned,
    m1_ninjadragon, book-stack, asa, ava).

---

## Task 2 — Qwen2.5-Coder ladder on the LAN Mac → uniform product family

### Context (already known, from `045.muchi-pal-agent/dev-agent-convo/02.LAN-nodes🌐.md`)
- Mac: `lfs.master@10.0.0.144` (pw `1234`), ollama LAN-open at
  `http://10.0.0.144:11434` (via `~/start_ollama.sh`).
- Linux box: `jb@10.0.0.187`, ollama LAN at `http://10.0.0.187:11434`.
- This box already runs ollama locally (127.0.0.1:11434) with
  qwen2.5-coder:7b, gemma3:latest, gemma3:270M, llama3-groq-tool-use:8b.
- hai + agent45 already share this LAN setup.

### Models to install (the "ladder", one uniform family)
- `qwen2.5-coder:0.5b` (smol, tool-router tier)
- `qwen2.5-coder:1.5b`
- `qwen2.5-coder:3b`
- `qwen2.5-coder:7b` (manager tier — already on this box + Linux node)
- `codeqwen1.5:7b` (CodeQwen1.5-7B, code completion/FIM specialist)

### Step 1 — Reach the Mac + verify ollama
- `ssh lfs.master@10.0.0.144` (sshpass w/ `1234`) → confirm ollama running,
  `curl http://10.0.0.144:11434/api/tags`.

### Step 2 — Install the ladder on the Mac
- `ssh ... ollama pull qwen2.5-coder:0.5b` (and 1.5b/3b/7b, codeqwen1.5:7b).
- Prune decision from LAN-nodes doc still open ("delete the non-gemma models on
  Linux?") — ask user; leave as-is unless told.

### Step 3 — Smoke-test every tier from this box
- `curl http://10.0.0.144:11434/api/generate` for each model — confirm real
  answers + rough latency per tier.

### Step 4 — Research local "qwen-agent" strategies (get the most from toolcalls)
- Tool-call routing: 0.5B/1.5B as fast intent routers (like gemma3:270m today),
  3B/7B as coders, 7B as manager/curator — matches existing
  `15.CHAT-HAI-PLAN.md` ladder.
- qwen2.5-coder FIM / `insert` capability (available in the ollama tags) for
  real in-file code completion.
- Small-model agent loop: model writes text, app decides who speaks/acts next
  (Harnecient Way) — no model-driven control flow.
- Save findings to the plan's research file.

### Step 4b — chat-hai is the TESTGROUND for the ladder + harness (user decision)
- chat-hai (`&.hq-apps/chat-hai`, selectable under 14.h-ai; slender side-bar
  GUI, constantly scrolling chat feed) is where the qwen ladder + Harnecient
  harness get proven — NOT just "a client to migrate" (older Step 5 framing).
- Proof-of-concept per user (`chat-hai.2026` notes): 4 smol models always
  chatting on the Mac, recording memories (truncated sometimes), simple
  relationships, user can jump in anytime; 1-2 bigger "moderator" models that
  are slower but piece together outputs from the smols, review/adjust their
  memories, tune joints — and later take real guided tasks. App-side
  dispatch = Harnecient (no model-driven control flow).
- Start: get them "jumproping" together as a demo, then dev jumps in with more
  ops/challenges, eventually real user tasks with jobs/personalities and
  priority-marked memory for fsm/rl-deterministic recall.

### Step 5 — Migration of APIs to one uniform endpoint
- Decide canonical base: `http://10.0.0.144:11434` (Mac) as primary inference
  node, this box + Linux as mirrors/backup.
- Create a single model-tier registry (e.g. `net/ollama-lan.pdl` or .json):
  `router=0.5b / coder=3b / manager=7b / fim=codeqwen1.5:7b`.
- Point clients at it: chat-hai (TESTGROUND, first), muci-pal-agent
  (`045.muchi-pal-agent🤖️+1++`), iqabod, khtpm h-ai cell 14.

### Step 6 — Begin the uniform family of products
- One shared wrapper (e.g. `ops/qwen.sh` — `qwen.sh ask <tier> <prompt>`,
  `qwen.sh stream <tier>`, `qwen.sh tool <tier> ...`) all apps call.
- Onboarding first app = chat-hai ladder (the testground) onto the wrapper as
  the proof case, then migrate the rest (h-ai cell 14 next).

### Task 2 status markers
- [x] Mac reachable + ollama confirmed
- [x] Ladder pulled (0.5b/1.5b/3b done; 7b + codeqwen1.5:7b left) on Mac
- [ ] All tiers smoke-tested from this box
- [ ] qwen-agent research notes written
- [x] Tier registry created (`net/ollama-lan.pdl`)
- [ ] First app on the shared `qwen.sh` wrapper
  - `net/qwen.sh` wrapper written but has a known bug: uses `jq` (not installed
    on this box) — must switch to python3 before it works.
