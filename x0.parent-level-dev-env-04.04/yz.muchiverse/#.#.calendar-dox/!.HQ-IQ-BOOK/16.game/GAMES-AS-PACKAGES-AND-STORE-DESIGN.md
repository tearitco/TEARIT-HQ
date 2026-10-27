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

## 14. Disputes, REP and the house "courts": owner decisions (2026-10-27) and the my-lawyer reuse
### 14.1 Decisions (answered in chat)
| # | Question | Owner's answer | Consequence |
|---|---|---|---|
| 1 | Arbiter | **Rule-based auto-resolve for small amounts** | deterministic rules decide disputes under a threshold; larger ones go to the courts (14.3) and finally to the owner; no model decides a number |
| 2 | Dispute window before the seller is paid | **7 days** | `PAYOUT` is released only 7 days after a verified install; until then the money stays in escrow (`REFUND` is possible). Make it a data row, with a **dev override** (a short window for testing) so development is not blocked |
| 3 | What REP gates | **p2p posting / social reach, and selling / being paid immediately** | low REP: posts rate-limited or flagged, payout waits the full window; high REP: faster payout. (Not chosen: store-repo publishing; it stays owner-only per decision 5) |
| 4 | Whose REP | **Hybrid: shared but not complete liability** ("corp veil, but a bit more accountable"); a pet can be **jailed and bailed out / represented in our official "courts"**, using the **my-lawyer-style local AI mechanics** | see 14.2 and 14.3 |
### 14.2 REP and liability as data (PROPOSAL)
* **Each account (user, pet, agent) has its own REP ledger**, append-only, signed events: `REP | account | delta | reason | ref(dispute|tx) | ts`. REP is derived from the ledger, never edited; decay and appeal are rows too.
* **Shared, limited liability (the veil):** an owner's REP takes a **fraction** of a pet's or agent's penalty, capped: `LIABILITY | kind=pet | share_pct=25 | cap=<points per period>` (numbers are placeholders, OPEN). An owner is accountable but not wiped out by one bad pet; repeated penalties across their pets raise the share (the "more accountable than a corp" part) as a rule row.
* **Jail** (a state, not deletion): a jailed account's wallet is **frozen** (no PAYOUT/sends, no p2p posting, no selling), its ledger rows stay readable; it is a `STATUS | account | jailed | until | case_id` row that the transaction ops must read. **Bail:** the owner (or anyone) posts a bond from their balance into escrow (`LOCK`); the account is released; the bond is returned (`REFUND`) or forfeited (`PAYOUT` to the injured party) by the case result. **Representation:** the account or its owner may appoint a lawyer agent that builds their side of the case.
* **Settlement delay and REP:** the 7-day window is the default; accounts with enough clean REP may get a shorter one, brand-new accounts never shorter (data rows).
### 14.3 The courts: reuse `@.apps/my-lawyer` (read: `MY_LAWYER_DESIGN.md` in full, the judge worker header)
FACT, what my-lawyer already is: a turn-based game where NPCs post lawsuits to a **docket** (`data/docket.txt`: `id|title|claim|side`), each case has real files `data/cases/<id>/{case_meta.txt, plaintiff_case.txt, defendant_case.txt}` that a research agent grows one `[Argument N]` at a time with **deterministic tool calls** (search corpus, search precedent) and one closing call; async workers with pid/status files; a **judge worker** reads BOTH real files, asks gemma3:270m an **open-ended compare** (never "A or B"), derives the winner with a **hand-written keyword and argument-density scorer**, applies tie-breaks, and writes `status=judged`, `winner=`, `raw_comparison_text=` to `case_meta.txt` and the `master_ledger.txt`. It follows the house law: **describe, never classify**; code decides.
**Mapping (PROPOSAL):**
| Court need | my-lawyer piece reused | New |
|---|---|---|
| a dispute becomes a case | docket row + `case_meta.txt` | `case_meta` gains `tx=`, `amount=`, `parties=`, `kind=dispute` |
| each side states its position | `plaintiff_case.txt` / `defendant_case.txt`, built from real ledger rows by deterministic tool calls (the tx, the install check, the screening result) | evidence tools that read `owned.pdl`, `screening.txt`, the tx ledger (not a model's memory) |
| a ruling | judge worker (compare, deterministic scorer, tie-break) | **the model only describes; a rule table maps the result to reverse / uphold; an owner confirm is required above a threshold** |
| jail / bail / representation | none | new rows and ops (14.2) |
| audit | `master_ledger.txt` | append `RULING | case | winner | rep_deltas | ts` |
**Do NOT carry over the game's "office and bias" mechanic (my-lawyer section 5) into real disputes.** There, buying political office to bias settlements and judging is a deliberate in-game corruption loop; in the platform courts bias must be **zero by design** (a rule row, tested). Likewise `search_precedent` invents plausible but unreal case names (its section 7 item 6): in a real court, evidence is **only ledger rows**, never model recall.
**Limits to state plainly:** a 270M judge is not reliable enough to move real value on its own (the project's own measurements: classify wrong 2 of 3, describe 6 of 6), which is why decision 1 uses deterministic rules for small amounts and keeps a person in the loop above a threshold. **Nothing here is built**: no dispute ledger, no REP ledger, no jail/bail rows, no signing, no evidence tools; the my-lawyer ops run as a game, not against real transactions. I did not run them.
### 14.4 Build order (PROPOSAL; each slice with a negative test)
1. **Signed transactions** (prerequisite for everything: who did it) on the test chain.
2. **Dispute and resolution ledgers** + the **deterministic small-amount rules** (e.g. install check failed, or payout not yet released, or duplicate charge: refund) with a harness: a stolen-key purchase is disputed and reversed; a false dispute is refused; a reversal cannot be reversed back by the thief.
3. **REP ledger** and the two gates (payout wait, posting rate); a thief's REP drops, the victim's does not.
4. **Court case files** for disputes (adapt the docket and case files; evidence tools read ledgers) and the judge worker in **describe-only** mode feeding a **rule table**, owner confirm above the threshold.
5. **Jail / bail / representation** rows and the checks in the transaction ops.
6. The liability share and cap rules; tune with data.
**Footprint:** all event-driven; the judge worker is a niced one-shot, never a polling loop (AGENTS.md coexistence rule).
### 14.5 Open for the owner
The small-amount threshold; the liability share and cap; REP decay and appeal rules; whether jail applies to a user's own account or only to pets/agents; whether bail may be posted by third parties; who the final appeal is (assumed: the owner); whether the courts are one window (`court-hq`, a Steam-like sibling of `store-hq`) or part of the Store.

## 15. Everything adjustable by .pdl per chain and per game; where `court-hq` lives (owner 2026-10-27)
### 15.1 Owner decisions
* **All thresholds, windows, shares and rules are adjustable through `.pdl`, per chain and per game** (not constants in code). This answers the open numbers of section 14 (threshold, share, cap, decay, jail scope, bail) as **settings with defaults**, not as one fixed answer.
* **Build a `court-hq` window and try it under the taskbar's Network menu** (question was: Network or HQ, global admin or networks?).
### 15.2 Settings format (PROPOSAL, same house pattern as `tuning.pdl` TUNE rows)
```
POLICY | scope | key                   | value | text                                              | default
POLICY | chain:cones-test | dispute_window_days    | 7  | days a purchase stays disputable before PAYOUT      | 7
POLICY | chain:cones-test | dispute_window_dev_days| 0  | override used when dev_mode=1                       | 0
POLICY | chain:*          | small_amount           | 50 | disputes at or below this are rule-resolved (millicones) | 0
POLICY | game:rpg-space   | liability_share_pct    | 25 | share of a pet/agent penalty the owner takes        | 25
POLICY | game:rpg-space   | liability_cap_per_30d  | 40 | cap on REP points an owner takes from pets          | 40
POLICY | *                | rep_decay_per_30d      | 5  | REP points forgiven per 30 days of clean record     | 5
POLICY | *                | jail_applies_to        | pet,agent | account kinds that can be jailed             | pet,agent
POLICY | *                | bail_by                | owner | who may post bail (owner|anyone|none)             | owner
POLICY | *                | judge_bias             | 0  | MUST stay 0 in real courts (tested)                 | 0
```
* **Scope resolution** reuses the scoped framework of the companion doc section 8: most specific wins: `chain:<id>` / `game:<id>` / `kind:<k>` / `*`; a missing row falls back to the default column; **a value outside its declared range is refused and the default is used (logged)**. Numbers above are **placeholders to show shape, not decisions**.
* **Where files live:** `policy.pdl` next to each chain's `chain.pdl` (escrow already has an enable flag there: `ESCROW enabled = 1`, per `chain_escrow.c`) and next to each game's `tuning.pdl`. A generated settings window (companion section 10 item 6) edits them with the TUNE text as help, keyboard-numbered.
* **Safety of tunable safety:** rows that weaken protections (`judge_bias`, `dispute_window_*`, `small_amount`, `bail_by`) are **owner-signed**: a change is an append-only `POLICYCHG | who | key | old | new | ts` row; the court ops read the file at use time, never cache across a ruling. A player-facing game must not be able to edit its own court's policy (the "never edit the grader" rule).
### 15.3 Network or HQ? (checked in `#.desktop/livedesk_taskbar.pdl`)
* **HQ menu (FACT, 16 rows):** restart, quit, kill hq, always-on-top, hide/show, dir, settings, stats, cli, tpmos, cursword, debug, proc-mon, tomom. That is **machine administration and diagnostics** (global admin of the house itself).
* **Network menu (FACT, 8 rows):** IRC Chat, Forum, Chain, Browser, Exchange, Auction, hq-ftp (LAN drop, not built), cancel. These are the **windows where users meet and value moves between accounts**.
**Recommendation: `court-hq` goes under Network, next to Exchange and Auction.** Reason: a court exists because of what happens between parties (a purchase, an exchange trade, an auction, a social post, a chain transfer), and it reads exactly those ledgers; the people who need it (a buyer, a seller, a pet's owner posting bail) are network users, not house administrators. It is closest to **Exchange/Auction/Chain** (money disputes) and **Forum/IRC** (REP for posting).
The **admin part is narrow and stays inside the window**: the owner's appeal / confirm-in-batches queue and the policy editor are shown only to the owner role (once `role=` is enforced; today it is only a marker). Do **not** add a separate HQ-menu entry for it; the HQ menu is for running the machine. If the taskbar later grows a role-gated "admin" group, the owner queue could move there.
**Adding the menu row:** the Network menu is data-driven in `livedesk_taskbar.pdl` (`network_menu_N_label` / `_cmd`, with `cancel` last). A new row means inserting `court` before `cancel` and renumbering; that file and the taskbar manager are **shared, with a taskbar owner who must review** (XYZFS design section 10), so this is not done here. A `court-hq` mockup can be built first as a standalone window (like `store-hq`) and wired in after review.
### 15.4 Next steps (PROPOSAL)
1. `court-hq` mockup from `_template-hq` (views: My cases, Docket, Case file, Rulings, Jail/Bail, owner queue), data from mock rows; no real transactions.
2. `policy.pdl` schema + a reader with range checks and a harness (a value out of range falls back; an unsigned change is refused).
3. Then the build order of section 14.4.
### 15.5 Still open
The actual numbers (placeholders above); whether jail applies to users' own accounts (default set to pets/agents until you say otherwise); who may post bail (default owner); the court window layout; who is the taskbar owner to review the Network menu change.

## 16. Wired into the taskbar and `court-hq` mockup (2026-10-27, tested)
* **Store, cell 13:** clicking `store` opens `store-hq` (`launcher_store` in `livedesk_launchers.pdl`, 14 lines of C in `ktb_hq_open`, commit c3b99adcf). **Tested live** after restarting the strip with `run_khtpm_strip.sh new` (the `$.restart` HQ row): relay nav-jump `13` + Enter started a new `store-hq` process that registered a window titled "Store" in `livedesk_hq_windows_<pid>.txt`. **Not proven:** a real mouse click (my relay MOUSE_EVENT at the cell did nothing; wrong coordinates or event shape, not investigated). Cell 14 is `network`.
* **Courts under Network:** `court-hq` is a standalone mockup (`&.hq-apps/court-hq/`: Docket / Rulings / Jail-bail tabs, mock cases, a mock `policy.pdl`). The Network menu got one data row (`network_menu_8` = `court (mockup)`, `livedesk:open-network:court`; `cancel` moved to 9) and `launcher_network_court` in the launchers file. **No C change.** Tested live through the relay: Network (14) -> arrow-down to row 24 -> Enter opened a window titled "Courts". (A first attempt with typed digits `2`,`4` opened Network Browser instead; I closed that window by pid. Arrow keys are the reliable way to pick a dropdown row.)
* **POLICY rows drive behaviour (shown in the mockup, mock data):** the handling line is computed from `policy.pdl`: a 30-coin case with `small_amount = 50` reads "rule-resolved", a 400 case reads "goes to court case files, then owner confirms", and raising `small_amount` to 500 changes a case's line (negative control; file restored). The policy line in the window shows the chain's window, threshold, liability and judge bias.
* **Still mock:** no ledger is read or written; "Apply/Post bail/Get" only write a note. `notes-network` also appears in the Network menu (another agent's row, not mine). Footprint of the two windows with a browser open: not measured.

## 17. Store and blockchain as EVENTS, so any app or game can use them (owner 2026-10-27)
Owner: "figure it out and make sure stuff like store and blockchain is using events as much as possible so users can make their apps and games interface with store and blockchain."
### 17.1 What I found (checked)
* The house already has a **data-driven event command registry** (`#.ref/menu/event_commands.registry.pdl`: one `COMMAND` block per type, a `TEMPLATE` that `exec`s a single-verb op; adding a command needs **no C change in events-hq/event-ez**). Ops take the entity state dir (`$ENT`) first and return results as `key=int` lines in `variables.txt` (the convention of `var_math`, the ring-board ops).
* **Before today the registry had NO chain, escrow or store command.** The chain ops (`chain_balance/send/escrow/faucet/miner`) existed as compiled programs driven by their own window and scripts, so a game page could not use them. The Store window and catalog had no ops at all.
* Mouse click (the earlier open item): works when the **pointer is over the element and the relay MOUSE_EVENT follows** (`khtpm-house-standards`/`rpg-space-board-driving` say so); my first attempt skipped the pointer. Proven on the `store` cell (new Store window started).
### 17.2 Built today (tested; scratch chain and scratch house, nothing real touched)
Two ops following the house op convention, plus nine registry blocks:
| Op (source, binaries are not tracked) | Verbs | Registry commands |
|---|---|---|
| `&.widgits/chain-events/ops/ev_chain.c` | `balance`, `send`, `lock`, `payout`, `refund`, `status` | `chain_balance`, `chain_send`, `chain_escrow_lock`, `chain_escrow_payout`, `chain_escrow_refund`, `chain_escrow_status` |
| `&.widgits/store/ops/ev_store.c` | `price`, `owned`, `get` (free items only) | `store_price`, `store_owned`, `store_get_free` |
Results land in `variables.txt` (`chain_ok`, `chain_rc`, `chain_balance`, `chain_locked/paid/refunded/remaining`; `store_ok`, `store_known`, `store_price`, `store_owned`); each call appends one row to `chain_events.txt` / `store_events.txt` in the entity dir (append-only: an event page or an AI can follow it by marker growth); a failure also appends an `ERR` row to `op_errors.txt` (a pal `exec` hides exit codes).
**Safety rules built in (each is a refusal, tested):** (1) the **wallet is never a parameter**: it is the entity's own, read from `$ENT/wallet_id.txt`, so a page cannot spend someone else's wallet; (2) the chain id must be a **`chain_new` chain** (has `chain.pdl`), and **`cones`, the real chain, is refused** (the house rule "never move the old chain's cones from game code"); (3) ids and amounts validated (no path characters, 1..1e9, no negatives); (4) children run by **fork+exec with a 10 s watchdog**, never popen; (5) store: the player is `$ENT/user_uuid.txt` and must name an existing `xyzfs/users/<uuid>/`; `get` takes **free items with `requires=none` only**, is idempotent (a second `get` writes no second `OWN` row), and **installs nothing**.
**Observed results.** Chain (a scratch `evtest-claude` chain made with `chain_new`, deleted afterwards): balance read 11500; `lock esc1 500` then `status` showed locked=500, remaining=500; a payout by a wallet that is NOT the escrow agent was refused (exit 1); the agent's payout of 500 to the seller made the seller's balance 500; refused with a row and no change: chain `cones`, an unknown chain, an entity with no wallet bound, amount 0, amount -5, a destination with `/`, and an overspend. Store (a scratch house, the real catalog copied): `price` free = 0, `owned` 0 then `get` wrote exactly one `OWN` row, a second `get` wrote none; refused: a paid item, a role-gated item, an unknown item, a path-like item id, an unknown user folder, a path-like uuid. The exact registry TEMPLATE forms were also run through `sh -c` with `$D`/`$ENT` set.
**NOT done / not proven:** no pal harness (house rule: harnesses are pal; mine was a manual scripted run, so it is not repeatable by others yet); I did not generate a page through events-hq and play it (only the template form); the registry already had one more `COMMAND` than `END` before my edit (156 vs 155, not mine; now 165 vs 164); `get` is the only store write (no `buy`, `install`, `review`); chain `send` / escrow are **unsigned honor fields** (`chain_escrow.c` header), so this is meaningful on test chains only until signing exists.
### 17.3 What "events as much as possible" should mean (PROPOSAL, the guidance)
1. **One path for window and games.** `store-hq`, `court-hq` and the chain windows must become thin clients of the **same ops** the event commands use (today `store-hq`/`court-hq` are mockups with their own shell actions, and the chain windows use their own scripts). A feature exists once, as an op + a registry command; a window button and a game page both call it. This is the rule that keeps apps and games from being second-class.
2. **Calls and notifications.** The commands above are **calls** (the page asks). Apps also need **notifications** (the store/chain tells the page): the append-only ledgers (`chain_events.txt`, `store_events.txt`, the chain's own `blockchain.txt`, `owned.pdl`) are the notification source; an event page with trigger `parallel` can follow a ledger by marker growth. A small **watcher op** that turns a ledger row into a house event (`OWN` recorded, escrow paid out, dispute opened) is the missing piece; the existing `chain_inbox_watcher` is the precedent.
3. **Every step a ledger row with actor, reason and reference** (also what the courts of section 14 need): a reversal is a new row, never an edit.
4. **Policy by `.pdl`** (section 15): per chain / game settings (who may call which verb, amount caps per entity per day, allowed chains per app) are `POLICY` rows the ops read at call time, so a game cannot raise its own limits. NOT built: the ops currently validate shape only, there is no per-game allow-list or spend cap, which is the next thing to add before any real value moves.
5. **Capabilities, not trust** (section 6): an outside creator's data-only pack may call only the commands its manifest declares (`CAP | chain.send | max=100/day`); a pack asking for more is rejected at screening.
### 17.4 Next slices (PROPOSAL)
`ev_store buy` (escrow lock on a chain, payout after the dispute window, refund on failure) over the existing chain commands; `store_install` (quarantine + screening); `store_review` (rule filter); dispute and REP verbs (`chain_dispute`, `rep_event`) as registry commands so the courts are callable from games; the ledger-to-event watcher; a pal harness for `ev_chain` and `ev_store` with the negatives above; per-game spend caps in `policy.pdl`; and the windows moved onto the same ops.

## 18. `petslike_life.c` to an op: assessment and staged plan (owner 2026-10-27: "move them to ops", hot paths)
**Assessment (read all 102 lines and the rpg-space call sites; nothing changed yet).** Unlike the decision ledger, the life layer is not a clean digest. It is coupled to the running game in memory: it moves pets (`use_ent`, `pet.room/x/y`, `pet_save`), plays event pages (`run_event_x("death" | "respawn")`), uses the skin's in-memory drop list through hooks (`PL_DROP*`), draws the bottom-bar HP state for the UI, and rpg-space's own code reads and writes the `pl_life[]` array directly (air damage, `take`, hunger from events: `rpg_space.c` lines about 789-844). It is shared by 11 skins and sits on the **tick** (hunger/hp for every character). A one-shot rewrite would touch all 11 skins and the 83-check gates at once, which is why I did not do it in the same pass as the decision ledger.
**What is already file-based (so an op is possible):** per-character `state/life_<id>.txt` (hp, hunger, dead, deaths, inv), `life.pdl` config, `events.txt` rows (`DEATH`/`RESPAWN`/`TAKE`), and (rpg-space) `state/drops.txt`.
**Staged plan (each slice ships alone, gate green, negative test):**
1. **`life_op tick`**: hunger growth, starvation hp loss, death detection for every character, driven by elapsed time, reading and writing the same `life_<id>.txt` files and `life.pdl`; the daemon calls it on a **slow cadence** (about every 1.5 s, niced, one at a time) instead of running the loop in C each tick. The death/respawn **effects** stay where they are for now (the op writes a `DEATH_DUE|<id>` row; the game reads it and runs the existing page/effect). Gate: the p6_life phases unchanged.
2. **`life_op` verbs** (`status hurt feed give set reset`) as the op the `life` verb forwards to; the C verb becomes a thin forwarder. The `@actor` plumbing stays.
3. **Effects as events:** `die` / `respawn` become registry commands (the death and respawn pages already exist: `events/death`, `events/respawn`); the op appends the rows, the page does the moves via existing commands (`set_event_location` family), so no game C runs the respawn.
4. **UI bar state** read from a tiny `life_bars.txt` the op rewrites on change (marker growth), not computed in the skin.
5. **Drops** through `drops.txt` rows (already the interface) so the skin hooks disappear; then delete `petslike_life.c` and the 11 hook definitions.
Rule while migrating: the hot path (the tick) never forks; the op runs on the slow cadence, and verbs run it synchronously (they are fresh processes already).
**Not started.** First slice needs its own gate runs for rpg-space and rpg-dust (the only two with life enabled; the other nine are OFF), plus a CPU measurement of the cadence (tick cost file `tick_stats.txt`).
## 19. Cryptographic signatures (owner 2026-10-27: "we will do the crypto sigs as well")
**What exists (FACT, checked):** wallets are `wallet_id + password` with an **unsalted SHA-256** password hash (`chain_create_wallet.c`; the doc names "no salt in v1" as a gap and states the owner's tolerance: coins are refunded if stolen); **transactions carry no signature** (`chain_send.c`: "tx_id is a local dedup key only, NOT a signature"); `chain_escrow` / `chain_miner` say the `agent` / `by` fields are an **honor field**. Tooling available: OpenSSL 3.0.2 with `libcrypto` (`evp.h` has `EVP_PKEY_ED25519`; I generated an Ed25519 key with the CLI to confirm); `ssh-keygen` exists; libsodium does not.
**Proposal (mine):**
* **Algorithm: Ed25519** through OpenSSL EVP (small keys and signatures, deterministic, already linkable like the SHA-256 code). One keypair per account (user, pet, agent); the **public key** goes in the wallet record (`pubkey=`), the **private key never enters a chain folder, pack, ledger, prompt or the repo**: it lives in the owner's own folder, encrypted under a password with a salted slow KDF (this also fixes the unsalted-hash gap).
* **What is signed:** a canonical string `chain_id|kind|from|to|amount|ts|nonce` (and the same shape for `LOCK`/`PAYOUT`/`REFUND`, with the escrow id). Including the **chain id** stops replay on another chain; a per-wallet **nonce** (monotonic) stops replay on the same chain. The line becomes `TX|from|to|amount|ts|id|nonce|sig`.
* **Verified in the same three places the rules are already checked:** issue time (`chain_send`, `chain_escrow`), inclusion (`chain_miner`), and replay (`chain_balance`), so a forged line cannot ride in through any one of them. `agent`/`by` must equal the signer.
* **Migration by policy, not flag-day:** `chain.pdl` gets `SIGNING | required | 0|1`; unsigned lines are accepted only on chains with 0 (today's test chains), refused where 1. The real `cones` chain stays untouched (game code never moves it).
* **Who signs for events:** `ev_chain` never holds a key. It asks a separate **`sig_op`** (keygen / sign / verify) that holds or is handed the signer for the running entity; pets and agents get their own keys under their owner's policy (per-entity daily cap as a `POLICY` row, section 15). A key that cannot be used (locked, missing) makes the call refuse, not fall back to unsigned.
* **Social signing key is separate from the wallet key** (section 12): the same `sig_op`, a different keypair and context string, so a social post can never be replayed as a payment.
* **Rotation and revocation** are signed ledger rows (`KEYROT | account | new_pubkey | sig_old`, `KEYREV`); **recovery** is the dispute/courts path (sections 13-14), which stays the backstop for stolen keys, exactly as the owner decided.
* **Disputes, REP and rulings are signed rows too** (who filed, who ruled), so "who did it" is established before any reversal.
**Build order (each with negative tests):** (1) `sig_op` + pal harness: sign/verify round trip, a tampered amount fails, a wrong key fails, a replayed nonce and a cross-chain replay fail, a private key is never printed or written outside the owner folder; (2) wallet `pubkey` field + salted password hash with migration; (3) signed `TX` on a test chain behind `SIGNING required = 1`, with the verification in all three places; (4) escrow `agent`/`by` bound to the signer; (5) `ev_chain` through `sig_op`; (6) signed dispute/REP/KEYROT rows.
**Open for the owner:** where private keys live per account type (owner's folder; pets/agents: the owner's folder under policy?); password-protected only, or an OS keyring where available; whether signing is mandatory on all new test chains from day one; the nonce scope (per wallet per chain, assumed).
