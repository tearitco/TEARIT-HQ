# 🧭 HARNECIENT × h-ai RELAY PLAN 🧭

> 🟢 STATUS: **DESIGN PHASE — approved direction, not yet built.**
> 📅 Date: 2026-08-12 · 🏠 Home: `au11-hq/` · 🧬 Predecessor: `HARNECIENT-HACK.md` (the pattern) + `&.widgits/ai-cell/code-tools-harness/api-test-results.md` (the 9/9 proof)
>
> This is **HIGH PRIORITY + LOAD-BEARING** for the Harnecient company and its customers/stakeholders.
> If you are a new agent or a stakeholder reading this: every claim below is grounded in real code and
> real measured results, and the whole plan ends in a **lasting, reproducible, sellable proof harness**.

---

## 🎯 WHAT WE'RE BUILDING

Three connected deliverables, in order:

1. **🔀 h-ai gets a CHOOSABLE MODEL** — today the model is hardcoded
   (`stable-code:latest`, `g_model_name` in `&.widgits/ai-cell/ops/khtpm_ai_cell_render.c:273`).
   We add a real switcher so the user can pick the model per session, from a fixed whitelist.
2. **🧠 HARNECIENT becomes a real backend MODE** inside h-ai — when a non-tooled model is selected
   (`gemma3:270m`, `gemma3:1b`, `stable-code:latest`/3B), h-ai uses the my-lawyer protocol
   (plain `/api/generate`, persona forbidding structure, short plain-text prompts, tolerant
   parsing, **deterministic app-side dispatch**, real-file folding, fallback everywhere) —
   **never sending a `tools` field**.
