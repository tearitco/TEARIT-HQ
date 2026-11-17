# +phone Texting app: design (2026-11-16)

Status: **DESIGN ONLY, nothing built.** Written by Claude Sonnet 5.5 after surveying the code (headers and file listings, nothing run except the `pet_phones` harness: 11 PASS). Label key: OWNER = said by the owner, FOUND = read in the repo, DOC = my proposal.

## 1. Goals (OWNER 2026-11-16, kept in his words)
- A "texting" app on the +phone: users text back and forth, "group" chats later, then AV (voice/video) later; "maybe we can do now".
- Messages go to other users too; **all messages go on the tx ledger for the chain; they can be hashed or whatever for privacy.**
- Contacts auto-populate: other users, and **entities that have phones, especially the ones we own**.
- **Entities answer if their AI/FSM dictates, through an event trigger**; build a test case for that and document it for users/agents in the docs and in XO.

## 2. What already exists (FOUND: do not rebuild)
| Piece | Where | State |
|---|---|---|
| A phone in every entity | `<entity>/inventory/zz.phone/` (`phone.pdl`, `inbox.txt`, `outbox.txt`, `history.txt`); 53 on disk | built, design in `08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` sections 3 to 3f |
| Phone numbers | `NNN-NNNN-NNNN`, 11 digits derived from the entity hash, never reissued; `^.hai-server/phones.index` rows `number|uid|label|entity_dir|phone_dir` (76 rows) | built |
| Send | `&.widgits/_shared-lib/ops/phone_send_op.c`: one line `<ms>|<from>|<to>|<kind>|<ref>|<text>` into the sender's own outbox; the sender number comes from the sender's `phone.pdl` (no spoofing); refuses bad numbers, `|` or newline in text, empty text, a full outbox; kinds `say task ask-human answer status spawn done fail command lease release result` | built |
| Delivery | `&.widgits/_shared-lib/ops/server_route_op.c`: the ONLY writer of inboxes; byte cursors per outbox; rate limit (60 per sender per minute), batch cap; unknown recipient gives the sender a `fail` message; ledger `^.hai-server/ledger.txt` rows `<ms>|<what>|<from>|<to>|<kind>|<ref>` (**no text**), `observations.log`, `tunables.conf` | built; harness `pet_phones` 11 PASS; `router.pal` loop is a template (`button.sh start` fills it) |
| Text input on the phone | the renderer's generic `<cli_io>` element (used on the cursword screen, harness `phone_entity`) | built |
| Phone app packaging, store, icons | `PLUS-PHONE-APP-STORE-AND-APPROVED-REPO-DESIGN-2026-11-16.md`, `app_check`, `build_phone.sh` | built |

**Gaps (FOUND):** (a) no phone that is clearly the HUMAN user's own was found (every phone read belongs to an entity); (b) the chain's TX lines (`041.pal-chain/ops/chain_send.c`, `data/pending_tx.txt`) are money-only and v1 has no transaction signing, so a message commitment is a NEW tx type; (c) groups, the auto-reply trigger and AV are not designed.

## 3. Design (DOC)
### 3.1 One authority per fact
- The **message** lives in the sender's `outbox.txt` and the recipient's `inbox.txt` (router-written). `history.txt` is the per-phone projection. The app never writes an inbox.
- The **audit trail** is `^.hai-server/ledger.txt` (append-only, no text). The app reads, never writes it.
- The app keeps only a **view cache**: `<instance>/texting/` (contacts, the open thread projection, byte cursors). Safe to delete; rebuilt from the phone files (same rule as every projection, NO-GHOST-DATA).

