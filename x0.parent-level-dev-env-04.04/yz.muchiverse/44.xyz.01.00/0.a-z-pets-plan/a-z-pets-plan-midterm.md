# A-Z Pets Mid-Term Vision: Shell, Login, Avatar Economy, xyzfs

**Created**: 2026-07-27
**Status**: Exploratory / evolving — NOT a phase tracker
**Relationship to stable doc**: This is the working scratch space for the
next layer of vision above `a-z-pets-plan.md`. Once a piece here is scoped
enough to get hour estimates and checkboxes, promote it into the stable
doc (most likely Phase 3, Player System). Until then it lives here so the
stable doc doesn't get destabilized by half-formed ideas.

Supersedes the "FUTURE: xyzfs/" note that was living at
`a-z-pets-plan.md` Phase 3.3 — that note is now just a pointer back here.

---

## 0. Why this doc exists

Drag-and-drop (zoo/egg-window Xdnd swap) is now roughly stable. Next focus
area, in order: **user login → avatar creation/ownership → the
filesystem everything else hangs off of (xyzfs) → a pre-login shell
(start button/hotbar)**. These four things are tightly coupled — avatar
ownership needs a real user-scoped place to live, and that place (xyzfs)
is also where the shell will look to restore what a player was doing.

---

## 1. User Login (near-term, active)

Current state: `0.user-pal👤️/00.login-signup/` has session isolation and
a real login/signup GUI (ops: `userpal_create_account.c`,
`userpal_login.c`, `userpal_logout.c`, `userpal_whoami.c`,
`userpal_menu_input.c`, `userpal_compose_frame.c` + `login.chtpm`),
no orchestrator yet, writes into a plain `users/<id>/` folder
(e.g. `users/jb/profile.txt`) rather than a real per-user filesystem.

### 1.1 Decision log (2026-07-27) — GUI login + harness first

Source of truth for this slice: **this midterm doc + live code under
`0.user-pal/00.login-signup/`**. `#.haiku+` is useful for UX-injection
rules (`!.local-ux-testing-ai.txt`, testing methodology) and for the
identity model (xyzos-standards §26 / USER-PAL-STANDARD), but its
session priorities (Gemma scaffolding etc.) are stale.

**Do now (done this session):**
1. Treat the existing login GUI as the product — not rewrite it.
2. Add `test-harn-same/` (copy tk_* ops from pal-forum/chat) + scenario
   `demo_login_signup.sh`: create (auto-login) → logout → login →
   logout → refuse unknown user. Real keystrokes only.
3. Fix first-paint `[Map Loading...]` race: `button.sh` pre-seeds
   `view.txt` via `userpal_compose_frame` before launching chtpm.

**Done 2026-07-27 (signup + uuid + xyzfs multi-user):**
4. On Create Account: mint UUID, write `uuid=` + `xyzfs_path=` into
   `users/<user_id>/profile.txt`, provision
   `xyzfs/users/<uuid>/{home,projects,meta.txt}` under the durable
   install root (not the session throwaway). Many users = many sibling
   uuid directories under `xyzfs/users/`.
5. Login writes `current_user_uuid` + `current_xyzfs` into
   `current_login.txt`. Legacy profiles without uuid get backfilled on
   first login.
6. Harness asserts two users get distinct uuids and coexisting xyzfs
   trees. Character creation auto-launch is deliberately NOT wired yet.

**Defer (next slices):**
- auto-open `01.avatar-creation/` after signup (separate dir, later)
- orchestrator / kill_all 101 conversion
- passwords — family v1 rule: no auth layer unless asked

**Run the harness:**
```
cd 0.user-pal👤️/00.login-signup
./test-harn-same/button.sh compile
./test-harn-same/button.sh demo
```
Proof: `0.user-pal👤️/00.login-signup/proof/harness-*/`

## 2. Avatar Creation & Ownership

**Status 2026-07-27:** `0.user-pal/01.avatar-creation/` is a
**muchi-pals-shaped clone manager** (not a one-shot wizard):

```
Main -> Faucet (tokens) -> Store (free starter / buy clone @20)
     -> Avatars list (select, sleep, open desktop window)
     -> Customize DNA (name, age, gender, skin-tone face emoji,
        hair/shirt/pants color, height, weight)
```

Each clone is a unique **avatar UUID**, stored:

- Local (desktop window contract, same as egg_window path):
  `01.avatar-creation/pieces/world_01/map_lobby/<avatar_uuid>/`
- Player fs (source of truth, multi-avatar):
  `00.login-signup/xyzfs/users/<user_uuid>/home/avatars/<avatar_uuid>/`
  + `inventory.txt` + `home/wallet.txt` (tokens)

Login context: sibling `00.login-signup/current_login.txt` via
`USERPAL_LOGIN_ROOT`. Desktop: `system/avatar_window` (egg_window copy).

**Ownership model** (still mirrors muchi-pals pets/trade):
- 1 free starter clone (Store "Claim Free Starter" while inventory empty).
- More clones bought with faucet tokens (local wallet in xyzfs for now).
- Multiple clones on desktop; DNA recustomizable anytime.
- Cross-user trade / shared store with muchi-pals: still later.

