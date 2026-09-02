# open-hai

Taskbar cell 14 ("ai")'s real X11 window — a chat GUI backed by raw
Ollama over LAN. Built 2026-08-12. Full design history/rationale:
`#.#.calendar-dox/au11-hq/OPEN-HAI-GUI-DESIGN.md` (house root) — read
that for the "why," this file is the "how to work on it."

## What this is

- Real managed X11 window (`_MOTIF_WM_HINTS`, not `override_redirect`
  — real keyboard focus depends on this, don't change it).
- Sidebar with saved chat history (real, on disk, deletable) + a
  transcript pane with real scrolling + a composer.
- Backend: raw HTTP to Ollama (`10.0.0.144:11434`, model
  `stable-code:latest` — hardcoded in `g_model_name`, no UI switcher
  yet). Credentials/SSH access for that LAN host:
  `#.Z.HUMAN_LLM/.MAC-ACCESS.txt` (house root).

## Build / run

```
cd ops && sh build_open_hai.sh
sh ../button.sh <house_root>          # launches it detached, logs to pieces/audit/open-hai.log
```

Also launches from the real taskbar: click header cell "ai" (14th
button) → "Open open-hai" row.

**`button.sh` is single-instance-safe** (real fix, 2026-08-13): it kills
any already-running `khtpm_open_hai_render` process (TERM, then escalates
to KILL if still alive after 1s) BEFORE launching, and confirms exactly
one PID is alive after launch. Do not call the binary directly with
`setsid nohup` yourself for testing — always go through `button.sh`, or
you can end up with multiple concurrent processes racing on the same
relay file and session directory (this happened live, see the testing
section below and `_.0.aigent-testing-k9.txt` "SCOPE ADDENDUM
2026-08-13" for the full incident). The binary also now writes a
pidfile (`pieces/audit/open-hai.pid`) and handles `SIGTERM`/`SIGINT`
with a clean shutdown (unlinks its own pidfile, exits its event loop
within ~150ms) rather than requiring `kill -9`.

## Files

- `ops/khtpm_open_hai_render.c` — the whole app, one file (~950 lines).
- `ops/build_open_hai.sh` — build script. Syncs `stb_image_write.h` from
  `&.widgits/_shared-lib/` at build time (single canonical copy, see
  that dir's own README — don't hand-copy it here).
- `button.sh` — launcher (build-if-missing + detached launch).
- `sessions/<epoch>/transcript.txt` — real chat history, plain text,
  one line per message (`U|text` or `A|text`, `\n`/`\` escaped). Not a
  binary format on purpose — meant to be human-readable for auditing.

## How input works (read this before testing/driving it)

Two ways in, both go through the exact same `handle_key()`:

1. **Real X11 keyboard** — normal window, normal focus.
2. **Relay injection** — append bare decimal ASCII codes, one per
   line, to `<house_root>/#.desktop/open_hai_agent_relay.txt`. Same
   convention as every other khtpm window (db-hq, events-hq, taskbar).
   Codes: 48-57 = digits, 13 = Enter, 27 = Escape, 8 = Backspace,
   32-126 = printable ASCII.

**Nav is a real bracket-badge system** (`[1]`, `[>2]`, `[^3]` etc.),
same convention as db-hq/events-hq: a digit key MOVES FOCUS to that
numbered item, it does NOT activate it — you always need a separate
Enter to actually do something. This is deliberate and consistent
across this whole house, not a bug (confirmed directly with the user
2026-08-12 after an initial "should digit alone load it?" idea was
explicitly rejected: "i do expect to press enter. (as usual)").

**Nav numbers are NOT fixed** — they shift as the sidebar's session
list grows/shrinks. Always check the CURRENT frame (see below) before
assuming which number is the composer, a session row, etc. There is no
static map of "6 = composer" you can hardcode into a test script.
**This got violated live (2026-08-13) by a test loop that itself
pressed "New Chat" (nav item 1) on every pass** — each press grows
`n_sessions`, permanently shifting every later nav index, so the loop
could never land on its own target. Never run a test sequence that
performs an action which changes the nav layout it's trying to
navigate — always re-read the CURRENT nav/session counts (via a `'p'`
live dump, see below) immediately before every jump, not once at the
start.

**STALE WARNING, CORRECTED 2026-08-18** — this section used to say nav
items beyond index 9 were unreachable via relay digit-jump. Direct
re-check of the live code this session (`handle_key()`, ~line 1740)
found this is **already fixed**: real greedy multi-digit accumulation
("digit accumulation, ported from the house standard in chtpm_parser.c
(~line 2621)") — a first digit sets the accumulator if valid, a second
digit combines into a 2-digit jump if THAT'S valid, so "2" then "6"
correctly reaches nav item 26, not item 2. The bug this section
originally described is real bug-report history (see
`_.0.aigent-testing-k9.txt` "SCOPE ADDENDUM 2026-08-13" for that), but
the fix has since landed and this doc just never got updated — a real
doc/code drift example, not a currently-open bug. Don't re-derive a
fix for this from scratch; verify against the live code first if
something still looks broken here.

Backspace on a focused session row (not the composer) deletes that
chat from disk for real. If you delete the currently-open one, a fresh
session auto-starts (not a bug — documented fallback so there's never
a dangling view of a deleted chat).

## How to verify it's actually working (IMPORTANT — read before using xwd/screenshots)

**Do not trust external screen-capture tools (`xwd`, PIL, etc.) once a
real human might be using the same desktop at the same time.** This
bit us live: a window that's been dragged or is partially covered by
another real window can make `xwd` return garbage or another window's
content entirely — it captures whatever's composited on screen at that
instant, not a guaranteed snapshot of this one window's own content.

**Use the app's own PNG dump instead** — it reads directly from this
process's own offscreen compose buffer, so it can never be fooled by
window stacking or occlusion:

- **Live**: press `p` (works via real keyboard or relay injection;
  only active when the composer isn't armed, so it never collides with
  typing).
- **Headless**: `ops/+x/khtpm_open_hai_render.+x <house_root>
  --dump-and-exit` — renders one frame, dumps, exits, no window
  interaction needed.

**These two are NOT interchangeable — do not use `--dump-and-exit` to
check on a live relay-testing session.** `--dump-and-exit` spawns a
BRAND NEW, DISPOSABLE process (fresh window, fresh `g_focus_nav=1`,
fresh model load, zero knowledge of anything relay-injected into a
different already-running instance) — then immediately exits. Real
mistake made live (2026-08-13, cost ~2 hours): relay-injecting digit
codes into a running instance, then "verifying" the result by spawning
`--dump-and-exit` and reading ITS receipt — that receipt describes the
throwaway process's own default startup state, never the live one you
actually injected into. If you're testing a live relay session, send
`'p'` (ASCII 112) THROUGH THE SAME RELAY FILE instead — this triggers
`dump_frame_png()` from inside the process you've been injecting into
the whole time, so the receipt actually reflects your prior codes.
`--dump-and-exit` is only for confirming "does this binary render
without crashing," never for reading live relay-driven state. Full
incident writeup: `_.0.aigent-testing-k9.txt` "SCOPE ADDENDUM
2026-08-13" (house root).

Both write (into the house tree, xyzfs auditable dir - NOT /tmp, direct
user instruction "dont use tpm/ use the xyzfs dirs for auditability"):
`<house_root>/&.widgits/open-hai/pieces/audit/`:
- `open-hai-frame.png` — the actual real frame.
- `open-hai-frame.png.receipt.txt` — **check THIS, not just
  whether the PNG exists.** One line:
  `ok=<0|1> w=<px> h=<px> t=<unix epoch> nav=<focused nav index>
  n_nav=<total nav items> n_sessions=<saved chat count>
  n_msgs=<messages in current view> tool_pending=<0|1> tool=<name>`.
  The receipt is written right after the PNG, with real state
  snapshotted alongside — a partially-written PNG existing doesn't
  mean the write finished; the receipt existing means it did, and its
  fields tell you what state the app was actually in at that moment
  without needing to re-derive it from a pixel image.
Same audit dir also holds: `payload-<pid>.json` (Ollama request),
`response-<pid>.json` (raw curl reply), `toolout-<pid>.txt` (tool
output), `open-hai.log` (runtime log) — every run is auditable by a
human. Emoji voxel CSVs live in
`&.widgits/open-hai/pieces/registry/emoji_assets/<hex-codepoint>/
voxels_16.csv` (generated once via chtpm's emoji_gen_atlas +
emoji_xtract, same offline-first pipeline chtpm_rgb_render.c uses).

This same receipt-over-screenshot approach is documented as the
house-wide standard now in `_.0.aigent-testing-k9.txt` (house root,
"SCOPE ADDENDUM 2026-08-12") — worth using for any other khtpm/-hq
window too, not just this one.

## What's real vs. not yet built

Real: window, nav, relay injection, raw Ollama backend, disk-persisted
session history with delete, transcript scrolling, PNG+receipt
verification, taskbar wiring, real tools (list/read/write/edit/search/
run with approve-deny), drag-resize of the window, real emoji
rendering (chtpm voxel-CSV pipeline), hierarchy typography (bold+underline
bullets / italic subtext), user-blue / ai-purple transcript colors.

Not built yet (see design doc §9/§10 for the full list): khtpm CSS
engine styling (hand-rolled pixel layout for now), agent-45 as an
alternate backend (`g_backend_mode` enum exists, nothing implements
`BACKEND_AGENT45_LEGACY` yet), model switcher UI (hardcoded to
`stable-code:latest`), live tool-call feed pane.

One known real gap, not resolved: driving cell 14's own menu through
the taskbar's PARSER-level relay (`nav.sh nav 14` → `row 1`) didn't
register reliably in testing, even though the MANAGER-level dispatch
(the actual code this app depends on) is proven correct via direct
injection. Don't assume the parser-level path works without checking.
