# 📖 WIDGIT BIBLE — Everything About Widgets in 44.xyz House
> **Spelling**: It's "WIDGIT" not "widget". The codebase, docs, and this bible use **WIDGIT** (with an `i`) because that's how the house spells it. Get it right in every commit, comment, and filename.
>
> **House**: `44.xyz❤️‍🔥️00.10` — the user-level dev environment at yz.muchiverse

---

## 📚 INDEX

| § | Chapter | Emoji |
|---|---------|-------|
| 0 | [📚 INDEX](#-index) | 🏠 |
| 1 | [🏗️ ARCHITECTURE — Two-Program Pattern](#️-architecture--two-program-pattern) | 🏗️ |
| 2 | [👻 FALSE ASSUMPTIONS vs TRUTH](#-false-assumptions-vs-truth) | 👻 |
| 3 | [🪟 GL RENDERING PIPELINE](#-gl-rendering-pipeline) | 🪟 |
| 4 | [🔤 FONT GLYPH REGISTRY](#-font-glyph-registry) | 🔤 |
| 5 | [📋 XYZFS RUNTIME LEDGER](#-xyzfs-runtime-ledger) | 📋 |
| 6 | [🎮 ORCHESTRATION — button.sh Lifecycle](#-orchestration--buttonsh-lifecycle) | 🎮 |
| 7 | [📨 WIDGET CMD BUS](#-widget-cmd-bus) | 📨 |
| 8 | [⚙️ BUILD SYSTEM](#️-build-system) | ⚙️ |
| 9 | [🧪 TESTING & DEBUGGING](#-testing--debugging) | 🧪 |
| 10 | [🚫 HEADLESS / CI MODE](#-headless--ci-mode) | 🚫 |
| 11 | [🧭 NAVIGATING THE CODEBASE](#-navigating-the-codebase) | 🧭 |
| 12 | [🎯 QUICK REFERENCE — Common Commands](#-quick-reference--common-commands) | 🎯 |

---

## 🏗️ ARCHITECTURE — Two-Program Pattern

A widget is **never** a subprocess or thread of the host. Widgets and their hosts are **two completely separate programs**, each with:

```
🧑‍💻 host program                      🪟 widget program
├── own session dir                  ├── own session dir
├── own system/ (binaries)           ├── own system/ (binaries)
├── own ops/ (C tools)               ├── own ops/ (C tools)
├── own PAL loop                     ├── own PAL loop
├── own keyboard_input               ├── own GL window (gl_mirror)
├── own renderer (ASCII)             ├── own chtpm_rgb_render
└── PID: 1234                        └── PID: 5678
```

### 🧩 Communication is FILE-MEDIATED only

No sockets, no pipes, no shared memory, no D-Bus, no IPC. Everything flows through **plain text files**:

| Direction | Mechanism | File(s) |
|-----------|-----------|---------|
| Host → Widget | Key forwarding | `interact_relay.txt` |
| Widget → Host | Command bus | `inbox.txt` → `status.txt` |
| Both | Discovery | `xyzfs/home/runtime/ledger.txt` |

### 🎭 Two Launch Profiles

Every program in the house supports **two launch profiles**:

| Profile | GL | ASCII Renderer | TTY | Use Case |
|---------|-----|---------------|-----|----------|
| `app` (default) | ✅ primary | ✅ secondary | owns TTY | Full interactive app |
| `widget` | ✅ REQUIRED | ❌ off (stdout→/dev/null) | ❌ none | GL-only chrome window |

Profile is set via `RUN_PROFILE=widget` env var or `button.sh run-widget`.

### 🔄 Session Isolation

Each launch creates a **fresh session directory** with a timestamp-PID id:

```
/tmp/.text-editor-xyz-editor-1743292800-12345/   # editor session
&.widgits/file-menu/pieces/sessions/1743292800-56789/  # widget session
```

The session dir contains symlinks to the project's read-only assets (system/, ops/, pal/, pieces/chtpm/) and real files for mutable state (pieces/system/*.txt).

---

## 👻 FALSE ASSUMPTIONS vs TRUTH

This is the **most important chapter**. These are things an AI agent (me) got wrong and had to be corrected on. Read this before touching any widget code.

### 👻 2.1 `<button>text</button>` works

**Assumption**: `<button>NEW FILE</button>` renders with label "NEW FILE" in the chtpm parser.

**Truth**: The chtpm parser reads button labels from the **`label` attribute only**, not from inner text content. Inner text is ignored entirely for `<button>` elements.

✅ **Correct**: `<button label="NEW FILE" />`
❌ **Wrong**: `<button>NEW FILE</button>`

This differs from `<text>` elements where inner text IS the content. Buttons are special.

### 👻 2.2 chtpm_parser_pal uses PRISC_PROJECT_ROOT

**Assumption**: `chtpm_parser_pal` resolves its project root via the `PRISC_PROJECT_ROOT` env var, like every other binary in the system.

**Truth**: `chtpm_parser_pal` uses **`getcwd()`** — it reads its cwd at startup and treats that as the project root. If launched from the wrong directory, it silently does nothing (no error, no `current_frame.txt`).

✅ **Correct**: Always `cd "$SESSION_DIR"` before launching `chtpm_parser_pal`.
❌ **Wrong**: Launching from `$SCRIPT_DIR` or anywhere else.

Every other binary (`chtpm_rgb_render`, `gl_mirror`, ops, `prisc+x`) uses `PRISC_PROJECT_ROOT` and works from any cwd. Only `chtpm_parser_pal` is different.

### 👻 2.3 gl_mirror.c from 045.muchi-pal-agent is canonical

**Assumption**: The most recent `gl_mirror.c` (045.muchi-pal-agent) is the latest/best version.

**Truth**: Two `gl_mirror.c` versions exist, and **014.wsr-pal** is the canonical one:

| Version | interact_relay forwarding | Used by |
|---------|--------------------------|---------|
| `014.wsr-pal💸️📌️+2/system/gl_mirror.c` | ✅ YES — writes keypresses to `interact_relay.txt` | Widgets (correct) |
| `045.muchi-pal-agent/system/gl_mirror.c` | ❌ NO — only writes to `history.txt` and `chtpm_keyboard_history.txt` | Mutaclysm (doesn't need relay) |

Using 045's version = GL keyboard input never reaches the PAL loop = widget appears frozen.

✅ **Always use 014.wsr-pal's version for widgets.**

### 👻 2.4 frame_changed.txt drives the rgb renderer

**Assumption**: The rgb renderer triggers on `frame_changed.txt` changes, written by prisc+x's `hit_frame` opcode.

**Truth**: `frame_changed.txt` stays **0 bytes forever** in widget mode. The `hit_frame` opcode in the PAL loop doesn't fire because the loop's execution path never reaches it. The rgb renderer actually triggers on **`renderer_pulse.txt`** instead, written by `chtpm_parser_pal`'s internal `compose_frame()` function.

Two pulse files exist:
- `frame_changed.txt` — written by prisc+x `hit_frame` opcode → **never written in widget mode** (legacy from PAL-only pipeline)
- `renderer_pulse.txt` — written by chtpm_parser_pal each compose cycle → **the real driver**

✅ **Don't debug frame_changed.txt being empty — it's normal. Check renderer_pulse.txt instead.**

### 👻 2.5 Font glyphs are shared from wsr-pal

**Assumption**: `chtpm_rgb_render` finds font glyphs via a shared path or system-wide registry.

**Truth**: `chtpm_rgb_render` loads glyphs from **`pieces/registry/fonts/ascii/<code>/glyph.txt`** relative to `PRISC_PROJECT_ROOT`. Every project must have its own local copy.

Missing registry = every character renders as invisible (all glyph pixels are zero) = **blank GL window**. No crash, no error, just black.

✅ **Every project/widget that uses chtpm_rgb_render must have `pieces/registry/fonts/ascii/` with 95 glyph files (codes 32-126).**
✅ **Glyphs are project-local. NEVER symlink from another project's registry.**

### 👻 2.6 focus.txt scanning is the discovery pattern

**Assumption**: Scanning widget session dirs for `focus.txt` is the correct way for the editor to find widgets.

**Truth**: The user replaced this with a **xyzfs runtime ledger** — a shared file where every program writes its presence on start and removes it on stop. The editor neither knows nor cares about widget session dirs; it queries the ledger.

✅ **Old**: scan `&.widgits/file-menu/pieces/sessions/*/focus.txt`
✅ **New**: `ledger_peers widget` → get session_root + inbox_path from ledger

### 👻 2.7 Widgets are editor-initiated

**Assumption**: The editor initiates widget discovery (editor scans for widget sessions).

**Truth**: Widgets should be **standalone** — they can run without an editor, display their UI in GL, and discover peers via the ledger. The widget doesn't wait for the editor; both write to the shared ledger and can find each other.

### 👻 2.8 NO_GL=1 is a harmless test flag

**Assumption**: Setting `NO_GL=1` just skips GL rendering for testing.

**Truth**: `NO_GL=1` prevents `gl_mirror` AND `chtpm_rgb_render` from starting. Without `chtpm_rgb_render`, no `rgb_frame.raw` is produced. The GL window opens (from gl_mirror) but receives no texture data → blank/black window.

✅ **Never set NO_GL=1 unless you genuinely want to test the ASCII-only path.**
❌ **Don't set NO_GL=1 and wonder why the GL window is blank.**

### 👻 2.9 Sessions should be in /tmp

**Assumption**: Widget session dirs should be in `/tmp` like editor sessions.

**Truth**: Widget session dirs live in the **project tree** (`&.widgits/file-menu/pieces/sessions/<id>/`), NOT in `/tmp`. This keeps them close to the widget's assets and makes them discoverable via the project directory. Only the editor's session is in `/tmp` (because it's transient and may be large).

### 👻 2.10 `button.sh run-widget` needs an editor session arg

**Assumption**: The widget always requires a focused editor session to start.

**Truth**: The widget should start standalone, display its UI, and use the ledger to find peers. The `FOCUS_SESSION` arg should be **optional** — without it, the widget shows its menu and waits for a host to connect.

### 👻 2.11 Process registry info belongs in /tmp or house root

**Assumption**: A process registry file should live in `/tmp` or the house root directory.

**Truth**: The ledger lives in the **current user's xyzfs tree** (`xyzfs/users/<uuid>/home/runtime/ledger.txt`) — same tree as avatars, wallets, and projects. The user owns their processes like a Linux user owns their processes with `pkill`. xyzfs is durable across session cleanup.

### 👻 2.12 `system/renderer` is needed for widget mode

**Assumption**: The ASCII renderer (`system/renderer`) should run alongside the widget for debugging.

**Truth**: In widget mode, `ascii_renderer=0` — the ASCII renderer is **not started**. It would fight the parent TTY. All output goes to the GL window only. `stdout` is redirected to `/dev/null`.

---

## 🪟 GL RENDERING PIPELINE

The 4-stage pipeline that turns a CHTPM layout into pixels in a GL window:

```
┌──────────────────────────────────────────────────────────┐
│  Stage 1: fm_compose_frame / editor_compose_frame       │
│  ─────────────────────────────────────────────           │
│  reads state → writes view.txt                           │
│  (the semantic content: what the frame SHOULD look like) │
│  Input:  fm_state.txt, piece.pdl                         │
│  Output: view.txt                                        │
│  Binary: ops/+x/fm_compose_frame.+x                      │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Stage 2: chtpm_parser_pal                               │
│  ─────────────────────────                                │
│  parses view.txt + chtpm layout → current_frame.txt      │
│  (the pixel layout: character grid with coordinates)      │
│  ALSO writes: renderer_pulse.txt (triggers stage 3)      │
│  Binary: system/chtpm_parser_pal                          │
│  ⚠️ MUST be launched from session dir (getcwd!)           │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Stage 3: chtpm_rgb_render                               │
│  ──────────────────────────                               │
│  reads current_frame.txt + font glyphs → rgb_frame.raw   │
│  (the raster: RGBA32 pixel buffer, 640×768 = 1,966,080B) │
│  ALSO writes: rgb_frame.receipt.txt (checksum)            │
│  Font source: pieces/registry/fonts/ascii/<code>/glyph.txt│
│  Binary: system/chtpm_rgb_render                          │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Stage 4: gl_mirror                                       │
│  ─────────────────                                         │
│  reads rgb_frame.raw → uploads as GL texture → displays   │
│  ALSO handles keyboard input → writes interact_relay.txt  │
│  Writes: gl_display.receipt.txt (checksum match)          │
│  Requires: $DISPLAY (X11 server)                          │
│  Binary: system/gl_mirror                                 │
└──────────────────────────────────────────────────────────┘
```

### 📐 RGB Frame Spec

| Property | Value |
|----------|-------|
| Resolution | 640 × 768 |
| Format | RGBA32 (4 bytes per pixel) |
| Total size | 640 × 768 × 4 = **1,966,080 bytes** |
| Checksum file | `rgb_frame.receipt.txt` (hex string) |
| GL receipt | `gl_display.receipt.txt` (matches rgb receipt) |

### ⚡ Pulse Signals

The rgb renderer polls two pulse files. **Either one changing** triggers a re-render:

| Signal | Written by | Works in widget mode? |
|--------|-----------|----------------------|
| `frame_changed.txt` | prisc+x `hit_frame` opcode | ❌ Never written (0 bytes) |
| `renderer_pulse.txt` | chtpm_parser_pal `compose_frame()` | ✅ Yes (the real trigger) |

In widget mode, `renderer_pulse.txt` is the only active trigger. Don't worry about `frame_changed.txt` being empty — that's expected.

### 🔑 Keyboard Input Path (GL → PAL Loop)

```
gl_mirror captures keypress
  → append_key() writes to:
      1. pieces/keyboard/history.txt
      2. pieces/apps/player_app/chtpm_keyboard_history.txt
      3. pieces/apps/player_app/interact_relay.txt  ◄── THIS IS THE IMPORTANT ONE
  → PAL loop reads interact_relay.txt
  → dispatches to fm_menu_input op
```

The `interact_relay.txt` forwarding is only present in the **014.wsr-pal** version of `gl_mirror.c`. Without it, keyboard input from the GL window never reaches the PAL loop.

---

## 🔤 FONT GLYPH REGISTRY

### 📁 Location

```
pieces/registry/fonts/ascii/
├── 32/glyph.txt   (space)
├── 33/glyph.txt   (!)
├── 34/glyph.txt   (")
...
├── 65/glyph.txt   (A)
├── 66/glyph.txt   (B)
...
└── 126/glyph.txt  (~)
```

Each directory is the **ASCII code** of the character. Each `glyph.txt` contains a 20×20 pixel bitmap in... (the format is defined by the chtpm_rgb_render code).

### 🚫 What Happens Without It

If `pieces/registry/fonts/ascii/` doesn't exist (or any glyph file is missing):

1. `chtpm_rgb_render` tries to open `<code>/glyph.txt` for each char in `current_frame.txt`
2. Every fopen returns NULL
3. Every character renders as an **all-zero glyph** (invisible)
4. `rgb_frame.raw` is all zeros
5. GL window is black
6. **No error message. No crash. Just blank.**

### 📦 Canonical Source

Glyphs originate from `014.wsr-pal💸️📌️+2/pieces/registry/fonts/ascii/`.

**Each project must have its own local copy.** NEVER symlink from wsr-pal's registry. The build script copies them:

```bash
# build.sh
cp -r "$WSR_DIR/pieces/registry/fonts" "$PROJECT_DIR/pieces/registry/"
```

### 🗺️ 95 ASCII Characters (codes 32-126)

| Range | Chars | Includes |
|-------|-------|----------|
| 32 | 1 | space |
| 33-47 | 15 | ! " # $ % & ' ( ) * + , - . / |
| 48-57 | 10 | 0 1 2 3 4 5 6 7 8 9 |
| 58-64 | 7 | : ; < = > ? @ |
| 65-90 | 26 | A B C D E F G H I J K L M N O P Q R S T U V W X Y Z |
| 91-96 | 6 | [ \ ] ^ _ ` |
| 97-122 | 26 | a b c d e f g h i j k l m n o p q r s t u v w x y z |
| 123-126 | 4 | { \| } ~ |

---

## 📋 XYZFS RUNTIME LEDGER

### 🧠 Concept

A single pipe-delimited file in the **current user's xyzfs** where every program writes its presence on start and removes it on stop. Peers read the file to discover who's running.

Think `~/.bash_logout` for file-mediated IPC — the user owns the processes, the user's xyzfs owns the ledger.

### 🧭 Resolution Chain

```
house_root.txt                          — "where is the house?"
  → <house>/0.user-pal👤️/00.login-signup/current_login.txt
                                        — "who am i?" (current_user_uuid, current_xyzfs)
    → <house>/<current_xyzfs>/home/runtime/ledger.txt
                                        — "what's running?"
```

Three links, no config, no env vars (except PRISC_PROJECT_ROOT for the house root).

### 📄 Schema

Pipe-delimited:

```
timestamp|event|type|project_id|session_root|pid|display_name|inbox_path
```

| Field | Example | Meaning |
|-------|---------|---------|
| `timestamp` | `2026-07-29T07:00:00` | ISO 8601 |
| `event` | `ONLINE` / `OFFLINE` | Lifecycle |
| `type` | `editor` / `widget` / `app` / `daemon` | Category |
| `project_id` | `agy-editor` / `file-menu` | Project |
| `session_root` | `/tmp/.text-editor-xyz-editor-17432...` | Session dir |
| `pid` | `3877162` | Process ID |
| `display_name` | `text-editor-xyz` / `FILE MENU` | Human-readable |
| `inbox_path` | `pieces/system/widget_cmds/inbox.txt` | Command inbox |

### 🛠️ Two Ops

#### `ledger_append <event> <type> <project_id> <session_root> <pid> <display_name> <inbox_path>`
Appends ONE line. Resolves ledger path via the 3-step chain. Same shape as `101.ledger-player-npc-simple+3/ops/ledger_append.c`.

#### `ledger_peers <type>`
Scans ledger for latest `ONLINE` entry of each `project_id` matching `<type>`, checks `/proc/<pid>` for aliveness. Returns active peers. **Replaces `find_widget_session()`** in `editor_menu_input.c`.

### 🔄 Lifecycle

```
Start (button.sh run / run-widget):
  ledger_append ONLINE ${TYPE} ${PROJECT_ID} ${SESSION_ROOT} $$ "${DISPLAY_NAME}" ${INBOX_PATH}

Stop (cleanup trap, EXIT/INT/TERM):
  ledger_append OFFLINE ${TYPE} ${PROJECT_ID} ${SESSION_ROOT} $$ "${DISPLAY_NAME}" ${INBOX_PATH}

Crash (no OFFLINE written):
  ledger_peers checks /proc/<pid> — stale entries are skipped automatically
  No heartbeat. No sweep daemon.
```

### 🏘️ Why xyzfs, Not /tmp

| Location | Problem |
|----------|---------|
| `/tmp` | Cleaned on reboot, session dirs cleaned on exit, no user isolation |
| House root | No user isolation, multiple users can't coexist |
| **xyzfs/users/<uuid>/** | ✅ User-owned, durable, supports multi-user, same tree as avatars/wallets |

---

## 🎮 ORCHESTRATION — button.sh Lifecycle

### 🧩 App Launcher (`@.apps/text-editor-xyz/button.sh`)

```
run_app()
  │
  ├── 1. Create EDITOR session dir in /tmp
  ├── 2. Symlink assets (system/, ops/, pal/, pieces/chtpm/)
  ├── 3. Init editor state files
  ├── 4. Set up widget cmd bus (inbox.txt, status.txt)
  ├── 5. Compose initial frame
  ├── 6. Write house_root.txt (for discovery)
  ├── 7. Start editor daemons: renderer, chtpm_parser_pal
  ├── 8. Start widget cmd drainer (background loop)
  ├── 9. START WIDGET: RUN_PROFILE=widget ./button.sh run-widget
  ├── 10. Set cleanup trap (kill daemons, rm session dir)
  └── 11. Start keyboard_input (foreground, blocking)
```

### 🪟 Widget Launcher (`&.widgits/file-menu/button.sh`)

```
run_widget_session(FOCUS_SESSION)
  │
  ├── 1. Create widget session dir in pieces/sessions/<id>/
  ├── 2. Symlink assets (system/, ops/, pal/, pieces/chtpm/, pieces/registry/)
  ├── 3. Init widget state (fm_state.txt)
  ├── 4. Set focus (if FOCUS_SESSION provided)
  ├── 5. Compose initial frame
  │
  ├── if PROFILE=widget:
  │     ├── Start gl_mirror (GL window + keyboard)
  │     ├── Start chtpm_rgb_render (rasterizer)
  │     ├── Start chtpm_parser_pal ◄── MUST cd to session dir first
  │     └── wait for parser to exit
  │
  └── if PROFILE=app:
        ├── Start renderer (ASCII)
        ├── Start chtpm_parser_pal
        ├── Start keyboard_input (foreground, blocking)
        └── Kill daemons on exit
```

### 🧹 Cleanup Trap Pattern

Both launchers follow the same cleanup pattern:

```bash
cleanup() {
    # 1. Kill daemon PIDs
    kill "$DAEMON1" "$DAEMON2" 2>/dev/null || true
    # 2. Kill own prisc+x module (pid matching cwd)
    for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
        cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
        [ "$cwd" = "$SESSION_DIR" ] && kill -9 "$pid" 2>/dev/null
    done
    # 3. Remove session dir
    rm -rf "$SESSION_DIR" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
```

### 🏷️ Profile Switching

```bash
# In widget button.sh:
PROFILE="${RUN_PROFILE:-app}"

if [ "$PROFILE" = "widget" ]; then
    # GL-only path
    ./system/gl_mirror >/dev/null 2>&1 &
    ./system/chtpm_rgb_render >/dev/null 2>&1 &
    ./system/chtpm_parser_pal pieces/chtpm/layouts/file-menu.chtpm >/dev/null 2>&1 &
else
    # App path (ASCII + keyboard)
    ./system/renderer &
    ./system/chtpm_parser_pal pieces/chtpm/layouts/file-menu.chtpm >/dev/null 2>&1 &
    ./system/keyboard_input
fi
```

---

## 📨 WIDGET CMD BUS

### 🗺️ Architecture

The command bus is how the widget sends commands to its focused host program (e.g., the editor). It's a one-way bus: widget → host.

```
Widget generates command (e.g., SAVE)
  → fm_enqueue_cmd writes to host's inbox.txt
    → host's drainer loop reads inbox.txt
      → host executes command
        → host writes ACK/NACK to widget's status.txt
```

### 📂 Files

| File | Owner | Purpose |
|------|-------|---------|
| `pieces/system/widget_cmds/inbox.txt` | Host (editor) reads | Incoming commands from widget |
| `pieces/system/widget_cmds/status.txt` | Widget reads | Command status (ACK/NACK) |

### 📋 Command Format

| Command | Example | Effect |
|---------|---------|--------|
| `NEW` | `NEW` | Create new empty buffer |
| `SAVE` | `SAVE` | Save current buffer to file_path |
| `SAVE_AS:<path>` | `SAVE_AS:docs/foo.txt` | Save as path |
| `LOAD:<path>` | `LOAD:docs/untitled.txt` | Load file into buffer |
| `PING` | `PING` | Check if alive |

### 📨 Commands Are Enqueued via `fm_enqueue_cmd`

```bash
# From fm_menu_input or widget ops:
./ops/+x/fm_enqueue_cmd.+x "<command>" "<target_session_root>"
```

This writes to `<target_session_root>/pieces/system/widget_cmds/inbox.txt`.

### 🔄 Drainer Loop (Host Side)

The host runs a background loop that polls its inbox:

```bash
while [ -f pieces/system/widget_cmds/inbox.txt ]; do
    ./ops/+x/editor_widget_cmds.+x 8 >/dev/null 2>&1 || true
    sleep 0.2
done
```

---

## ⚙️ BUILD SYSTEM

### 📜 `scripts/build.sh`

The build script:
1. Compiles C ops (gcc)
2. Symlinks system binaries from `014.wsr-pal`
3. Copies font glyphs from `014.wsr-pal/pieces/registry/fonts/`

### 🔗 Symlink Sources

| Binary | Source | Destination |
|--------|--------|-------------|
| `prisc+x` | `014.wsr-pal/system/prisc+x` | `system/prisc+x` |
| `chtpm_parser_pal` | `014.wsr-pal/system/chtpm_parser_pal` | `system/chtpm_parser_pal` |
| `gl_mirror` | `014.wsr-pal/system/gl_mirror` | `system/gl_mirror` |
| `chtpm_rgb_render` | `014.wsr-pal/system/chtpm_rgb_render` | `system/chtpm_rgb_render` |
| `renderer` | `014.wsr-pal/system/renderer` | `system/renderer` |
| `keyboard_input` | `014.wsr-pal/system/keyboard_input` | `system/keyboard_input` |

### 🗂️ Ops Compilation

Ops are compiled from `ops/*.c` to `ops/+x/*.+x`:

```bash
gcc -o ops/+x/fm_compose_frame.+x ops/fm_compose_frame.c
gcc -o ops/+x/fm_menu_input.+x ops/fm_menu_input.c
gcc -o ops/+x/fm_set_focus.+x ops/fm_set_focus.c
gcc -o ops/+x/fm_enqueue_cmd.+x ops/fm_enqueue_cmd.c
```

### 🔤 Glyph Copy

```bash
WSR_DIR="014.wsr-pal💸️📌️+2"
cp -r "$WSR_DIR/pieces/registry/fonts" "$PROJECT_DIR/pieces/registry/"
```

---

## 🧪 TESTING & DEBUGGING

### 🔍 Pipeline Verification

Test each stage of the pipeline independently:

```bash
# Stage 1: Compose
export PRISC_PROJECT_ROOT=/tmp/fm-test
ops/+x/fm_compose_frame.+x && echo "→ view.txt written"

# Stage 2: Parse (MUST cd to session dir!)
cd /tmp/fm-test
system/chtpm_parser_pal pieces/chtpm/layouts/file-menu.chtpm &
sleep 2
cat pieces/display/current_frame.txt
kill %1

# Stage 3: RGB render
system/chtpm_rgb_render &
sleep 1
ls -la pieces/display/rgb_frame.raw
checksum=$(cat pieces/display/rgb_frame.receipt.txt)
[ "$checksum" != "0000000000000000" ] && echo "✅ Non-zero checksum = real pixels"
kill %1

# Stage 4: GL (requires DISPLAY)
system/gl_mirror &
sleep 1
cat pieces/display/gl_display.receipt.txt
```

### 🩺 Process Health Check

```bash
# Check all processes are alive
kill -0 $PARSER_PID && echo "✅ parser" || echo "💀 parser"
kill -0 $RGB_PID && echo "✅ rgb" || echo "💀 rgb"
kill -0 $GL_PID && echo "✅ gl" || echo "💀 gl"
```

### ⚠️ Common Debugging Checklist

1. **Is `chtpm_parser_pal` launched from the session dir?** `getcwd()` check.
2. **Does the session dir have `pieces/registry/fonts/ascii/`?** 95 glyph files, codes 32-126.
3. **Is `gl_mirror` from 014.wsr-pal?** Check for `interact_relay` in the source.
4. **Is `NO_GL` set?** Prevents gl_mirror AND chtpm_rgb_render from starting.
5. **Does `rgb_frame.raw` exist?** Check size (1,966,080 bytes) and checksum (non-zero).
6. **Does `renderer_pulse.txt` have content?** Written by chtpm_parser_pal.
7. **Are all session subdirs created?** `pieces/{display,apps/player_app,keyboard,system}/`.
8. **Is `DISPLAY` set for GL?** Required by gl_mirror.

### 🔬 Checksum Verification

```bash
# rgb_frame.receipt.txt → hex checksum of rgb_frame.raw
# gl_display.receipt.txt → same checksum (prefixed with 0x)
# They should MATCH if gl_mirror loaded the texture successfully

rgb_chk=$(cat pieces/display/rgb_frame.receipt.txt)
gl_chk=$(cat pieces/display/gl_display.receipt.txt)
gl_chk_clean="${gl_chk#0x}"  # strip 0x prefix
[ "$rgb_chk" = "$gl_chk_clean" ] && echo "✅ Checksums match" || echo "💀 Checksums differ"
[ "$rgb_chk" != "0000000000000000" ] && echo "✅ Non-zero = real data" || echo "💀 Zero = blank frame"
```

---

## 🚫 HEADLESS / CI MODE

### 🖥️ No DISPLAY Environment

In headless environments (CI, SSH without X forwarding):

- `gl_mirror` will fail to open a window
- The rest of the pipeline (compose → parse → rgb) works fine
- ASCII renderer works (if started)
- File-based IPC still works

### ✅ What Works Headless

| Component | Works headless? | Notes |
|-----------|----------------|-------|
| `fm_compose_frame` | ✅ Yes | Pure file ops |
| `chtpm_parser_pal` | ✅ Yes | No display needed |
| `chtpm_rgb_render` | ✅ Yes | Writes to file, no display |
| `gl_mirror` | ❌ No | Requires X11 DISPLAY |
| `renderer` (ASCII) | ✅ Yes | Terminal output |
| `keyboard_input` | ✅ Yes | Terminal input |
| `prisc+x` | ✅ Yes | No display needed |

### 🧪 Headless Testing Strategy

```bash
# Test pipeline up to rgb_frame.raw (no GL needed)
export PRISC_PROJECT_ROOT="/tmp/headless-test"
system/chtpm_parser_pal pieces/chtpm/layouts/file-menu.chtpm &
system/chtpm_rgb_render &
sleep 2
cat pieces/display/current_frame.txt
wc -c pieces/display/rgb_frame.raw
```

### 🐳 xvfb for Headless GL

If GL integration testing is needed in CI:

```bash
xvfb-run -s "-screen 0 640x768x24" bash button.sh run-widget
```

---

## 🧭 NAVIGATING THE CODEBASE

### 📁 Project Layout

```
44.xyz❤️‍🔥️00.10/                        # House root
├── &.widgits/                          # 🪟 Widgets live here
│   ├── WIDGIT_BIBLE.md                 # ← You are here
│   ├── file-menu/                      # 🪟 File menu widget
│   │   ├── button.sh                   # 🎮 Launcher
│   │   ├── scripts/build.sh            # ⚙️ Build
│   │   ├── ops/                        # ⚡ C ops
│   │   │   ├── fm_compose_frame.c
│   │   │   ├── fm_menu_input.c
│   │   │   ├── fm_set_focus.c
│   │   │   └── fm_enqueue_cmd.c
│   │   ├── pal/                        # 📜 PAL scripts
│   │   │   └── main_loop_chtpm.pal
│   │   ├── pieces/
│   │   │   ├── chtpm/layouts/          # 🎨 CHTPM UI layouts
│   │   │   │   └── file-menu.chtpm
│   │   │   ├── registry/fonts/ascii/   # 🔤 Glyph files
│   │   │   └── sessions/               # 📂 Runtime session dirs
│   │   ├── fm-widget-fix.md            # 📝 Sprint doc
│   │   ├── house-ledger-arch.md        # 📝 Ledger design
│   │   └── widget+plan.txt             # 📝 Product plan
│   │
│   ├── WIDGETS_ROADMAP.txt             # 🗺️ Widget roadmap
│   └── (future widgets: tile-picker, map-picker, etc.)
│
├── @.apps/                             # 📱 App launchers
│   └── text-editor-xyz/                # 📝 Text editor app
│       ├── button.sh                   # 🎮 App launcher (orchestrates editor + widget)
│       └── sessions/                   # 📂 Autosave backups
│
├── 102.editor-📄️00.00/                 # 📝 Editor project
│   ├── ops/editor_menu_input.c         # ⌨️ Editor input handler (FM dispatch)
│   ├── button.sh                       # 🎮 Editor launcher
│   └── ...
│
├── 014.wsr-pal💸️📌️+2/                 # 🏭 System binary source
│   ├── system/gl_mirror.c              # 🪟 GL window (CANONICAL VERSION)
│   ├── system/chtpm_rgb_render.c       # 🎨 RGB rasterizer
│   ├── system/chtpm_parser_pal.c       # 📜 CHTPM parser
│   └── pieces/registry/fonts/ascii/    # 🔤 Source glyphs
│
├── 101.ledger-player-npc-simple+3/     # 📋 Reference ledger impl
│   └── ops/ledger_append.c             # 📝 Ledger append op
│
├── 0.user-pal👤️/00.login-signup/       # 👤 User identity
│   └── current_login.txt               # current_user_uuid, current_xyzfs
│
├── xyzfs/users/<uuid>/                 # 🏘️ User xyzfs tree
│   └── home/runtime/                   # 📋 Ledger location
│       └── ledger.txt
│
└── #.haiku+/!.xyzos-standards+1.txt      # 📖 Standards
    └── §35.5 — House runtime ledger
```

### 📚 Key Documents

| Document | What It Covers |
|----------|----------------|
| `&.widgits/file-menu/widget+plan.txt` | 🎯 Product plan, phases, KPIs (the WHAT) |
| `&.widgits/WIDGIT_BIBLE.md` | 📖 This document — architecture, pitfalls, HOW TO |
| `&.widgits/file-menu/fm-widget-fix.md` | 📝 Sprint doc — everything that broke and why |
| `&.widgits/file-menu/house-ledger-arch.md` | 📋 Ledger architecture design |
| `#.haiku+/!.xyzos-standards+1.txt` | 📖 House standards (incl. §35 GL, §35.5 ledger) |

---

## 🎯 QUICK REFERENCE — Common Commands

```bash
# 🚀 Launch editor + widget (full app)
./button.sh run                                    # from @.apps/text-editor-xyz/

# 🪟 Launch widget standalone (for testing)
RUN_PROFILE=widget bash button.sh run-widget       # from &.widgits/file-menu/
# or
bash button.sh run-widget <editor_session_dir>     # with explicit focus

# ⚙️ Build widget
bash button.sh compile                             # from &.widgits/file-menu/

# 🧪 Check widget binaries
bash button.sh check                               # from &.widgits/file-menu/

# 🔍 Check dependencies
bash button.sh compile                             # from @.apps/text-editor-xyz/

# 🛑 Kill all widget/editor processes
bash button.sh kill                                # from either button.sh

# 📋 View current frame
cat pieces/display/current_frame.txt

# 📊 Check RGB frame
ls -la pieces/display/rgb_frame.raw
cat pieces/display/rgb_frame.receipt.txt

# 📜 View ledger
cat <house>/<current_xyzfs>/home/runtime/ledger.txt

# 🔬 Check all processes alive
kill -0 $PARSER_PID $RGB_PID $GL_PID 2>/dev/null && echo "✅ all alive" || echo "💀 dead"
```

---

> **Final Words**: Widgets are **standalone GL programs** that communicate via **files**, not sockets. Every program has two launch profiles (app/widget). The **ledger** is how they find each other. The **glyph registry** is what makes text visible. `chtpm_parser_pal` uses **`getcwd()`**. `gl_mirror.c` from **014.wsr-pal** has `interact_relay`. And it's spelled **WIDGIT** with an `i`.
