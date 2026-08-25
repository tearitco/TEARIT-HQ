# 🌌 XYZOS — Onboarding Bible  
### For humans · AI agents · curious customers  

> **One house. Many programs. File-truth. GL windows. Widgets that talk.**  
> If it isn’t in a file, it’s a rumor. If it isn’t harness-green, it’s a wish.

**House path (this tree):** `44.xyz…` / muchiverse  
**Living docs:** `#.haiku+/` · `%.harnesses/` · `&.widgits/` · `CHTPM_ARCHITECTURE_GUIDE.txt`  
**Status of this README:** understanding snapshot for onboarding (dev era — expect growth)

---

## 🧭 What is XYZOS?

**XYZOS** is not “another Electron app.” It’s a **local operating *culture*** for building:

- 🕹️ **Games & makers** (mutaclysm-class worlds, maps, entities)  
- 📝 **Tools** (text editor, loaders, harnesses)  
- 👤 **Identity** (login, guest UUID, avatars, wallets)  
- 🧩 **Widgets** (small GL programs that command big ones)  
- 📦 **Future apps** (a *recipe*: project + widgets + toolbar — only after you’ve lived the combo)

Everything important is **files on disk**. Processes are thin. UIs are **renderings of state**, not the source of truth.

```text
   👤 you
    │
    ├─► 🖥️ GL window (PRIMARY UX)
    ├─► ⬛ ASCII terminal (secondary / dev / harness)
    │
    ▼
  pal / chtpm / ops (+x)
    │
    ▼
  pieces/ · xyzfs/ · exchange/ · saves/ · docs/
         ▲
         └── if it’s real, it’s here
```

---

## 🎯 Goals (what we’re optimizing for)

| Priority | Goal | Why |
|----------|------|-----|
| 🥇 | **File-mediated truth** | Replay, audit, AI harnesses, multi-process without sockets soup |
| 🥇 | **GL-primary interaction** | Users click windows, not “open terminal #7” |
| 🥈 | **Composable widgets** | File-menu, tile-picker, map-picker *control* projects |
| 🥈 | **Harness before hype** | Multi-project demos prove integration *before* human QA theater |
| 🥉 | **@apps as recipes** | Only after a combo is painful to relaunch by hand |
| 🥉 | **Identity & xyzfs** | Per-user trees, pretend-prod install lists, later cloud/billing |

**Non-goals (for now):**  
❌ Forcing everyone to write PAL by hand for content  
❌ Dual writers on `current_frame.txt`  
❌ Widgets that require a second human terminal  
❌ Shipping `@.apps` before the combo has been *lived*

---

## 📜 Standards & principles (the house law)

Full text lives in **`#.haiku+/!.xyzos-standards+1.txt`** and **`!.xyzos-pitfalls+1.txt`**.  
Agents: **read those before inventing architecture.**

### 🏛️ Core principles

1. **📁 Files are the API**  
   State, buffers, maps, saves, cmd inboxes, bridges — all paths.  
   Two programs “integrate” by **agreeing on paths + line protocols**.

2. **🪟 GL is primary · ⬛ ASCII is secondary** (§35)  
   Product UX = windows. Terminal = dev, SSH, harness, audit scrollback.  
   **Widgets** launch as `run-widget`: GL **on**, ASCII **off** (no TTY fight).

3. **🧩 One visible frame writer** (§20)  
   Ops write `view.txt` (and friends). **chtpm** owns `current_frame.txt`.  
   Dual writers = flicker = lies.

4. **😴 Idle must not spam frames** (Pitfall 48)  
   `menu_input 0` every tick is fine; **bump/recompose only on real change**.  
   mutaclysm-style: paint when the world moved, not when the clock did.

5. **🧪 Harness placement** (§36)  
   - One project → `<project>/test-harn-same/`  
   - Two projects interacting → **`%.harnesses/<a>+<b>/`**  
   Example: `file-menu+editor`, `file-menu+mutaclysm`  
   Ancestor pattern: `#.drag-drop-test/`

