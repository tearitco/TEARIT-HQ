# chat-hai — design & build notes (2026-08-14)

Source of truth for the chat-hai product. Read `chat-hai.2026` (the raw
user notes) too — this doc is the structured working version of it.
_Update this doc at every checkpoint; it survives disconnects._

---

## 1. What chat-hai is

chat-hai is a **selectable option under 14.h-ai** (the ai-cell) AND a
**standalone app**. Its GUI is a slender side-bar (Discord/IRC-like) with a
constantly scrolling chat feed. Per user:

- "i want a side 'chat bar scroll' like discord but its just entities chatting"
- "it doesn't even have to be entities" — headless personalities are fine
- "i want to constantly have 4 gemma270 models chatting, recording memories,
  sometimes truncating them, always on mac. they should have relationship and
  i should be able to jump in some times"
- "there should be bigger models running 2 that 'moderate' — since they take
  longer to respond. & they can piece pieces from smols, look over their
  memories & adjust them etc; tune its joints etc"
- "they dont have to do a task just yet, but later they can be guided to do
  task... and we can even jump in and moderator will do what WE say"
- Endgame: proof of concept = get them "jumproping" together; then dev jumps
  in with more ops/challenges; one day user gives real tasks. Jobs /
  personalities, storing memories, marking them as priority depending on
  topics/situations so they can use fsm/rl deterministic choice=ing to recall.

This is the **testground for the qwen ladder + Harnecient harness** (app
decides who speaks/acts next, model only generates text — no model-driven
control flow). All model calls go through the shared `net/qwen.sh` wrapper +
`net/ollama-lan.pdl` registry.

## 2. Architecture (house-standard)

```
&.hq-apps/chat-hai/
  chat-hai.2026            raw user notes (read-only reference)
  chat-hai-design.md       this doc
  button.sh                run|stop|status|ledger|check|help (standalone launcher)
  pieces/
    personas/              ONE .pdl per personality (modular, swappable, sellable)
      <persona>.pdl        PERSONA | name / TIER | router|quick|coder|manager|fim
                           PERSONA | glyph / PERSONA | system-prompt / SKILL | <file>
    skills/                reusable skill .pdl/.txt files (later editors wire these)
  ops/
    chat_hai_render.c      X11 side-bar renderer + feed + composer (modeled on
                           khtpm_ai_cell_render.c — Xft, async curl child,
                           /proc self path, receipt/png audit conventions)
    chat_hai_loop.sh       round-robin scheduler (Harnecient: shell decides
                           who speaks next, calls net/qwen.sh, appends ledger)
  state/
    transcript.ledger      the "master ledger" formula: [ts] <persona>: <msg> | Trigger: chat-hai
    chat_hai.pid
    chat_hai.log
```

### 2a. Persona .pdl (modular personality/skill format)

One file per persona — swap/sell/customize by editing a file, no recompile.
Livedesk entities can later be wired in with these as their
personality/fsm layer (see hooks §5). First pass ships 4 smol personas
(router/quick tier) + 1 moderator persona (manager tier).

```pdl
SECTION      | KEY            | VALUE
PERSONA      | name           | moxie
PERSONA      | glyph          | 🐺
PERSONA      | tier           | router
PERSONA      | tagline        | curious scout, short replies
SKILL        | introduce      | pieces/skills/introduce.txt
```

### 2b. Transcript ledger (master-ledger formula)

Every line one message, append-only, plain text:
`[YYYY-MM-DD HH:MM:SS] <speaker>: <message> | Trigger: chat-hai`
(moderator lines can add a `| moderated-by:` note). Same shape the
hard-vvar-agent brain ledger uses. This is the human-readable receipt AND the
memory seed for the loop (recent N lines are fed as context to the next
speaker).

## 3. First pass (this build session) — MINIMAL CHAT BAR

Scope discipline: build the minimal Discord/IRC-like side bar with **hooks**
for the full POC left in place (comments + doc), so the next agent has max
momentum. Deliverables:

1. `button.sh run|stop|status|ledger|check|help` — standalone launch.
2. `chat_hai_loop.sh` — round-robin: pick next persona, build a short prompt
   (recent transcript + persona system prompt), call `net/qwen.sh ask <tier>`,
   append reply to `state/transcript.ledger`, keep `state/chat_hai.log`.
   Harnecient: the loop (shell) decides order/frequency — model just talks.
3. `chat_hai_render.c` — X11 slender side bar (~360px wide, full height) on
   the right edge of the desktop: scrolling feed of `[glyph] name: message`
   lines (Xft UTF-8), bottom composer for the user to **jump in** (type,
   Enter → sent as `user` → loop feeds it to next persona + shows in feed),
   Backspace/scroll, quit button. Reads the ledger to draw the feed.
