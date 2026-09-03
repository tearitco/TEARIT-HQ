# 🧭 PHONDO_INSTALL_IDEAS.md — install, store, shipping-the-house pipeline

> **Status: EXPLORATION, 2026-09-01.** Not a plan yet. Purpose: get the
> user and the agent on the same page about install / "GitHub app
> store" / shipping-as-a-pipeline before any design doc gets written.
> User's own words going in: *"very confused and inexperienced about"*
> install — so this doc leads with **plain-language current state**,
> then open questions, not a proposal to approve.

## 1. What "install" currently means in this house (plain language)

Today there is **no single real installer that works**. There are two
separate, both-incomplete things wearing that name:

1. **`xyzos-starter-install.sh` ("install v1")** — OLD, predates the
   current khtpm taskbar entirely. It installed two standalone apps
   (login-signup + avatar-creation). Marked **obsolete** in the repo's
   own docs. Still has real proof it once worked (`%.harnesses/
   install-xyzos/proof/`), but proves the wrong product now.
2. **HARNECIENT fresh-install** — a **design document only**
   (`x0.parent-level-dev-env-04.04/xyz-installer-dev/dev-doc/
   04.harnecient-fresh-install-design.md`), dated 2026-08-12, status
   explicitly "**code not started**". It describes, on paper, a
   `harnecient-install.sh` that would: copy the taskbar + entity
   window + login/signup app + a blank `xyzfs/` into one self-
   contained folder (default `$HOME/harnecient/`), compile everything
   in place, and give you a terminal command (`harnecient`) to launch
   it. **This is the plan, but no code exists for it yet.**

Also stale right now: `xyz-installer-dev/pointers.pdl` — the file
that's supposed to say "where does the real dev tree live" — still
points at an ancient folder name (`44.xyz❤️‍🔥️00.10`, not even last
week's `44.xyz❤️‍🔥️00.17`, let alone today's freshly-renamed
`44.xyz.01.00`). Nobody has touched it in a while. Real, concrete
proof that "install" has been dormant since well before today's path
migration.

**So: there is currently no way to hand this house to a second real
user on a second real machine and have it work.** That's the honest
starting line.

> **UPDATE 2026-09-02:** there is now — a first-cut CLI pipeline
> (`tearitco/tearit-install` + `tearitco/tearit-hq-payload`) installs a
> minimal desktop (taskbar + login/signup + cursword + clock) via
> `curl … | sh`, verified end to end. Much of the exploration below
> (store, versioning, multi-machine accounts) is still unbuilt; §5's
> decisions still hold. Source: `xyz-installer-dev/` (see its README).

## 2. What "versioning" currently means (also plain language)

There's a separate draft, also **strategy-only, not wired**:
`01.versioning-with-gitlet🌳🪵️.md` + `02.rollout-channels-alpha-beta-
stable.md`. The idea: use a tiny, already-in-house, from-scratch git
clone called **gitlet** (822 lines, no deps) to track *which version
of which app is on which channel* (alpha/beta/stable) as small
"descriptor" files (`catalog.pdl`, `installed_apps.pdl`, per-app
`RELEASE.pdl`) — NOT to version the actual app code (gitlet can't
really do whole-tree versioning well; real git already does that,
here in the actual TEARIT-HQ repo). The installer would still be the
thing that copies real files; gitlet would just be the ledger of
"what's the current official alpha/beta/stable pointer."

This is a genuinely different, smaller job than "put things on GitHub
so users can push/pull store items" — see §4.

## 3. What "app store" currently means (plain language — least developed)

Weakest area. What exists:
- `appstore.chtpm` — a **layout file** (the visual/menu shape of an
  app-store screen) in an old, pre-khtpm codebase
  (`1.TPMOS_c_+rmmp.0103.0001/`). Not the current architecture.
- `@.app-store/` — a folder that exists under the current house
  (`44.xyz.01.00/@.app-store`) but its actual contents/wiring haven't
  been read yet as part of this doc — real next step, not yet done.
- `store.chtpm` under `*.START_BUTTON/` — another layout-only artifact.
- Nothing found yet that actually fetches an app/toy/pal FROM a
  network source (GitHub or otherwise) INTO a running house. Every
  real "install" mechanism found so far is dev-tree → local-machine
  copy, not remote-repo → local-machine pull.