3. **🔌 DEMO + 🧪 LASTING HARNESS** — prove the whole loop the way the house tests everything:
   **relay injection** (typing into the real h-ai window through its relay file), ending in
   **control tb** (real, verified effects on the livedesk taskbar's state files) — then bake that
   into a reproducible harness with N/N proof artifacts.

---

## 🧠 WHY THIS IS LOAD-BEARING (read this, stakeholders)

- The company's bread-and-butter (see `HARNECIENT-HACK.md`): **tool-like power out of NON-tooled
  models, without the API ever knowing we want tools.**
- `HARNECIENT-HACK.md` + the `code-tools-harness` already **measured it 9/9** (read/edit/run) on
  `stable-code:latest` (3B), `gemma3:1b`, `gemma3:270m` — but that proof lives at **CLI level**
  (a standalone pure-C harness talking straight to Ollama).
- **h-ai is the storefront.** It is cell 14 in the livedesk taskbar — a real, managed X11 chat
  window humans actually use. Wiring the Harnecient mode into h-ai turns a measured CLI trick into
  **a product a customer can pick and drive.**
- The demo ("relay injection and control tb") is the **customer-facing proof**: a human types a task
  into the chat window → the non-tooled model does **real read/write/run** → the livedesk taskbar's
  real state changes. That loop is exactly what we sell.
- The harness is the **reproducible, auditable, sellable** version of that demo (house standing rule:
  every claim needs a repeatable test with artifacts).

---

## 🗺️ WHAT'S ALREADY REAL (verified, don't re-derive)

### ✅ Proven pattern + proof
- `HARNECIENT-HACK.md` — 6 components + DESCRIBE-don't-CLASSIFY + recipe. **9/9 measured** in
  `&.widgits/ai-cell/code-tools-harness/` (pure C, `run_native_tools.c`, scenarios
  `harnecient_read/edit/run`, transcripts in `results/<model>/`).
- Non-tooled facts: 3B rejects any `tools` field (server 400); naive text-JSON tool calls flake/hallucinate;
  the Harnecient hand-hold (deterministic app-side dispatch + canonical edits) is what makes it reliable.
- Live reference implementation: `@.apps/my-lawyer/` (`ops/mylawyer_case_worker.c`,
  `ops/mylawyer_judge_worker.c`, `ops/json_parser.c`, personas in `pieces/registry/personas/`,
  real gemma-270m outputs in `data/cases/1/`).

### ✅ h-ai (the storefront) already has the skeleton
- `&.widgits/ai-cell/ops/khtpm_ai_cell_render.c` (~1877 lines): real X11 window, real nav, real
  **relay injection** (`#.desktop/ai_cell_agent_relay.txt`, bare-decimal-ASCII-per-line: digits
  48–57, Enter=13, Escape=27, Backspace=8, printable 32–126), real disk-persisted chat history,
  PNG-dump + receipt verification, and **a real deterministic tool mechanism already ported from
  agent-45**: `detect_tool()` (line 517) parses the USER message → `read_file|write_file|edit_file|
  search_in_files|list_dir|cmd_exec`; read-only tools pre-execute, `write/edit/cmd` ask
  **approve/deny via the nav** (line 1311+). **The model never emits tool calls today.**
- Backend: `send_to_ollama()` (line 299) writes `{"model":"..","prompt":"..","stream":false}` to
  `/api/generate` via a detached `curl` child; `check_pending()` extracts the `"response"` field.
  Every run is auditable: `pieces/audit/payload-<pid>.json`, `response-<pid>.json`,
  `ai-cell-frame.png.receipt.txt` (fields `ok w h t nav n_nav n_sessions n_msgs tool_pending tool`).
- `g_backend_mode` enum already exists (`BACKEND_OLLAMA_RAW` / `BACKEND_AGENT45_LEGACY`) — the
  seam for a new `BACKEND_HARNECIENT` value is already there (line 271).

### ✅ The taskbar ("tb") — what we control
- Two-process livedesk taskbar: parser + manager, file-relay IPC through `#.desktop/`.
- **Real control surface** (the files a tool can read/write and a harness can assert on):
  - `#.desktop/strip_var_tabs.txt` — open bottom-bar tabs (what the taskbar renders).
  - `#.desktop/strip_var_username.txt`, `strip_var_datetime.txt`, `strip_var_shortcuts.txt`,
    `strip_var_*.txt` — header var labels.
  - `#.desktop/strip_state.txt` — saved state.
  - `#.desktop/strip_history.txt` — MANAGER relay: inject resolved codes (e.g. `4000+n`
    header-cell click → launches a cell; see `khtpm_strip_codes.h` `KSC_HQ_HEADER_BASE`).
- Reference harness for the relay contract: `#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh`
  (`nav/row/key/type/esc/frame/hqcell/mgrcode`), and the older `harnesses/db-hq/nav.sh`.

---

## 🔬 THE MEANING OF "RELAY INJECTION + CONTROL TB" (exact, so we build the right thing)

- **RELAY INJECTION** = the ONLY way the house tests any window (standing rule 1: "All testing goes
  through relay/inject (`nav.sh`), never direct CLI binary calls"). For h-ai it means: append the
  ASCII codes of a real typed task to `#.desktop/ai_cell_agent_relay.txt` — focus composer (digit),
  Enter (arm), type the task (printable codes), Enter (send). Same contract as every khtpm window.
- **CONTROL TB** = the task asks h-ai (Harnecient mode) to do real work against the livedesk
  taskbar's state: **read** `strip_var_tabs.txt`, **write** an observed value into an output file,
  **run** a command that appends a marker line to a taskbar-visible file (e.g. `strip_state.txt`).
  The "controlled tb" proof = the taskbar-visible files actually changed + h-ai's own receipt shows
  the tool fired + the whole transcript is on disk.
- So the demonstration is one continuous human-shaped loop:
  **👤 human → 🔌 relay inject → 🪟 h-ai window → 🧠 non-tooled model → 🛠️ deterministic
  Harnecient dispatch → 📁 real file effects on the taskbar → ✅ verified by receipt + files.**

---

## 🏗️ DESIGN

### 🔀 Phase 1 — Choosable model (small, first)

- Add a **sidebar nav item "Model"** (or composer command `model <name>`) that selects from a
  fixed whitelist. Persist the choice in `&.widgits/ai-cell/pieces/state/model.txt` so it survives
  restart; sidebar already draws `g_model_name` (line 1355).
- Whitelist (each entry carries its mode):
  | Model | Mode | Why |
  |---|---|---|
  | `stable-code:latest` | 🧠 Harnecient | current default; 3B, NO native tools (server rejects) |
  | `gemma3:1b` | 🧠 Harnecient | proven 9/9 |
  | `gemma3:270m` | 🧠 Harnecient | flagship underdog, proven 9/9 |
  | `llama3-groq-tool-use:8b` | 🛠️ NATIVE | real `tool_calls` (dict-or-string args, `\uXXXX`) |
  | `llama3:latest`, `llama2:latest` | 🧠 Harnecient | future-friendly |

### 🧠 Phase 2 — Harnecient backend mode (the wiring)

- Add `BACKEND_HARNECIENT` to the `BackendMode` enum (line 271).
- When active, the chat path becomes the my-lawyer protocol, ported into the GUI loop:
  1. **Persona**: load `&.widgits/ai-cell/pieces/registry/personas/<model>.txt` (new dir, mirrors
     `@.apps/my-lawyer/pieces/registry/personas/` — judge/lawyer style: forbids JSON/lists, short
     plain text). Prepended to the prompt sent to `/api/generate`.
  2. **Deterministic dispatch (already built)**: `detect_tool()` keeps firing on the user's message;
     read-only tools pre-execute, `write/edit/cmd` ask approve/deny via nav (unchanged).
  3. **Model-suggested hand-hold (NEW — the my-lawyer essence)**: after a read, h-ai asks the model
     one short plain-text question; tolerantly parses the noisy reply (`*…*` marks, ``` fences —
     reuse/extend the harness's fence-strip + tolerant extract), pulls the suggested action, applies
     the **CANONICAL app-side edit** (model text only credits `model_suggested_fix`, never trusted
     verbatim), then **deterministic run**; model interprets the real output. This is
     `harnecient_read/edit/run` from `code-tools-harness/run_native_tools.c` moved into the GUI.
  4. **Never send a `tools` field** on Harnecient modes.
  5. **Fallback everywhere**: any unparseable reply degrades to a plain chat answer — no crash
     (⚠️ recall the `fopen(dir)`+`ftell=LONG_MAX` malloc crash: never trust model output).

### 🔌 Phase 3 — Relay demo ("relay injection and control tb")

- **Scenario "control tb"**, driven purely through `nav.sh`-style relay injection into
  `#.desktop/ai_cell_agent_relay.txt`:
  1. Focus h-ai composer + arm + type (relay): `read #.desktop/strip_var_tabs.txt and write its
     contents to /tmp/code-tools-harness-work/tabs-snapshot.txt, then append "h-ai:verified" to
     #.desktop/strip_state.txt`.
  2. h-ai Harnecient mode: deterministic `read_file` (pre-executes) → optional model question →
     `write_file` (approve/deny resolved via relay digits+Enter) → `cmd_exec` append (approve/deny).
  3. Harness asserts: `tabs-snapshot.txt` == real tabs content; `strip_state.txt` contains the
     marker; h-ai receipt `tool=<name>` / `tool_pending=0`; payloads audited show **no `tools` key**.
- This is the **customer-facing proof of concept**, then immediately baked into Phase 4.

### 🧪 Phase 4 — The lasting reproducible harness

- New dir: `&.widgits/ai-cell/relay-harness/` (mirrors `code-tools-harness` layout + the
  `test-harn-same/` ops convention; **pure C asserts + bash driver, NO python**).
- Shape:
  ```
  relay-harness/
    button.sh            # ensure h-ai running (build-if-missing + launch)
    run_all.sh           # per-model: restore fixtures → run scenarios → write proof/SUMMARY.md
    nav.sh               # relay driver for the h-ai relay (same shape as taskbar harness's nav.sh)
    ops/
      +x/assert_file_contains.+x
      +x/assert_receipt_field.+x     # e.g. assert_receipt_field model=tool_pending=0
      +x/assert_no_tools_field.+x    # greps audited payload-<pid>.json for "tools"
      +x/assert_taskbar_state.+x     # assert strip_state.txt / strip_var_*.txt effects
    scenarios/
      01_model_switch.sh            # pick a model, assert receipt n_nav/model label
      02_relay_read.sh              # read-only tool via relay, assert tool=read_file + output
      03_control_tb.sh              # the full demo (read→write→run → tb files changed)
      04_write_approve_deny.sh      # approve + deny both proven through relay
    proof/                          # per-run artifacts: transcripts, receipts, PNGs, payloads
  ```
- **KPI: reproducible N/N on each Harnecient model** (270m/1b/3B), plus the 8b NATIVE row where it
  belongs. Same N/N table style as `code-tools-harness/api-test-results.md`, written to
  `proof/SUMMARY.md` and mirrored into the api-test-results doc when verdicts change.

---

## ✅ SUCCESS CRITERIA (measurable)

1. 🔀 **Choosable model works**: user switches models in the GUI; choice persists across restart;
   the sidebar label changes.
2. 🧠 **Harnecient mode is real**: on 270m/1b/3B, h-ai does read→(model hand-hold)→edit→run with
   **canonical edits** and **zero `tools` fields** in every audited payload.
3. 🔌 **Demo loops end-to-end**: relay-injected task → real window → non-tooled model → **taskbar
   state files provably changed** → verified by receipt + file asserts.
4. 🧪 **Harness reproducible**: a clean run scores N/N with artifacts on disk (`proof/SUMMARY.md`,
   transcripts, receipts, payloads); no python anywhere; relay-only testing honored.
5. 📚 **Docs updated**: `INDEX.md` row added, `HARNECIENT-HACK.md` measured section extended with the
   h-ai/GUI-level proof, findings appended to the h-ai `LEARNINGS.md`/`ONBOARDING.md` as they land.

---

## ⚠️ RISKS + FALLBACKS

- **270m latency/quality** in a GUI loop → the model is choosable; demo on 1b or 3B if needed; 270m
  stays a target row (it's the flagship).
- **Approve/deny reachable via relay?** Nav items are digit-jump + Enter (same as every window), so
  it *should* work — but must be tested first (scenario 04). Fallback: keep the approve/deny (it's
  the product's human-in-the-loop guarantee), don't bypass it.
- **`detect_tool()` keyword collisions** ("write a C function that…" → write_file) — the existing
  conservative pathish-token guard (line 483) already prevents this; keep it and add a scenario for it.
- **Model noise / fake tool shapes** → tolerant parsing + canonical-app-side-edit + fallback to chat;
  never trust model output (crash lesson documented in `LEARNINGS.md` §9).
- **Parsing payloads for the no-tools check** → grep the audited `payload-<pid>.json`, which already
  exists per run; no new capture machinery needed.

---

## 🚀 MILESTONES (ordered)

1. 🔀 Model switcher + persistence (whitelist, sidebar item, state file). [small]
2. 🧠 Persona registry (`pieces/registry/personas/`) + `BACKEND_HARNECIENT` path in the GUI loop.
3. 🔌 Manual relay demo of "control tb" once, end-to-end (proof of concept, human-visible).
4. 🧪 Bake into `relay-harness/` (nav.sh driver + C assert ops + scenarios 01–04 + run_all.sh).
5. 📚 N/N proof → `proof/SUMMARY.md` → update `INDEX.md`, `HARNECIENT-HACK.md`, `ONBOARDING.md`.

---

## 🔭 TELESCOPE — the future horizon, and reminders we must not lose 🔭

> 🛰️ A look-ahead + reminder section, added 2026-08-12 on direct instruction. NOT a spec yet —
> when these tasks become real, they get their own plans; this stays the north star.

### 🌠 What this loop will be USED FOR (after the design steps above land)

- The exact loop this plan builds (relay inject → h-ai → non-tooled model → deterministic
  read/write/run → real effects) is the **engine for real product work**, not just a demo:
  - 🎮 **CREATE EVENTS** — the livedesk events runtime (`EVENTS_RUNTIME.md`, `EVENT_AI_VISION.md`;
    right-click "Play" / `RUN_METHOD:Play`) gets real content built through h-ai's tools.
  - 🚶 **MOVE ONDESK ENTITIES** — entities should have **MOVE CAPABILITY + a MOVE RANGE** (a real
    rule, not free teleporting — range-limited movement is part of the game/toy design).
  - 🏗️ Any agentic task on a non-tooled LAN model, driven through the real windows, exactly like
    my-lawyer reads/writes real case documents today.

### 🕐 FAKE TIME — the big one, DO NOT FORGET

- The **time toolbar (the "15." slot in the header, today just the non-interactive `${datetime}`
  element in `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm`)** will STOP showing real time.
- ⏳ It will show **FAKE TIME**, and fake time starts at **YEAR 0 AD** (0 A.D.) — a calendar from
  the start of everything, not the real wall clock.
- ⏭️ Fake time advances by **TWO mechanisms**:
  1. **`endturn`** — turn-by-turn advance (like a strategy game: end your turn → time moves).
  2. **⏱️ TIME TICKER** — a real ticking clock that moves fake time continuously while running.
- 🎛️ Inside that time toolbar we will add **TIME OPTIONS** (a chooser, not buried settings):
  1. **"start/stop ticker"** — pause/resume the fake clock.
  2. **"ticker speed"** — a ticker chooser (e.g. slow / normal / fast).
- 📌 The taskbar header cell count/positions will shift when this lands — remember nav numbers are
  NOT static (README.md's own warning), re-derive from the current frame before any relay test.

### 📌 Reminder contract

- This section is a **telescope**: it tells future-us where we're aiming, and reminds us of
  commitments (move-range rule, 0 AD fake time, ticker options) so they don't get lost between
  sessions. When one of these becomes actual work, write its own plan doc and link it from here.

---

## 📚 RULES WE OBEY (house standing rules, INDEX.md)

1. **Testing = relay/inject only** (`nav.sh`), never direct CLI binary calls.
2. Check local chtpm → tpmos before inventing new UI/state patterns.
3. New content under `sessions/…`, not `@.apps/` (harness artifacts live in the app's own dir + `proof/`).
4. Own harnesses under your own `xyzfs/users/<you>/harnesses/` if sharing the desktop.
5. Blocked/uncertain → document in `au11-hq/` rather than guessing silently.
6. **No python, ever** — pure gcc or bash (user rule; `LEARNINGS.md` §3).
