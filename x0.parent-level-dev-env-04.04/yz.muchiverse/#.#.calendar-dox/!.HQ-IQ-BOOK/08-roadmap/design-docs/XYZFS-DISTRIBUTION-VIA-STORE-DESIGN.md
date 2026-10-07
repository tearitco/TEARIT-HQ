# Distributing xyzfs through a GitHub repo and the store ("download and install from 13.store") (design, 2026-10-07)

Owner (2026-10-07): "later we're going to want to upload xyzfs/ to a github repo and allow users to download and install it from 13.store, so we can just solve all this." ("All this" = the 2026-10-07 problem: `xyzfs/users` was not tracked, other machines were missing it, and copying it needed ssh/tar by hand.)
Status: **DESIGN only.** Today: `xyzfs/users` stays untracked/ignored on `claude`; the owner decided (2026-10-07) it is dev data and wants it shared, but the permission layer blocked tracking/pushing it from an agent session, so the interim path is `install-xyzfs-users.sh` over ssh (done for the Mac `MMEST.3000`; debil dry-run passed). **`13.store` was not found in the house** (no folder or doc by that name); this doc treats it as the store/marketplace described in `HARNESS-STORE-AND-SCORING-DESIGN.md` and `PLAY-ECONOMY-POT-FAUCETS-DESIGN.md`. Open question 1 asks what it is.

## 1. What xyzfs contains (so the packaging matches reality)
`xyzfs/_seed/` starter files (`seed-user.sh` copies them if missing: public-safe), `xyzfs/session.pdl`, and `xyzfs/users/<uuid>/` per user: pals (entities with glyphs, menus, event packages, histories, phones/chats), sessions (books, pages), harnesses, and wallets. At 2026-10-07: 24 users, 2,425 files, 4.7 MB compressed. Debil's copy: same 24 ids, 1,749 files (older).

## 2. Three kinds of distributable (do not ship them the same way)
1. **Seed pack** (public): the starter desk any new user gets. Safe for a public repo.
2. **User pack** (private by default): one user's folder as a pack. Contains wallets, chat/phone histories and personal entities. Published only by that user's consent, to a **private** repo or direct LAN transfer, never to a public repo by default.
3. **Content packs** (store items): entities, pages/books, harnesses, rule sets, music moods, quests. Each is a small pack with a manifest, listed in the store. This is the real "13.store" content (harness store = sellable, scored harnesses).

## 3. Pack format (one format for all transports)
`<name>-<version>.tar.gz` + `<name>-<version>.manifest.txt` (path, size, mode, sha256 per file, like `game_snapshot_op` manifests) + `<name>-<version>.archive.sha256` + `pack.pdl` (`PACK | name | version | kind=seed|user|content | requires | sensitive=0|1 | description`). Transports: (a) GitHub repo releases (assets), (b) the hq-ftp LAN protocol (`HQ-FTP-LAN-SYNC-SPEC.md`), (c) the local `user/*` data branches, (d) ssh/scp today. The format is identical, so the installer is too.

## 4. Installer (generalize what exists)
`$.crypts/install-xyzfs-users.sh` already: verifies the archive checksum, accepts only `users/...` safe paths (no `..`/absolute), refuses names that collide on a case-insensitive disk, shows a plan by default (dry run), never overwrites existing users (`--replace` moves aside to `users.bak-<time>`), checks the file count after unpacking, starts nothing. The store installer adds: pick a pack and version, per-user selection (not all 24), a `sensitive` confirmation list, **merge instead of replace** for existing installs (append-only ledgers merge by line union; conflicts keep both: same rules as `HQ-FTP-LAN-SYNC-SPEC.md` section 8), and a ledger row per install. Same Mac/Unicode hazards (spec section 9).

## 5. Safety (carry over the lessons of this week)
- **No secrets in packs**: provider keys (`raw_*.txt`, `*api_key*`), `.env`, private keys; wallets only with `sensitive=1` and a confirmed list. A pack builder runs the same compiled deny-list as hq-ftp and refuses to produce a pack containing a denied file.
- **Private repo first.** Pushed history cannot be fully removed; publish packs as **release assets** (replaceable/deletable) rather than as commits where possible.
- **Consent per user**: the owner of a user folder approves publishing it; guests' data is never published automatically.
- **Integrity**: checksum and manifest verified before install; later, signed manifests.

## 6. Joint contract (default autonomy 0)
`store.pack.max_mb`, `store.keep_versions`, `store.install.mode` (replace-aside | merge), `store.require_sensitive_confirm`. **Grow**: a pack grade from verified installs `(ok+1)/(ok+failed+2)` (bad installs lower it); **Heal**: failed install leaves the target untouched (the installer already refuses before writing); a half-unpacked tree is detected by the count check and moved aside.

## 7. Build order
1. Pack builder (`xyzfs_pack_op`: manifest + deny-list + checksum) with a pal harness (deny-list, case-collision, Unicode, determinism, tamper). 2. Generalize the installer (merge mode, per-user selection) with a harness on scratch trees. 3. A private GitHub repo for packs and release-asset upload (owner creates the repo). 4. Store listing (pack metadata in the harness-store index) and the store window. 5. Seed pack as the first public pack. **Not verified: all of this is design.**

## 8. Open questions for the owner
1. What exactly is **13.store** (not found in the house)? The harness store, a window, a folder to be created, or a separate repo?
2. Public or private GitHub repo for user packs; are wallets ever allowed in a pack?
3. Should existing installs (debil, Mac) be MERGED forward or REPLACED aside when a newer pack arrives (suggested: merge for ledgers, keep-both conflicts)?
4. Do guests' folders (`guest-*`) ever get published?