6. **🎮 Same TTY handoff ≠ widget model**  
   START_BUTTON may hand the terminal to a full app.  
   **Widgets run alongside** as separate processes + GL — not “open another shell.”

7. **🧾 Audit logs**  
   `frame_history.txt` (cleared per session / durable via `PRISC_FRAME_HISTORY`).  
   Status files after widget cmds. Proof folders next to harnesses.

8. **🌱 Don’t build the store before the desk**  
   Run project + widgets until *you* want a shortcut.  
   *Then* `@app-store` + install + profile memory.  
   Notes: `#.notes/AFTER-widgets-apps-store.txt`

---

## 🏗️ Architecture (how it actually runs)

### Four processes (classic pal / CHTPM shape)

```text
keyboard_input ──► history / interact_relay
chtpm_parser_pal ──► layout · focus · KEY:n · href · compose chrome
prisc+x + .pal ──► ops (+x) · game/widget logic
renderer / gl_mirror ──► ASCII and/or RGB window
```

**Signal flow (simplified):**

```text
key / GL input
  → files grow
  → pal loop / manager reads
  → ops update state
  → compose → view.txt
  → chtpm → current_frame.txt + pulse
  → ASCII renderer and/or RGB/GL
```

### Packages you’ll see in this house

| Emoji | Area | Examples |
|-------|------|----------|
| 🧟 | Games | `101.mutaclsym…` — maps, entities, GL mirror |
| 📝 | Tools | `102.editor…` — INTERACT canvas, widget cmd bus |
| 👤 | Identity | `0.user-pal…` — login, xyzfs users, avatars |
| 🚀 | Loader | `*.START_BUTTON` — System / Widgets / Apps / App Store |
| 🧩 | Widgets | `&.widgits/file-menu`, tile-picker, map-picker, **proc-monitor** |
| 🧪 | Harnesses | `%.harnesses/file-menu+editor`, `file-menu+mutaclysm` |
| 📦 | Future apps | `@.apps/`, `@.app-store/` (mostly empty — by design) |
| 📚 | Bible | `#.haiku+/`, this README, `muta-zoo.md` |

### Widget command bus (already real)

```text
Host (editor / mutaclysm session)
  pieces/system/widget_bridge.txt   ← paths + kind
  pieces/system/widget_cmds/inbox.txt
  pieces/system/widget_cmds/status.txt

Widget (file-menu ops)
  fm_set_focus  → points at host
  fm_enqueue_*  → writes inbox lines
  host drains   → LOAD / SAVE / NEW or SAVE_GAME / LOAD_GAME …
```

**Editor verbs:** `LOAD:`, `SAVE`, `SAVE_AS:`, `NEW`, `PING`  
**Mutaclysm verbs:** `SEED_DEMO`, `SAVE_GAME_AS:`, `LOAD_GAME:`, `NEW_GAME`, `PING`

---

## ✨ Functionality (today vs near)

### ✅ Proven (harness-green)

| Feature | Proof |
|---------|--------|
| Editor INTERACT canvas (type, arrows, BS, newline) | `102.editor…/test-harn-same` |
| file-menu → editor LOAD / SAVE_AS / NEW | `%.harnesses/file-menu+editor` |
| file-menu → mutaclysm user save slots + **demo-project** seed | `%.harnesses/file-menu+mutaclysm` |
| START_BUTTON categories (System / Widgets / Apps / Store) | `*.START_BUTTON` + harness |
| Login / xyzfs identity trees | `0.user-pal/00.login-signup` |
| Mutaclysm live world + GL culture | install game + docs |

### 🛠️ In roadmap (widgets / maker)

| Feature | Intent |
|---------|--------|
| file-menu **GL UI** | Click menus, not only ops |
| file-menu **mutaclysm profile** | Browse **save dirs**, not random .txt |
| **User FS** for editor docs + mutaclysm saves under xyzfs | Durable, per-user |
| **demo-project** always restorable after NEW wipe | Safety net |
| **tile-picker** | Emoji palette → paint mutaclysm cell |
| **map-picker** | List maps · switch · teleport xlector/hero |
| **proc-monitor** | List / focus / kill house processes (headless or GL) |
| Runtime **process registry** | Shared by CHANGE FOCUS + proc-monitor |
| Zoo pets in/out of simplified zoo | “muta-zoo” orbit |

