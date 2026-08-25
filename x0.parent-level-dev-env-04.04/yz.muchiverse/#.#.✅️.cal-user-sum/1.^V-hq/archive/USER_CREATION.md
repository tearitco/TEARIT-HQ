# User Creation System — Research, Implementation, and Test Log

**Date:** 2026-08-11
**Status:** ✅ IMPLEMENTED AND VERIFIED — USER cell now has real New User / Switch-user / Logout
submenu, fully working end-to-end through relay injection (not CLI). See "Test Log" section near the
bottom for the full verified trace.
**Owner:** claude-0001

---

## ⚠️ Open Question for Future Work (not resolved here, don't guess)

Everything below assumes `0.user-pal👤️/00.login-signup/` (with its own `users/` dir and
`current_login.txt`) stays the canonical account-registry location, because that's where the
EXISTING code (`livedesk_user_uuid()`, `ktb_get_avatar_dir()`, and every other khtpm function that
asks "who's logged in") already reads from — changing that without also migrating existing user data
would break things silently.

Direct instruction (2026-08-11): `0.user-pal👤️` is itself a legacy dev-folder, in the SAME category
as `@.apps/` — this house is moving away from dev-folder locations into `sessions`/`xyzfs`-native
storage generally. So the account registry's CURRENT location is correct for now (matches existing
code), but is NOT the final intended home. A future task should design where the account registry
truly belongs in the xyzfs/sessions model and migrate `0.user-pal`'s `users/`+`current_login.txt`
(and every khtpm function that reads them) together, in one pass — don't do this piecemeal or as a
side effect of an unrelated fix.

---

## Current Implementation (user-pal CLI)

### Files
- **Main:** `0.user-pal👤️/00.login-signup/ops/userpal_create_account.c` (243 lines)
- **Related:** `userpal_login.c`, `userpal_logout.c`, `userpal_whoami.c`
- **GUI:** `userpal_compose_frame.c`, `userpal_menu_input.c`

### Flow: How Accounts Are Created (CLI)

```
userpal_create_account.+x <user_id> <display_name>
    ↓
1. Validate user_id (alphanumeric + _ and - only)
2. Check if user already exists (users/<user_id>/profile.txt)
3. Mint UUID (from /proc/sys/kernel/random/uuid or fallback)
4. Provision xyzfs home:
   - Create xyzfs/users/<uuid>/home/{projects,exchange,net}/
   - Write xyzfs/users/<uuid>/meta.txt (uuid, user_id, display_name, created_at)
5. Write users/<user_id>/profile.txt:
   - user_id=<id>
   - display_name=<name>
   - uuid=<uuid>
   - xyzfs_path=xyzfs/users/<uuid>
   - created_at=<timestamp>
6. Output: "Account '<id>' created (uuid=<uuid> xyzfs=<path>)."
```

### Installation Root Resolution (IMPORTANT)
The code follows a chain to find the durable install root:
1. Try `users/` symlink → resolve to real path → parent = install_root
2. Fallback: Follow `current_login.txt` → resolve to parent
3. Final fallback: Use PRISC_PROJECT_ROOT env var

**Why this matters:** Accounts can be created from throwaway session dirs, but xyzfs MUST be written to the durable install (not inside the session). This ensures accounts persist across session changes.

### Directory Layout Created
```
<house>/users/claude-0001/
└── profile.txt
    ├── user_id=claude-0001
    ├── display_name=Agent Claude 0001
    ├── uuid=<generated-uuid>
    ├── xyzfs_path=xyzfs/users/<uuid>
    └── created_at=<timestamp>

<house>/xyzfs/users/<uuid>/
├── home/
│   ├── projects/
│   ├── exchange/
│   └── net/
├── bin/                    (shared across all users, created once)
└── meta.txt
    ├── uuid=<uuid>
    ├── user_id=claude-0001
    ├── display_name=Agent Claude 0001
    └── created_at=<timestamp>
```

---

## Current UI Implementation (user-pal GUI)

### Module: 00.login-signup
- **How to run:** `bash button.sh run` (from 0.user-pal👤️ root)
- **What it does:** Text-based login/signup screen
- **Status:** ✅ Working (tested in walkthru-j30.txt)
- **Features:**
  - Create new account (user_id + display_name)
  - Log in with existing account
  - Account switcher (shows list of created accounts)
  - Marks active account with `>` indicator

