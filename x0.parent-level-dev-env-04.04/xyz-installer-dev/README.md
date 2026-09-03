# xyz-installer-dev

Source of truth for shipping the house to other people.

## ✅ The current install pipeline (use this)

| Path | What |
|---|---|
| `tearit-install/` | The **installer** repo contents — `install.sh` (the `curl \| sh` bootstrap), `marketing-install-faq.sh` (shareable read-or-run FAQ), `install-faq.md` (install location / command word explainer), `tearit-legal-v2.md` (user-data/tracking plan), `README.md`. Pushed to **https://github.com/tearitco/tearit-install** |
| `make-payload.sh` | Assembles the curated minimal-desktop payload from the live house tree (`../yz.muchiverse/44.xyz.01.00/`). Output → `build/` (gitignored). |
| `payload-src/` | The non-copied parts of the payload: `bootstrap.sh` (compile in place), `start.sh` (per-root launcher — **not** the global-kill `run_khtpm_strip.sh`), `desktop-config/*.pdl` (trimmed config, loud yellow/red test-build theme). |
| *(generated)* | The payload tree is pushed to **https://github.com/tearitco/tearit-hq-payload** — regenerate with `bash make-payload.sh`, then sync into a clone of that repo and force-push. |

**End-user command:**
```sh
curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- tearit-hq
```

Ships: taskbar + login/signup + cursword + clock. Linux only for now.
Full status: `../yz.muchiverse/#.#.calendar-dox/BOOK/07-install-and-ship/`.

## 📐 Design notes (historical, still referenced)

| Path | Status |
|---|---|
| `dev-doc/04.harnecient-fresh-install-design.md` | The design the pipeline above implements. Payload-layout section still broadly right; retired binary names in it are stale. |
| `dev-doc/01`–`03` | Versioning (gitlet) / rollout channels / hardcoded-path fragility. Future-work references — nothing built. |
| `dev-doc/ha-install-oc.md`, `dev-doc/README.md` | Older index/notes. |
| `xyz-official-documentation/` | Engine/marketing docs from the "xyzos" naming era. Conceptually useful, names/paths pre-rebrand. |
| `#.wussp-in.txt` | Owner's own note-to-self about install priority. |

## ⛔ Superseded / do not use

`_superseded-2026-09/` — the pre-khtpm "install v1" (`xyzos-starter-install.sh`
and friends). Kept for history only; describes `~/xyzos/` and a product
that no longer exists. See that folder's own README.
