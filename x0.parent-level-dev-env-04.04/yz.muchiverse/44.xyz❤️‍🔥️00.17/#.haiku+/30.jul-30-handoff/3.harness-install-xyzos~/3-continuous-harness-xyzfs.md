# 🔁 Topic 3 — A continuous, real-user-shaped test harness, and getting agents out of 44.xyz

> Companion deep-dive to `#.haiku+/30.jul-30-handoff.md`. This topic is the
> most greenfield of the three — most of what's asked for genuinely
> doesn't exist yet. Where that's true, this file says so plainly and
> gives a concrete design grounded in what DOES exist, rather than vague
> aspiration.

## 🗺️ 0. The real state of the world

| Piece | Status |
|---|---|
| `xyzfs/` (multi-account filesystem) | ✅ real, working — but scoped INSIDE the house tree, multi-*account*, not multi-*OS-user* |
| Login/signup app | ✅ real, harness-proven (`0.user-pal👤️/00.login-signup/`) |
| Character/avatar creation | ✅ real app exists (`0.user-pal👤️/01.avatar-creation👤️/`) |
| Forum app | ✅ real, working, harness-proven (`041.pal-forum👥️/`) |
| IRC/chat app | ✅ real, working P2P (`044.pal-chat-irc👥️+2/`) |
| `@.app-store/` | ❌ directory exists, **zero files in it** |
| App-store/installer design | 📝 notes only, unbuilt (`#.notes/AFTER-widgets-apps-store.txt`) |
| Per-OS-user `~/xyzos/` | ❌ **does not exist anywhere, not even as a design note** |
| Key-injection driving of any CHTPM+PAL app | ✅ real, documented spec (`#.haiku+/!.local-ux-testing-ai.txt`) |
| Multi-user simulated UX script | ✅ real but single-run, 2-user, one-app (`044.pal-chat-irc👥️+2/testing/test_real_ux_2users.sh`) |
| Continuous/looping harness | ❌ does not exist |
| Multi-app agent chain (login→character→forum→game→chat→edit) | ❌ does not exist |
| LLM-driven (not scripted) fake-user behavior | ❌ does not exist (needs topic 2's shared `bot::*` vocabulary first) |

## 🧱 1. The load-bearing distinction: `xyzfs` is not `~/xyzos/`

This is the single most important correction to make before building
anything here. `xyzfs/` (confirmed real, live: `xyzfs/users/<uuid>/home/
{runtime/ledger.txt, projects/{...}}`, design doc at
`0.user-pal👤️/00.login-signup/xyzfs/README.txt`) already models "multiple
users" — but every one of those users is a **UUID-keyed account inside
ONE shared tree that lives under `44.xyz❤️‍🔥️00.10/`**, the same
directory this entire house's source code lives in. That's correct and
fine for interactive development (a human dev, or Claude, testing the
login flow), but it is the **wrong** foundation for autonomous agents,
for a reason this house has ALREADY hit in production, not hypothetically:
`045.muchi-pal-agent🤖️+1/jul-21-gemma-fix.txt` documents a real bug where
**concurrent unreaped sessions raced on shared `world_01/state.txt`**,
causing double-logged replies and a stuck state. That happened with
ordinary interactive use. Running multiple autonomous, always-on agents
against the SAME shared `xyzfs/` tree — all reading/writing session state,
all potentially touching the same `EMERGENCY_KILL.sh`-swept
`pieces/sessions/` directories — would hit that class of bug constantly,
not occasionally. **This is the concrete, evidence-based reason the
agents need real filesystem isolation, not just a style preference.**

`~/xyzos/` (per your instruction: literally the real shell home directory
`~`, i.e. `$HOME/xyzos/`) is the right unit of isolation because it's
already how a real multi-user Linux box works: **`$HOME` already differs
per OS-level user automatically** — `/home/alice/xyzos/` and
`/home/bob/xyzos/` are different real paths on disk even though both are
spelled `~/xyzos/` from inside each user's own shell. You get isolation
for free from the OS, without inventing a new namespacing scheme. For
REAL human users this means: each signs into their own Linux account
(or, on a single-user dev box, their own `$HOME`), runs the starter
install once, and has their own private `~/xyzos/`. For AUTONOMOUS
AGENTS, you don't need to provision real separate Linux accounts (heavy,
requires root) — the standard, well-understood sandbox technique is
**overriding `$HOME` per agent process** (`env HOME=/srv/agents/agent-07
your-agent-runner`), which makes every relative-to-`~` path in the whole
stack (xyzfs README's own `~/xyzos/users/...` convention, once built)
resolve into that agent's own private directory, with zero code changes
needed anywhere else — every op in this house already resolves paths via
`project_root`/`PRISC_PROJECT_ROOT`, which composes naturally with a
per-agent `$HOME` override.

## 🚀 2. The starter-install program (does not exist yet — concrete design)

This is new infrastructure. Nothing found this session builds it. Design,
grounded in what already works:

```
xyzos-starter-install.sh (new, lives OUTSIDE 44.xyz — e.g. house root's
                           own sibling, or packaged separately, since its
                           whole point is to NOT require the dev tree)
  1. mkdir -p "$HOME/xyzos"
  2. Copy (not symlink — a real agent's own xyzos tree should survive
     the source house tree changing/moving) a curated MINIMAL set of
     compiled apps into $HOME/xyzos/apps/:
       - 0.user-pal👤️/00.login-signup  (real, harness-proven — THE
         first thing that runs, matching the request: "install the
         signup and other basic apps")
       - 0.user-pal👤️/01.avatar-creation👤️  ("making a character")
       - a minimal starter game (once topic 1's rpg-xyz/rtp-xyz work
         produces a real generic-content variant, THAT becomes the
         default starter game here, not mutaclsym's own zombie world)
  3. Initialize $HOME/xyzos/xyzfs/ as a FRESH, empty xyzfs tree (same
     shape as the real one, session.pdl + users/, but with zero users —
     the signup app populates the first one for real, on first run,
     exactly like it already does inside the house today)
  4. Initialize $HOME/xyzos/app-store/ as a PER-USER catalog + installed-
     apps ledger (see §3) — separate from 44.xyz/@.app-store/, which
     stays the DEVELOPMENT/source catalog this installer reads FROM
  5. Write a top-level $HOME/xyzos/button.sh (or equivalent launcher)
     that boots straight into login-signup, matching the real, already-
     proven boot sequence login-signup's own button.sh already uses
     inside the house — copy that launch sequence, don't reinvent it
```

The key design commitment: **the starter install is a one-way copy from
the dev tree's known-good apps into a self-contained user tree.** It is
not a live symlink back into `44.xyz` — that would defeat the entire
purpose (agents/users would still be touching shared, mutable dev-tree
state, the exact thing being avoided).

## 🏪 3. The app store — grounded in the existing (unbuilt) design note

`#.notes/AFTER-widgets-apps-store.txt` (2026-07-28) already sketches the
right shape and should not be re-invented: **"project + widgets = an
`@app`, a saved launch recipe"** — i.e. an installed app isn't a binary
copy, it's a manifest naming which project + which widgets compose it,
plus a `installed_apps.pdl` ledger. That note proposes this ledger live
under `xyzfs/users/<uuid>/` — the ONE change this topic makes to that
existing proposal is: put it under **`~/xyzos/app-store/installed_apps
.pdl`** (per-OS-user scope) instead of inside the shared house's own
`xyzfs/`, for the identical isolation reason as §1. The CATALOG itself
(what's available to install, i.e. `44.xyz/@.app-store/`) can and should
stay inside the dev tree — that's the source of truth a developer
edits — but the per-user record of "what IS installed, and where its own
private data lives" belongs in `~/xyzos/`, never in the shared tree.

Concretely, `~/xyzos/app-store/` needs:
- `catalog.pdl` (or a plain copy of whatever `44.xyz/@.app-store/`
  eventually defines) — pulled at install time, not live-linked, same
  reasoning as §2.
- `installed_apps.pdl` — this user's own installed-recipe ledger, per
  the existing design note.
- An install op (new, doesn't exist) that: reads a catalog entry, copies
  the named project+widgets into `~/xyzos/apps/<name>/`, registers it in
  `installed_apps.pdl`. This is a small, scoped op — model its own
  file-based, no-shared-headers shape on `0.user-pal👤️/00.login-signup/
  ops/userpal_create_account.c` (real precedent for "an op that
  provisions new per-user state from a template," which is exactly what
  an app install does, just for an app instead of an account).

## 🔄 4. The continuous, real-user-shaped harness cycle

The reusable foundation for this already exists and is real — don't
rebuild it:

- **`#.haiku+/!.local-ux-testing-ai.txt`** is a genuine, detailed spec for
  driving ANY CHTPM+PAL app via real key injection (numbered-item nav,
  Enter/ESC semantics, the exact `pieces/keyboard/history.txt` line
  format, process-cleanup rules). This is the SAME real mechanism
  `test-harn-ed-app` and `%.harnesses/event-editor+desktop` already use
  in practice — read this spec, it's the formal version of the pattern
  you already have hands-on experience with from building the editor
  harness.
- **`044.pal-chat-irc👥️+2/testing/test_real_ux_2users.sh`** is the closest
  existing precedent for a MULTI-USER simulated session: two fake users,
  signup → login → room-join → chat, real key injection throughout. It
  is explicitly **single-run, one app, not looping, not LLM-driven** —
  this is your starting skeleton, not your finished harness.

What's missing, in the order it should be built:

1. **Make it continuous.** Wrap the existing single-run pattern in a real
   scheduler loop (a simple `while true; do <one full cycle>; sleep N;
   done`, or a proper cron-style loop — nothing exotic needed) that
   restarts the SAME simulated user's session repeatedly, each time
   asserting real state (did the post actually land on the forum? did
   the chat message actually reach the other simulated user? — same
   "assert the rendered frame / assert the file on disk," never
   "assert the op returned 0," lesson as `!.xyzos-standards+1.txt` §36.6
   already establishes).
2. **Make it multi-app.** Chain across app boundaries in one script: the
   SAME simulated user signs up (login-signup) → creates a character
   (avatar-creation) → posts on the forum (pal-forum) → joins a game
   session (mutaclsym today, rpg-xyz/rtp-xyz once topic 1 lands) → chats
   with the AI (muchi-pal-agent, once topic 2's open bug is fixed) — each
   hop is a real `button.sh run`/relay-file handoff, matching exactly how
   a real human would tab between these as separate GL windows.
3. **Isolate it in `~/xyzos/`.** Every simulated user in this harness
   should be provisioned via §2's starter-install into its own `$HOME`
   override, NOT inside `44.xyz` — this is the direct, literal answer to
   "these independent agents shouldn't run in the 44.xyz directory."
4. **Make it LLM-driven, eventually, not scripted.** The deterministic
   version (steps 1-3) is the right NEAR-term target — it's provable,
   debuggable, and matches this house's own "harness before hype" law.
   Only after that's solid, swap the fixed script's decision points for
   real calls into topic 2's `gemma_strategy.c`-style dispatch (ideally
   through the shared `bot::*` vocabulary topic 2 §4 proposes, so a test
   agent and a real AI game-player are driven through the SAME primitive
   ops, not two parallel dispatch systems) — this is what turns "a
   scripted UX test" into "an agent that behaves like a real user,"
   which is what was actually asked for.
5. **The knowledge-distillation capstone.** The ask that a long-running
   agent eventually use the editor tools to build ITS OWN game projects
   "that resemble clones of the games we were trying to clone (Dwarf
   Fortress/Civ/Pokemon/GoldenEye)" is real but has two hard
   prerequisites that must land first: topic 1's rtp-xyz/rpg-xyz tooling
   must be provably real (§5d of that companion doc), and topic 2's
   gemma→IQABOD distillation harvester must exist (so the agent has a
   trained model capable of DECIDING what to build, not just executing a
   fixed script). Treat this as the end-state the other two topics are
   building toward, not a near-term deliverable on its own — building it
   before its prerequisites exist would produce exactly the kind of
   "looks like it works" superficial result this whole handoff doc is
   trying to steer away from.
6. **Multiple concurrent agents.** Once 1-3 work for ONE simulated user,
   running N of them concurrently is "just" launching N processes each
   with its own `$HOME` override — the isolation design in §1 is what
   makes this safe. Verify it's actually safe by deliberately trying 2-3
   concurrent agents early (don't wait until you have 10) and confirming
   no cross-talk — this is a real, cheap way to catch a `world_01/
   state.txt`-style race before it's expensive to debug, matching the
   exact bug class `jul-21-gemma-fix.txt` already documents once.

## ✅ 5. Recommended build order

1. `~/xyzos/` starter-install program (§2) — nothing else here can be
   isolated without it existing first.
2. Port the app-store design note (§3) into real code, minimal version
   (catalog + installed_apps.pdl + one install op) — needed so the
   starter install has something real to install beyond hardcoded
   copies.
3. Wrap `test_real_ux_2users.sh`'s pattern into a real continuous loop
   for ONE app first (§4.1) — prove the LOOP mechanism works before
   adding app-chaining complexity.
4. Extend to multi-app chaining (§4.2) — login→character→forum, the
   pieces that already exist and are real today; don't wait on
   rpg-xyz/rtp-xyz or the gemma bug fix to start this part.
5. Move it out of `44.xyz` into `~/xyzos/`-rooted processes (§4.3) —
   at this point you have something worth isolating.
6. Try 2-3 concurrent agents (§4.6) — cheap, catches races early.
7. Only after topics 1 and 2's own prerequisites land: LLM-driven
   decision-making (§4.4) and the knowledge-distillation capstone
   (§4.5).
