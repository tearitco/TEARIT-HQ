# Games as packages on the engine, and the Store they ship through (design, 2026-10-27)

**Status: DESIGN. Nothing here is built.** Owner words, 2026-10-27: "we're gonna make whole games on the engine, swap parts, ship demos, modify them based on insights... in a user shared folder for now with all users having permission to use, since we're just in dev, but when we start letting in outside users they won't be able to access those folders, we may make them available through 'store'." And on the store: "changing games, online platforms, Steam/PSN... we will show store to look more like Steam, with each game you downloaded, games for sale, all cool looking, reviews etc., and it will look like an x11-hq window, not a dropdown; toys are truly for devs/betas, think of them as solitaire etc." "For now store items were meant to be stored in a GitHub repo specifically dedicated to these, and/or separate user repos in future; they will be screened for compatibility and safety before install."

Labels: **FACT** (read/observed), **PROPOSAL** (mine, owner decides), **OPEN**. Companion to `HANDOFF-2026-10-27-AI-SIDE-COMPANION.md` (section 10 item 5 `skin_port`, section 9 spawn policy) and an extension of `08-roadmap/design-docs/XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md` (read in full; its sections 9-10 already fix the pack format, deny-list, `owned.pdl`, `checkout-hq`, escrow, `role=dev`).

## 1. The vision in one paragraph
The engine (board-viewer + the petslike core + pc-hq books + events + pal harness) is a **voxel game-making engine**. A *game* is a bundle of parts a creator can swap (world generator, life rules, pet roster, ship, layout, event pages, assets). A creator ships a **demo**, watches how it is played (ledger rows, with consent), and **modifies the game from those insights**; the AI loop (companion doc) proposes the changes, a person promotes them. Games reach players through the **Store**, a Steam-like window. During dev everything lives in one shared folder every dev user can use; outside users never see that folder and get games only through the Store, after screening.

## 2. What a "game" is (PROPOSAL, grounded in what rpg-space already is)
FACT: rpg-space is already a bundle: `@.apps/rpg-space/` holds the skin C (`ops/rpg_space.c`, built on `petslike_core.c`), data (`space.pdl`, `party.pdl`, `tuning.pdl`, `life.pdl`, `scale.pdl`, `asteroids/`), events, board layout (`board/pchq-board.xhtpm` + css), a pc-hq book (`rpg-space-pchq/`), and a pal gate (`play/phases/*.phases.pdl`). rpg-dust is a copy.

**Manifest** `game.pdl` (house pipe format), one per game:
```
GAME  | rpg-space | 0.3 | engine>=petslike-1 | kind=game|demo | owner=<uuid8>
PART  | skin      | ops/rpg_space.c        | provides=skin:petslike | requires=core:petslike
PART  | world     | space.pdl,worlds.pdl   | provides=world:planets | requires=skin:petslike
PART  | life      | life.pdl               | provides=life:basic    | requires=core:life
PART  | pets      | party.pdl,pals.pdl     | provides=pets:4
PART  | layout    | board/                 | provides=layout:board
PART  | assets    | asteroids/,art/        | provides=assets:voxel
GATE  | play/phases/rpg_space_planet_map.phases.pdl | checks=83
BUDGET| idle_permille=20 | active_permille=150     (measured, see section 7)
DEMO  | cut: world=first-2-planets, pets=1, life=off, gate=demo.phases.pdl
```
**Swap = replace one PART with another that `provides` the same slot and satisfies `requires`.** The compatibility check is mechanical (section 6): contracts match, then the game's gate passes in a scratch copy. A **demo** is the same manifest with a `DEMO` cut (fewer parts, a smaller gate); a **mod** is a manifest that overrides parts of a base game and names it in `base=`.
FACT-based consequence: the `skin_port` op (companion section 10 item 5) and the common-skin extraction (section 11) are the first steps, because today swapping a part means editing a sed-copied C file.

