# _superseded-2026-09 — old install code/docs (DO NOT USE)

These are the pre-khtpm "install v1" artifacts. They are kept only for
history. **They do not work with the current house** and describe a
product that no longer exists (`~/xyzos/`, two standalone apps, an
`app-store/` folder layout).

The real, current install pipeline is one level up:
`../tearit-install/` + `../make-payload.sh` + `../payload-src/`.
See `../README.md`.

| File | Was | Why dead |
|---|---|---|
| `xyzos-starter-install.sh` | install v1 — copied `00.login-signup` + `01.avatar-creation` into `~/xyzos/apps/` and compiled them | Predates the khtpm taskbar entirely. The product is now the whole livedesk desktop, installed by `tearit-install`. |
| `pointers.pdl` | source-tree pointer for v1 | Points at `44.xyz❤️‍🔥️00.10` — two renames stale (now `44.xyz.01.00`). Nothing reads it. |
| `user-quick.txt` | "test the install in 2 minutes" quick guide for v1 | Describes `~/xyzos/` + KPI#4/#5. Wrong product. Current quickstart: `../tearit-install/install-faq.md`. |
| `xyz-official-documentation/for-users/01.install-and-first-login.md` | user-facing install guide for v1 | Tells users to run `xyzos-starter-install.sh`. Actively wrong. |

Still live (NOT moved here) elsewhere in `xyz-installer-dev/`:
- `dev-doc/04.harnecient-fresh-install-design.md` — the *design* the new
  pipeline implements; payload-layout section still broadly accurate
  (retired binary names in it are not — see the house book's
  `06-testing`/handoff notes).
- `dev-doc/01`–`03` — versioning / rollout-channel / path-fragility
  notes, still useful as future-work references, nothing built yet.
