# #.livedesk-editor-design.md — Livedesk → RPG-Maker-style Session/Desk Editor

House: 44.xyz…00.17  |  Date: 2026-08-09  |  Status: DESIGN — Q1–Q10 RESOLVED, ready to implement

## 0. Goal

Make the livedesk behave like an RPG Maker project editor:

1. **File menu (strip btn 3) creates/saves/loads "sessions".**
   A session = one RPG-Maker-style "project" (its own set of desks, and its
   own per-entity data — events, inventory, HP, positions).
2. **Sessions live in the logged-in user's folder**, not in the house preset.
   The state that opens on start/reset is **not** a preset — it opens because
   the user's storage has a **default open script/session**.
3. **Desks** populate dynamically under the **desks tab (strip btn 4)** with a
   constant `+new-desk` row and `cancel`. A **desk is what shows on the desktop
   screen** (which entities are open + where), and a session/project can have
   **many desks**.
4. Each session's entities can carry **different associated data** (events,
   inventory, hp, etc.) than the same entity in another session.

This doc records: what is correct in the current code, what must change, the
current technical state of the header, the recommended order of work, the
user-testable KPIs, and the questions I will grill you with.

---

## 1. Investigation: who coordinates what today (verified, 2026-08-09)

### 1.1 Identity chain (login → user → per-user storage) — WORKS

Single source of truth: `0.user-pal👤️/00.login-signup/current_login.txt`

```
current_user_id=jb
current_user_uuid=0a9558a7-7c74-4358-833c-2d5b21edc421
current_xyzfs=xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421
```

- `userpal_whoami.+x` (0.user-pal…/00.login-signup/ops/) reads it → prints `jb …`.
- `xyzfs/session.pdl` (same dir) mirrors mode/user_id/uuid/xyzfs_path.
- Apps already resolve per-user storage via the **current_xyzfs** key:
  - `@.apps/TSC_ELO/ops/ledger_append.c` → `<house>/<current_xyzfs>/home/runtime/ledger.txt`
  - `@.apps/TSC_ELO/ops/tsc_elo.c` → `<house>/<current_xyzfs>/home/games/tsc_ratings.txt`
  - `&.widgits/file-menu/house-ledger-arch.md` documents the same resolution chain.
- House-root `xyzfs/users/<uuid>/home/runtime/ledger.txt` already exists for jb.

**Conclusion:** the "user folder of the logged-in user" already exists as a
working convention: `<house>/xyzfs/users/<uuid>/home/`. **This is where
sessions belong.** This is correct and should be reused, not reinvented.

### 1.2 Entity lifecycle (open/register/nav/quit) — WORKS

`&.widgits/tile-picker/ops/tp_desktop_window.c` (LEGACY Linux megafile):

- Opens a package dir, registers in `#.desktop/livedesk_open.txt`
  (`PID|INDEX|ENTITY|PATH`) with flock + dead-PID self-healing.
- `ensure_livedesk_index()` assigns persistent INDEX via
  `#.desktop/livedesk_master_ledger.txt` + `livedesk_next_index.txt`, and
  writes per-entity `livedesk_index.txt`.
- Per-entity `desktop_pos.txt` (x=,y=) — the one deliberate live state write
  on drag. Grid-snapped (GRID_CELL_PX 80), WIN_PX from meta.pdl footprint.
- Per-entity `interact_relay.txt` — remote command inbox (CLOSE, OPEN_CONTEXT,
  RUN_METHOD, ACTIVATE_NAV, FOCUS_NAV). This is the A-Bus into every entity.
- Context menu = `meta.pdl` METHOD rows (Events, Play, Chat, Ledger, Dir,
  Close, Cancel). Re-read every right-click.

**Conclusion:** loading a session = launching `tp_desktop_window.+x <pkg_dir>`
per entity + reading/restoring positions. Closing = writing CLOSE to each
relay (already done by `quit_and_save_session`). This is correct and reusable.

### 1.3 Taskbar / header — WORKS, fully data-driven

`&.widgits/livedesk-taskbar/ops/tp_taskbar.c` (LEGACY — see §2.4):

- Persistent top-left strip (cells[0]=HQ, cells[1]=USER tag, cells[2..]=
  file, desks, player, db, plugins). `sync_strip_claims` gives buttons fixed
  nav 1..N (KIND=btn); tabs/menu rows claim from the same shared
  `livedesk_nav_claims.txt` pool.
- Buttons/menus are PDL-driven from `#.desktop/livedesk_taskbar.pdl`
  (`strip_btn_N_label/_cmd/_menu_M_label/_cmd`) — no recompile for labels/cmds.