## 3. Where games live during dev: a shared folder (PROPOSAL)
FACT: per-user data is `xyzfs/users/<uuid>/` (gitignored, on local orphan `user/*` branches, never pushed; AGENTS.md). Seed starter files are `xyzfs/_seed/livedesk/`. **No shared dev folder exists today**, and **nothing enforces permissions or `role=`** (`meta.txt` `role=dev` is only a marker; XYZFS design section 9, re-confirmed).
* **Location:** `xyzfs/shared/games/<game>/` (a sibling of `users/`, not under it, so game bundles are not mixed with private desks and wallets). Shared **content/code stays out of user branches**; it is a normal tracked folder or its own repo (OPEN: which).
* **Access as data**, `xyzfs/shared/access.pdl`:
```
ACCESS | games | role:dev   | rw   # dev phase: every dev user
ACCESS | games | role:user  | none # outside users: no direct access
ACCESS | store | role:user  | read # only through an installed copy from the Store
```
Dev phase = a single `ACCESS` row opens it to all accounts we run; the later switch to outside users is changing rows, not code. **Enforcement is not built**: to be real, the installer, the launcher and `game_*` ops must read `access.pdl` and the user's `meta.txt` role; until then it is a convention, and the doc must not claim protection.
* **Rule that survives the switch:** the installed copy of a game is **copied into the player's own space** (`xyzfs/users/<uuid>/games/<game>/`); a player never executes out of the shared folder. Save data and wallets stay in the player's folder.
* Shared games must not contain secrets, wallets, user histories (the existing deny-list applies; section 6).

## 4. The Store as a Steam-like window (PROPOSAL; owner direction)
FACT: today the Store is the inert header cell 13 (`strip-cell-13`, label "store") and `&.widgits/store/catalog.pdl` (12 lines, `status` = planned/data-ready; **nothing installable**; README: "Nothing here installs anything yet"). XYZFS design section 10 already plans a catalog list and a `checkout-hq` window.
**Change of direction (owner 2026-10-27):** the Store is a full **x11-hq window** (`store-hq`: a `.xhtpm` layout + css + one manager, nav-numbered and relay-drivable like every HQ window; generic renderer only, **no new per-project renderer C**; see `khtpm-house-standards`), not a dropdown. The taskbar cell opens the window. Views:
| View | Shows | Data source |
|---|---|---|
| **Library** | every game the user installed/owns, play, update, uninstall, "installed" badge | `owned.pdl` (exists in design) + `installed_apps.pdl` |
| **Store** | games for sale and free games, capsule art, price, tags, rating, screening badge | `catalog.pdl` (extend rows) |
| **Game page** | description, screenshots, version history, requirements, **reviews**, the compatibility and safety result | the pack's `store.pdl` + `reviews.txt` |
| **Toys** | the dev/beta tools: small things like solitaire, kept apart from games, visible to `role:dev` | existing `toy.pdl` scan of four roots |
Owner's rule recorded: **toys are for devs and betas** (solitaire-like), not the product; the product is games. Checkout reuses the planned `checkout-hq` (free path first; escrow on a test chain later; card payments "not available").
**Platform framing (owner: "changing games, online platforms, Steam/PSN"):** treat the Store like a platform layer: a catalog of games from several sources. OPEN: whether a later "source" adapter ever lists games from outside platforms; nothing in this doc designs that.
**Reviews (new, PROPOSAL):** an append-only `reviews.txt` per game: `REVIEW | user_id | version | stars | text | ts`, one per user per version, the text capped, no personal data. A review is user content: it needs moderation (OPEN: owner or a rule) and must not feed the AI bank without the same describe-never-classify rule. The review count and average are derived, never typed.
**Footprint:** the window must be idle-cheap (reads files on open and on marker growth, no polling loop); measure with a browser open before shipping (AGENTS.md coexistence rule).

