# The +phone App Store and the repo of approved apps (design, 2026-11-16)

> Owner 2026-11-16 (SAID): the phone's base apps are "settings, appstore"; the app store installs TSC and stock+x; "I want to test installing these from a GitHub repo of approved apps for LyfeDesk"; "eventually the repo will hold all toys/apps not already included in the install, which you can explain in the doc, but we will stick to just phone for now."
> Labels: SAID / READ / DOC (my design, not decided). Written by Claude Sonnet 5.5. Related: `PLUS-PHONE-DESIGN-2026-11-15.md` sections 12, 22, 24 (packages, install, app-edit), `PLUS-PHONE-LAYOUT-SPEC-2026-11-16.md`.

## 1. Scope now and later
| | NOW (phone only) | LATER (SAID: "all toys/apps not already included in the install") |
|---|---|---|
| What the repo holds | phone app PACKAGES (`manifest.pdl`, `icon/`, `screens/`, source `ops/*.c`): True Swords Clash, stock+x, later the WSR app | every LyfeDesk toy / HQ app / widget that is NOT part of the base install, same catalog, same checks; the package format for those is not designed yet (they are not `+phone` packages) |
| What ships in the base install | the shell, `settings`, `appstore` (the two base phone apps) | the house's core (taskbar, renderer, rpg-edit, ...) stays in the install; anything optional moves to the repo over time |
| Installed by | the App Store app on the phone (and `phone_op install` for scripts) | a desktop store window / the same verbs, DOC |
The phone code must not assume "phone" in the catalog and trust layers (a `KIND | phone-app` field per entry), so the later toys/apps are new kinds, not a new store.

## 2. What already exists (READ)
`phone_op install <house> <package_dir>` (runs `app_check` first; refuses on any failure; copies beside then renames; republishes every open phone), `phone_op uninstall <house> <id>` (refuses `settings`), `app_check` (format validator), `app-edit` (the IDE whose projects folder is `&.hq-apps/app-edit/projects/<id>/` and whose export installs). Package = a folder named after the app id. `settings` is a show-only app (PIN first). There is NO store UI and NO remote source today.

## 3. The App Store app (DOC)
A normal phone app (`appstore`, `PIN | first`, system app like settings) with one screen: a list of catalog entries, each a button `install <Title>` or `remove <Title> (installed)`, a status line, and the phone's stack (phone bars, app chrome row, phone bottom bar). It has its own projector (the `tsc_op` shape: a module that publishes `ui.txt`, a few UI verbs `ui_install <id>`, `ui_remove <id>`); the verbs call `phone_op install|uninstall` (fork/exec with a watchdog) and write the result line the screen shows. System apps (`settings`, `appstore`) are never offered for removal.
**Catalog sources (data, so a new source is a row):** (a) `LOCAL`: the app-edit projects folder (stage 1, no network, the test bed); (b) `GIT <url> <commit>`: a git repo at a pinned commit (stage 2). `phone_prefs.pdl` rows `STORE_SOURCE | local` and `STORE_SOURCE | git | <url> | <commit>` (a preference, never state).

## 4. The approved-apps repo and the trust model (DOC; this is a supply-chain surface: installing code onto a desktop)
**Repo layout:** `catalog.pdl` at the root (`ENTRY | <id> | KIND | phone-app | VERSION | <n> | SHA256 | <sha256 of the package tree> | NOTE | ...`), one folder per package (`<id>/manifest.pdl`, `icon/`, `screens/`, `ops/*.c`, a build recipe). **No compiled binaries** (the house rule: compiled programs are in no branch).
**Rules the store enforces, in order; any failure installs NOTHING and says which:**
1. **Pinned commit, never HEAD.** The store trusts a source only at the commit written in the local `STORE_SOURCE` row. "Approved" = the owner (or the owner-designated maintainer) merged the app into the repo AND the pin was updated on this machine. A newer remote commit is only a notice ("newer approved commit available"), never auto-applied.
2. **Fetch exactly that commit** over https (`git fetch` of the sha into a capped cache folder, or the tarball of that sha). Read-only: a plain GET, no login, no identity sent. A private repo needs a token: DESIGN ONLY, not built; the token would live in the user's own secret file, never in a log, a commit, or a doc.
3. **Verify** `SHA256` of the package tree against `catalog.pdl` (the catalog is part of the pinned commit), reject symlinks and paths outside the package folder.
4. **Reject prebuilt binaries** (any file that is ELF/executable in `ops/+x/`); only source is accepted.
5. **`app_check`** (the existing format validator).
6. **Build locally** with the house toolchain through `cpu_gate` (one heavy job at a time, niced), only the recipe in the package (`ops/build_<id>.sh`, same shape as `build_tsc.sh`); the recipe runs only after the owner sees the first install of that app version (a prompt, DOC) because compiling and running a package IS running its code.
7. **Install** with `phone_op install` (atomic), record a ledger row `INSTALL | id | version | commit | sha256 | ts` in the store's own ledger (the ledger is the authority for "what was installed from where"; the folder is a projection).
**Not covered, said plainly:** a malicious but correctly hashed package passes steps 1 to 5 because the owner approved it; the defence is review before pinning, and building/running only what you approved. There is no sandbox for a phone app today (it runs as the user).

## 5. Test plan (harness cases that can fail; pal + case + C ops; NO Python)
`appstore_local`: scratch house, the REAL `phone_op` and `store_op`, a fake projects folder with two packages: the catalog lists both, install makes the app appear in the picker list and its files in `apps/<id>/`, remove deletes them, `settings`/`appstore` are never offered for removal, a package that fails `app_check` is listed as refused with its reason and installs nothing, the screen buttons run the real verbs (control: a direct `phone_op install` gives the same result).
`appstore_git`: a LOCAL git repo built in the scratch folder (`file://` or a path), same flow: a pinned commit installs; a repo whose HEAD moved after the pin still installs the PINNED version (control: the new commit's content is NOT present); a tampered file (hash mismatch) installs nothing; a prebuilt binary in the package is refused; a symlink out of the package is refused; a missing commit is refused with a clear message; the network is never touched by the case.
Live: photographs of the picker with the store tile, the store screen, an install, the new tile; then ONE real fetch from the real GitHub repo once the owner has created it and put its url and commit in the prefs (the owner creates and pushes the repo; I do not).

## 6. Build order
1. (this turn) `appstore` app with the LOCAL catalog; a minimal `stock-x` package so it is listed; cases `appstore_local`.
2. The git source with the pin and checks (`appstore_git` on a local repo).
3. The repo folder prepared for the owner to push (`catalog.pdl` + packages); the real-GitHub test.
4. LATER: other kinds (toys, apps) in the same catalog.

## 7. Open questions
Public or private repo (private needs a token design); where the owner wants the first-install prompt (on the phone or in a desktop window); whether app versions can be pinned per app or only per repo commit (default: per repo commit); update flow (the notice only, for now).