4. Four persona .pdl files + one moderator .pdl (present but inert in first
   pass — moderator logic is a hook, not wired, see §5).
5. **ai-cell integration:** `khtpm_ai_cell_render.c` gains a nav entry that
   spawns chat-hai (`setsid nohup .../button.sh run`), so the user can launch
   it from under 14.h-ai — "proof for human sanity" (user: "make sure i can
   launch it from ai-cell sooner cuz thats proof 4 human sanity").
6. `AU14-work.md` progress log updated.

### First-pass verification
- `sh button.sh run` → window appears, feed starts scrolling as personas chat.
- `state/transcript.ledger` grows with real `[ts] persona: msg | Trigger:` lines.
- `sh button.sh status` shows PID; `stop` kills cleanly.
- Jump-in: user types in composer → appears in feed + ledger as `user`.
- ai-cell: launch chat-hai from the ai-cell window's nav.

## 4. Rules / conventions
- **Never hardcode paths.** Find house root via the khtpm_vars marker-walk
  (dir containing BOTH `#.desktop/` and `&.widgits/`). Model calls via
  `net/qwen.sh` (reads `net/ollama-lan.pdl`) — no model names/endpoints in
  chat-hai code.
- **Harnecient Way** everywhere: shell/C app decides, model generates text.
- **Exactly-one-process** guard on run (check `state/chat_hai.pid` + pgrep).
- Async model calls (fork+curl child like ai-cell) so the feed never blocks.
- Ledger lines are the memory; truncation/priority-marking is a later hook.

## 5. Hooks for the full POC (next agent)
- **Moderator pass:** after each smol reply (or every K lines), a manager-tier
  persona reads recent ledger + writes a short moderation note (piecing smols,
  adjusting memories). In first pass the moderator persona exists but the pass
  is NOT scheduled.
- **Memory file per persona:** `state/memory/<persona>.ledger` — first pass
  uses the shared transcript as context; per-persona memory + priority flags
  (`PRI=high`) + fsm/rl-recall is a later milestone.
- **Relationships:** `state/relations.pdl` persona↔persona affinity — first
  pass not wired.
- **Truncation:** cap context to last N ledger lines (a `TRUNC=nnn` in
  persona pdl or loop config) — stub, not enabled.
- **Skills/ops for personas:** when a persona's SKILL rows exist, the loop can
  hand the persona real ops (jump-in as dev mode, challenges) — first pass
  only the system-prompt row is used.
- **Livedesk entity wiring:** give existing pals (self, asa, ava...) these
  persona .pdl files as personality layers.
- **GUI niceties:** per-persona color, timestamps toggle, moderator thread
  column, pause-any-persona.

## 6. Status (update at every checkpoint)
- [x] 2026-08-14: design doc written (this file). Ladder live (all 5 tiers
      reachable @ 10.0.0.144:11434; `net/qwen.sh ask router|manager` verified).
- [x] 2026-08-14: chat-hai skeleton — 5 persona .pdl (moxie/pip router,
      sage/bravo quick, conductor manager w/ moderator hook), button.sh,
      loop + renderer compiled clean.
- [x] 2026-08-14: `chat_hai_loop.sh` round-robin VERIFIED live: bravo/moxie/pip/
      sage conversing on the ledger, context fed from last lines, user jump-in
      appended to ledger and answered by next speaker.
- [x] 2026-08-14: `chat_hai_render.c` side bar VERIFIED: 380x1536 window on
      right edge, feed reads the ledger, composer Enter appends `user:` line.
- [x] 2026-08-14: ai-cell NAV_CHATHAL entry added + binary rebuilt (spawns
      `button.sh run`). ai-cell was not running; new entry appears on next
      launch.
- [ ] end-to-end verify of ai-cell -> chat-hai launch (needs ai-cell running)

## TASKBAR INTEGRATION — REAL STATUS (updated 2026-08-15)

✅ Menu: 1. Open h-ai | 2. Chat-h-ai | 3. Cancel (cell 14/h-ai submenu)
✅ Dispatch fixed: menu command is now `"livedesk:open-chat-hai"` (a real
   dispatch string), matched by a `ktb_hq_activate()` branch mirroring
   `livedesk:open-ai-cell`'s own shape exactly
✅ `button.sh` rewritten to match `&.widgits/ai-cell/button.sh`'s contract:
   `argv[1]` = house_root (a directory), NOT an action keyword — the
   earlier `run`/`stop`/`status` action-based interface was the actual
   root cause of "clicking Chat-h-ai does nothing," not the taskbar side
- Full architecture, the exact bug chain that made this take ~15 rounds
  to fix, and the recipe for next time: see
  `au11-hq/TASKBAR-MENU-ARCHITECTURE.md` — **read this before touching
  cell 14's menu again**, it documents that cell 14's submenu is
  C-hardcoded (NOT read from the PDL, despite matching PDL rows existing
  and looking live) and the exact dispatch-string convention required.
- Verified: `button.sh <house_root>` launches the renderer with a real,
  single, confirmed PID directly. Taskbar-click path reaches the same
  code after the fixes above.

### Real layout spec (direct instruction, 2026-08-15) — copy ai-cell's C geometry, don't invent CSS

chat-hai's window must sit on the **right edge of the screen**, tall
(long vertical window), with:
- **chat feed**: fills the main body, scrolling
- **user input (composer)**: pinned to the **bottom**, using the same
  **cli-io input mechanism as ai-cell** (digit-jump→Enter activates, type,
  Escape disarms — NOT a separate custom widget)
- **sessions**: a sessions/history list, same idea as ai-cell's own
  disk-persisted deletable history sidebar

**REAL BUG FOUND 2026-08-15 causing "input takes up the entire vertical
pane":** `chat-hai.css` uses `flex`/`display: flex`/`position: relative`
properties to try to pin the composer to the bottom and let the feed fill
the rest. **`khtpm_css_parser.c` does not implement flexbox at all** —
`display`/`position`/`flex` are explicitly in its own "unrecognized
properties, ignored" list (see that file's own comment, ~line 87). The
`.css` file's layout is decorative fiction; it does nothing. This class of
`.chtpm`+`.css` app has NO real box-model/flex layout engine yet — real
geometry must be **pixel-computed every frame in C**, exactly like ai-cell
already does. Do not try to fix this by editing the CSS further.

**Fix approach, direct instruction: copy ai-cell's real geometry code
now; do NOT invent flexbox support in `khtpm_css_parser.c` as part of this
task** — that's real, separate infrastructure work, tracked as a TODO
below, not a blocker for chat-hai shipping.

**The exact ai-cell pattern to copy** (`&.widgits/ai-cell/ops/khtpm_ai_cell_render.c`):
```c
#define SIDEBAR_W 240    /* left sessions/history column */
#define TOPBAR_H  44
#define COMPOSER_H 64    /* base composer height; GROWS with wrapped text */
#define CHROME_H  28     /* window's own drawn title/chrome bar */
#define LINE_H    19

/* composer height recomputed every frame from current input text length,
 * capped at COMPOSER_MAX_LINES before it starts auto-scrolling instead of
 * growing further (ai-cell's own g_composer_h / update_composer_height()) */

static int transcript_geom(int *x0, int *y0, int *w, int *h) {
    *x0 = SIDEBAR_W; *y0 = CHROME_H + TOPBAR_H;
    *w = g_win_w - SIDEBAR_W;
    *h = g_win_h - CHROME_H - TOPBAR_H - g_composer_h;   /* feed fills what's left ABOVE the composer */
    return (*h - 8) / LINE_H;
}

static void draw_composer(void) {
    int x0 = SIDEBAR_W, y0 = g_win_h - g_composer_h;     /* pinned to the BOTTOM, feed above it */
    ...
}
```
The key idea: **the composer's height is computed FIRST each frame**
(`update_composer_height()`, from current wrapped input length, capped),
**then the feed's available height is `window_height - chrome - topbar -
composer_height`** — this is why the feed correctly shrinks/grows around a
composer that itself grows with multi-line input, instead of either
element hardcoding a fixed split. chat-hai's renderer must do the exact
same two-step (composer height first, feed geom derived from what's left),
not a CSS-driven split.

For the sessions sidebar, copy ai-cell's own disk-persisted deletable
history list pattern (same file, sidebar draw/nav code) rather than
inventing a new one.

For screen positioning (the one real difference from ai-cell — ai-cell
does not anchor to a screen edge, chat-hai must anchor to the RIGHT edge):
use `XDisplayWidth(dpy, screen)` to get screen width at window-create time,
then set the window's X position to `screen_w - WIN_W` (Y position/height
following the existing house window-standard conventions — see
`!.HOUSE_STDS.md` §B for the rendering pipeline setup ai-cell's own window
creation already follows).

### TODO (separate task, not blocking): real CSS flexbox support in khtpm_css_parser.c
`display`/`position`/`flex`/`flex-direction` are parsed-but-ignored today.
A real box-model/flex layout engine in the shared `khtpm_css_parser.c`
would let `.chtpm`+`.css` apps declare layout declaratively instead of
every app hand-rolling pixel math in C (as ai-cell/chat-hai both now do).
This is real, decent-sized infrastructure work — track it as its own task,
don't fold it into a feature-shipping session.

---

## 2026-08-15, session 2 — the REAL "chat isn't updating" root cause, geometry made PDL-driven, reliable pause, no PNG needed for data checks

Continuation of the same day's work above. This session found the actual
root cause of "chat isn't updating" (not a rendering bug — a data-loading
bug), made all window geometry PDL-driven per direct instruction, replaced
the unreliable SIGSTOP-based pause with a real LAN-call gate, and added a
text frame-history log so future verification never needs a screenshot
for a pure data question.

### THE real bug behind "i dont see chat moving" / "i still dont get results from start/stop"
Two separate real bugs were compounding, and the SECOND one is why the
FIRST session's live-poll fix (stat() the ledger, reload on mtime change)
didn't visibly fix anything:

1. **`load_ledger()` read from the START of the file and stopped at
   `MAX_EVENTS` (128)** — once a session's ledger grew past 128 lines
   (trivial after real testing; `main.ledger` was already at 175), every
   reload kept re-parsing the SAME first 128 (oldest) lines and NEVER
   reached anything appended after that point. The mtime-poll from
   earlier in this session WAS firing correctly, calling `load_ledger()`
   right on schedule — it just kept reloading identical stale content
   every time, which is indistinguishable from "not updating at all"
   from the outside. Real fix: count total lines first, `rewind()`, skip
   to `(total - MAX_EVENTS)`, so this always loads the TAIL.
2. **Start/Stop used `pkill -STOP/-CONT` on `chat_hai_loop.sh`'s own
   process** — unreliable, because SIGSTOP on the PARENT bash script
   does not reliably freeze a `curl` call already running inside a
   command-substitution subshell (`$(bash "$QWEN" ...)`); a "stopped"
   chat kept producing replies mid-flight. Direct instruction: "the
   start stop of chat should be of the logic of the ai lan call itself
   if need be." Real fix: `state/paused.txt`, checked in a wait-loop
   **immediately before the `qwen.sh` call itself** (see
   `chat_hai_loop.sh`'s `speak()` function) — guarantees zero new LAN
   calls while paused, resumes the instant Start is clicked, no OS
   signal timing races. Both the loop's own startup AND the renderer's
   toggle-pause handler reset this to unpaused on a fresh launch (a
   stale "1" from a killed-while-stopped process was the exact
   `[stopped]`/no-chatting confusion hit live this session).

### Window geometry — now fully PDL-driven, not hardcoded
Direct instruction, after 3 rounds of hand-editing C constants and
rebuilding for "on right side of screen" → "still wide and stout" →
"move up 25px" → **"all window dims can be read from .pdl isntead of
hardcoded"**: `chat_hai_config.pdl` (app root) now has
`window_width`/`window_bottom_margin`/`window_right_margin`/
`window_top_offset` (unscaled base pixels, `font_scale` still multiplies
them same as everything else in this renderer). Read once at startup by
`load_window_geometry_config()`. **Real bug found and fixed along the
way**: the FIRST screen-anchor attempt set `window->style.width/height`
directly in `main()`, once — but `layout_pass()`'s own `apply_css(window,
0)` call re-reads `chat-hai.css`'s fixed 900×700 into `window->style`
**every single redraw**, silently reverting the override back to "wide
and stout" on the very next frame. Fixed via `g_forced_win_w`/
`g_forced_win_h`, applied INSIDE `layout_pass()` itself, after its own
`apply_css()` call, every time — not a one-time pre-loop mutation.

### Composer — full width, no Send button
Direct instruction: "id like input to be wider, send should auto send
when enter is pressed... we dont need a send button." Enter-sends and
clear-after-send were already correct (pre-existing, unchanged — see
`handle_key()`'s Enter branch and `send_composer()`'s own reset). Removed
`<button id="send">` from `chat-hai.chtpm`; composer-text now spans the
full row width.

### Verification convention — text frame-history, not PNG, for data questions
Direct instruction: "you should check it with injection and
framehistory.txt (we dont need a png dump to see if frames are
updating from chat)." Added `#.desktop/chat_hai_frame_history.txt`
(same convention as the taskbar's own `khtpm_strip_frame_history.txt`)
— one line per `redraw()`: `session=… n_events=… paused=… focus_nav=…
win_x=… win_y=… win_w=… win_h=… last="…"`. Use this (`tail -f` or repeat
`tail -1` over time) to verify the feed is actually advancing or the
window geometry is what's expected — reserve the `'p'`-key PNG dump
(`dump_frame_png()`, relay code 112) for genuine visual/layout questions
only, per that report's own framing.

---

## HANDOFF NOTE 2026-08-15 (end of day) — "is it showing latest chat", highlight/readability bugs, current confirmed-working state

Written per direct instruction: "write a document about why its still
not working so we can hand off." Short version: **by the end of this
investigation, the feed WAS confirmed live-updating** (see the "REAL
root cause" section above — the ledger tail-read bug was real and is
fixed) — but two separate real VISUAL bugs made the window look broken
even with fresh data behind it, and this note exists so a fresh agent
doesn't have to re-derive that distinction from scratch.

### What was actually reported, in order
1. "is it not showing latest chat in window? isnt that what would show
   in frame history?" — reasonable question, prompted a real
   investigation (see below).
2. "why is last chat highlighted? i cant read it. get rid of that for
   now" — real bug, found and fixed.
3. "also why is the red chatters name and timestamp cutoff?" — reported
   in the same breath as #2; investigation below concludes this was
   very likely the SAME root cause as #2, not a separate wrapping bug.

### Investigation into #1 — is frame-history actually proof the WINDOW shows it?
Yes, with one important caveat now resolved. `append_frame_history()`
logs `last=g_events[g_n_events-1]` — the newest event `load_ledger()`
loaded — and `inject_panel_feed()` (the function that actually builds
the on-screen feed Elems) walks `g_events` from `g_n_events-1` BACKWARD,
so the newest event is always what's nearest the bottom of the rendered
feed. There is no code path where frame-history's `last` field could
show something newer than what's on screen — **if** the process writing
frame-history is the SAME process the human is looking at.

Real thing checked and ruled out this session: multiple/stale renderer
instances, or a display/session mismatch (agent's shell targeting a
different X display than the human's real desktop) — `DISPLAY=:0`
matched, `who` showed one real session, only one
`chat_hai_hq_render.+x` process existed at check time. **The human
confirmed seeing the live window** in the same exchange (reporting the
highlight/cutoff issues, which requires actually seeing rendered
content) — so display/session mismatch was a real hypothesis worth
ruling out, but was NOT the actual explanation here.

### Investigation into #2 + #3 — highlight and "cutoff", same root cause
`load_ledger()`'s callers always run
`if (g_n_events > 0) g_selected_event = g_n_events - 1;` — the newest
message auto-becomes "selected" every time the ledger reloads (which is
constantly, given the live mtime-poll). `draw_elem()` painted a light
blue (`#cce5ff`) background behind any `item->active` Elem. Feed message
TEXT color comes from `chat-hai.css`'s per-speaker classes
(`.data-item.moxie`, `.data-item.bravo`, etc.) — all light/pastel colors
chosen to read against the app's DARK background (`#16181f`/`#1e2130`).
Light pastel text on a light blue highlight is very low contrast —
readable as "hard to read" (direct report #2) or, depending on the
exact color pair and a quick glance, as text that looks like it's
missing/cut off (direct report #3) — **both symptoms plausibly trace to
the same bug**, not two separate ones. Disabled the highlight fill
entirely (`draw_elem()`'s own `item`+`active` branch, see that code's
own comment) per direct instruction ("get rid of that for now") rather
than reworking it into something with real per-speaker contrast — that
rework is real future work if a "currently selected message" UI is
wanted again, not done this session.

**Verified via a fresh PNG dump after the fix**: no highlight visible,
multiple consecutive `bravo`-colored messages each show their full
`HH:MM speaker:` prefix with no truncation. Real, confirmed fix — not
just a theory.

### Confirmed-working state as of this handoff
- Chat loop runs, calls the LAN model, appends to the active session's
  ledger — confirmed via `chat_hai_loop.sh`'s own log (real, current
  timestamps, real replies).
- Renderer loads the ledger TAIL (not head — see the fix earlier in this
  doc) and live-polls it via mtime, no restart/interaction needed to see
  new messages.
- Start/Stop gates the actual LAN call (`state/paused.txt`, checked
  right before `qwen.sh` in `speak()`) — real, not OS-signal-based.
- Window geometry (width/position/margins) is PDL-driven
  (`chat_hai_config.pdl`), screen-right-anchored, narrow, tall.
- Composer is full-width, Enter sends + clears (no Send button).
- Selection-highlight is OFF (was unreadable) — no known-open bug behind
  the earlier "cutoff" report as of this write-up.

### Real remaining gaps (not bugs — never built yet)
- No up/down scroll (arrow keys) or a thumb/scrollbar — feed only shows
  whatever tail fits the window height. Directly requested, not started.
- `+ New Session` label visibly clipped in the thin sessions strip (90px
  unscaled sidebar can't fit the full label) — cosmetic, not filed as a
  priority yet.
- Model quality (generic/repetitive replies, moxie+pip sharing one
  0.5b model, bravo+sage sharing one 1.5b model, manager/conductor tier
  never scheduled) — documented earlier in this file, not addressed.
- Repeated/near-duplicate messages visible in testing (e.g. two
  near-identical "Inception" messages from bravo, ~4 minutes apart, in
  the same PNG used to verify the highlight fix) — not yet investigated;
  worth checking `CONTEXT_LINES`/`recent_context()` in
  `chat_hai_loop.sh` actually gives each persona enough of the real
  recent conversation to avoid re-covering the same ground, or whether
  this is just an inherent small-model failure mode (see the
  model-quality section above).

---

## 2026-08-15, session 3 — typing indicator, session-list black-text bug

### Real feature: "who's typing" indicator
Direct ask: "is it possible to show whos 'thinking' (AKA TYPING) if
waiting for a request?" `chat_hai_loop.sh`'s `speak()` writes the
persona's name to `state/typing.txt` immediately before its own `qwen.sh`
call (the actually-slow part — 20-40s per this session's own logged
timings, vs. the few-second `sleep_between` gap) and clears it right
after, success or failure either way. The renderer polls this file the
same way it polls the ledger (own `stat()`+reread in the main loop) and
composes it into the status row via a new shared `update_status_label()`
(so the toggle-pause handler and the typing poll never clobber each
other's write to the same label) — shows `[running] · sage typing…` when
someone's mid-request, plain `[running]` otherwise. **Verified working
via frame-history alone** (new `typing=` field added to
`append_frame_history()`'s own log line) — watched it toggle cleanly
across bravo/moxie/pip/sage/-, no PNG needed, confirming the text-only
verification convention (see session 2's own note on this) scales to a
second feature.

### Real bug found + fixed: session sidebar text was black-on-black
Direct report: "THE MAIN/NEW+ FONTS ARE BLACK ON DARK BACKGROUND." Root
cause: `chat-hai.css`'s original rule for these rows was
`.sessions .data-item { color: #d6d9e3; ... }` — a descendant combinator
(space between two classes). `khtpm_css_parser.c`'s own
`selector_tier_match()` (see `!.HOUSE_STDS.md` §J for the full writeup of
this parser family's real, confirmed feature gaps) only supports a bare
tag, a single compound class chain (`.foo.bar`, same element), or an id —
descendant selectors silently match nothing, no warning. These rows fell
back to this renderer's `CssStyle` default (black), unreadable against
the dark background — same underlying parser limitation as the earlier
wrapping/flexbox findings, a THIRD real symptom of the same root cause
this session. Fixed by giving session rows their own distinct class
(`session-item`, not shared with feed messages' `data-item`) and a real,
matchable single-selector CSS rule — colored **taskbar green** (`#00ff00`,
matching `#.desktop/livedesk_theme.pdl`'s own `COLOR|fg|#00ff00`) per
direct instruction: "lets fix that to be green like tb green."

---

## 2026-08-15 fix session — real bugs found and fixed, live testing

This was a long real-debugging session, not just the layout port above.
Recording every real bug found (not just the headline layout fix) so a
future agent doesn't rediscover any of these the hard way.

### Bugs found and fixed
1. **Elem pool exhaustion crash** (direct report: "clicking ON the
   message in window crashed window") — `g_n_elems` (the Elem pool
   bump-allocator index) never rewound across frames; `inject_sessions()`/
   `inject_panel_feed()` called `elem_new()` fresh every redraw with no
   NULL-check, so after ~`MAX_ELEMS` (512) cumulative allocations across
   the session's whole redraw history, `elem_new()` returned NULL and the
   next `item->parent = ...` write segfaulted. Not tied to any specific
   click — whichever redraw happened to be the exhausting one crashed.
   Fixed via `g_n_elems_static` (captured once after `parse_chtpm()`) +
   rewinding `g_n_elems` to that baseline at the top of every
   `layout_pass()`. Pre-existing flaw (the original `inject_sidebar_items()`
   had the identical bug), just newly triggered once feed items started
   living in the panel.
2. **composer-text never got a cli-io nav index** (direct report: "open
   chat user input is supposed to have nav [] and number so its relay and
   human index accessible. why did we deviate from these stds?") —
   `assign_nav_indices()`'s panel loop only numbered `tag=="button"`
   elements, a blanket rule copied from the events-hq/db-hq template this
   file started as (see this file's own top-of-file header comment) where
   the panel genuinely only ever held buttons. Never updated when
   composer-text (tag `"text"`) landed. Fixed: explicit allowlist (button
   OR `g_composer_text_elem`), not a blanket tag check.
3. **`send_cli_prompt()` referenced an undeclared `g_cli_prompts[]`** —
   pre-existing, never-called dead code that made the file fail to
   compile; the binary that had been running all session was stale
   (built before this landed, never rebuilt since — the exact "always
   fully rebuild+restart" lesson `TASKBAR-MENU-ARCHITECTURE.md` already
   documents for the taskbar side). Fixed with a minimal declaration;
   real cli-io quick-prompts (digits 1-9) are still unwired.
4. **`button.sh`'s new (taskbar) form never started `chat_hai_loop.sh`**
   — only launched the renderer. A perfectly-rendered window with an
   empty/frozen feed and zero error output, because nothing was actually
   generating messages. Fixed: `button.sh` now starts the loop
   (single-instance-guarded) alongside the renderer.
5. **Renderer never re-polled the ledger on its own** (direct report:
   "i dont see chat moving") — the main loop only ever called
   `load_ledger()`+`redraw()` on relay input, a real X11 event, or after
   the human's OWN composer send — never on a timer. `chat_hai_loop.sh`
   writes new messages completely independently on its own schedule, so
   a user just watching (not typing) never saw the feed advance, directly
   contradicting this doc's own stated intent ("a constantly scrolling
   chat feed"). Fixed: `stat()` the active session's ledger every main-loop
   tick (~150ms, piggybacking the existing `select()` timeout), reload+
   redraw on any mtime change.

### Real feature added: sessions (add/delete/switch)
Direct instruction: "we should beable to add / delete new sessions (that
will start fresh, new memories)". Real, working implementation, not a
stub:
- One `.ledger` file per session under `state/sessions/`, `active.txt`
  names the live one.
- Renderer: `load_sessions_list()` scans the dir every frame;
  `inject_sessions()` builds real sidebar rows + a trailing "+ New
  Session" row; click a row to `switch_session()`; **Backspace while a
  session row is focused deletes it** (matches ai-cell/open-hai's own
  documented "Backspace on a sidebar row deletes it" convention this was
  built to mirror) — refuses to delete the last remaining session.
- `chat_hai_loop.sh` re-reads `active.txt` at the top of every round (via
  `ledger_path()`, not a cached path var), so a session switch made in
  the GUI takes effect in the running chat within one round.
- One-time migration on first launch after this landed: seeds
  `sessions/main.ledger` from the old single `state/transcript.ledger` so
  existing history isn't dropped (done in BOTH the renderer's
  `migrate_legacy_ledger_if_needed()` and the loop script's own startup
  block — either may win the race, both idempotent).

### Real feature added: live-adjustable chat speed
Direct instruction: "make sure chat isn't 2 fast... maybe we can have an
input to modify chat speed" then "can have an input in gui also".
`SLEEP_BETWEEN` (seconds between each persona's turn, default 6 — already
well above "at least 1 sec", never was spam-fast) used to be a hardcoded
shell constant, a real `!.HOUSE_STDS.md` §A.7 violation. Now:
- `chat_hai_config.pdl` (new file, app root) holds `sleep_between`.
- `chat_hai_loop.sh`'s `sleep_between()` re-reads it every round (not
  cached) — hand-editing the file takes effect within one round.
- A real GUI control too: the **"Speed: Ns" button** (own control row below
  the status row, three equal cells: Stop/Start toggle-pause, Speed,
  Sound) cycles fixed presets (2/4/6/12/20s) and writes the `.pdl`
  itself — not just a file for developers to hand-edit. Both Speed and
  Sound share ONE writer (`write_chat_hai_cfg`), so a click never drops
  the other keys.
- Incoming-message tone (2026-08-16): the **"Sound: on/off" button**
  (control-row cell 3) toggles a short notification tone played by
  `chat_hai_loop.sh`'s `ledger_msg()` whenever a non-system (persona)
  message is posted to the active ledger. State lives in
  `chat_hai_config.pdl`'s `sound_on` key, re-read fresh on every posted
  message, so a click goes live within one round — no restart.

### Known, reported, NOT yet fixed — why the conversation reads generic/repetitive
Direct report: "the chats are not really good or meaningful... why do
they all give extremely similar answers?" Root cause, confirmed by
reading the actual persona `.pdl` files + `net/ollama-lan.pdl`'s tier
map:
- `moxie` and `pip` both use `tier=router` → **the exact same model**,
  `qwen2.5-coder:0.5b` — only the system-prompt text differs.
- `bravo` and `sage` both use `tier=quick` → both **the exact same
  model**, `qwen2.5-coder:1.5b`.
- `conductor` (the one persona on a genuinely different, bigger model —
  `tier=manager` → `qwen2.5-coder:7b`) never actually speaks:
  `MODERATOR_EVERY=0` in `chat_hai_loop.sh` makes that whole scheduling
  hook permanently inert (by design, per this doc's own §5 "Hooks for
  the full POC" — it was always meant to be wired up later, this isn't a
  regression).
- All 5 models are from the **qwen2.5-**CODER** family — a coding
  model, not a conversation/chat-tuned model**, at very small (0.5B/1.5B)
  parameter counts. Generic, repetitive, loosely-on-topic replies are
  expected from this combination regardless of how the persona
  system-prompts are written — this isn't a prompt-engineering problem.
- **Not fixed this session** — real options for a next agent: (a) enable
  the moderator hook (`MODERATOR_EVERY>0`) so the 7b tier actually
  participates, (b) give each persona a genuinely distinct model/tier
  instead of doubling up two personas per tier, (c) swap the model
  family entirely for something chat-tuned (gemma/llama-instruct) rather
  than a coder-family model, matching this doc's own original vision
  (§1: "4 gemma270 models chatting") which was never actually wired to
  qwen2.5-coder in the first place — worth checking why that swap
  happened before making a new one.

---

## URGENT SPEC-DRIFT FIXES (next agent)

**CRITICAL: These are architecture standard violations, fix ASAP**

1. **Renderer consolidation — spec-drift (CRITICAL) — LIVE COST CONFIRMED 2026-08-15:**
   The slow-typing bug this session (redraw() unconditionally re-wrapping
   the ENTIRE feed on every keystroke) is a direct, concrete illustration
   of this debt's real cost: `khtpm_ai_cell_render.c` already solved this
   exact problem (its own `draw_transcript()` comment: "also (re)builds
   the flat-line cache used for scroll math" - i.e. cached, not
   recomputed every redraw) but chat-hai's separate copy-pasted renderer
   never inherited that fix, so the same bug class had to be
   rediscovered independently, in a different file, the hard way. Direct
   user question after being shown this: "why were not just using the
   same layout renderer, and merging both features, for both cell and
   chat-hai? is that bad idea or what?" — confirmed: not a bad idea, the
   correct one. Two real, confirmed perf fixes landed this session as
   stopgaps (font-open caching in `measure_text_px()`; moved the
   per-pixel `XGetPixel` image-unpack out of the redraw hot path into
   on-demand `dump_frame_png()` only, matching ai-cell's real shape) -
   but the STRUCTURAL fix (cached/dirty-checked feed layout, not
   unconditional per-keystroke re-wrap) still needs the real
   consolidation below to land properly rather than being re-solved a
   third time in a fifth renderer copy.
   - **Problem:** Each app (chat-hai, db-hq, stats-hq, etc.) has its own renderer copy
   - **Standard:** ONE `khtpm_hq_render.c` (house standard), all apps pass `.chtpm`+`.css` to it
   - **Action:** Delete chat-hai's `chat_hai_hq_render.c`, use shared `khtpm_hq_render.c`
   - **Impact:** Eliminates duplication, unified maintenance, spec compliant
   - **Status:** chat-hai currently has custom copy (wrong); db-hq also has khtpm_hq_render.c (fragmented)

2. **Taskbar menu externalization (CRITICAL REFACTOR FOR NEXT AGENT):**
   - **Current state (WORKING BUT NOT SPEC-COMPLIANT):** Menu items are hardcoded in C source
     - Chat-hai entry added to `livedesk_build_ai_menu()` in `khtpm_taskbar_manager.c` (line ~2067)
     - Each menu is built by a C function (`livedesk_build_hq_menu()`, `livedesk_build_file_menu()`, etc.)
     - This works but violates spec: configuration should be EXTERNAL, not in C code
   - **Standard (future refactor):** Move menu definitions to `.pdl` config files
     - Create `ai_menu.pdl` with: `MENU_ITEM | 1 | Open h-ai | livedesk:open-ai-cell` etc.
     - Manager reads `.pdl` at startup instead of calling hardcoded `livedesk_build_ai_menu()`
     - NO C recompile needed to add/remove menu items
     - Same pattern as `livedesk_shortcuts.pdl` (already external)
   - **Why this matters:** Prevents spec-drift (C code is not the config layer). Enables runtime menu changes.
   - **Status:** Works now (C-based). Refactor deferred to next agent (non-blocking).

3. **Layout refactor — IRC-like structure (BLOCKING):**
   - Current: sidebar (feed) left, panel (input) right
   - Target: sidebar (sessions) left, panel (messages+input) right, input at bottom
   - Requires: renderer/layout changes to inject_sessions() + inject_messages()
   - Can't proceed until spec-drift #1 (renderer consolidation) is done

4. **Session picker implementation:**
   - Stub: sidebar currently shows "sessions loading"
   - Need `load_sessions()` function to populate left sidebar
   - For first pass: just "Main Chat" (one session), later: multi-channel support
