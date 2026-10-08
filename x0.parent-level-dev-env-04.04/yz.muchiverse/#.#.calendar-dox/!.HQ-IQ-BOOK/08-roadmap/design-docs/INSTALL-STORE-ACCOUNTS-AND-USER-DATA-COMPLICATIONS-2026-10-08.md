# Install, store, accounts and user data: the complications this house keeps hitting (2026-10-08)

Written by Claude. Companion to the Grok handoff. The earlier docs describe each piece alone (`USER-DATA-BRANCHES-DESIGN.md`, `XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md`, `07-install-and-ship/`, the xyz-installer-dev README). **None of them says how the pieces fight each other, or lists what has actually gone wrong.** This does. Marks: ✅ built and checked, 🟡 partial, 📐 design only, ❌ missing. Anything not checked is labelled **unverified**.

## 1. The four things that have to coexist

1. **Code** (git, many agents, many branches).
2. **Compiled programs** (not in any branch; built on each machine).
3. **User data** (entities, histories, phones, wallets, books, pages, API keys: live, private, constantly changing).
4. **Content packs** (what a store would sell or share: entities, harnesses, quests, music moods, rule sets).

The recurring bug is treating any of these as another one of them.

## 2. Install today

- ✅ `curl -fsSL …/tearit-install/main/install.sh | sh -s -- tearit-hq` fetches a **payload** from `github.com/tearitco/tearit-hq-payload`. The payload is assembled by `xyz-installer-dev/make-payload.sh` from the live house, then synced and pushed by hand.
- It ships taskbar, login/signup, cursword and clock. **Linux only.** Everything in the roadmap games is *not* in the payload.
- `make-payload.sh` creates a fresh `xyzfs/` skeleton with `mode | guest` and strips dev-only state from the login app. It excludes `.git`, archives and test folders. **Unverified:** whether it can ever copy a key file or a user folder (I read the exclude list; there is no explicit exclusion of `&.widgits/open-hai/state/*`, which holds provider keys). Check before the next payload push.
- Compiled programs are in no branch. A fresh clone or install must run `sh '$.crypts/button.sh' build`.

## 3. Accounts and users today

- `xyzfs/session.pdl` records the current login (`logged_in`, a user id). There are **24 user folders** under `xyzfs/users/`, named by uuid, plus `guest-<8>` guests.
- `seed-user.sh` (run by `button.sh build`) copies the tracked starter content in `xyzfs/_seed/` into the *current* user's folder, **never overwriting**, and never seeds identity or runtime files (entity uids, histories, phones) so two installs cannot share an identity or a phone number.
- 🟡 Login/signup exists (it ships in the payload). ❌ There is no account recovery, no migration of a user between machines, no password or key policy documented, and no way to say which user a running game belongs to when two are logged in on one machine. **Unverified:** how the login app maps the session user id (e.g. `claude-0001`) onto the uuid folders; I did not read that code.

## 4. User data and git