## 5. Where store items are kept (PROPOSAL)
Owner: a GitHub repo **dedicated to store items** now; separate **per-user repos** later.
* **Now:** one dedicated repo (name OPEN; `tearit-co/store-items` is my placeholder, not decided). Layout: `<game>/<version>/{pack.tar.gz, game.pdl, store.pdl, manifest.txt, archive.sha256, screening.txt, art/}`. Prefer **release assets** over commits for the archives (replaceable and deletable; the existing design's rule).
* **Later:** a catalog row points at a repo: `ITEM | id | name | kind | version | price | requires | source=repo:<owner>/<name>@<tag> | status | note`. Each creator's repo is a **source**; the Store lists items from the sources the house trusts.
* The Store never installs straight from a user repo: it **fetches into a quarantine folder, screens, then installs** (section 6). The first network feature needs a harness against a local `file://` repo (XYZFS design 9, step 4).
* The existing `catalog.pdl` `root` column (relative to the house root) becomes `source` for remote items; `status` gains `screened`.

## 6. Screening before install: compatibility and safety (PROPOSAL)
Every item passes a **pipeline whose result is data** (`screening.txt`, written by the screener, never by the item):
1. **Integrity:** archive checksum and per-file manifest match; safe paths only (no `..`, no absolute, no case collisions). FACT: `install-xyzfs-users.sh` already does these.
2. **Deny-list:** no provider keys, `.env`, private keys, wallets, histories (the compiled deny-list shared with hq-ftp and the pack builder).
3. **Manifest/contract compatibility:** `game.pdl` parses; every `requires` is met by the installed engine version and parts; the declared engine version is supported.
4. **Gate in a scratch copy:** the game's own `GATE` runs (pal harness, scratch copy, the real binary) and must PASS; a **negative test** that the gate can fail is required (AGENTS.md "verify a passing assertion can fail"). Dev runs must not feed the AI bank (companion section 12).
5. **Safety scan of code and data:** see the next list.
6. **Budget:** measured idle and active cost (`tick_stats.txt`, `cpu_permille`) must be within the declared `BUDGET` and a store-wide cap, with a browser open.
7. **Result:** `screening.txt` rows `SCREEN | check | PASS|FAIL|WARN | detail`, a verdict (`screened` / `rejected` / `needs_review`), and who signed it. A person promotes `needs_review`. No auto-approval for code that executes.

**The hard question (OPEN, owner):** games contain C (the skin) and `.sh`/`.pal` event pages, i.e. **executable code**. Static scanning cannot prove arbitrary C safe. Options, strongest first:
* **A. Data-only for outside creators:** outside users ship parts that are **data and pal/event pages only** (rows, models, layouts, art), run by the already-trusted engine and skin; only the house (and trusted dev users) ship C. Pages run inside the pal VM and the verb surface, so the capabilities can be allow-listed (the same way `auto` manifests allow-list verbs).
* **B. Source-only with a house build:** C is accepted as source, reviewed by a person, built by the house, never shipped as a binary.
* **C. Binaries from trusted devs only.**
Recommend **A for outside creators, B/C for trusted devs**, with a capability allow-list per pack (`CAP | exec | none`, `CAP | net | none`, `CAP | write | own-game-folder`). A pack that asks for a capability its kind may not hold is rejected at step 5. Also: **safe paths on install** (never write outside the game's folder), **no network calls from a game** unless the capability is granted and the owner approves, and **resource caps** (voxel counts, file sizes, `gen_vox_max`-style) as rows.

## 7. Modify from insights (how it ties to the AI loop)
* **Insights = ledger rows, with consent.** FACT: the decision ledger and the gate/result ledgers are plain append-only text. A demo may report *aggregates* (e.g. "players die at planet 3 at 62%") from `state/events.txt`/`decisions.txt`, shared only if the player opted in; raw user data never leaves their folder (AGENTS.md).
* **Loop:** insight -> a candidate change (a tuning row, a world-gen row, a layout row) written as a **proposal** -> judged by the game's gate in a scratch copy -> a person promotes (companion sections 2, 9, 13). Models describe, code decides, no auto-promotion.
* **Versioning:** every promoted change is a new version of the pack with a changelog row; a mod/demo records `base=<game>@<version>`.
* **Demos:** a `DEMO` cut is generated from the full manifest (so the demo cannot drift); its gate is a subset; its report is the insight source above.

## 8. Build order (PROPOSAL; each slice ends with evidence and a negative test)
1. **`game.pdl` schema + `game_check`** for rpg-space/dust (parses; contracts resolve; reports parts). No store needed.
2. **Common-skin extraction** (companion section 11), so a part is a swappable file, not a sed copy.
3. **Shared folder + `access.pdl`** under `xyzfs/shared/games/` with rpg-space/rpg-dust moved in as the first residents; one reader (`access_check`) used by the launcher; negative test: a `role:user` account is refused.
4. **Pack builder** for a game (`game_pack`: manifest, deny-list, checksum, demo cut) and **screener** (`store_screen`, steps 1-6 above) with a pal harness (tampered archive, `..` path, denied file, failing gate, over-budget, capability violation each must be refused).
5. **`store-hq` window**, Library + Store + Game page views from `catalog.pdl`/`owned.pdl` (free path), then reviews, then Toys; relay-driven test, PNG for the owner; footprint measured with a browser open.
6. **Install op** (quarantine -> screen -> copy into the user's folder; idempotent; rollback) against a local `file://` repo, then the dedicated GitHub repo.
7. Checkout escrow on a test chain; per-user repos as sources; moderation of reviews.
This builds on the already-designed order in XYZFS design sections 7, 9, 10; where they conflict (a window instead of a dropdown, games as the product), this document is the later word.

## 9. Owner decisions (2026-10-27, answered in chat) and what is still open
| # | Decision | Consequence recorded here |
|---|---|---|
| 1 | **Outside creators ship data only** (rows, voxel models, layouts, art, event pages); only the owner and trusted dev users ship C | section 6 option A is the rule; step 5 rejects any pack asking for `exec` / `net` capability from an outside creator |
| 2 | **An agent proposes the screening verdict, the owner confirms in batches** (from the store-hq window) | the screener writes `needs_review` + a recommendation; promotion stays a person's act; needs a batch-confirm view |
| 3 | **Shared dev folder is tracked in the code branch** | keep large art/assets out (size cap row); never put user data or secrets in it; watch history bloat |
| 4 | **Insights: opt-in per game and per player**, off by default, visible toggle in the Library; aggregates only | section 7 stands as written |
| 5 | **Store-items repo is public, owner publishes only** | **public history is permanent**: the deny-list and screening run BEFORE anything is published; archives as release assets; nothing from `xyzfs/users` ever; I will not publish or create the repo without your explicit go |
| 6 | **Outside-platform listings (Steam/PSN): a later real goal** | keep the catalog `source=` column open to adapters; no adapter is designed now |
| 7 | **Reviews: rule-based filter, AI describes only** | a deny-word/length filter plus owner override; the AI may DESCRIBE review text into the usual review file, never act on it |
| 8 | **First build: `game.pdl` + `game_check` on rpg-space and rpg-dust**, then the common-skin extraction | section 8 steps 1-2, in that order |

**Tension to watch (mine, not a decision):** decision 3 puts the shared folder in the code branch while decision 5 makes the *store* repo public. Those are different things (the first is the dev working copy, the second the published catalog), but a game copied from one to the other becomes public: the screening and deny-list must gate that copy, not only installs.

**Still open:** the real name of the store repo (placeholder `tearit-co/store-items`); who may become a reviewer later; the reviews deny-word list; moderation of images/screenshots; how a taskbar cell opens the store-hq window without shared-file review.

## 10. Checked after the first edition (2026-10-27) and what is still unchecked
* **`store_module.pal` (01.muchi-pals) is NOT the taskbar Store.** It is the muchi-pals project's own "store" screen (a per-screen module of that project, 59 lines, July 2026). Ignore it for this design; do not build on it.
* **Renderer and art:** the generic `khtpm_core_render.c` has the `<item class="swatch">` tile grid and `<row class="sprite-grid-row">` (64 px sprite blits, data-driven), so **capsule art can be a sprite/tile grid with no new renderer C** (FACT: the paths exist; NOT CHECKED: a real game-capsule layout drawn). **Star ratings:** no layout uses them today; plain text glyphs (e.g. `4.5 / 5` or repeated characters) need nothing new (NOT CHECKED visually). Screenshots on a game page would be PNG-to-sprite conversions (OPEN, size limits).
* **`desk_copy_op` does not fit a game install.** It copies a desk and renames its pals inside `xyzfs/users/<uuid>/home/livedesk` (dry run by default, refuses to overwrite). A game bundle is a folder of C, data and layout, not a desk of pals; a game install needs its own idempotent copy op (quarantine -> screen -> copy into `xyzfs/users/<uuid>/games/<game>/`), and `desk_copy_op` is only relevant for any *pals/desks* a game ships.
* **Still unchecked:** whether a taskbar cell can open a full x11-hq window without shared-file review (the XYZFS design says the taskbar owner must review `khtpm_taskbar_manager.c` / the strip header); any enforcement of `role=`; the `toy.pdl` scan roots beyond the one example read (`101.mutaclsym.../toy.pdl`: `SECTION | title | ...`, `SECTION | launch | button.sh`).

## 11. Store mockup built (2026-10-27) and cover images
**`&.hq-apps/store-hq/` is a runnable MOCKUP** (copied from `_template-hq`: `store-hq.xhtpm` + `.css`, `refresh.sh`, `actions.sh`, `button.sh`, `mock_catalog.pdl`). Launch: `nice -n 15 sh '&.hq-apps/store-hq/button.sh' <house_root>`. Generic renderer only, no new renderer C. Observed (screenshot of the live window via `dump_frame_png_op`): a Steam-ish dark-blue window with Library / Store / Toys tabs, a game list, a game page (name, kind, price, rating, screening badge, tags, blurb, Get/Play button, sample reviews). Action logic checked from the shell: tab switch, select, `Get` marks owned and the Library tab then lists it, a bad id is refused. **Nothing installs, screens or downloads**; every review and the two `sample-*` games are labelled sample text. The shell backend follows the template (the sanctioned skeleton); a real Store needs the compiled manager the template README points to.
**Cover images (owner: "each game should have an image, however basic, auto-generated until uploaded").**
* FACT: the generic renderer draws `.png` sprites natively and tile dirs with `sprite.csv` (`# resolution=N` then `r,g,b,a` per pixel). `sprite=` items only get a tall slot **inside a scrolllist / sprite-grid-row**; a lone item in the panel stayed a thin bar and drew nothing (tried three layouts).
* BUILT in the mockup: `art_for()` uses `art/<id>/cover.png` if the creator provides one, else writes a **deterministic 48x48 placeholder** (hash of the id picks the colours and stripe width) once to `state/art/<id>/sprite.csv`. Seen live: distinct tinted thumbnails per game in the list.
* **Known mockup flaws (observed, not fixed):** the game name text draws over the thumbnail in list rows; the large cover on the game page is not shown (panel layout); `state/art/` is runtime output and is not meant to be committed. A proper capsule layout needs a renderer-side fix or a different grid (OPEN).
* A real upload flow must also **validate images**: PNG only, size/dimension caps, decoded by the trusted renderer, never executed; screening step 2/5 applies.

## 12. Uploads, p2p and user-owned data (owner direction 2026-10-27)
Owner: "we will also start building out and using p2p social the same way, and let users' social accounts upload to GitHub as well as p2p so they can store and own their own data."
**What exists today (FACT, checked):** there is **no user image/file upload to the house.** `hq-ftp` (LAN peer drop) is a placeholder window, "Not built". The only peer code is `041.pal-chain/ops/palnet_peer.c` (chain), which hardcodes 127.0.0.1, so multi-user testing is same-machine only (memory: cross-machine networking plan, not started). Forum, IRC and chat HQ apps exist (`&.hq-apps/forum-hq`, `irc-chat-hq`, `chat-hai`; not examined here). The network browser's "upload" hits are web forms, not house uploads.
**Direction (PROPOSAL built on the owner's words):**
* **The user owns their data.** Each user's social account (profile, posts, chats, covers, game saves they choose to share) lives in **their own folder**, and the user may **publish it to their own GitHub** (a per-user repo, the same "per-user repos as sources" idea as section 5) **and/or share it p2p**. The house never needs to hold the only copy, and never publishes a user's data on their behalf.
* **One pack format for all transports** (existing XYZFS design section 3): the same manifest + checksum + deny-list pack goes to a LAN peer (`hq-ftp`), a p2p peer, or a user's GitHub repo. Store items, social posts, images and profiles are all "packs of kind X".
* **Social = the same screening and consent rules:** content a user shares is screened before another user's house renders it (images PNG-validated; text through the review rule-filter; no executable content from outside users, per decision 1). Posts are append-only rows like every house ledger.
* **Identity:** a user's key (not the house) signs what they publish; a p2p or GitHub copy is verifiable without trusting the host. OPEN: which key (the wallet key must NOT be reused for social signing).
**Conflicts with existing rules, to resolve before building (mine, not decisions):**
1. AGENTS.md: **never push `user/*` branches** (they hold wallets and private chat history). User-owned publishing must therefore be a **separate, explicit, per-user export** (a pack built by the pack builder with the deny-list, to the user's own repo with the user's own credentials), never a push of the data branches. Wallets and private chats stay out by default (`sensitive=0`).
2. Public GitHub history is permanent: a user must see exactly what leaves (a plan/dry run) before it is published; deletion is not guaranteed.
3. Credentials: a user's GitHub token must live in their own folder, never in a pack, ledger or prompt, never in the house repo.
4. p2p today is same-machine; real p2p needs the cross-machine plan (peer addresses, NAT, authentication) first.
**Build order (PROPOSAL):** (1) image validation + `cover.png` support in the mockup's art path; (2) pack builder with kind `social`/`profile` and the deny-list; (3) `hq-ftp` for two LAN houses (the first real transfer; local test first); (4) the social window (feed from append-only rows); (5) per-user GitHub export with a dry-run plan; (6) real p2p after the cross-machine work. Each slice needs a negative test (a denied file or oversized image must be refused).
**Open questions for the owner:** the signing key for social; whether the first social feature is a feed, profiles, or comments on store reviews; whether user GitHub export is per user opt-in only (assumed yes); which free GitHub-compatible hosts, if any, besides GitHub.

## 13. Theft model: disputable, reversible transactions with a REP penalty (owner 2026-10-27)
Owner: "it doesn't matter if you have a user's key for stealing bitcoins, because we let transactions be disputed and reversed (penalizing the thief with a bad REP down-rating on the platform), so it's not such a big deal."
**The model (owner's design):** value moved on the platform is not final at once. A transaction can be **disputed**; if upheld it is **reversed**, and the offending account takes a **REP (reputation) down-rating**. Security therefore rests on **detect, reverse, penalize** more than on keeping every key secret.
**What exists (FACT, `041.pal-chain/ops/chain_escrow.c` header, read):** `LOCK` / `PAYOUT` / `REFUND` transactions with the invariant paid out + refunded <= locked, mined and replayed by the chain ops; escrow is disabled unless `chain.pdl` enables it; **it is documented as trustworthy on local/test chains only because the `agent` / `by` fields are unsigned ("an honor field")**. A REFUND by the lock's agent is the one reversal primitive that exists.
**What does NOT exist (searched, nothing found):** a dispute record or process, reversal of an already-settled PAYOUT, a REP score or its storage, an arbiter, or a settlement delay (a hold window in which a dispute can be raised). So the owner's model is **design only**; escrow is its only built piece, and only for the lock-then-release case.
**How this lowers my concern, and its limits (stated so the doc is honest, not to argue):**
* It does lower the stakes of a stolen key **for value that lives on the house ledger** (cones, test coins, store purchases): a thief's gain can be clawed back and the account marked. That fits the store design (escrow at checkout: LOCK, PAYOUT when the install verifies, REFUND on failure) and makes a **dispute window** natural (PAYOUT only after N hours, or held for a REP-weighted period).
* It does **not** cover: value on an **external chain** (real Bitcoin cannot be reversed by us); **non-monetary harm** (a leaked key can read private chats and histories, post as the user, publish their data); a **new throwaway account** that a REP penalty cannot hurt; **abuse of the dispute system** itself (false disputes). So the cheap protections stay on regardless: the key/secret **deny-list in packs**, never putting keys in prompts, ledgers or the repo, and the separate social signing key. This is not extra caution against the owner's design; reversal and key hygiene cover different losses.
* The design needs these pieces before it can be relied on: (1) **signed** transactions (otherwise "who did it" cannot be established, and a forged dispute is as easy as a forged payment); (2) a **dispute record** (append-only, `DISPUTE | tx | by | reason | ts`) and a **resolution record** with who decided (arbiter rule: owner, a role, or a rule + agent proposal confirmed by the owner, consistent with decision 2); (3) a **settlement delay** policy as data; (4) **REP** as an append-only ledger of signed events, never a field a user can edit, with decay and an appeal path; (5) bounds so a reversal cannot be used to steal back (a reversal must itself be a ledger row naming the dispute).
* Practical rule for builders: **write transaction code as if it will be disputed**: every value movement a ledger row with the actor, reason and a reference, so a later reversal is a new row, not an edit.
**Open for the owner:** who arbitrates; the length of the dispute window for store purchases; whether REP gates anything (publishing to the store repo, selling, p2p posting); whether REP is per account or per signing key; whether REP penalties apply to pets/agents with wallets.
