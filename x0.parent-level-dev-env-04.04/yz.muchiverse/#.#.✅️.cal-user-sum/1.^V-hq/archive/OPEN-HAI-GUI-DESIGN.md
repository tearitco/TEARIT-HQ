# AI cell GUI — design document

> STATUS: **v1 REAL AND WORKING**, built + verified live 2026-08-12
> (same day as the design). See §9 for the as-built state, what's
> genuinely done vs still deferred, and exact file locations — read
> that section before assuming anything below is still "just a plan."
> Date: 2026-08-12. Direct instruction: "we haven't wired up agent45 or
> scm up to 14.ai yet, but we should with new x11 gui... we should write
> a design document about this." Then: "get started." Then: "since u
> said raw use for ollama is better, lets just use that as main."

---

## 0. One-sentence version

A new, real X11 GUI window — visually a Claude/Electron-style chat app
(sidebar + message thread + composer), khtpm-nav-integrated like
db-hq — that becomes the shared, backend-agnostic interface for every
"talk to an AI running in this house" surface: agent-45 today, SCM
later, launched from the taskbar's `ai` cell.

---

## 1. Cell number — CORRECTED 2026-08-12 (was wrong earlier, fixed now)

**Cell 14 = "ai" — confirmed directly from the live header layout**,
`*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm`:

```
<button label="plugins" onClick="ACTIVATE:10"/>
<button label="menus" onClick="ACTIVATE:11"/>
<button label="store" onClick="ACTIVATE:12"/>
<button label="network" onClick="ACTIVATE:13"/>
<button label="ai" onClick="ACTIVATE:14"/>
<button label="${datetime}"/>   <!-- no onClick - not an ACTIVATE cell -->
```

An earlier pass of this doc said "cell 13," sourced from a comment in
`khtpm_taskbar_manager.c` (line ~2135) — that comment is STALE: a
`menus` cell was inserted at position 11 sometime after the comment
was written, shifting `store`/`network`/`ai` each up by one. The
comment was never updated to match. **Lesson: trust the live
`.chtpm` layout file over a `.c` comment describing it** — the comment
can drift, the layout file is what actually renders. **This GUI wires
into cell 14.**

---

## 2. What's real today (checked, not assumed)

| Piece | Status | Evidence |
|---|---|---|
| **agent-45** | Real, working CLI agent. Real tool-calling loop: `list_dir`, `connect_op`, `send_message`, `switch_model`, `check_response`, `cmd_exec`, `compose_frame`, `edit_file`, `search_in_files`, `json_parser`, `web_search`, `opencode_ask`. | `045.muchi-pal-agent🤖️+1++/` — `button.sh`, `agent-45-aug3.md` |
| **agent-45's own UI today** | Runs inside a chtpm `<cli_io>` panel (`pieces/chtpm/layouts/chat.chtpm`) — text-only, no sidebar, no message-bubble rendering, but a REAL wraith-alpha-style nav (`[>] 1. Message: []` bracket badge, `Nav > _` prompt) already exists. Confirmed live end-to-end this session (2026-08-12) — see §6. | `chat.chtpm`, live test |
| **SCM** | Real design doc, ZERO code yet ("v1 scope... fresh standalone system... wired into 045 chat only LATER" — direct quote, locked decision S1). Not ready to wire into anything today. | `047.scm🎓️+1/!.SCM-DESIGN.md` |
| **LAN model endpoint** | Already real and in use: `10.0.0.144:11434` (Mac LAN) is SCM's own "judge" model endpoint — port 11434 is Ollama's default API port, so this is very likely already an Ollama install, not a new thing to stand up from scratch. `10.0.0.187` (Linux node) is used for iqabod training. | `047.scm🎓️+1/!.SCM-DESIGN.md` §1 table, "Compute constraint" row |
| **Taskbar cell 14 "ai"** | Confirmed bare inert placeholder — no dedicated dispatch case, falls through the generic no-submenu/inert-cell pattern (same as palettes/edit/etc). | `khtpm_strip_header.chtpm` (real layout), `khtpm_taskbar_manager.c` (dispatch) |
| **A "watch an agent work" UI** | Does not exist anywhere. This is genuinely greenfield. | grep across house, no hits |

**Consequence:** the LAN-Ollama piece isn't new infrastructure to build — it's an already-running endpoint this project can point a client at. Verify the actual model(s) currently pulled on `10.0.0.144` before assuming a 3b model is available there (see §7, open question 1).