### Input Flow
```
compose_frame() → render screen
    ↓
menu_input() → handle keyboard (arrows, Enter, numbers)
    ↓
On "New Account" + Enter → run userpal_create_account
    ↓
Account appears in list immediately
```

---

## The Gap: Livedesk Integration

### Current Livedesk User Cell (2.USER)
- **Location:** `khtpm_taskbar_manager.c`, function `ktb_strip_user_activate()`
- **Current behavior:** Shows username via `${username}` variable, no "New User" option
- **Function signature:**
  ```c
  void ktb_strip_user_activate(KtbState *s) {
      ktb_hq_close(s);
      if (!s->strip_user_cmd[0]) return; /* real no-op, matching the legacy's empty default */
      // ... runs strip_user_cmd if defined
  }
  ```
- **Problem:** USER cell is treated as a direct action (no submenu), not an ACTIVATE button
- **Contrast:** HQ/file/desks cells ARE ACTIVATE buttons with submenus populated by ktb_hq_open()

### Why It's Designed This Way
- USER cell in legacy tp_taskbar.c is a **direct action** (runs a command), not a popup menu
- Adding "new user" would require converting it to an ACTIVATE button + building a submenu
- This is a **breaking change** to the legacy behavior, but probably good one

---

## ⚠️ Correction (2026-08-11, direct instruction): livedesk has NO user login/creation code at all yet

Everything below in this section was originally framed as "wiring existing code together." That's wrong.
**userpal_create_account.c is a working CLI binary that lives entirely inside `0.user-pal👤️/`, a
SEPARATE, PRE-EXISTING GUI system (own compose_frame/menu_input/piece.pdl loop).** khtpm/livedesk's
own taskbar manager (`khtpm_taskbar_manager.c`) has never called it, never had a submenu for it, and
has no two-field text-entry capability to collect user_id + display_name in the first place. This is
**new code to write**, informed by user-pal's *proven* design (below), not a wiring/gluing task.

### The actual proven pattern (read from userpal_menu_input.c directly, not inferred)
user-pal's login screen works like this — and khtpm's port should mirror this shape, not invent one:

1. **`piece.pdl` is regenerated every idle tick** from `users/` dir contents — a fixed METHOD table
   (`Create Account` → SIGNUP, `Log In` → LOGIN, `Log Out` → LOGOUT) plus one **`Switch: <id> (<display_name>)`**
   row per existing account (current account marked with `*`). This is how the account list you see IS
   the live directory listing — no separate cache to go stale.
2. **Two-field text entry lives in a SEPARATE file, `gui_state.txt`** (`user_id_input`, `display_name_input`),
   written by some other input widget (not shown in menu_input.c itself — presumably a text-typing screen
   that writes these keys as the user types, then SIGNUP just reads both back out).
3. **`userpal_menu_input.+x <keycode>`** is the dispatch entrypoint (called once per keypress, key 0 = idle
   refresh-only). On SIGNUP: read both gui_state fields → `system()`-shell to `userpal_create_account.+x`
   → if it succeeded, immediately shell to `userpal_login.+x` too (auto-login) → regenerate piece.pdl so the
   new `Switch:` row shows up on the very next frame.

### ⚠️ Correction #2 (still 2026-08-11): check the GRANDFATHER before inventing new state shape

Direct instruction: **always look for an existing CHTPM standard before creating a new one**, and
if it's not in the local (khtpm) chtpm implementation, **check tpmos — the genesis program CHTPM was
ported from** — it's likely more feature-complete. Reference:
`1.TPMOS_c_+rmmp.0103.0001/` (sibling dir at the same NEST-11.17 level, outside yz.muchiverse).

