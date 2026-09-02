# 🧭 First-install → publish-a-toy: the real user journey, and what's actually built

**Status: PLANNING, 2026-09-02.** Direct request: walk the entire real
experience end to end, install through publishing your own toy to the
store, as a completion graph — what exists today (checked against
real code, not assumed) vs. what's genuinely new work. Companion to
`PHONDO_INSTALL_IDEAS.md` (the install/store shape decisions) and
`SECURITY.md` (the risk side of several steps below).

## The real journey, step by step

### 1. `sh` one-liner → install script → adds `hq` to PATH
**Status: ❌ NOT BUILT.** No install script pulls from GitHub and
installs a `hq` command today. `xyzos-starter-install.sh` (v1) is
obsolete (pre-dates the khtpm taskbar entirely). The HARNECIENT design
doc (`design-docs/04.harnecient-fresh-install-design.md`) describes
almost exactly this shape but was never coded. **Real new work,
highest-priority — everything downstream depends on this existing.**
Cross-ref: the dynamic-product-name decision already made means this
script can't hardcode `hq` as a literal name either — it's whatever
name the shipper passes in, `hq` being the example/default.

### 2. User runs `hq` → taskbar populates
**Status: ✅ REAL, WORKS TODAY**, once step 1 delivers a correctly laid
out install root (see `04.harnecient-fresh-install-design.md` §4 for
exactly which subtrees need to exist at which relative paths — this is
a real, already-solved layout problem, not new design work). The
actual taskbar/manager/entity binaries are proven, live, verified
constantly this session.

### 3. Not logged in yet → cursword offers to walk them through signup, via nav-injection, no AI API needed
**Status: ❌ NOT BUILT, but every real primitive it needs already
exists.** Concretely:
- Cursword itself is real, always-present, already the account's "soul"
  entity (`CURSWORD-SOUL-VISION.md`) — the right entity to do this,
  already decided.
- The injection mechanism (write decimal key codes into a relay/
  history file, exactly how this house tests everything) is real and
  proven — an FSM/BT driving cursword to literally write those same
  codes into the USER header cell's own relay file, rather than a
  human typing them, is a genuinely small step from what already
  works, not a new input mechanism.
- What's missing: the actual FSM/BT logic that (a) detects "guest, no
  session" state, (b) decides to interrupt/offer, (c) drives the real
  nav sequence (open USER cell → New User → arm typing → username →
  confirm) via the relay file. Real, scoped, buildable — a state
  machine over an already-real dispatch surface, "simple FSM/BT" per
  direct instruction, no LLM call needed for this specific flow.
- Real open design question: what actually triggers the offer (time
  since launch? first click on anything? explicit "help me" nav
  item?) — worth a real decision before building, not assumed.