### 3.2 The app: `texting` (phone app package, standard icon a speech bubble)
Screens (phone layout rules: `PHONE_BARS`, `APP_CHROME`, `PHONE_FOOTER`):
1. **Threads**: one row per peer, newest first, last line and an unread mark (inbox bytes past the app's own read cursor).
2. **Thread**: the lines exchanged with one peer (from `inbox.txt` and `outbox.txt`, filtered by number, merged by `<ms>`), a `<cli_io>` field, **Send** = `phone_op`-style verb of the app's op `text_op` which calls `phone_send_op` (kind `say`, ref = the thread id).
3. **Contacts**: see 3.3. Tap = open (or start) the thread.
Ops in the app's `ops/` (C only, no new `.sh`): `text_op contacts|threads|thread|send|read`.

### 3.3 Contacts auto-populate
Source = `phones.index` (the only list; the server never rescans the house). Order: (1) my own phone's thread peers, (2) **entities I own** (their `entity_dir` is under my own user folder), (3) other users and their entities. A contact shows the `label` and the number only. Other users' folders are private: the app reads **only** the index row, never the other user's files; delivery into their inbox is the router's job, not the app's.
Blocking: a per-user `block.txt` (numbers) honoured by the router (a router change, one more joint); default = nobody blocked, a phone may set "only owned" (DOC).

### 3.4 Privacy and the ledger (OWNER: hashed or whatever)
- **Slice 1 (safe, no chain change):** extend the router's ledger row with `sha256(text)` and a message id (`<ms>-<n>`), so a message is committed without its text. The numbers are already pseudonymous (derived from hashes).
- **Slice 2 (chain):** a commitment tx `MSG | <msg_id> | <from> | <to> | <sha256(text)>` appended to the chain as a new tx type. **This needs `PAL-CHAIN-STANDARD` changed first** (v1 only knows money TX lines, no signing, tx_id is a local dedup key): read the standard, add the type and the miner rule, add a harness, then wire it. Never inject into the real chain before that. The text itself stays off-chain (phone files); the chain proves "this message existed at this time".
- Encryption of the text at rest is NOT in slice 1 (phone files are plain text today): say so in the app's help, do not claim privacy the files do not have.

### 3.5 Entities answer (OWNER: AI/FSM via an event trigger)
- **Trigger:** the router already knows when an inbox grew. A small op `phone_reply_op <phone_dir>` (resident or fired by the router after delivery) reads the entity's inbox from its own byte cursor (size growth, never mtime) and for each new `say` line asks the entity's **reply policy**: `<entity>/reply.pdl` rows `REPLY | <match> | <action>` where action is `text <fixed reply>`, `event <page>` (plays an event page, which may call `phone_send_op`), or `ai <backend>` (the house AI layer, rate-limited, off by default). No policy = the entity stays silent (the safe default of slice 1).
- **Safety:** an auto-reply is a normal outbox line (kind `say`) so it passes the same rate limit and ledger; a per-pair reply cap (for example 3 per minute) stops two entities talking forever; the loop guard is a joint in `tunables.conf`.
- **Test case (documented for users and agents, harness `texting_reply`, scratch house, NO network, no real entity):** two scratch phones A (a human-like sender) and B (an entity with a `reply.pdl`). Steps: (1) A sends `say` to B with `phone_send_op`; (2) one `server_route_op` pass: B's inbox has the line, the ledger has one row and no text; (3) one `phone_reply_op` pass: B's outbox has the reply addressed to A; (4) a second router pass: A's inbox has the reply; (5) negatives: a phone with no `reply.pdl` stays silent; a message from a blocked number gets no reply; the loop cap stops a ping-pong after N messages; a spoofed sender number is refused by `phone_send_op`; sabotage proof for each (remove the cap, remove the block check). The same steps, as prose with the exact commands, go into this doc and into `XO/31.Nov/nov-16/` when the case passes.

### 3.6 Group chats (later)
A group = one append-only **group ledger** `^.hai-server/groups/<gid>/messages.txt` plus `members.txt` (numbers). The router fans each group line out to every member's inbox (one `say` line each, `ref = g:<gid>`); membership changes are rows (`JOIN`, `LEAVE`, `KICK`) so history is replayable. The thread screen is the same screen filtered by `ref`.

### 3.7 AV (voice/video) later
Signalling is the same text path (`command` kind: `call <gid-or-number>`); media is a separate transport (palnet peer or WebRTC) and is NOT designed here; the phone has no camera or mic API yet. Out of scope until text and groups pass.

### 3.8 Caps and costs (RUNTIME-FILE-GROWTH-STANDARD rows to add when built)
`inbox.txt`, `outbox.txt`, `history.txt` rotate at the phone's own caps (2000 lines / 262144 bytes, 3 generations, server-rotated, already in `phone.pdl`); `ledger.txt` and `observations.log` need a rotation row (check the standard); the app's `<instance>/texting/` cache is capped at 64 KB per contact list and 256 KB per open thread. Idle cost: the router loop (2 s sleep, one exec per pass; measure with a browser running before shipping, as the standard requires).

### 3.9 Search, added contacts, notes (OWNER 2026-11-16: "that stuff can be searchable, add contacts, note, etc.")
- **Address book = a user-owned, append-only file** `<my phone dir>/contacts.txt` (it lives with the human's phone, NOT in the phone instance, so deleting a phone window or instance never loses it). One row per change: `<ms>|CONTACT|<number>|<name>|<tags>`, `<ms>|NOTE|<number>|<text>`, `<ms>|RENAME|<number>|<name>`, `<ms>|DELETE|<number>`. The current book is derived by replaying the rows (a projection, safe to rebuild; same rule as every ledger here). A contact can be a number typed by hand (it need not be in `phones.index` yet: the router answers an unknown number with a `fail` message, so a typo is visible), or picked from the auto list (3.3). The auto list and the address book are merged by number; the user's name wins over the index label.
- **Notes** are private: a note about a contact or a thread, never routed, never in the ledger, never on the chain, never sent to another phone (the router does not read `contacts.txt`). Tags (`friend`, `bot`, `owned`) are free words.
- **Search verb** `text_op search <query>` (C, case-insensitive substring, no shell): looks in (1) contact names, numbers, tags and notes, (2) message text in MY OWN `inbox.txt` / `outbox.txt` / rotated generations (only my phone's files, never another user's). Results are rows `contact: <name>` or `<name>: <line snippet> (<date>)`; tap opens the thread. Slice 1 scans with caps (stop after 200 hits or 4 MB read); if it gets slow, add a derived `search.idx` projection with a byte cursor (never mtime), rebuilt from the same files. The house already has a search-field pattern (`@.apps/layout-studio/templates/menu-search.xhtpm`, the Tools dropdown search): reuse the `<cli_io>` + results-list shape, do not invent a second one.
- **Privacy note:** because search reads message text, it works only on files the user already owns; a hashed ledger row (3.4) is not searchable and does not need to be.
- **Tests (added to `texting_app`):** add a contact by hand then see it in the list; a note is found by search and is NOT in the router ledger or the other phone's inbox (negative); a search term in another user's file finds nothing (negative); a rebuilt projection after deleting the cache is identical; a DELETE row removes the contact but keeps the thread; the caps stop a huge scan.

## 4. Phases (each ends with a harness that can fail and a photo)
| Phase | What | Test |
|---|---|---|
| 0 | decide the two blockers (section 5), `PAL-CHAIN-STANDARD` read | (doc) |
| 1 | `text_op` contacts (auto + added by hand), notes, search, send, thread projection, the app screens | `texting_app` (scratch house, real ops, stub renderer like `phone_entity`) |
| 2 | router ledger gets `sha256(text)` + message id; block list | extend `pet_phones` |
| 3 | `phone_reply_op` + `reply.pdl` + loop cap | `texting_reply` (3.5) |
| 4 | live on a test phone with scratch users, photo of both ends | photos |
| 5 | chain commitment tx (after the standard changes) | chain harness |
| 6 | groups | `texting_group` |
| 7 | AV design | (doc) |

## 5. Open decisions (owner)
1. **The human's own phone.** Which phone is jb's? If none exists, create one for the account with `phone_ensure_op` (writes into the user's folder; do it only when told).
2. **Chain message tx** (slice 5): change `PAL-CHAIN-STANDARD` for a commitment tx type: yes or later?
3. **Reply default:** silent unless the entity has a `reply.pdl` (DOC recommendation) or reply for owned entities by default?
4. **Blocking default** for other users' messages: open, or "owned only" until a phone opts in?
5. **Where the address book lives** (3.9): `contacts.txt` beside the human's own phone (needs decision 1). Search covers my own phone files only: OK?

## 6. Evidence
Read: `phone_send_op.c` (header and helpers), `server_route_op.c` (header), `^.hai-server/{README.md,router.pal,tunables.conf,phones.index}`, `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` (headings and sections 3b/3c), `041.pal-chain/ops/chain_send.c` (header), one `phone.pdl`. Run: harness `pet_phones` (11 PASS, 2026-11-16). NOT read: the rest of the router, `phone_ensure_op`, `PAL-CHAIN-STANDARD`, any other user's phone (private).