---

## 3. Visual reference (already provided, already read in full)

`au11-hq/hai-desktop-gui.html` — a real, self-contained HTML/CSS/JS
mock (not wired to any backend, `mockClaudeReply()` is a canned-response
stub). Concrete shape to port to X11/khtpm:

- **Left sidebar** (fixed width, collapsible): "New chat" button, nav
  items (Chats/Projects/Artifacts — for THIS GUI, likely: Chats/Tools/
  Sessions or similar, needs a decision, see §7), a scrollable recent-
  chats list, a footer with user avatar + model name.
- **Top bar**: chat title, a "model pill" (clickable, shows/cycles
  active backend — this is the natural place to show "agent-45" vs
  "SCM" vs whichever LAN model, once more than one exists).
- **Message thread**: user messages right-avatar/grey-bubble,
  assistant messages green-avatar/transparent-bg, markdown-lite
  rendering (code fences, inline code, paragraphs), a typing indicator
  (3 bouncing dots) while waiting.
- **Composer**: multi-line textarea (auto-grows), attach/tools icon
  buttons (real ones here could map to agent-45's own tool list),
  send button (disabled when empty/streaming).

This is a strong, already-approved visual target — port the CSS
variables/layout shape into a real `.chtpm`+`.css` pair using the same
khtpm CSS engine `db-hq`/`events-hq` already use (`khtpm_css_parser.c`,
now the single shared canonical copy in `&.widgits/_shared-lib/` — see
that dir's README), not a from-scratch design pass.

---

## 4. Nav integration (must match db-hq's real, hard-won conventions)

Per `!.HOUSE_STDS.md` #21-24 (all real, live-caught fixes this house
already paid for once — do not re-derive or regress any of these):

- **Managed window + `_MOTIF_WM_HINTS`**, NOT `override_redirect` — the
  actual fix for real keyboard focus under Mutter/XWayland. Every list
  item in the sidebar, every message action, the composer's send
  button — all need real `[>N]`/`[ N]` bracket-badge nav (wraith-alpha
  convention), digit-jump, arrow-step, Enter-activates. This is the
  whole reason db-hq exists as a proof-of-concept — reuse its pattern,
  don't reinvent.
- **RGB compose→present** (`XGetImage`→`XPutImage`, not `XCopyArea`) —
  this is now the default rendering pattern across db-hq, the taskbar,
  and entities (`tp_desktop_window_rgb.c`). This new window should be
  built on this pattern from day one, not bolted on later.
- **Real sprite/text rendering** — Xft for text (matches
  db-hq/events-hq), no GL, no emoji-font crash risk (see standard #24's
  non-fatal `XSetErrorHandler`).
- **Real entity/session portrait**, if relevant — same `sprite.csv`
  convention, not a placeholder.

---

## 5. Backend abstraction (why "agent-45 today, SCM later" matters for the design)

The GUI's own composer/message-thread/sidebar code should NOT
hardcode "talks to agent-45." Concretely:

- One real, already-existing, NOW VERIFIED (§6) integration point:
  agent-45's own `pieces/keyboard/history.txt` relay
  (`[TIMESTAMP] KEY_PRESSED: <code>` format — this project's family
  predates khtpm's bare-decimal convention, see
  `_.0.aigent-testing-k9.txt`'s own scope note on the two different
  formats/families, don't conflate them). The new GUI can drive this
  SAME file, or call agent-45's tool loop more directly if a cleaner
  API exists in `ops/` — needs a real read of agent-45's actual
  dispatch code before deciding which is better, not decided here.
- A second, distinct backend: a raw LAN Ollama call (`10.0.0.144:11434`
  or wherever the real model ends up) for the low-risk experiment
  described in §8 below — this is NOT agent-45, no tool loop, just a
  chat completion call.
- SCM, later: per its own design doc, not ready to wire in yet — the
  GUI's backend abstraction should be a real interface/dispatch point
  (not necessarily a full plugin architecture — see `!.HOUSE_STDS.md`'s
  own "don't build for hypothetical futures" bias) but shouldn't
  actively make wiring SCM in later harder than it needs to be.
- The **model pill** in the topbar (§3) is the natural, already-in-the-
  reference-design UI surface for picking which backend a given chat
  targets.

---

## 6. VERIFIED live test — real relay injection into agent-45 (2026-08-12)

Direct correction received and confirmed by testing: "those older
programs inject into `history.txt` not relay (they aren't x11)" —
`interact_relay.txt` (referenced in `agent-45-aug3.md`'s own
`<interact>` tag docs) was the WRONG file for this. Per
`_.0.aigent-testing-k9.txt`'s scope section, the `chtpm_parser_pal`/
`prisc+x` family (which agent-45 IS — confirmed via its own live
process list: `system/chtpm_parser_pal`, `system/prisc+x`,
`system/keyboard_input`) reads `pieces/keyboard/history.txt`, format
`[TIMESTAMP] KEY_PRESSED: <code>` — NOT khtpm's bare-decimal-per-line
convention. Confused these once this session, corrected, retested.

**Real, confirmed end-to-end procedure** (`./button.sh run`, then
inject into `<session_dir>/pieces/keyboard/history.txt`):

1. UI boots in **Nav mode** (`Nav > _` prompt, `[>] 1. Message: []`
   bracket-badge row — same wraith-alpha nav convention as everywhere
   else in this house).
2. Inject `KEY_PRESSED: 49` (ASCII '1') + `KEY_PRESSED: 13` (Enter) —
   this SELECTS AND ARMS the Message row for typing. Frame changes to
   `Active [^]: (ESC to exit)`, row shows `[^] 1. Message: [_]` with a
   live cursor. **You cannot just start typing from Nav mode — the row
   must be armed first**, exactly like db-hq/events-hq's own nav
   convention requires selecting before acting.
3. Inject the message text, one `KEY_PRESSED: <ascii-code>` line per
   character, then `KEY_PRESSED: 13` to submit.
4. Frame shows `[SYS]: Querying AI...` / `Thinking...`, then the real
   model response appears in the transcript (`You: ...` / `Aida: ...`),
   `[SYS]: Response received.`

**Confirmed real** (not simulated): the model was actually called over
the LAN Ollama endpoint fixed earlier in this session — full
round-trip, injection → nav → type → submit → real inference → real
rendered response, all via file-based relay, zero XTest/direct-input
tooling needed. This is the concrete mechanism the new X11 GUI's own
`<interact>`-equivalent (or whatever its actual dispatch turns out to
be) needs to either reuse directly or translate into.

**One real caveat found, not a mechanism failure**: the actual answer
content was confused/hallucinated (referenced an unrelated earlier
tool-call error instead of answering "what is 2+2"). Likely cause: the
session's own pre-seeded `world_01` chat history (copied in as
demo/template conversation context, see `button.sh`'s own "world_01 is
COPIED... into the session" comment) confusing a 270M-parameter model's
limited context handling, not evidence the relay/nav mechanism itself
is unreliable. Worth a clean/fresh session for the next real test,
and/or trying a bigger model (`stable-code:latest`, `llama3:latest`) if
using this for anything beyond a mechanism smoke-test.

---

## 7. Open questions (real decisions, not yet made)

1. **RESOLVED 2026-08-12.** `#.Z.HUMAN_LLM/.MAC-ACCESS.txt` already had
   real SSH credentials (verified working, tried in this session too).
   The earlier direct `curl` failure was real, not a sandbox
   limitation — Ollama only binds `localhost:11434` after a Mac
   restart, `launchctl setenv` alone doesn't fix it for the GUI app's
   child process. Fixed live (see that doc for the exact commands) —
   Ollama now reachable directly at `10.0.0.144:11434` with real models
   pulled: `gemma3:1b/270m`, `llama3-groq-tool-use:latest/8b` (has
   `tools` capability), `llama2:latest`, `llama2-uncensored:latest`,
   `nomic-embed-text:latest` (embeddings), **`stable-code:latest`** (3B,
   a real CODE model — almost certainly the "ollama-3b" meant for
   event-building work), `llama3:latest`. Verified end-to-end with a
   real completion call. **Caveat found during this test:** the model
   has zero knowledge of THIS house's codebase (confabulated a
   plausible-sounding but wrong expansion of "khtpm") — any real task
   prompt needs real file context pasted in, not reliance on background
   knowledge. Also: this fix does not survive a Mac reboot/Ollama.app
   relaunch, re-apply if unreachable again.
2. **RESOLVED 2026-08-12 — sequencing, not a permanent either/or.**
   Relay (§6) is the fast FIRST connectivity proof — already verified
   end-to-end, zero new agent-45-side work. But it means the GUI is
   effectively "a robot typing into agent-45's own terminal UI" — it
   inherits every quirk of that Nav-mode/arm-then-type state machine,
   and breaks if `chat.chtpm`'s own layout ever changes. **Direct
   tool-loop call is the real v1 architectural target** — a native GUI
   calling agent-45's own tool loop directly is the right shape
   long-term, not routing through a fake keyboard. This needs one real
   investigation pass into agent-45's `ops/` dispatch code (not done
   yet) before it can be built — use relay to prove the GUI shell/nav/
   transcript pipeline works first, then do that investigation, then
   swap the backend call — not a rewrite, a contained swap if the
   backend abstraction (§5) is built the way it's described there.
3. **RESOLVED 2026-08-12 — sidebar taxonomy.** Not "Chats/Projects/
   Artifacts" (that's Claude's own taxonomy, not this house's). Real
   equivalent, grounded in what's actually true here:
   - **Backend/model switcher** — agent-45, SCM (once ready per its own
     design doc), and the raw LAN models directly (`stable-code:latest`,
     `llama3-groq-tool-use:latest/8b` — has a real `tools` capability
     flag per `/api/tags`).
   - **Recent sessions list** — agent-45 already creates a real
     timestamped session dir (`pieces/sessions/<epoch>-<pid>/`) per
     `./button.sh run` — surfacing that as chat history is free, not new
     plumbing.
   - **Live tool-call feed** — agent-45's own frame already renders
     `[calling write_file {...}]` / `[result]: ...` inline in the
     transcript (confirmed live, §6); pulling that into its OWN visible
     pane (not buried in the transcript) makes it much easier to watch
     what it's actually doing — the whole stated point of this GUI.
   - **Working-directory/project picker** — agent-45's tools
     (`edit_file`, `cmd_exec`, etc.) operate on real paths; a visible
     "where is it pointed right now" selector avoids not knowing what
     dir a command actually ran against.
4. **RESOLVED 2026-08-12 — window scope for v1.** Full shape from day
   one (sidebar + topbar + transcript + composer), not staged — direct
   instruction, overriding this doc's own earlier "prove toy-scale
   first" bias suggestion for this specific decision.

---

## 8. Separate, lower-risk parallel track: Ollama-3b experiment

Direct instruction, same message: "turns out u wont beable to use
haiku from here, but u can use the ollama-3b model on lan, so we will
set this up, experiment with u using that and see how that goes for
doing the event building since that is low impact low risk."

This is DELIBERATELY decoupled from the GUI design above — it's an
experiment in delegating bounded implementation work (the events-hq
gap tasks already written up in `HAIKU_TASKS.md` H6-H8) to a LAN model
instead of a Claude subagent, evaluated on real, low-blast-radius work
(event-building, not taskbar/db-hq/entity core code). Concrete next
steps, separate from the GUI:

1. Confirm the real LAN endpoint + model (§7 open question 1) — reuse
   SCM's already-proven `10.0.0.144:11434` connection details if that
   turns out to be the right target, rather than standing up a new
   Ollama install from scratch.
2. Decide the actual harness shape: does this session (Sonnet) drive
   the Ollama model via a real tool-call/API integration and relay
   its output the same way `HAIKU_TASKS.md` describes reviewing
   Haiku's work, or is this closer to a fully separate process this
   house's own agent-45 infra could run?
3. Pick ONE of H6/H7/H8 (`HAIKU_TASKS.md`) as the first real test case
   once the harness above is decided — don't design the harness and
   pick the task in the same breath, verify the harness works on
   something trivial first if possible.

This doc does not resolve #2/#3 — flagging them as the concrete next
conversation, not deciding unilaterally here.

---

## 9. AS-BUILT (2026-08-12) — what's real right now

**Files:**
- `&.widgits/open-hai/ops/khtpm_open_hai_render.c` — the whole window (~570 lines)
- `&.widgits/open-hai/ops/build_open_hai.sh` — build script
- `&.widgits/open-hai/button.sh` — launcher (build-if-missing + setsid nohup, same shape as `open_db_hq.sh`)
- `*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c` — `livedesk_build_ai_menu()` (new), `ktb_hq_open()`'s `which==14` case (new), `livedesk:open-open-hai` dispatch case (new)

**Genuinely working, verified live (not assumed):**
- Real managed X11 window, `_MOTIF_WM_HINTS`, RGB compose→present (`XGetImage`→`XPutImage`) — same pattern as db-hq/events-hq/entities, chrome-bar drag works.
- Real nav: `[1]` New chat, `[2]` Composer (arm-then-type, matching agent-45's own real Nav/Active state machine).
- Real relay injection: `#.desktop/open_hai_agent_relay.txt`, same bare-decimal-per-line contract as db-hq/events-hq. **This is the actual "you type as human, I inject via relay" mechanism the whole GUI was requested for** — verified end-to-end this session: relay → nav-select → arm → type → submit → real Ollama call → real response rendered, multi-line word-wrap and all.
- Real backend: raw Ollama HTTP (`curl` to `10.0.0.144:11434/api/generate`, async/non-blocking via fork+`waitpid(WNOHANG)`, hand-rolled JSON field extraction — no library). Default model `stable-code:latest`. One transient failure was observed on a cold-start request (empty `response` field) and self-recovered on retry — not reproduced since, own error path displayed it cleanly instead of crashing.
- Real taskbar wiring: cell 14 ("ai") now opens a real 1-row menu ("Open open-hai"), confirmed launching the window via **direct manager-level dispatch test** (`nav.sh mgrcode 5000`, i.e. `ktb_hq_activate(s, 0)` — the actual code path any real click/digit-select ultimately reaches).

**One real loose end, not resolved, don't assume it's fixed:** driving cell 14's single-row menu through the PARSER-level relay (`nav.sh nav 14` then `nav.sh row 1`/`key 1`+`key Return`) did not register in this session's testing — `hq_focus` stayed at 0 and nothing launched. The MANAGER-level direct test (`mgrcode 5000`) proved the actual dispatch logic (the code this session wrote) is correct, so the gap is somewhere in the parser's own digit-jump-into-a-1-item-popup handling — possibly a pre-existing quirk unrelated to this new cell (untested with a fresh single-row menu before now), possibly a test-sequence/timing issue on this session's part. **Next agent: don't assume this is fixed just because the manager-level path works — verify the parser-level path for real before relying on it**, especially since a human clicking the header cell + a single visible row should go through the exact same parser path nav.sh's `row`/`key` commands exercise.

**Deferred, not built this pass (documented, not silently dropped):**
- khtpm CSS engine NOT wired in — v1 uses hand-rolled pixel-math layout only (see `build_open_hai.sh`'s own comment on why).
- agent-45 "legacy hook" backend — `g_backend_mode` enum exists (`BACKEND_AGENT45_LEGACY`) but nothing implements it yet; raw Ollama is the only real path.
- Sidebar is mostly cosmetic for v1: "New chat" row is real (clears transcript), but no persisted chat history across sessions, no per-chat switching, no live tool-call feed pane, no working-directory picker — all real ideas from §7's resolved sidebar-taxonomy answer, none built yet.
- No scrolling in the transcript (renders from the bottom up, oldest messages simply become invisible once they scroll off — no scrollback UI).
- Model switcher (topbar "model pill" from the visual reference) not built — model is hardcoded to `stable-code:latest` in source (`g_model_name`).

---

## 10. Round 2 as-built (2026-08-12) — scrolling + real session history + delete

Direct instruction: "lets make sure we have transcript scrolling so we
can audit history... i want a way i can read previous historic chat
with sidebar option or something (and delete it if i want)."

**Scroll convention — ported, not invented.** User's own correction
after an initial research pass surfaced two DIFFERENT house scroll
conventions (wraith-alpha's nav-badge `scroll_offset`/`VISIBLE_ENTRIES`
pattern vs. tpmos's separate joystick/GL-thumb scrollbar in
`gl_desktop.c`): "it just uses a [] up and [] down nav button to
scroll view up or down its nothing spectacular" — confirmed
wraith-alpha's is the right one, tpmos's GL/joystick scrollbar was
never a fit for this file's plain X11/nav shape. Ported: `g_scroll_offset`
+ a fixed visible-line window + nav-badge-numbered up/down buttons.
Unit is FLAT WRAPPED LINES (`g_flat[]`, rebuilt every redraw from
`g_msgs[]`), not raw messages, since message heights vary — this keeps
the scroll math stable regardless of how long any one reply is.

**Real session persistence** — `&.widgits/open-hai/sessions/<epoch>/
transcript.txt`, one line per message (`U|text` / `A|text`, `\n`/`\`
escaped), plain text on purpose (direct ask: "audit... history" — not
a binary/opaque format). Sidebar lists every saved session
(timestamp + first-user-message snippet), newest first, `*` marks the
currently-open one. Loading a past session is NOT read-only — sending
a new message from a loaded historic chat continues appending to that
SAME file, matching normal "continue this conversation" expectations.

**Real delete** — Backspace on a focused sidebar session row deletes
it from disk (`rm -rf` on that session's dir) and refreshes the list.
If the deleted session was the one currently open, a fresh session is
auto-started (documented fallback, not a silent bug) so there's never
a dangling view of a chat that no longer exists on disk.

**Nav is now a real dynamic array** (`g_nav[]`, rebuilt fresh every
redraw) instead of the old hardcoded 1/2 scheme, because the sidebar's
session list grows/shrinks — same shape db-hq/events-hq already use
for their own tag-tree nav, adapted to this file's flat layout.
**Important for future testing/injection: nav indices SHIFT as
sessions are added/removed** — composer was `[2]` in round 1 with zero
saved sessions, `[6]` once two sessions existed in this round's live
testing. Always re-check the current frame/screenshot for the real
index before assuming a fixed nav number, same lesson this house has
already learned the hard way with the taskbar's own header cells.

**Verified live, real round trip, this session:**
1. Sent a real message via relay injection, confirmed `U|audit test`
   landed on disk within the session's `transcript.txt` immediately.
2. Waited for the real Ollama response, confirmed the full reply
   (including real embedded newlines, correctly escaped/unescaped)
   landed on disk too — genuinely auditable as plain text, not lossy.
3. Scroll-up confirmed moving the visible window (`line 2-29/29` →
   `line 1-28/29` after 3 scroll-up activations, clamped at 0 as
   designed).
4. Delete confirmed twice: deleting the CURRENTLY OPEN session
   triggered the documented new-session fallback (session count
   stayed the same because a replacement was created, not because
   nothing happened — verified by checking the actual dir names
   before/after, not just a count); deleting a NON-current session
   dropped the count cleanly with no replacement, confirming delete
   itself works correctly in both paths.

Window left running (`open-hai` process, house-root argv) for direct
inspection rather than killed after testing.


---

## 11. Testing method fix (2026-08-12) — receipts, not screen capture

**Real bug hit live**: external `xwd`-based screenshot capture (used
throughout this whole session for db-hq/events-hq/taskbar/entities
verification) broke once the REAL user started actively using their
own desktop concurrently with agent testing — a capture returned
another real window's content entirely (bleed-through), because the
target window had been dragged and was partially occluded at the
moment of capture. Direct instruction: "u should use png dump not pil
capture. from now on (or receipt) learn 2 rely on receipts."

**Real fix, ported from db-hq's own proven pattern**: `dump_frame_png()`
reads directly from THIS PROCESS'S OWN offscreen `buf` pixmap (the
compose buffer, same standard `0xRRGGBB` byte-layout fix db-hq already
established — `XGetImage` on a bare Pixmap returns zeroed mask fields,
don't trust them) — this can never race with window stacking/occlusion
because it captures what the app itself drew, not what's visually
composited on screen. Two ways to trigger it:
- **Live**: press `'p'` (only dispatched when the composer isn't armed,
  so it can never collide with typing a real message) — via real
  keyboard or the same `open_hai_agent_relay.txt` relay injection.
- **Headless**: `khtpm_open_hai_render.+x <house_root> --dump-and-exit`
  — renders one frame, dumps, exits immediately, no window interaction
  needed at all.

Writes `/tmp/open-hai-frame.png` **plus a receipt**,
`/tmp/open-hai-frame.png.receipt.txt` — `ok=<0|1> w=<px> h=<px>
t=<epoch> nav=<focus> n_nav=<count> n_sessions=<count> n_msgs=<count>`.
**The receipt is the thing to poll/trust, not just the PNG's
existence** — a partially-written PNG file existing doesn't mean the
write finished; the receipt is written right after, with real state
snapshotted alongside the pixel dump, so a caller can confirm BOTH
"a frame was written" and "the state was what I expected" from one
file, without guessing a sleep duration.

**Verified working this session**: `--dump-and-exit` produced a real,
correct 1000x680 PNG with `n_sessions=3` matching the real on-disk
session count at that moment, receipt fields all correct.

**Standing rule going forward for this file (and worth applying to any
future khtpm/-hq window)**: once a real human may be concurrently using
the same desktop, external screen capture (`xwd`, PIL-based tools) is
NOT reliable for agent-side verification — use the app's own in-process
PNG dump + receipt instead. This isn't unique to open-hai; the same
occlusion risk applies to db-hq/events-hq/entities too, though it
wasn't hit live for those this session.
