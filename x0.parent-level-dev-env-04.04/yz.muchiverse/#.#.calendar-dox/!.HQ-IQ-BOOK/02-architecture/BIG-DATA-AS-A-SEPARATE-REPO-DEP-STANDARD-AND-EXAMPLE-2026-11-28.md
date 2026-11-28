# Big or shared data lives in a separate repo and is downloaded as a dep (standard + worked example, 2026-11-28)

Labels: **OBS** done and checked in this session; **DOC** the rule, not enforced by a test yet. Companion: `16.game/PLUS-PHONE-APP-STORE-AND-APPROVED-REPO-DESIGN-2026-11-16.md` sections 4, 8, 9 (the mechanism). Decision: owner 2026-11-28 ("put the earth data in a separate repo and download it ... for other users install").

## 1. The rule (DOC)
Data a game needs but the code repo should not carry (large, licensed on its own terms, or shared by several apps) is published as a **data package** (`KIND | phone-dep`) in the public approved-apps repo `https://github.com/tearitco/lyfedesk-approved-apps`. An app declares it with `DEP | <id> | <min version>`. The App Store downloads it at a **pinned commit**, verifies a **tree hash**, and installs it under `&.hq-apps/plus-phone/deps/<id>/`. The code repo never tracks the file (one authority per fact: the data repo is the authority, the install is a projection).

Package rules (the store refuses otherwise): files only, no binaries, no symlinks, no `+x` folders, paths inside the package folder; a README with the source and the licence; the original bytes unchanged (so the hash is the proof). Never push a package without the owner; never publish anything whose licence you have not read; the repo is public and history is permanent, so big files are added once.

## 2. How to add one (step by step, OBS: this is exactly what was done for `etopo5`)
1. Work in a **scratch clone** (never beside the house): `git clone https://github.com/tearitco/lyfedesk-approved-apps`.
2. Make `<id>/` with the data file(s) and `README.txt` (what it is, its source, its licence, any restriction).
3. Hash it with the store's own tool and add the catalog row: `&.hq-apps/app-edit/projects/appstore/ops/+x/store_op.+x treehash <id>` gives the sha256; append `ENTRY | <id> | KIND | phone-dep | VERSION | 1 | SHA256 | <hash>` to `catalog.pdl`. (The same hash by hand: `(cd <id> && find . -type f | sed "s#^\./##" | LC_ALL=C sort | xargs sha256sum | sha256sum)`.) **Control:** run `treehash` on an existing package (`bible-verses`) and check it reproduces its published hash.
4. Commit as `tearitco` (not a personal identity) and, with the owner's go, push: `gh auth switch --user tearitco`, push, `gh auth switch --user jbro885wgu`.
5. **Verify from GitHub, not from your clone:** fresh `git clone`, `git checkout <new commit>`, `treehash <id>` must equal the catalog row, and the file's sha256 must equal the original's.
6. Move the pin: edit `STORE_SOURCE | git | <url> | <40-hex commit>` in `&.hq-apps/plus-phone/store_sources.default.pdl` to the new commit. The store never follows a branch; a release moves the pin.
7. Make the consumer read the dep path (see the example), declare `DEP`, add a harness case with a control that fails when the dep is missing or its hash is wrong (`appstore_dep` is the pattern).

## 3. Worked example: the Earth elevation grid (OBS)
- **What:** NOAA ETOPO5, 4320 x 2160 signed 16-bit values, 18,662,400 bytes, sha256 prefix `471d3dd534144aa9`. Public domain (not subject to copyright, 17 U.S.C. 105); **not for navigation**.
- **Before:** `#.NNEST_ASSETS/rta-sources/earth-moth/ETOPO5.DAT` at the repo toplevel, untracked, so a fresh clone had no Earth map (`civ_op` refuses with "the ETOPO5 earth data is missing"; `earth_moth_op` fails).
- **Now:** package `etopo5/` (`ETOPO5.DAT` + `README.txt`) in the approved-apps repo, catalog row `ENTRY | etopo5 | KIND | phone-dep | VERSION | 1 | SHA256 | 51259b0948363a6bf4dad2501db41a574e14c316561b3f33259d5e8afe03d450`, commit `147f21128a2b4ad727498f1cad2c12ca09a1b4ea`, which is now the pin in `store_sources.default.pdl`. A fresh clone from GitHub reproduced the hash and the file byte for byte; `.git` of the clone is 13 MB (git compresses it).
- **The precedent:** `bible-verses` (commit `1204e7e`, owner OK 2026-11-16), same mechanism; `treehash` reproduced its published hash as the tool's control.

## 4. What is NOT done (DOC, open)
- **The consumers do not read the dep path yet.** `@.apps/civ-snes/ops/civ_op.c` (`etopo_path`, around line 127) and `@.apps/earth-moth/ops/earth_moth_op.c` read only `#.NNEST_ASSETS/rta-sources/earth-moth/ETOPO5.DAT`. Fix: try that path first, then `&.hq-apps/plus-phone/deps/etopo5/ETOPO5.DAT` (house-relative); build on debil; run `civ_snes_setup` and the earth case with the first path hidden (control: both paths missing still refuses with the same words). A customer on a fresh install gets the file only after this lands.
- **The dep installs under a phone folder** (`plus-phone/deps/`); a game outside the phone reads across it. If more game data follows, generalise the dep root once, not per app.
- **No automated install test for `etopo5` yet:** the install path itself (a real `sync` + `install` of the dep in a scratch house) was not run for this package; the hash and bytes were verified from a fresh GitHub clone only. Run `appstore_dep`-style proof before telling a customer it works.
- **History identity:** older commits in that repo carry a personal identity; the scrub in the design doc (section 10) is still the owner's decision. New commits use `tearitco`.
- The repo's pin moves with every package: commit the pin file change in the code repo with the message that names the new commit.

## 5. External references
`https://github.com/tearitco/lyfedesk-approved-apps` (pin `147f21128a2b4ad727498f1cad2c12ca09a1b4ea`); NOAA NCEI ETOPO5 metadata; harness cases `appstore_local`, `appstore_git`, `appstore_dep`; store op `&.hq-apps/app-edit/projects/appstore/ops/store_op.c`.