### 4. Signup completes → suggest avatar creation
**Status: ⚠️ EXISTS, BUT LEGACY — real rework needed.** The login/
signup app (`0.user-pal👤️/00.login-signup/`) is real and khtpm-native
already. Avatar creation is the part that's stale: it was v1's own
separate app (`01.avatar-creation👤️`), pre-dating the khtpm taskbar
entirely (confirmed: no `avatar` directory exists at the current house
root at all — it's gone/never migrated). **Direct instruction
confirmed here**: rework avatar creation onto the real khtpm/
`CENTROID_GOLD_STD` standard as its own real task, not a revival of
the old app. `archive/USER_CREATION.md` (referenced in the book's own
Tier 3, now under `design-docs/`) has real prior research on wiring
account creation into the USER cell — read before building, it may
already answer some of the "how" here.
- The cursword-driven FSM from step 3 is the natural place to also
  suggest this next step once signup completes — same mechanism,
  next state.

### 5. Suggest mining tearit-chain to earn tokens (buy/trade in-house)
**Status: ⚠️ REAL, WORKING LOGIC — on the WRONG rendering engine.**
Confirmed by reading the actual source: `041.pal-chain⛓️/ops/` has
real, complete C ops — `chain_miner.c`, `chain_create_wallet.c`,
`chain_login.c`, `chain_balance.c`, `chain_send.c`, a real P2P peer op
(`palnet_peer.c`). This is not vaporware — it's a real, working
chain/wallet/mining system. **But it renders via `system/chtpm_rgb_
render.c`** — the Stage 2 "blind rasterize" mirror engine
`CENTROID_GOLD_STD.md` §2 traces as a real, working, but structurally
limited dead-end for anything meant to look like a native window (no
box model, no real styling, text-mirror only). Bringing pal-chain onto
the real khtpm/Elem-tree standard is genuinely the same class of work
as the network-browser conversion already done — a real, scoped,
precedented migration, not a design unknown.

### 6. Ask what they're interested in (work/networking/game-dev/etc) → suggest relevant toys
**Status: ❌ NOT BUILT** as a real onboarding flow, though the
underlying catalog of "what toy matches what interest" already has
real raw material: `MARKETABLE-FEATURES.md` (now in `design-docs/`)
is a real, cited inventory of every taskbar cell's actual working
state — the natural source data for "if they say 'networking', suggest
the network cell's real apps" style mapping. Needs: (a) the actual
interest-capture UI/flow (cursword-driven again, likely), (b) a real,
maintained interest→toy mapping table (start small, it's just data).

### 7. Piececraft-hq ships standard; other toys install via "12.store" on demand
**Status: ❌ STORE MECHANISM NOT BUILT AT ALL** — confirmed by grep,
there is no "store" cell wired into `khtpm_taskbar_manager.c` today.
`@.app-store/` exists as a folder but (per `PHONDO_INSTALL_IDEAS.md`
§3) is effectively empty/unwired. This is the single biggest real gap
in the whole journey — everything upstream (steps 1-6) can be built
and tested without it, but the actual "install this specific toy on
demand, minimal footprint" mechanism is greenfield. Real, concrete
shape this step implies (new information, worth capturing precisely):
- **A base install ships standard with piececraft-hq only** (owner's
  own stated default) — not every toy, not assets for toys not
  installed. This directly supports a genuine minimal-footprint test
  install (§8 below) as a first-class goal, not an afterthought.
  Depends on the CLI-bootstrap being able to run a store-driven
  per-toy install after step 1's base install, not just once at the
  start — i.e., install and store share the SAME underlying "fetch and
  place a toy" mechanism, whether triggered at first-boot or later
  from the store cell.
- **Assets are opt-in, separate from code.** A toy's own compiled ops
  can be small; its real asset bundle (sprites, sounds) may be large —
  worth keeping these as two separately-installable pieces from day
  one of designing the store's own manifest shape, not bolted on
  later once a toy already assumes its assets are always present.
- This is real, new plumbing: a manifest format (what is a "toy" —
  name, version, code location, optional asset location, dependencies
  if any), a fetch mechanism (git clone a repo? download a release
  archive? — undecided, see `PHONDO_INSTALL_IDEAS.md` open questions),
  and a real UI (the store cell itself, khtpm-native from the start,
  no legacy-engine detour needed since it doesn't exist yet).

### 8. Minimal-footprint test installs (no bundled toys/assets, just enough to test install/store itself)
**Status: ❌ NOT BUILT**, but directly falls out of step 7's design
once the store mechanism exists — a test install is just "run the
bootstrap with zero optional toys selected." Worth stating as an
explicit acceptance criterion for step 1+ step 7's design (not a
separate mechanism to build): the base installer + store client should
be genuinely runnable and testable with nothing else installed,
without special-casing a "test mode."

## Completion graph — at a glance

| Step | What | Status |
|---|---|---|
| 1 | Install script (GitHub → `hq` on PATH) | ❌ Not built — top priority |
| 2 | `hq` launches, taskbar populates | ✅ Real, works today |
| 3 | Cursword-driven guest→signup onboarding (FSM/BT, nav-injection) | ❌ Not built; every primitive it needs already exists |
| 4 | Avatar creation suggestion + real build | ⚠️ Exists but legacy/gone from current house; real khtpm rework needed |
| 5 | Tearit-chain mining/wallet suggestion | ⚠️ Real, working logic; wrong (legacy) rendering engine |
| 6 | Interest-based toy suggestions | ❌ Not built; real source data (`MARKETABLE-FEATURES.md`) already exists |
| 7 | 12.store on-demand toy install (code+assets, minimal footprint) | ❌ Not built at all — biggest real gap |
| 8 | Minimal-footprint test installs | ❌ Falls out of #7's design, not separate work |

**Read as**: two real, working subsystems (taskbar/entities, pal-chain's
actual chain logic) plus one working-but-legacy app (avatar creation,
partially) already exist to build ON; the actual connective tissue —
install, store, and the guided onboarding flow linking them — is
almost entirely greenfield. This is not a discouraging picture: it
means the hard, uncertain "does the core engine work" question is
already answered yes, and what's left is real, scoped plumbing and UX
work with clear precedent to build from at every step (the network-
browser conversion for #5's rendering migration, the relay-injection
mechanism for #3, the existing manager/op pattern for #7's store
client).

## Cross-references
- `PHONDO_INSTALL_IDEAS.md` — the install/store shape decisions (CLI-
  first, dynamic product naming, central-catalog-first store model)
  this graph assumes.
- `SECURITY.md` — real risk concerns that apply directly to steps 1
  (install-script trust) and 7 (store supply-chain risk).
- `CENTROID_GOLD_STD.md` — the rendering standard step 4/5's rework
  targets, and the precedent (network-browser conversion) both should
  follow.
- `design-docs/MARKETABLE-FEATURES.md`, `design-docs/archive/
  USER_CREATION.md` (via Tier 3 pointer) — real source material for
  steps 4 and 6.