### 🚀 Later (apps & pretend-prod)

| Feature | Rule |
|---------|------|
| **@app** = project + widget set + optional toolbar | Build only after the combo is loved |
| **@app-store** catalog | List installable recipes |
| **Install** remembers on user profile | Local pdl/txt first |
| Cloud · billing · AI meters · premium · CS chat | User-scoped; **not** blocking maker |

---

## 🧑‍🚀 Future use cases

1. **🧟 Solo RPG maker desk**  
   Mutaclysm + file-menu + map-picker + tile-picker open as one GL “desk.”  
   Save slots named by the player. Demo world always one click away.

2. **📝 Writer + OS feel**  
   Editor + file-menu widget; docs under *their* xyzfs home.

3. **🧪 AI co-dev**  
   Agents inject keys, assert frames/files, leave `proof/` — no pixel-guessing only.

4. **🏫 Classroom / workshop**  
   Everyone has a guest or login tree; install the same “Muta Maker” app recipe; no git clone theater for players.

5. **🐾 Pet + world lab**  
   Zoo simplified + mutaclysm: move creatures without opening C.

6. **🛒 Micro-store (pretend → real)**  
   Install apps, remember them, later attach billing/premium if you must.

---

## 👥 Potential customers (who this is for)

| Who | What they get |
|-----|----------------|
| 🧑‍💻 **Indie makers** | RPG-maker-shaped visibility without Unity tax |
| 🤖 **AI-assisted teams** | File truth + harnesses agents can run |
| 🎮 **Players of local worlds** | Saves, demos, widgets — not “edit JSON by hand” |
| 🏫 **Educators / labs** | Reproducible sessions, guest identities |
| 🛠️ **House maintainers** | One standards bible, fewer invent-a-protocol nights |
| 🏢 **Future “OS product” people** | Local-first OS story: identity, store, windows |

*Not* the first customer: teams that want pure cloud SaaS with no local files. This house is **local-first**.

---

## 💪 Strengths

- ✅ **Debuggable by reading disks** — frame_history, buffers, saves, status  
- ✅ **Multi-process without microservice hell** — files + ops  
- ✅ **AI-testable** — `test-harn-same` / `%.harnesses` culture  
- ✅ **Separation of concerns** — widgets don’t own game loops  
- ✅ **Identity path already started** — xyzfs users, guests, avatars  
- ✅ **GL + ASCII dual render** — product *and* headless  
- ✅ **Honest layering** — apps only after pain of manual launch  

---

## ⚠️ Weaknesses (be honest with customers & agents)

- ⚠️ **Learning curve** — not “npm create and pray”; paths matter  
- ⚠️ **Emoji / Unicode paths** — beautiful, sometimes painful in shells  
- ⚠️ **Docs sprawl** — many `.txt`/`.md`; this README is the map  
- ⚠️ **GL deps** — some machines lack GLUT/display; harnesses must still pass on files  
- ⚠️ **Incomplete surfaces** — widgets ops-first; GL chrome still catching up  
- ⚠️ **Consistency tax** — every new project must copy house laws or rot  

---

## 💬 Opinions (house stance)

> **Prefer a boring file over a clever socket.**  
> **Prefer a green harness over a pretty slide.**  
> **Prefer one writer of the visible frame.**  
> **Prefer GL for humans, ASCII for agents.**  
> **Prefer composing widgets over rewriting mutaclysm.**  
> **Prefer “I ran it” before “I shipped an app.”**

If you’re about to add a global singleton in RAM: stop. Put it in `pieces/` or `xyzfs/`.

If you’re about to open a second terminal for a widget: stop. Read §35 / Pitfall 49.

If you’re about to register `@.apps` because the folder exists: stop. Read `#.notes/AFTER-widgets-apps-store.txt`.

---