- User data **was** tracked in some branches. On 2026-10-06 `claude` stopped tracking `xyzfs/users`; each user's files now live on **local orphan branches** (`user/jb`, `user/<uuid8>`, `user/guest-<8>`; 24 verified). `sh '$.crypts/button.sh' save-data` commits them with plumbing, never touching your worktree, and runs after `quit` and `reset`.
- Other branches (`opencode`, `kilo`, `grok`, `main`, `debian*`) may still track those files. **Switching a live checkout from such a branch to one that does not deletes the files from disk** (reproduced: all 1,725 files; the same mechanism deleted the owner's API key files). Merge in a scratch worktree, move the live tree only with a verified backup.
- `user/*` branches contain wallets and private chat history. **They are local only. Never push them.**
- The owner decided user dev data should be shareable, but the permission layer blocked tracking/pushing it from an agent session, so the interim is the local branches above.

## 5. Things that have actually gone wrong (so you don't re-learn them)

| Date | What happened | Lesson |
|---|---|---|
| 2026-10-05 | A bulk script/branch switch wiped the working tree: **31,571 files** | After any reset/switch/bulk script run `git ls-files --deleted | grep -v /pieces/sessions/ | wc -l`; more than a few dozen means stop |
| 2026-10-06/07 | Tracking → untracking user data deleted tracked files and API-key files on checkout | Never move a live checkout across branches with different tracking of `xyzfs/users` |
| 2026-10-07 | A disk-full emergency produced the `debian` branches' "emergency save" of per-user data | Keep a tarball + sha256 of `xyzfs/users` outside the repo before any bulk operation |
| 2026-09-26 | Two agents shared one index; one's staged 254-file rename was committed by the other | Commit by explicit pathspec; one worktree per agent |
| 2026-10-08 | `reset` left the desktop down because a stale shadow copy of a header broke the build | A failed build in `reset` means no desktop; the build guard now refreshes shadow copies |
| 2026-10-08 | Stop game deletes entity folders; Start makes new identities | Save a slot first; prefer Pause |
| ongoing | Eden history grows ~1 MB/hour with no cap and the disk was 93% full | Retention policy needed (design written) |

## 6. Why a copied or installed desk breaks (measured)

- **Absolute paths are baked into generated files.** `eden_button/ctl.sh` sets `G=` to the full `/home/no/Desktop/github/work/NNEST-12.00/…` path; `#.desktop/ai_instances_registry.txt` has absolute paths for entities. Move or install the house anywhere else and these point at the wrong place. Installers and generators must write paths relative to the house root or resolve at run time from `argv[0]`.
- **Entities carry identity** (`entity_uid.txt`, hash, phone number). Copying an entity folder between desks duplicates identity; the spawn and seed code is careful about this, so *any pack format must regenerate identity on install*.
- **Cross-machine is not real yet.** `palnet_peer.c` hardcodes `127.0.0.1`. macOS has known portability gaps (e.g. the Eden daemon-stop `/proc` check refuses there).
- **The store is data only.** `&.widgits/store/catalog.pdl` lists items; there is no pack/unpack op and no install/uninstall. `XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md` §2 defines three kinds, which must never ship the same way: **seed pack** (public, safe), **user pack** (private, consent, wallets inside, private repo or LAN only), **content packs** (small, with a manifest, the real store items).
- **Harnesses as store items** (scored, sellable) is design only (`HARNESS-STORE-AND-SCORING-DESIGN.md`) and depends on a chain signing gate that is not built.

## 7. What the games need from this layer

| Game need | Install/store/accounts piece | State |
|---|---|---|
| A new player gets a working starter desk | seed pack + `seed-user.sh` + login | 🟡 |
| A game (Eden, Pokemon…) installs as a pack | content pack with manifest; regenerate identity; relative paths | ❌ no `xyzfs_pack_op` |
| Two players in one auction | two users, one transport, one ledger | ❌ transport hardcoded to localhost |
| Wallet / price on the chain | per-user wallet in user data; signing gate | 🟡 wallet files exist; signing gate ❌ |
| Save/load a player's game | slots under the user's folder; restore model | 🟡 v1; owner must decide restore |
| Share a character without leaking data | user pack with consent | 📐 |

## 8. Build order for this layer (each step: harness with a mutant first)

1. **Relative-path audit + fix** in generators (`install_eden`, registry writer): a harness that installs into `/tmp/x` and greps the result for the source house's absolute path. (tier W/M)
2. **Payload leak check**: a harness that runs `make-payload.sh` into a scratch dir and fails if it finds a key file, `wallet.txt`, or any `xyzfs/users/<uuid>` content. (W)
3. **`xyzfs_pack_op`**: pack and unpack a content pack with a manifest, regenerating identity, dry-run default, on a scratch tree. (M)
4. **Backup op**: tarball + sha256 + count of `xyzfs/users` outside the repo, run before bulk operations (the house rule, currently manual). (W)
5. **Transport**: make the peer address configurable, then verify two machines. (M)
6. **Account basics**: documented session/user mapping, then a migration path for a user between machines. (needs owner design)

## 9. Owner decisions

Whether the store may sell harnesses; the restore model for save slots; who may publish a user pack and where; whether keys ever leave a machine (my recommendation: never, each machine gets its own); the chain signing gate.