Open: wsr-pal / pal-chain currency vs local faucet tokens — local for now.

## 3. xyzfs/ — the real user filesystem

**This is the central architectural shift.** Right now cross-project
handoff folders (`exchange/`, `net/` outboxes/inboxes) are ad-hoc
siblings of the project dirs at the top level of
`44.xyz❤️‍🔥️00.08/`. That doesn't scale once there's a real "signed up
user who owns data across many projects" concept.

**Target shape** (multi-user; each signup mints a UUID and gets its own
tree — live under `0.user-pal/00.login-signup/xyzfs/` as of 2026-07-27):
```
xyzfs/
├── bin/                         # shared ops across projects (later)
└── users/
    └── <uuid>/                  # UUID tag = multi-user isolation
        ├── meta.txt             # uuid, user_id, display_name, created_at
        ├── home/                # exchange/, net/, saves, avatars, …
        └── projects/            # user's project instances ("desktop")
```
Human login name stays `users/<user_id>/profile.txt` (holds `uuid=` +
`xyzfs_path=xyzfs/users/<uuid>`). Apps that need the fs read
`current_login.txt` → `current_xyzfs`.

**New framing from 2026-07-27 conversation** (this is the part that
wasn't in the original xyzfs note): xyzfs isn't just a place for
exchange/net folders — it becomes the **primary save/load target for
everything**. When any game (zoo, house, muchi-pals, etc.) saves or
loads, it should be reading/writing under a user's `xyzfs/` by default,
not scattered per-project `data/` directories.

Corollary: projects that currently live as top-level siblings (002.zoo,
041.pal-chain, etc.) are, conceptually, **not yet "installed"** for a
given user. The mental model going forward: pretend we're already
inside the fs as developers. A project gets "installed" into a user's
`xyzfs/user/projects/` — either from a "cd" (i.e. copied/linked in from
the outside dev tree) or built directly from within the fs. This mirors
a real Linux user home / package-install mental model deliberately.

Flagship app: **mutaclsym** — most developed GUI, treat it as the
reference for what "installed and running under xyzfs" should look
like. Other apps (networking/blockchain — pal-chain, pal-forum,
wsr-pal economy) are secondary but will also read/write through xyzfs
once this lands, since they hold user-scoped data too.

Still deferred, same as before: not scoped or started. Revisit once
Phase 3 (login/avatar) implementation actually begins — arguably that's
now, so this doc should start getting a concrete first-slice plan (e.g.
"provision empty xyzfs/ tree on signup, migrate one project's save/load
to read from it") before it goes back into the stable doc as a real
phase.

## 4. Pre-Login Shell: Start Button / Hotbar

New concept, very early / speculative — recorded here deliberately
without over-specifying, per direct instruction ("i dont expect u 2
know what this is, just record it in our plan for now").

- Before login even happens, there's a **"start" button or hotbar
  menu** — a mimic of "wraith" functionality. ("wraith-alpha" is
  referenced in existing code comments — `01.muchi-pals-🥚️-13.01/system/
  chtpm_parser_pal.c` — as a prior/sibling desktop-shell precedent; worth
  checking that codebase directly when this gets scoped instead of
  re-deriving from memory.)
- Visual language: **numbered `[]` nav brackets** for menu items.
  - Pets/entities may **not** have numbered nav brackets, either within
    the game world or on the desktop.
  - Sub-options within desktop artifacts (windows/panels) may or may not
    have nav brackets — undecided, case by case.
- Flow: player hits "start" (or equivalent) → their avatar pops up on
  screen → along with whatever else they were working on, restored —
  including GL window folders / houses (zoos), etc.
- This flow is what makes xyzfs load-bearing: "whatever else they were
  working on" is exactly session/window state that needs to live
  somewhere per-user and be restorable — i.e. under `xyzfs/user/home/`.

No implementation plan yet — this section exists to not lose the idea,
not to scope it.

---

## 5. Open Questions (mid-term specific)

1. Does xyzfs provisioning happen synchronously at signup, or lazily on
   first save?
2. Avatar store currency: wsr-pal economy vs pal-chain — or both?
3. Nav-bracket rules for desktop-artifact sub-options — per-artifact
   config, or a global shell rule?
4. "Installed from a cd" — literal filesystem copy-in, or a manifest/
   symlink scheme? (Relevant once a project needs updates without
   clobbering a user's live save data under it.)
5. Session restore on "start" — is this a snapshot taken at last clean
   shutdown, or continuous checkpointing?

---

## 6. When to promote pieces of this into the stable doc

- xyzfs first-slice (empty tree provisioning + one migrated save/load
  path) → Phase 3 of `a-z-pets-plan.md`, once there's an hour estimate.
- Avatar creation/store/ownership → also Phase 3, once the store
  mechanism (shared with muchi-pals) has a concrete shape.
- Start button/hotbar shell → likely its own new Phase, since it's
  pre-login and cross-cuts every project, not scoped under 0.user-pal.