## 🗣️ Testimonials (illustrative — onboarding voice, not paid quotes)

> 🧑‍💻 *“I stopped grepping 40 processes and started reading `status.txt`.”*  
> — Hypothetical house dev after first widget cmd bus  

> 🤖 *“I can prove LOAD/SAVE without a human staring at GLUT.”*  
> — Agent after `file-menu+editor` and `file-menu+mutaclysm`  

> 🎮 *“There’s always a demo-project. NEW doesn’t delete the universe.”*  
> — Future player (that’s the point of seed)  

> 🧩 *“File-menu is the same widget; mutaclysm just gets save *dirs*.”*  
> — Maker discovering focus-adaptive widgets  

> 🚀 *“START_BUTTON is my dock. Widgets are my panels. Apps can wait.”*  
> — Power user living the desk before the store  

---

## 🧪 How to verify the house (start here)

```bash
# Editor × file-menu (text buffer bus)
%.harnesses/file-menu+editor/button.sh demo

# Mutaclysm × file-menu (user save slots + demo-project)
%.harnesses/file-menu+mutaclysm/button.sh demo

# Editor canvas alone
102.editor-…/test-harn-same/button.sh demo

# START_BUTTON catalog
*.START_BUTTON/button.sh compile && # then run in a real TTY
```

Read proof under each harness’s `proof/harness-…/`.

---

## 🗺️ Mental model: System / Widgets / Apps / Store

From **START_BUTTON** pre-screen:

| Category | Meaning |
|----------|---------|
| **System** | House programs under `./` with `button.sh` (not widgets/apps trees) |
| **Widgets** | `&.widgits/*` — tools that command focused programs |
| **Apps** | `@.apps/*` — installed **recipes** (project + widgets) |
| **App Store** | `@.app-store/*` — available to install (catalog) |

**Install** (future): write to user profile under xyzfs so the next login still has it.  
That’s “pretend prod” until cloud/billing exist.

---

## 📁 Where to look (agent map)

| Need | Open |
|------|------|
| Architecture patterns | `CHTPM_ARCHITECTURE_GUIDE.txt`, `TPMOS_PATTERN_FINAL.md` |
| Laws | `#.haiku+/!.xyzos-standards+1.txt` |
| Traps | `#.haiku+/!.xyzos-pitfalls+1.txt` |
| UX key injection | `#.haiku+/!.local-ux-testing-ai.txt` |
| Widgets roadmap | `&.widgits/WIDGETS_ROADMAP.txt` |
| Muta + harness narrative | `%.harnesses/muta-zoo.md` |
| Apps later | `#.notes/AFTER-widgets-apps-store.txt` |
| This bible | **`XYZOS_README.md`** (you are here) |

---

## 🏁 Onboarding checklist

### For human devs
- [ ] Skim this README  
- [ ] Run both multi-project harness demos  
- [ ] Open one proof folder and read a status/buffer/map artifact  
- [ ] Read §35–§36 + Pitfall 48–49  
- [ ] Touch one op, re-run the matching harness  

### For AI agents
- [ ] Do **not** invent dual frame writers  
- [ ] Do **not** put cross-project tests only inside one app  
- [ ] Do **not** require second TTY for widgets  
- [ ] Prefer ops + files; leave `proof/`  
- [ ] Update USER_REPORT / muta-zoo when green shifts  

### For customers / sponsors
- [ ] Understand: local-first, file-auditable, maker-shaped  
- [ ] Ask for harness proof, not only screenshots  
- [ ] Apps/store/billing are **layers**, not the foundation  

---

## ❤️ Closing

XYZOS is a bet that **small processes + honest files + GL surfaces + widgets** can feel like an OS for games and tools — without disappearing into a proprietary black box.

Build the **desk** (project + widgets).  
Prove it with **harnesses**.  
Only then bottle it as an **@app**.  

Welcome to the house.  
**Read the files. Run the demos. Don’t invent a second universe.**  

🌌📁🪟🧩🧪🚀

---

*End of XYZOS_README.md — living onboarding bible. Update when the house’s truths change.*