**In short: "app store" today is a couple of unused UI layout mockups
and an empty-ish folder, not a working pull-from-network mechanism.**
This is the part of your message ("pulling more toys/pals from a
network/local repo mirror", "portioning out the github so users can
push/pull store items... or a dedicated repo for approved store
items") that has the least existing groundwork — closer to a blank
page than the installer/versioning pieces above.

## 4. What's NOT designed yet at all (gaps, not claims)

- **Signup/login as part of a real multi-user product**, as opposed
  to a single-dev-machine account system. The current login/signup
  app (`0.user-pal👤️/00.login-signup/`) works house-locally; nothing
  here yet addresses accounts across machines, a real backend, or
  auth beyond one local `users/` folder.
- **Any network transport at all.** Everything real and working today
  (install v1's proof, the HARNECIENT design, gitlet's `push`/`pull`)
  is **local-path only** — no HTTP, no real GitHub clone/fetch, no
  server. gitlet's own doc says this outright: "remotes are local
  paths; that's a feature (offline), not a bug."
  **Direct implication of your message**: pulling from "a network /
  local repo mirror" and pushing "store items" to GitHub as individual
  repos is a materially bigger, different kind of task than anything
  drafted so far — it's the first time this house would need to talk
  to the actual internet as a product feature (not just as an agent
  tool).
- **Any notion of "approved" vs "unapproved" store content**, review/
  moderation, versioning of THIRD-PARTY content (as distinct from the
  house's own first-party apps), or a trust/signing model.
- **Testing pipeline for "does the shipped house actually work for a
  stranger"** — the closest existing thing is the relay-only KPI
  harness pattern (`%.harnesses/install-xyzos/`, and the *planned*
  `%.harnesses/install-harnecient/`), which is real, house-standard,
  and reusable — but it currently tests "did the install produce the
  right files/processes on THIS machine", not "does a totally clean
  second machine/user experience work end to end."

## 5. Real decisions made in conversation, 2026-09-01 (direct from owner)

- **No fixed product name.** "HARNECIENT" and "tearit co" are both
  placeholders, not the real ship name — the name is chosen **per
  customer, at ship time**, because the same house may be shipped to
  different customers for different purposes (white-label). Direct
  consequence: **nothing in the install/build/CLI tooling may hardcode
  a product name** — it must be a parameter, passed in by whoever is
  shipping, every time. This invalidates §11 of the HARNECIENT design
  doc (`$HOME/harnecient/`, `~/.local/bin/harnecient`) as written — the
  *shape* of that design stays useful, the literal names don't.
- **`xyz-installer-dev/` is being kept, not deleted** — real design
  content worth harvesting (payload layout, gitlet versioning draft,
  rollout-channel gates) — but it needs a de-hardcoding pass of its
  own: the folder name, `xyzos-starter-install.sh`'s name, and every
  literal "HARNECIENT" in the design doc all assume one fixed product
  identity that no longer holds.
- **The very first real shippable artifact is a simple CLI program**,
  not a GUI installer — it installs everything else FROM the CLI.
  Design-first, slow pace, deliberately — direct instruction: *"we're
  gonna slow down and discuss the pipeline of user experience of
  install before we really get started."*
- **A `livedesk start`-style CLI entry point must take the product
  name as a dynamic argument** — not hardcoded per-build. The SAME
  name, passed in by the shipping owner, must also be used to:
  - name the terminal shortcut command,
  - name the installed "app" entry in Linux/Mac/Windows' native apps
    listings,
  - name the desktop shortcut.
  Concretely: no more literal strings like `🔐-Livedesk-Start` baked
  into a shortcut/binary — the name is an install-time parameter that
  flows through to every OS-level entry point consistently.
- **Target users, in order, strangers being the real goal**: owner
  first → a friend → strangers off GitHub. All three matter, strangers
  are the actual bar, but testing proceeds in that order.
- **Store pulls from "tearit co" GitHub for now**, but the mechanism
  must be portable — a differently-shipped/white-labeled house may
  point at a different GitHub org/catalog entirely. Don't bake
  "tearit co" in as a constant either.
- **Catalog shape**: mostly one central "approved store items" catalog
  repo, but flexible enough that other users/orgs can register bigger,
  custom "toys" or their own internal specifications — not a fully
  rigid one-repo-only model.
- **Approval gate**: manual review by the owner for now; automated
  (harness-based, matching the house's existing KPI-proof pattern) is
  the explicit later goal, not the starting point.

## 6. Open questions — let's get on the same page before any doc gets written

(Answered live in conversation, not in this file — this file is the
shared map, not the transcript.)

1. **What's the actual shipping unit, right now, this pass?** Just
   the HARNECIENT single-machine installer (finally writing the code
   for the design that already exists)? Or do you want to jump
   straight to network/store concepts? These are very different sizes
   of task.
2. **Who is "a user" in your head right now?** You, testing on a
   second machine/VM? A friend? A stranger who finds this on GitHub?
   The answer changes how much onboarding/safety/error-handling
   actually matters yet.
3. **GitHub-as-appstore — how literal?** Is the idea "toys/pals are
   literally individual git repos a user's install can `git clone`/
   `pull`", or more like "one central catalog repo lists approved
   items, each with its own download location"? These lead to very
   different amounts of new infrastructure.
4. **What does "approved" mean, concretely, if anything yet?** Is
   there a reviewer (you), an automated check (the KPI harness must
   pass), both, neither for now (anything-goes alpha channel)?
5. **How much of the versioning/channel draft (`01`/`02` docs) do you
   actually want to keep vs. was that a different-day tangent?** They
   were drafted, never built — fine to keep, revise, or shelve.