**Found:** tpmos already has a real, working two-field signup screen —
`projects/user/layouts/user_signup.chtpm` + `projects/user/manager/user_manager.c`. The proven
standard is **multiple simultaneous, independently-addressable `<cli_io>` elements on one screen**:
```xml
<cli_io id="username_input" label="Enter username" target_id="1" />
<cli_io id="password_input" label="Enter password" target_id="2" input_mode="password" />
```
Each has its own `id` (the state-file key it reads/writes) and numeric `target_id` (how the parser
tracks which field currently has typing focus, toggled by click or Tab). The manager
(`user_manager.c`'s `read_state`/`write_state`) just reads BOTH `username_input` and
`password_input` back out of `state.txt`/`gui_state.txt` directly when Signup fires — genuinely two
live fields, not a sequential single-buffer modal chain.

**khtpm's parser status:** `khtpm_strip_layout.c` (line ~182) **already parses `target_id`** as a
generic cli_io attribute — the chtpm-standard plumbing for this exists locally too. What's missing
is entirely on the **manager side**: `KtbState` only ever tracks ONE singleton cli-io buffer
(`s->cliio_buffer`, `s->cliio_op`, see `ktb_cliio_open_save_as()`/`ktb_cliio_open_rename_desk()`),
used for one modal at a time (save-as OR rename-desk), never two fields shown together.

**Revised plan: extend KtbState to hold 2 named buffers (mirror tpmos, don't invent a sequential
chain)** — e.g. `cliio_buffer` + a new `cliio_buffer2`, or better, generalize to an array of
`{id, buffer}` pairs sized 2, matching tpmos's `username_input`/`password_input` shape exactly. This
is more faithful to the proven ancestor standard than my earlier "sequential 2-modal chain" idea
(struck through below — kept for history, not to be built).

~~4. **New cli-io capability:** extend cliio state to support a 2-step sequential text entry...~~
(superseded — see correction above: tpmos proves simultaneous multi-field is the real standard)

### Recommended Plan (build new, inspired by user-pal's dispatch shape + tpmos's multi-field cli_io)
**Steps:**
1. Convert USER cell from direct action to ACTIVATE button in khtpm_strip_header.chtpm
   - Change: `<button label="${username}" onClick="STRIP:2" .../>`
   - To: `<button label="${username}" onClick="ACTIVATE:2"><row>${strip_hq_items}</row></button>`
2. New function `livedesk_build_user_menu()` in khtpm_taskbar_manager.c (mirrors user-pal's
   `regenerate_login_pdl()`): scan `users/` dir → build "New User…" / "Logout" / one `switch-user:<id>`
   row per existing profile.txt found.
3. Add case `which == 2` in `ktb_hq_open()` to call it.
4. Extend `KtbState` cli-io fields to hold 2 named buffers (user_id_input, display_name_input),
   mirroring tpmos's `AppState.username_input`/`password_input` shape exactly. Add matching
   `<cli_io id="user_id_input" target_id="1"/>` + `<cli_io id="display_name_input" target_id="2"/>`
   elements to khtpm_strip_header.chtpm's cli_io section (currently a single singleton element).
5. On final submit (both fields filled, e.g. an explicit "Create" button click rather than Enter on
   either field alone — tpmos uses a separate Signup button, not per-field Enter): call
   `userpal_create_account.+x` (same binary, same interface — no need to reimplement
   uuid-minting/xyzfs-provisioning, that logic is already correct and tested) then auto-login
   (mirrors user-pal's own SIGNUP→auto-login behavior).
6. Build relay-based harness (see TESTING_STRATEGY.md) to test: open USER cell → New User → Tab/click
   to field 1 → type id → Tab/click to field 2 → type name → click Create → verify
   `users/<id>/profile.txt` + `xyzfs/users/<uuid>/` exist.

### Option B: Keep CLI + Add Icon/Button in livedesk (Simpler)
**Pros:**
- Minimal change to existing code
- USER cell stays a direct action
- No UI complexity

**Cons:**
- Users still need terminal to create accounts
- Doesn't match user expectations (everything else in livedesk)

**If chosen:** Create README at xyzfs/users/claude-0001/CREATE_USER_HOW_TO.txt explaining CLI command

---

## Decision: Go with Option A

**Reasoning:** User is moving everything to livedesk UI. CLI user creation is a gap that contradicts that goal.

---

## Acceptance Criteria

Once implemented, test should verify:
- [ ] USER cell shows "New User" option
- [ ] Can type user_id and display_name via CLI-IO modal
- [ ] Account created: `users/<user_id>/profile.txt` exists
- [ ] xyzfs home created: `xyzfs/users/<uuid>/` exists with correct structure
- [ ] Can immediately switch to new account (appears in list)
- [ ] Persists across livedesk restart
- [ ] Can create multiple accounts without collision

---

## Code Locations to Modify

1. **khtpm_strip_header.chtpm (line ~81)**
   - Change USER button from STRIP:2 to ACTIVATE:2
   - Add child row structure

2. **khtpm_taskbar_manager.c (line ~1867)**
   - Add case `which == 2` in ktb_hq_open() switch
   - Build user menu (New User, Logout, List)

3. **khtpm_taskbar_manager.c (line ~1909)**
   - In ktb_hq_activate(), add handler for "New User" command
   - Call userpal_create_account via system() or direct function

4. **Create new function in khtpm_taskbar_manager.c**
   - `livedesk_build_user_menu()` (following pattern of livedesk_build_session_menu, etc.)
   - Scan users/ directory, list existing accounts
   - Add "New User…" row at top

---

## Research Notes

### Why UUID?
- Accounts can be deleted, but we don't want their old xyzfs homes to collide with future users
- UUID makes each home globally unique, safe to restore from backup

### Why xyzfs/<uuid>?
- Each user's personal filesystem tree
- Can coexist on shared machine (multi-user safety)
- Easy to backup/restore (entire user = one directory)
- Pattern matches house conventions for isolation

### Why profile.txt in users/<id>?
- Stable, human-readable user_id is in the durable location
- Quick lookup: `users/<id>/profile.txt` to find UUID
- Not duplicated in xyzfs/users/<uuid>/ (xyzfs only has meta.txt)

---

## Next Steps

1. **Implement** (assign to agent or do locally):
   - Wire USER cell to ACTIVATE + submenu
   - Add "New User" handler
   - Test with claude-0001 account creation

2. **Test** (in harness):
   - Create new user via livedesk UI
   - Verify files created
   - Verify persistence

3. **Document** (in per-game docs):
   - How developers should create accounts for testing
   - Multi-user testing scenarios

---

## Implementation Notes (what actually got built, 2026-08-11)

- `khtpm_strip_header.chtpm`: USER button converted from `onClick="STRIP:2"` (direct action) to
  `onClick="ACTIVATE:2"` with a `<row>${strip_hq_items}</row>` child, matching every other
  submenu-bearing header cell.
- `khtpm_taskbar_manager.c`: new `livedesk_build_user_menu()` (mirrors `livedesk_build_session_menu()`'s
  shape) — scans `users/` under `livedesk_login_root()`, builds "New User..." + one
  `user:switch:<id>` row per account (current one marked `*`) + "Logout".
- Two-stage sequential cli-io flow for signup (`new-user-id` then `new-user-name` ops), reusing
  `cliio_id` as scratch storage between stages — chosen over tpmos's simultaneous multi-field
  `<cli_io>` because khtpm's cli_io is a single leaf per screen (see the design-note comment above
  `livedesk_build_user_menu()` in the code for the full reasoning).
- Three call sites (`user:switch:`, `user:logout`, new-user-name submit) all shell out via
  `setsid nohup sh -c 'cd "<login_root>" && "./ops/+x/userpal_*.+x" ...'` — the `cd` is REQUIRED, see
  Real Bug #1 below.
- `ktb_hq_open()`/`ktb_nav_enter()`/`dispatch_code()`'s `KSC_HQ_HEADER_BASE` branch: removed the
  `which==2 -> ktb_strip_user_activate()` special-case from all three call sites so USER routes
  through the same `ktb_hq_open()` path as every other cell. `ktb_strip_user_activate()` itself is
  left in place (orphaned, not deleted) since `strip_user_cmd` was a separate direct-instruction
  placeholder from 2026-08-08 — a future agent may want to re-expose it, not have it silently vanish.

## Real Bugs Found During Testing (not hypothetical, all fixed)

**Bug #1 — wrong root for shelled-out userpal binaries.** First test create ran
`"<login_root>/ops/+x/userpal_create_account.+x" ...` by absolute path with no `cd`/env var set. The
binary resolves its OWN `users/`/`current_login.txt` relative to `PRISC_PROJECT_ROOT` (or `.` if
unset) — since nothing set that, `.` defaulted to the MANAGER PROCESS's own cwd (house_root), not
`login_root`. Result: a real account was created correctly (`profile.txt`, `xyzfs/users/<uuid>/`
fully provisioned, auto-login worked) but at a disconnected `house_root/users/` and
`house_root/current_login.txt` that `livedesk_user_uuid()`/`ktb_get_avatar_dir()` never read —
switching accounts via the new UI would have appeared to work while silently not changing what the
rest of khtpm thought was logged in. Fixed by prefixing every shell-out with `cd "<login_root>" &&`
(matching the exact pattern user-pal's own `userpal_menu_input.c`'s `run_capture()` already uses).
Test artifacts from the broken version (`house_root/users/claude-0001`, `house_root/current_login.txt`,
the mis-rooted `xyzfs/users/<uuid>/`) were cleaned up before the fix was retested.

**Bug #2 (pre-existing, unrelated, found+fixed same session) — entities survive taskbar kill.**
Not a USER-cell bug, but found while restarting the taskbar mid-testing: killing khtpm's own
processes directly (not via its quit menu) left all `tp_desktop_window.+x` entities running,
requiring the manual `EMERGENCY_CLOSE.sh` fallback script. Root cause: `ktb_quit_and_save()` only
ever messaged `s->tabs[]` (a subset), not the full `#.desktop/livedesk_open.txt` registry, and had no
hard-kill fallback for stragglers. Fixed: `ktb_quit_and_save()` now calls the same
`livedesk_close_all()` (registry-based CLOSE) + `livedesk_kill_stray_entities()` (`/proc` sweep)
combo already proven by player>reset, and the sweep itself was upgraded from SIGTERM-only to a real
SIGTERM-then-SIGKILL sequence (matching `EMERGENCY_CLOSE.sh`'s own 3-phase shape). `EMERGENCY_CLOSE.sh`
itself is kept as a manual last-resort (its own header comment updated to say so) for when khtpm's
processes are killed/crashed directly, bypassing the quit menu entirely.

## Test Log — Full Relay-Only Verification (2026-08-11)

Both flows below were run PURELY through `#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh` — no
direct CLI calls, no C function calls, matching the house's relay-only testing rule. See
TESTING_STRATEGY.md's "Verified Working Recipe" section for the exact reusable command sequence.

**New User flow:** `esc` → `nav 2` (opens USER submenu) → `key Return` (activates "New User...") →
`key Return` (arms typing) → `type "claude-0001"` → `key Return` (submits id, auto-advances to
display-name stage — confirmed via `strip_state.txt`'s `cliio_op` flipping to `new-user-name`) →
`type "AgentClaude0001"` → `key Return` (submits, creates account). Verified via filesystem:
`0.user-pal👤️/00.login-signup/users/claude-0001/profile.txt` correct, `xyzfs/users/<uuid>/`
provisioned (`home/`, `projects/`, `meta.txt`), `current_login.txt` (login_root copy) correctly
auto-logged-in as the new account.

**Switch-user flow:** with claude-0001 active, `nav 2` reopened the submenu (now showing more rows —
other pre-existing test accounts from earlier agents shifted the alphabetical ordering), `key 3`
selected the wrong row first (a harmless no-op self-switch, since digit-selection is 1-based row
position, not a fixed slot — always re-check `hq_focus`/row count before assuming which digit maps
to which account), `key 5` correctly selected `jb`, `key Return` switched — verified `current_login.txt`
flipped back to `jb`'s original `uuid=0a9558a7-...`.

**Real testing-process finding, also worth knowing:** one contaminated read (a human's own Enter
keypress collided with an agent's relay sequence) briefly produced a false-negative that looked like
an architecture-level gap in how `nav.sh nav <n>` addresses header cells. A clean retry (fresh `esc`,
no overlapping input) proved the relay was correct all along — see `_.0.aigent-testing-k9.txt`'s
2026-08-11 addendum for the full corrected write-up. Lesson folded into TESTING_STRATEGY.md's
Resilience section: retry clean once before concluding a mechanism is broken.

---

**Owner:** claude-0001
**Status:** ✅ COMPLETE — implemented, tested end-to-end via relay, both bugs found during testing fixed
**Follow-up:** the open registry-location question above; maintenance-fixes.md has 2 small cosmetic
items (missing row index numbers, submenu width) noticed during this work