- `run_popup_row`: `"quit"` → quit_and_save_session; any other cmd →
  `setsid nohup …` from house_root (chdir'd at startup, §F-18).
- Digit accumulation now defers activation to Enter (au9-accum-fix applied,
  verified at tp_taskbar.c:2300).

**Conclusion:** the header already has the exact UI plumbing we need
(submenus, nav, cmd execution). We add behavior behind the existing
file/desks menus rather than building new chrome.

### 1.4 Startup — WORKS but is a HOUSE PRESET (the thing to change)

- `$.crypts/autostart.pdl` (STATE|enabled, MOUNT, LAUNCH rows) →
  `$.crypts/ops/+x/crypt_autostart.+x` quits current livedesk then launches
  each LAUNCH row (toolbar, asa&ava, hard-vvar-agent, m1, m8, book-stack).
- `$.crypts/restore-list.txt` + `$.crypts/scrypts/openall/run.sh` are the
  "always open" set, used when restore-last-open=1.
- `button.sh run` → crypt_autostart (or openall).

**Conclusion:** this is the "preset" the user says must stop being the reason
things open. It must become: "read logged-in user's **default open script /
default session** from their xyzfs home; if none, offer/create one."

### 1.5 Save/load guidance that exists (the "fm" widget) — WORKS as reference

`&.widgits/file-menu/` (fm widget):

- `focus.txt` contract: `session_root`, `inbox_path`, `state_path`,
  `status_path`, `kind=text_buffer|game_world`, `live_world`, `saves_root`.
- Verbs: NEW / SAVE / SAVE_AS <path> / LOAD <path> via `fm_enqueue_cmd`.
- Pairing: ledger discovery / menu pick / drag-drop; file browser UI exists.
- `house-ledger-arch.md`: runtime ledger in user xyzfs, ONLINE/OFFLINE rows,
  peers discover each other.

**Conclusion:** this is the house's own precedent for save/load UX and for
"widget commands a project's inbox". Our X11 session layer should follow its
vocabulary (NEW/SAVE/SAVE_AS/LOAD, saves_root) but implemented in C/X11
(tp_taskbar/tp_desktop_window), not GL/PAL. RPG-maker-clone
(`201.rpg-maker-clone/ARCHITECTURE.md`) gives the data-model shape:
`project.pdl` + `maps/` + `events/ev_X_Y/state.txt` + `switches.pdl` —
PDL tables we can echo for `session.pdl` + `desk.pdl`.

### 1.6 Entity per-session data — the gap

Today entity mutable state is **one shared copy per package dir**:
- `#.desktop/entities/asa/state.txt` → `name,type,glyph,hunger,hp,created_at`
  (the `hp=100`, `hunger=0` the user mentioned).
- `@.apps/asa-&-ava/pieces/asa/state.txt` + `inventory/` + `event_pkg/`
  (events/pages) + `master_ledger.txt` + `history.txt`.
- `desktop_pos.txt` is written live on drag into the same shared package.

So today there is exactly ONE asa with ONE set of HP/events/inventory across
the whole house. "Entities per session have different data" does not exist yet.
**This is the largest design decision** (§4.5 Q1).

---

## 2. What is correct (keep) vs. what will change

### 2.1 Correct / keep

| Area | Keep |
|---|---|
| Identity | `current_login.txt` as whoami source; `userpal_whoami`; `current_xyzfs` resolution. |
| Storage root | `<house>/xyzfs/users/<uuid>/home/` as per-user home. |
| Entity open/close | tp_desktop_window package model; livedesk_open.txt registry; flock; PID self-healing. |
| Nav | Shared livedesk_nav_claims.txt pool; digit-accum/Enter-only. |
| Header | PDL-driven strip; HQ/user/file/desks/player/db/plugins layout; submenu popups. |
| Quit | `quit_and_save_session` (CLOSE relays + pid unlink). |
| Save/load vocab | fm's NEW/SAVE/SAVE_AS/LOAD + focus.txt/saves_root; rpg-mkr's project.pdl shape. |
| Startup reboot | crypt_autostart's quit-then-launch machinery (reuse for "switch session"). |

### 2.2 Will change

| # | Change | Why |
|---|---|---|
| C1 | **Fix "guest" bug** in tp_taskbar.c:1850 (double-encoded emoji path breaks whoami popen). | Real, verified root cause of "USER: guest" while jb is logged in. |
| C2 | **Sessions**: new `sessions/` layer under user home; File menu items become real ops. | No session concept exists today. |
| C3 | **Default-open**: autostart.pdl/openall defer to user's default session/script. | Today the house preset decides; user says storage must decide. |
| C4 | **Desks**: desk registry per session; desks tab lists desks + `+new-desk` + cancel; switching desk re-renders desktop. | No desk concept exists; "desks" button is a no-op. |
| C5 | **Per-session entity data**: FULL-copy model (Q1) — each session snapshots/restores its own state.txt/inventory/event_pkg/desktop_pos; compress/decompress scripts deferred to shipping. | Entities are single-copy today. |
| C6 | **User button**: wire strip_user_cmd to a real user-switcher (userpal login) OR make the tag show the live user + session. | Button is a no-op today; sessions are per-user. |
| C7 | **Where new design lives**: KHTPM-ARCH.txt says tp_taskbar.c/tp_desktop_window.c are LEGACY; new logic should land in `khtpm_core.c`. Strip port must land first. | Prevents re-adding design to LEGACY files (see §2.4 for the trap). |

### 2.3 The one confirmed bug today (C1), evidence

Source `tp_taskbar.c:1850` builds the whoami path with bytes:
`c3 b0 c5 b8 e2 80 98 c2 a4 c3 af c2 b8 c2 8f` (double-encoded UTF-8 👤️),
but the real dir is `f0 9f 91 a4 ef b8 8f`. Result: `popen` opens a path that
does not exist → no output → `username` stays `"guest"`. Verified identical
bytes in the running binary (`ops/+x/tp_taskbar.+x`). `userpal_whoami.+x`
called directly with the correct root prints `jb`. Fix = correct byte
encoding + rebuild + relaunch.

### 2.4 LEGACY architecture trap (must decide before writing code)

`&.widgits/tile-picker/ops/KHTPM-ARCH.txt`:
- `tp_desktop_window.c` and `tp_taskbar.c` = LEGACY Linux megafiles.
- New design belongs in `khtpm_core.c` + `khtpm_plat_x11.c` (+ taskbar
  core/plat equivalents).
- **But:** `ops/build_khtpm.sh` currently overwrites the running taskbar
  binary with one that has **zero strip-button support** (`grep -n strip
  khtpm_taskbar_core.c` is empty). The README explicitly warns: "Do not run
  that script until the strip feature is ported into the khtpm core."

Two viable strategies for this task (grill question Q5):
- **A. Stay LEGACY for now**: implement sessions/desks in the LEGACY files,
  keep them building/running on Linux, and port to khtpm core in a later
  dedicated pass. Fastest path to user-testable KPIs.
- **B. Port first**: add strip support to khtpm_taskbar_core, then implement
  sessions/desks in core. Correct long-term, but a big up-front cost with no
  user-visible KPI for weeks.

**RESOLVED (Q5, 2026-08-09): A — stay LEGACY.** Implement in
tp_taskbar.c / tp_desktop_window.c, keeping a hard rule: all new pure-logic
(session schema, desk registry, snapshot format) is written as self-contained
functions that later port cleanly into khtpm_core (no X11 calls inside them).
Do NOT run build_khtpm.sh until the strip is ported into the core.

---

## 3. Current technical state of the header (the strip), as-built

Order of cells (livedesk_taskbar.pdl): `HQ | USER:<name> | file | desks |
player | db | plugins`.

- **file** (btn[0]): submenu = `new-desk`, `save`, `save-as`, `load` — all
  four **cmds are empty** today → clicking any row is a no-op
  (run_popup_row ignores empty commands).
- **desks** (btn[1]): no submenu (`n_menu=0`), no cmd → clicking is a no-op.
- **player** (btn[2]): submenu `play`,`pause`,`reset` — all empty cmds.
- **db / plugins** (btn[3..4]): nothing.
- **USER** tag (cell[1]): `cmd = strip_user_cmd` (empty) → click is a no-op;
  label shows `USER: guest` today because of C1.
- Nav numbers: strip buttons = fixed 1..N (KIND=btn); tabs + menu rows share
  the same live pool above N; digit typing + Enter activates (already fixed).
- Popup rows claim KIND=row numbers while open, released on close.

Everything a session editor needs UI-wise already exists here. The work is
**behavior behind existing labels**, not new windows.

### 3.1 pals button — owned default-tile library (MODEL LOCKED 2026-08-10)

**The model (user, 2026-08-10 — Pokémon analogy):** pals is the
**Pokédex**, file/desk positions are the **party**.

- **Pals = the master owned library.** A pal ALWAYS appears in the pals
  list, for ALL sessions, whether or not it is currently placed somewhere.
- **File/desk = placements (party).** A placed pal is still in the pals
  list — like a Pokémon in your party is still in your Pokédex.
- **1 pal = 1 canonical copy = 1 HASH.** There is never a divergent copy.
  Desk/file rows REFERENCE the pal (path + position); the pal is the real
  location, so **moving a pal moves it from its file/desk spot** too.
- **NFT identity (future):** the hash is the pal's identity and will later
  be recorded to the pal-chain blockchain — pals are tradable NFTs.

- Naming correction (user, 2026-08-10): a "palette" is NOT the grand
  inventory — it is the set of **default tiles not yet in inventory**.
- **BUILT 2026-08-10 (pdl-only, no recompile)**: `pals` strip button added.
  User then corrected the order (2026-08-10) — `edit` moves AFTER `palettes`.
  Final strip order (12 cells; nav 1..12, tabs from 13):
  `HQ | USER | file | desks | pals | palettes | edit | player | db |
  plugins | store | network`.
- Button is wired with the reserved `livedesk:pals` cmd (dispatch ignores it
  until the pals popup is implemented — silent no-op today). The pals popup
  behavior is IN SCOPE (approved 2026-08-10): implement after the §4.8
  runtime-ownership change.
- **pals popup (TO BUILD, §4.9 for storage):** lists all owned pals
  (pokedex) — one row per pal (glyph + name + hash), available in every
  session. Selecting one PLACES it onto the current desk (adds a desk row
  referencing the pal + spawns it from the canonical pals copy). Never
  removes it from the list. The strip's **store** cell is where pals/
  entities are acquired (populated from the dev-folders catalog); **pals**
  is what's already owned; **desks** is where they get placed; per-session
  placements then live under `desks/` (§4.9). The existing **palettes**
  cell is separate (the palette app) — `pals` is the owned-tile library, a
  distinct button.
- **Dev folders are the STORE's catalog** (user, 2026-08-10): `*.monads`,
  app/widget packages populate the store for selection; they are never a
  runtime home (see §4.8). Acquiring from the store copies the package into
  the user's pals registry.
- **Economy (future, §4.9):** sprites/tiles are acquired in **palettes**
  (free or purchased); once acquired the pal is **minted**; later minted
  pals are **added to blocks for mining** — a verified ledger (pal-chain),
  making pals tradable NFTs. The pals registry's hash is the on-ledger
  identity.

---

## 4. Session/desk data model (RESOLVED by Q&A, 2026-08-09)

### 4.1 Roles (Q2)

- **Session = RPG Maker "project"**: owns the overarching data — DB data,
  common events, plugins — that applies to all of its desks/maps.
- **Desk = RPG Maker "map"**: its own tiles + events position set (which
  entities are on screen + where). A session/project has many desks.

### 4.2 Location (Q3) — per-user, mirrors the widget `pieces/sessions` spirit

```
<house>/xyzfs/users/<uuid>/home/
├── runtime/ledger.txt                    # existing runtime ledger (keep)
└── livedesk/                             # livedesk's per-user storage root
    └── sessions/
        ├── session.pdl                   # WHICH session is default + last
        │     STATE | active_session  | <id>
        │     STATE | default_script  | <rel path>   # Q4 data pointer
        │     STATE | last_session    | <id>
        ├── <session-id>/                 # one FULL copy per saved session (Q1,Q10)
        │     ├── session.pdl             # STATE | name | <display name>
        │     ├── desks/
        │     │   ├── desk_01.pdl         # DESK rows: entity_id,path,x,y,
        │     │   │                       #   grid_x,grid_y,glyph,index
        │     │   └── desk_02.pdl
        │     └── entities/               # per-session FULL entity data (Q1)
        │         └── asa/                #   state.txt, inventory/, event_pkg/,
        │                                 #   pos — full copy of the package
        └── <user-saved-id>/              # File→save-as → new dirs
```

Note: the earlier `home/sessions/livedesk/sessions/` was invented — verified
(grep) that no such parent-sessions structure exists. The real house
convention is `<app>/pieces/sessions/<id>/` = one **full** copy per session
(e.g. `@.apps/piececraft-xyz/pieces/sessions/<ts>/`). We mirror that spirit
per-user at `home/livedesk/sessions/<id>/`.

### 4.3 Position storage (Q7)

Livedesk stores pixels itself (`x=`,`y=`, grid-snapped like `desktop_pos.txt`
today); other layers draw from "master data" of that sort — not their own
pixel copies. `grid_x`/`grid_y` kept for the editor.

### 4.4 Default open (Q4)

- Short-term: the **current desk** opens by default; you can name/save it
  into a session.
- Long-term: **last project + last desk**. **Last save wins** — saving a new
  session makes that session what opens thereafter, until changed.
- autostart.pdl shrinks to: ensure logged-in user → launch taskbar → run the
  user's default session/desk via the data pointer (no house preset).

### 4.5 Per-session entity data (Q1) — FULL copies now, delta on ship

Each session owns a **full copy** of its entity data (state.txt, inventory/,
event_pkg/, positions) — stored **decompressed**, matching the existing
`pieces/sessions/<id>/` house convention. No overlay/delta layer while
building.

**Entity OWNERSHIP (intent, 2026-08-10):** long-term the entity's permanent
home is the USER's own livedesk storage, not a dev dir. Today entities sit
in dev locations (`*.monads/*.muchi-pet/entities/…`,
`#.desktop/entities/…`) as scaffolding; super-long-term the user
**downloads the entity from the "store"** and it lands **permanently in
their own storage**. Consequences for the snapshot model:
- The per-session copy is a **FULL package copy** — the whole entity dir,
  every file (sprite.csv, meta.pdl, objects.pdl, assets, state.txt,
  inventory/, event_pkg/, desktop_pos.txt, …). No whitelist of
  "state files" and no asset/state split: the user's storage owns the whole
  entity, so per-session snapshots are simply full clones of it.
- When the store flow lands, "download" = place the full package into user
  storage directly; per-session snapshots already handle the rest, so C5's
  copy code is the SAME in both eras.

Shipping-stage optimization (later, not now): a **compress** script creates a
**delta** of a session (space-efficient form); a **decompress** script expands
a delta back to a full session. During development we use full copies only;
delta packaging arrives when the desktop ships.

### 4.5b C5 — AS BUILT + runtime-verified (2026-08-10)

- Helpers in `tp_taskbar.c`: `livedesk_base_name()` (rel-path basename →
  entity-dir key) and `livedesk_copy_full()` (rm + `cp -r`, single-quoted
  paths so literal `*`/emoji/space dirnames stay literal).
- **Snapshot** (`livedesk_snapshot_desk`): after writing the desk pdl, clones
  every live entity's whole package dir → `<session>/entities/<basename>/`.
  Triggered by save / new-desk / new-session / load / switch (outgoing) /
  save-as — one hook covers all flows.
- **Restore** (`livedesk_spawn_desk`): per desk row, replaces the live
  package with an exact clone of `<session>/entities/<basename>/` before
  writing the saved position and launching `tp_desktop_window.+x`.
- **save-as** (`cp -r` of the whole session dir) clones the `entities/`
  tree along with the desks — K6 independence is structural, no extra code.
- **Verified (KPI K5)**: live `m8_redhorned/hp.txt` = 100 → save (session
  copy captured it) → `+new-desk` (empty `desk_01`, all entities closed) →
  session copy edited to hp=42 → switch back to `office` → 6 entities
  respawned, live hp = **42** (session copy authoritative). office.pdl rows
  intact (6).
- **Known follow-up**: snapshot currently runs BEFORE `close_all()`, so an
  entity that writes state only on CLOSE could lose its final write to the
  outgoing snapshot. Harmless today (no entity does that); revisit ordering
  once entities gain `state.txt` write semantics.
- Current state carries the test artifact: s1 `m8_redhorned` hp=42 in both
  session copy and live (consistent — the point of the test).

### 4.6 Save/load/desk semantics (Q6, Q9, Q10)

- **File→save** writes session entity data (Q9); only position writes live on
  drag (keep today's desktop_pos behavior).
- **`+new-desk`** (Q6) = create an **empty** desk in the current session and
  switch to it; `cancel` just closes the popup.
- **Load list** (Q10) = scan `sessions/` dirs; names from each `session.pdl`
  `STATE|name`. No master index file to keep consistent.
- Session switching = quit-then-launch via crypt_autostart-style teardown.
- File menu verbs: `new` (blank template), `save`, `save-as` (clone to new
  id), `load`.

### 4.6b C2b — AS BUILT + runtime-verified (2026-08-10)

Verbose feature-work summary of the whole sessions/desks arc (C5 + C2b +
K11 + harness): `#.livedesk/livedesk-report-2026-08-10.md`.

- **File menu = `new | save | save-as | load`** (design §4.6). pdl row 0 was
  `new-desk`; changed to `new` (`livedesk:new` → new session). `+new-desk`
  stays in the desks popup (§4.6) so desk-creation is still reachable.
  PDL-driven — no recompile.
- **Bug fixed in `run_popup_row()`**: a `livedesk:*` popup-row command that
  opens a REPLACEMENT popup (load → sessions picker, desk-props) was being
  killed the instant it opened — the row handler always returned keep_open=0,
  so the caller's `close_popups()` destroyed the fresh popup. Now it returns
  1 (keep open) only when the popup now showing has a DIFFERENT menu pointer
  than the row just run (`g_strip_popup_menu != menu`); commands that leave
  the source popup alone (new/save/save-as/rename→cli-io) still return 0.
- **Verified (KPI K6 / full RPG-maker cycle)** — driven entirely by nav-index
  input via `/tmp/opencode/nav.sh`:
  - `new`: s1 (office, 6 entities) → s2 `session2`, empty `desks/desk_01.pdl`,
    all entities closed; strip `file:session2` / `desks:desk_01`.
  - `save`: s2 gains `entities/` (empty), desks pdl preserved.
  - `save-as`: s2 → s3 `session3` = independent clone (own desks/ + entities/).
  - `save-as` (from s1): s1 → s4 `session4` = FULL clone — 6 entity packages,
    office.pdl + desk_01.pdl, m8_redhorned hp=42.
  - **K6 independence**: edited s4's `m8_redhorned/hp.txt` → 7; s1's copy
    stayed **42**, s2/s3 stayed empty, each session has its own desks/ set.
  - `load`: sessions picker lists pre-design/session2/session3/session4;
    loading pre-design restores s1 = office + 6 entities relaunched, hp=42.
- Test sessions s2/s3/s4 created by this verification are real saved sessions
  and stay (they demonstrate the File menu working). s4 carries the K6 probe
  (m8 hp=7) proving copy independence.
- Harness hardening: `nav.sh row` now retries popup discovery (the popup can
  lag its claim in xwininfo) and aborts if no popup is actually open, instead
  of typing digits into whatever holds focus.

### 4.7 Desk identity + names (K11, approved 2026-08-10)

- **Desk identity = the `.pdl` filename** under `<session>/desks/`
  (`desk_01.pdl` → name `desk_01`). No `name` field inside the pdl yet.
- **Desk names are file-backed**, so rename = rename the `.pdl` file; if the
  renamed desk is the active one, update that session's `STATE|active_desk`.
  Names are sanitized to safe ASCII `[A-Za-z0-9_-]` (matches the existing
  `desk_NN` convention and keeps shell/tool paths safe). Emoji/space names
  would need a `name` field + spawn/glob changes — deferred.
- **Desk pdl paths are HOUSE-RELATIVE** (portable save files, no machine
  paths): `*.monads/*.muchi-pet/entities/...`. Spawn re-joins with
  `house_root`; the live registry `livedesk_open.txt` stays absolute (runtime
  state owned by the entity windows).
  > Superseded for sessions created after 2026-08-10: desk pdl `path=` points
  > at the CANONICAL xyzfs session-entities path, not dev folders — see §4.8.

### 4.8 Runtime ownership — entities RUN from user xyzfs (2026-08-10)

**The requirement (user, stated 2026-08-10):** what we test must be what
ships. The dev folders (`*.monads/*.muchi-pet/entities/...`,
`#.desktop/entities/...`) are dev-time fixtures — they do NOT exist for a
user on independent cloud storage. The entity's canonical home is its
session FULL copy in user xyzfs, and entities must RUN from that copy so
their writes (hp.txt, inventory/, desktop_pos.txt, …) land in user storage.

**Target runtime model:**
- **Canonical entity home** =
  `xyzfs/users/<uuid>/home/livedesk/pals/<name>/` (the pals registry, §4.9)
  for owned pals. The live window's package dir IS this path; nothing stages
  through a dev folder at run time.
- **Desk pdl `path=`** = house-relative reference to the canonical pal
  (e.g. `xyzfs/users/<uuid>/home/livedesk/pals/<name>`). Re-joined with
  `house_root` on spawn as today. Placements never copy the pal — one pal,
  one canonical copy (§4.9).
- **Spawn** (`livedesk_spawn_desk`): launch from the session copy directly.
  DROP the `access(dev-path)` skip — a missing dev folder must NOT drop the
  entity. Skip only if the SESSION copy itself is missing. `desktop_pos.txt`
  is written into the same (canonical) package dir before launch, as today.
- **Snapshot** (`livedesk_snapshot_desk`): maintains the SINGLE canonical pal
  copy. If the live registry path already IS the pal copy → no copy
  (self-copy guard — `rm -rf` then `cp` of the same dir would delete the
  package). Otherwise copy live → pals (covers dev-authored entities during
  the transition only).
- **save-as** (`cp -r` of the session dir) clones the session LAYOUT
  (`desks/` referencing pals, `session.pdl`) — it must NOT clone the pals
  themselves (1 pal = 1 copy). Sessions are independent in their placements;
  the pal is shared by identity (§4.9). KPI K6 reframes accordingly.

**Consistency with prior design:** §4.5 already declared the user's storage
owns the whole entity, and the store-flow note says download = place the
package into user storage directly. §4.8 makes the RUNTIME honor that: the
canonical pal copy IS the live package. The C5 copy layer stays — it
populates the pals registry (acquisition + snapshot); the dev→xyzfs
direction remains only as the migration/catalog path.

**Migration (existing s1..s4):** move each session's `entities/<name>/` into
the pals registry (dedupe by hash, §4.9), compute hashes, then rewrite each
desk pdl's `path=` to reference the pal. Verify by spawn + live write. Dev
folders become read-only catalog sources (the store), never the runtime.

**Implications:**
- `livedesk_open.txt` PATH= becomes an xyzfs pals path — format unchanged
  (absolute, audit trail).
- `tp_desktop_window.+x <package_dir>` takes the xyzfs pals dir — no change
  to the entity window.

### 4.9 Pal identity + ownership (hash / NFT — MODEL LOCKED 2026-08-10)

**One pal = one canonical copy = one HASH.**

- **Pals registry (pokedex):**
  `xyzfs/users/<uuid>/home/livedesk/pals/<name>/` — the single canonical
  package dir per owned pal. A manifest (`pal.pdl`) records:
  `PAL | name | <name>` + `PAL | hash | <content hash>` + glyph + acquired
  timestamp. `<name>` is the pal's stable, sanitized id (`[A-Za-z0-9_-]`).
- **Hash = identity (NFT-ready):** a content hash (e.g. SHA-256 over the
  package tree) uniquely identifies the pal. Same content → same hash → the
  pal is deterministic and tamper-evident; later the hash is recorded to the
  pal-chain blockchain and the pal becomes tradable (NFT). Until then the
  hash lives in the registry manifest only.
- **Acquisition & minting (economy, user 2026-08-10):**
  - Sprites and tiles are acquired through **palettes** (and the store) —
    some are **free**, others are **purchased**.
  - **Once acquired, the pal is MINTED** — ownership is recorded on the
    ledger. Free and purchased acquisitions both mint (minting is the act
    of becoming a verifiably-owned pal, not just paying).
  - **Later, minted pals are added to blocks for MINING**, placing them on a
    **verified ledger** (pal-chain) — the tradable, provable NFT layer.
  - Lifecycle: acquire (palettes/store, free or paid) → **minted** →
    **added to block / mined** → verified, tradable pal.
- **Placements reference, never copy:** a desk pdl row's `path=` is the
  house-relative pals path (position + glyph live in the row). Placing a pal
  does not duplicate it; there is exactly one copy, so edits are visible
  everywhere it's placed and moving a pal moves it from its file/desk spot.
- **Pals list (pokedex) semantics:** available in ALL sessions, every owned
  pal always listed even while placed. Selecting a pal = place onto the
  current desk (append row to the active desk pdl referencing the pal +
  spawn). It is never removed from the list by placing.
- **Acquisition:** store (future, populated from the dev-folders catalog)
  or initial dev-authored default — copies the package into the pals
  registry and computes the hash. Per-session `entities/` copies from C5
  become the migration path into the registry, then placements take over.
- **KPI K6 reframed (2026-08-10):** independence = sessions own independent
  PLACEMENTS (layouts); the pal itself is ONE shared copy. Editing the pal
  affects all placements — that is the point (Pokédex/party, NFT identity).

---

## 5. Order of work (recommended sequence + dependencies)

| Step | Task | Depends | KPI gate |
|---|---|---|---|
| 1 | **C1 fix "guest"** (encode + rebuild taskbar) | — | Taskbar shows `USER: jb` |
| 2 | C6a minimal: user button reflects live user + opens a 1-row session picker | 1 | Click USER → see "jb / guest" correct |
| 3 | C3: default-open reads user's xyzfs `session.pdl`; migrate autostart.pdl → bootstrap | 1 | Reset opens what user's storage says, not preset |
| 4 | C2a: session scaffold — `home/livedesk/sessions/` dirs, save/load-as of a **desk snapshot** (positions only) | 3 | File→save/load round-trips positions |
| 5 | C4: desk registry + `desks` tab popup (`+new-desk`, cancel, list) + desk switching | 4 | Make 2 desks, switch, positions change |
| 6 | C5: per-session entity data — **FULL copies** model (Q1) + snapshot/restore state.txt/inventory/event_pkg | 4 | Change hp in session A, session B still old |
| 7 | C5b (shipping-stage, deferred): **compress**→delta / **decompress**→full scripts | 6 | Delta round-trips back to identical full project |
| 8 | C2b: File→new / save / save-as / load full sessions UI | 5 | Full RPG-maker cycle |
| 9 | C6b: user-switcher (userpal login) + per-user default sessions | 8 | Second user has their own sessions |
| 10 | (post-Q5=A) port pure logic to khtpm_core + strip support, if time | — | build_khtpm.sh no longer strips buttons |

### 5.1 Why C5 + C2b must land BEFORE the khtpm port

> **IMPORTANT — port end-state (intended, 2026-08-10):** the khtpm port is
> NOT merely a C core/plat refactor (that is only what `KHTPM-ARCH.txt`
> currently describes). The intended end-state is to make khtpm **close to
> chtpm**: layouts as `.chtpm` HTML-like markup (`<panel>`, `<text>`,
> `<cli_io>`, `<button href=…>`) and **logic as `<module>`-tagged `.pal`
> instruction files** ("htpm js" analog) that `launch_module()` spawns —
> i.e. behavior editable as text/markup, not C. Real examples:
> `041.pal-chain⛓️/pieces/chtpm/layouts/login.chtpm` has
> `<module>system/prisc+x pal/login_module.pal</module>`, and
> `041.pal-chain⛓️/pal/login_module.pal` is the PAL instruction logic
> (`li x1,0` / `chain_menu_input x9` / `compose_frame` / `beq` / `j loop`).
> Parser support: `launch_module()` in `chtpm_parser.c` (e.g. line 1358).
> **Decision pending** (recorded so nobody assumes "port = pure C refactor"
> when we get there): scope the port as (a) core/plat C split now, or
> (b) migrate khtpm's strip/desks UI onto the chtpm layout + `.pal` module
> model. This does NOT change the C5→C2b→port ordering below — it makes it
> *stronger*: settle the feature logic while it's cheap, then migrate it
> into the markup/`.pal` model once, instead of porting to a C core and
> re-porting into markup later.

The port (step 10) is a **refactor** of settled logic — it ships no
user-visible KPI of its own (see the §2.4 strategy-B warning), and its
end-state is the chtpm-aligned markup/`.pal` model noted above. C5 and C2b
are the last two feature steps that DO have KPIs (K5 per-session data, K6
save-as independence), so they close out the user-testable list while the
code is still in the LEGACY file where fixes are cheap. Four concrete
reasons for the ordering:

1. **Port once, not twice.** C5 changes the storage/snapshot schema
   (per-session FULL copies of state.txt/inventory/event_pkg/desktop_pos)
   and C2b rewrites the File-menu cycle to use it. If the taskbar is ported
   to khtpm_core before those land, the ported core's data layer is wrong
   the day it ships and the whole feature set must be re-ported — twice the
   refactor, zero user value in between.
2. **The port is FOR this logic.** What actually carries across to
   khtpm_core is exactly the snapshot/restore format (C5) and the
   save/load/new session cycle (C2b). Porting before they exist ships an
   empty shell, and the storage schema would be guessed now and corrected
   later anyway — same rework as reason 1.
3. **KPI order stays intact.** The gates in §5 run 1→9: each step is only
   demonstrable once its dependency's KPI holds. K6 (C2b's gate) literally
   reads "save-as → its own desk set AND its own entity data", which only
   passes after C5's FULL-copy model exists. Doing C2b first would make the
   save-as UI look finished while silently sharing entity state between
   sessions — exactly the bug C5 exists to kill.
4. **The strip gate is independent.** build_khtpm.sh is blocked on the
   strip port no matter what, so landing C5/C2b in the LEGACY file adds
   nothing to that gate's cost. They are pure-logic additions to the same
   file (Q5 rule: self-contained, no X11 inside), so they port just as
   cleanly as the K9/K10/K11 logic already in place.

**Resolution (2026-08-10): C5 → C2b → report → khtpm port.** The report
summarizes the feature work; the port is the final refactor pass over a
complete, verified feature set, not a step that features are built on.

---

## 6. User-testable KPIs (each step must be demonstrable by the user)

1. **K1 — Identity**: Top strip shows `USER: jb` (not guest) after C1, with no
   manual env hacks. (FAIL today — verified.)
2. **K2 — Default open**: `button.sh run` after `X.quit` reopens exactly the
   session named in the user's `sessions/session.pdl` — edit that file to
   point at a different desk, reset, and the desktop opens differently.
3. **K3 — File save/load (desk level)**: drag asa to a new grid cell, File→
   save; X.quit; File→load; asa is back at the saved cell. (No data yet, just
   position.)
4. **K4 — Desks tab**: under `desks` there are at least: each desk in the
   session, `+new-desk`, `cancel`. `+new-desk` creates desk_02 and switches
   to an empty desk. Switching back to desk_01 restores the old entity set.
5. **K5 — Per-session data**: open asa in session A, set `hp=42` via a
   method/editor; save; load session B; asa's hp is still the session-B value
   (e.g. 100), not 42.
6. **K6 — Multi-session**: `save-as` "my-jb-save"; load it; it appears as its
   own desk set and its own entity data; the original session is untouched.
7. **K7 — Multi-user**: log in as a second user (userpal), start desktop —
   their `sessions/` is empty/new; jb's sessions are untouched.
8. **K8 — No preset**: deleting `$.crypts/autostart.pdl` LAUNCH rows (or
   setting STATE|enabled|0) does NOT stop the default session from opening.

### User's first KPIs (flows, given 2026-08-09 — the first things to make work)

9. **K9 — Session flow (sanity round-trip)**: File→new project → load the old
   default project (named e.g. "pre-design") → File→new project again → open
   an app or monad → File→save as session2. Load session2 back and it shows
   that app where it was saved.
10. **K10 — Desk flow**: new (blank/clear) → load session2 → create a
   "new desk" → switch between desks → save → clear → load. Desks switch
   cleanly and save/load round-trips.

Order note: K9 exercises steps 4+8 (scaffold + File menu), K10 exercises
step 5 (desk registry) on top of K9. They are the user's acceptance flows,
so implementation order below follows them (C1 → scaffold → File menu →
desks → entity data).

---

## 7. Grill questions (RESOLVED — answers locked in Q&A, 2026-08-09)

**Q1 — Per-session entity data model.** Copy, delta/overlay, or pointer?
**RESOLVED: FULL copies, decompressed.** Each session owns a complete copy of
its entity data (state.txt, inventory/, event_pkg/, positions) — matches the
existing `pieces/sessions/<id>/` convention. Full copies during building;
**compress**→delta / **decompress**→full scripts arrive at shipping stage.

**Q2 — Is a "session" the RPG Maker "project", and a "desk" a saved camera/
view of that project?**
**RESOLVED: yes, and deeper — session = project (owns overarching DB data,
common events, plugins); desk = RPG Maker map (its own tiles + events
position set).** Sessions have many desks.

**Q3 — Where exactly is the user's home for sessions?**
**RESOLVED:** `<house>/xyzfs/users/<uuid>/home/livedesk/sessions/<id>/` —
per-user mirror of the widget `pieces/sessions` spirit. (The earlier
`home/sessions/livedesk/sessions/` was invented; grep-verified that no such
parent-sessions structure exists.)

**Q4 — What is "default open script in user storage"?**
**RESOLVED: short-term = current desk opens by default; long-term = last
project + last desk; saving a new session makes it the default (last save
wins) until changed.** Data pointer over code; `open.sh` allowed but
optional.

**Q5 — LEGACY vs khtpm-core (strategy A or B)?**
**RESOLVED: A — stay LEGACY.** Implement in tp_taskbar.c/tp_desktop_window.c
as self-contained pure functions; port to khtpm core in a later pass if time.
Do NOT run build_khtpm.sh until the strip is in core.

**Q6 — "new-desk" semantics.**
**RESOLVED: (a) empty new desk in the current session, switch to it; `cancel`
just closes the popup.**

**Q7 — Do desks/sessions persist positions in grid coords or pixels?**
**RESOLVED: livedesk stores pixels itself (x=,y=, grid-snapped like
desktop_pos today); other layers draw from "master data" — not their own
pixel copies.** grid_x/grid_y kept for the editor.

**Q8 — Multi-user scope now or later?**
**RESOLVED: per-user paths are mandatory now (K7); the in-desktop
user-switcher UI is deferred (C6b).**

**Q9 — What triggers a session's entity data write-back?**
**RESOLVED: File→save for session data; only position writes live (keep
today's desktop_pos behavior).**

**Q10 — Where does the "sessions live" list come from for the File→load
popup?**
**RESOLVED: scan `sessions/` dirs; names from each `session.pdl`
`STATE|name`. No master index file.**

---

## 8. Open risks

- **Encoding**: emoji paths are a real failure class (C1). New code must
  never hand-write emoji into source; read names from disk where possible.
- **Restart cost**: every session/desk switch is quit+relaunch of entities
  via relay CLOSE then spawn. If that feels heavy, a lighter "hide/unhide +
  reposition" path exists for desk switches (only spawn truly-new entities).
- **fm/GL vs X11**: fm widget vocab is guidance, not the implementation; do
  not pull the PAL/chtpm stack into the taskbar. C/X11 only.
- **build_khtpm.sh trap**: never run it during this work (it clobbers the
  strip taskbar). See README warning + §2.4.
- **history.txt binary-read pitfall**: entity history files are `data` (may
  contain binary), not line text — snapshot code must treat them opaquely.
- **Compress/decompress (Q1 shipping-stage)**: canonical format is full
  copies during building; compress→delta / decompress→full is deferred until
  the desktop ships. Round-trip equality is the KPI when it lands.
- **Default-open pointer**: K2/K8 depend on `session.pdl` fields
  (`default_script`/`active_session`) being present; until then desktop opens
  the last-saved desk (Q4 "last save wins").

## 9. User-storage schema cleanup (found during avatar-bug investigation)

Investigation of the lost-avatar bug (jb created a brown-skin avatar that
never saved) exposed that the per-user storage schema drifted. Two trees claim
to be "the user home" and the avatar module carries stale guest-mode identity
files. Cleanup scope; the livedesk sessions work must build on the CLEANED
schema.

### 9.1 Drift evidence

- **Two xyzfs trees, same concept.**
  - Tree A (house-root): `<house>/xyzfs/users/<uuid>/home/` — has `runtime/`.
    Used by `ledger_append`, `tsc_elo`, fm. Resolves `current_xyzfs` relative
    to `<house>`.
  - Tree B (login-signup): `<house>/0.user-pal👤️/00.login-signup/xyzfs/
    users/<uuid>/home/` — has `exchange/`, `net/`. Used by userpal ops,
    avatar system, `ledger_peers`. Resolves the SAME `current_xyzfs` string
    relative to `00.login-signup/`.
  - Same relative path `xyzfs/users/<uuid>`, two different absolute roots.
- **Duplicate identity files.** `01.avatar-creation👤️/` holds its own
  `xyzfs/session.pdl` (`mode=guest`, empty user) and `current_login.txt`
  (empty) — stale copies that can silently win when login_root resolution
  falls back to `project_root`.
- **Naive login_root resolution.** Ops use `../00.login-signup` from
  `project_root` (fallback chain ends at `project_root`). If launched from the
  wrong root the resolver reads the stale guest state instead of the live
  session.
- **Owner attribution drift.** The only avatar clone
  (`map_lobby/9bcba485…/state.txt`) is owned by `afx_3263291`, which matches
  no current userpal identity. jb's uuid never references it.

### 9.2 Clean schema (single source of truth)

```
<house>/
├── 0.user-pal👤️/00.login-signup/      ACCOUNT REGISTRY only
│   ├── current_login.txt               whoami (single source)
│   └── xyzfs/session.pdl               session pointer
└── xyzfs/users/<uuid>/home/            THE one per-user home
    ├── runtime/  exchange/  net/       (merged from Tree B)
    ├── livedesk/sessions/              (our sessions)
    └── avatars/                        (avatar truth)
        ├── inventory.txt
        └── <avatar_uuid>/state.txt …
```

- **Rule everywhere:** `current_xyzfs` is ALWAYS relative to `<house>`. No
  login-signup-relative interpretation.
- `00.login-signup/` keeps account registry (user meta.txt, current_login.txt,
  session.pdl) and NO per-user xyzfs tree.
- Avatar module keeps `pieces/world_01/map_lobby/` as a LOCAL working cache;
  `hydrate_avatars` syncs against `<house>/xyzfs/users/<uuid>/home/avatars/`.

### 9.3 Migration — EXECUTED 2026-08-09 (was "proposed, not yet executed")

1. Backed up both trees to `/tmp/opencode/schema-backup-1786340314/`.
2. Union-merged every old `00.login-signup/xyzfs/users/<uuid>/home` (meta.txt,
   home/{net,exchange}, runtime/ledger.txt, non-empty projects/) into
   `<house>/xyzfs/users/<uuid>/home/`.
3. Removed `00.login-signup/xyzfs/users/` (account registry only now). KEPT
   `00.login-signup/xyzfs/session.pdl` — it is the login session pointer.
4. Deleted stale avatar-module guest identity
   (`01.avatar-creation👤️/xyzfs/session.pdl`, its `current_login.txt`).
5. Login ops (`userpal_login.c`, `userpal_create_account.c`) + all 11
   avatar ops now mint/resolve per-user homes at `<house>/xyzfs/users/<uuid>/
   home/` (house_root via `HOUSE_ROOT` env else parent-of-0.user-pal, emoji-free
   upward walk). tsc_elo + both yahoo compose fallbacks re-pointed to
   `<house>/xyzfs/users`.
6. Recreated jb's avatar against the clean schema — persists at
   `<house>/xyzfs/users/0a9558a7-…/home/avatars/<uuid>/` (brown skin = skin_index
   3, black hair), appears on the avatar screen AND the taskbar USER cell.
7. Avatar sprite = full-body MC character synthesized from state.txt DNA
   (`make_avatar_sprite.c`, same pixel layout as avatar_window's
   synthesize_mc_front_sprite), NOT a font emoji — emoji fonts drop
   Fitzpatrick skin tones (base glyph renders default-skinned).

### 9.4 Livedesk coupling

Livedesk sessions (§4) already target `<house>/xyzfs/users/<uuid>/home/
livedesk/sessions/` = Tree A. This cleanup makes that unambiguous and fixes
the environment the sessions work depends on. Do not build sessions on the
login-signup-relative Tree B interpretation.

---

## 10. Desk properties / rename / edit-nav (K11 — approved 2026-08-10)

Follows option **(a) focused-row "edit" entry** (user approved; the
per-row right-nav column variant (b) was rejected). Feature set:

### 10.1 Desk name on the button

- The **desks button shows the open desk's name**, exactly like the file
  button shows the session name: `desks:<active-desk>` (e.g. `desks:desk_01`).
- Same poll-driven refresh as `file:` — re-read the active session's
  `STATE|active_desk` on the 1s tick; only rewrite + `mark_strip_frame_changed`
  when the value changed.
- `+new-desk` already creates the next `desk_NN.pdl` + switches; the label
  update is a side effect of the poll, no extra call needed.

### 10.2 Right-click on a desk row → desk properties

- Right-click (button 3) on any desk row in the desks selector opens a
  dynamic **desk properties** popup for THAT desk. Rows:
  1. `name: <desk>` — read-only header (current name).
  2. `rename...` — opens the modal cli-io input window (10.4).
  3. `entities: N` — N = `DESK` row count in the desk pdl (info only).
  4. `delete` — refuses if it's the only desk; if deleting the ACTIVE desk,
     switch to another desk first, then remove the pdl file.
  5. `cancel`.
- Properties popup reuses `livedesk_open_dyn_popup`; right-click routing is a
  new ButtonPress branch on the desks selector (the popup's current row is
  known at click time).

### 10.3 "edit" accessibility nav (option a)

- The desks selector gains a dedicated `edit` entry at the FAR RIGHT of the
  row list claiming the **next index after the popup's left nav block**
  (rows `1..N` → edit = `N+1`), symbolizing "right-click here".
- Behavior: Enter/digit on `edit` opens the properties popup for the
  **currently focused** desk row (unambiguous — no per-row nav column).
- Purely keyboard-visible: draws as its own row; mouse users just right-click.

### 10.4 Rename input — the `cli-io` user-input standard

Researched from the real CHTPM source
(`pieces/chtpm/plugins/chtpm_parser.c` + wsr-pal's faithful small
equivalent `014.wsr-pal…/ops/wsr_wizard_input.c`). cli-io shape to replicate:

- **Per-element state**: `input_buffer` (bounded, ~64-256 chars),
  `input_mode` (per-keystroke filter), distinct `target_id` so multiple
  cli-io fields never collide on a shared "input_text".
- **Key behavior**: printable chars append to the buffer; **Backspace trims**;
  **Enter submits** (sends the value onward, then clears the buffer);
  **Esc cancels**. Numeric/text filtering happens BEFORE the char reaches the
  buffer (`input_mode`), exactly the wsr wizard's `if (numeric_step &&
  !isdigit(key)) return 0;`.
- **For desk rename**: `input_mode` = `[A-Za-z0-9_-]` only (safe ASCII — desk
  identity is the pdl filename, §4.7). Enter commits = rename the pdl file +
  update `active_desk`/`last_desk` if needed; Esc discards.
- **Persistence** (later, optional): save/restore the in-progress buffer
  keyed by project/session + target_id (cli-io's gui_state pattern), so a
  mid-rename survives a restart.

### 10.4b Rename input — AS BUILT (2026-08-10)

The built modal (`cliio_*` block in `tp_taskbar.c`) follows the standard
above with the approved nav-entry + separate-window shape:

- **Standalone centered edit window** (`300x56`, two `HQ_POPUP_ROW_H` rows),
  NOT anchored to the toolbar popup. override_redirect, class
  "MuchiverseLivedesk", Exposure|KeyPress|ButtonPress mask.
- **NAV-ENTRY two-mode**: row 0 renders `[>] 1. rename desk: [<buf>]` while
  focused; Enter turns the pref to `[^]` and captures keys until **Escape
  deactivates** (back to nav, buffer untouched). Enter again re-activates;
  the final Enter commits. In nav mode Up/Down and digits `1`/`2` move
  focus; row 2 = `[ ] 2. cancel` (Enter or click cancels). Escape in nav
  mode closes the window.
- **Focus ownership**: `cliio_open()` maps + focuses the editor; while
  `g_cliio_active`, `close_popups()` skips its focus restore so the
  modal owns the keyboard for the whole edit (direct instruction — nav
  entry must capture keys).
- Row numbers ALWAYS shown (`1.` / `2.`), digits selectable in nav mode.

### 10.5 Implementation home

Stays in LEGACY `tp_taskbar.c` (Q5-A): new pure-logic functions
`livedesk_desk_props`, `livedesk_rename_desk`, `livedesk_delete_desk`, a modal
cli-io input mode in the existing select loop, plus a ButtonPress button-3
branch on the desks selector. khtpm port stays deferred.

### 10.6 K11 acceptance

- Desks button reads `desks:desk_01`.
- Right-click `desk_01` → properties shows `name: desk_01`, `entities: 6`,
  `rename...`, `delete`, `cancel`.
- `rename...` types a new name → Enter → desk pdl renamed, active_desk
  updated, desks button + popup list show the new name.
- Keyboard: popup rows `1..N` + `N+1 edit`; Enter on `edit` opens properties
  for the focused row.

### 10.7 K11 — built + runtime-verified (2026-08-10)

All acceptance items landed on the legacy taskbar (see worklog). Rename
receipts (every rename below went through the app's modal, not the shell):
`desk_01 → office` → `office22222222` (typed digits) →
`officeoffice1finaldesk` (sanitizer + focus-capture proof — user keystrokes
while idle landed in the focused editor, and `!` never appeared in any
name). Final state: `office.pdl`, `STATE|active_desk | office`, strip label
`desks:office` nav=4. Mouse click path (selector → right-click → rename →
edit window) re-verified on the clean build; Escape closes the editor.
Temporary debug logging removed; taskbar rebuilt (EXIT=0) + restarted
(PID 319840). Delete-only-desk refusal + active-desk-switch-first also
landed (`livedesk_delete_desk`).

---

## 11. KHTPM Refactor — Dynamic UI from Pals Registry (Design Debt, 2026-08-10)

### 11.1 Current State: Hardcoded Strip Buttons

The strip buttons (file, desks, **pals**, palettes, edit, player, db, plugins, store, network) are currently:
1. **Hardcoded in `tp_taskbar.c`** (`btns[]` array initialization, lines ~420-435)
2. **Overridable via `#.desktop/livedesk_taskbar.pdl`** (`SECTION | strip_btn_N_label`, etc.)
3. **Static layout** — order and positions don't change based on what's in the system

### 11.2 Design Issue (Fuzz-Op "Thin Theater" Pattern Violation)

Following the Fuzz-Op **Manager Projection** pattern (documented in `#.haiku+/tpmos-re-dox/fo-menu-sys.md`):
- **Current:** Layout is hollow but buttons are hardcoded (thick theater).
- **Ideal:** Manager reads pals registry → publishes dynamic button state → UI renders from that state.

### 11.3 Scope for KHTPM Port

When refactoring to `khtpm_taskbar_core.c` (after Q10 Pals validation), consider:

1. **Dynamic Button Discovery**: Manager scans pals/, builds button list (not hardcoded array).
2. **Projection File**: Write `#.desktop/livedesk_strip_state.txt` with dynamic layout + commands.
3. **Parser Sovereignty**: Keep button rendering in khtpm parser; keep button discovery in manager only.
4. **Copy/Paste/Delete Desk UI**: Right-click on desk name → popup with copy/paste/delete (partial fix 2026-08-10: design documented, implementation deferred).
5. **Pals as First-Class Buttons**: Consider making each pal a button (or slider section) for quick access, vs. the current popup-based model.

### 11.4 Risk Mitigation (Legacy Taskbar, Now)

Current pals button wiring is complete and functional:
- ✅ Dispatch handles `livedesk:pals` correctly
- ✅ Pals registry populated on migration
- ✅ Popup builder scans pals/ correctly
- ⚠️ Strip button order hardcoded (acceptable until khtpm port)

**No action required before khtpm port.** The hardcoded approach works; the design debt is architectural, not functional.

---

*End livedesk-editor-design.md*
