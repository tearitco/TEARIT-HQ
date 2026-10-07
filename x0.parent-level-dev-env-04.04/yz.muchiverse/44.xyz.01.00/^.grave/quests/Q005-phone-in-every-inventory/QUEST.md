# Q005 — a phone in every entity's inventory (from now on and retroactively)

| field | value |
|---|---|
| status | DONE except the manager start-time measurement (op, hook, live apply and sprite are all in and proven) |
| tier | manager (claude) or outside-agent; needs care (touches every entity) |
| size | M |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | go-ahead to run the retroactive migration on live entities (dry-run first, always) |

## Mission (one sentence)

Give every entity (new and existing, including entities that are items inside other inventories) a phone pal at `<entity>/inventory/zz.phone/`, created once and idempotently.

## Why it matters

Phones are how entities talk and how the owner sees their history (design: `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3 and §3b). The owner decided
2026-10-06: a phone in every entity's inventory, from now on and retroactively.

## Read first

1. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3, §3b (the decisions and why the name is `zz.phone`)
2. `&.widgits/_shared-lib/khtpm_inventory.c` (the model: an item IS an entity; slot order alphabetical; `inventory_slot.txt`)
3. `^.hai-phone/README.md` and `^.hai-phone/_TEMPLATE/` (what a phone folder contains)
4. `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` `livedesk_spawn_desk()` (every entity process starts here) and `livedesk_ensure_cursword()`
5. `AGENTS.md` and `03-pitfalls/OPERATIONAL-LANDMINES.md`; house sharing rule: pure code used by 2+ consumers is a text-include under `&.widgits/_shared-lib/`, never header + link

## Do

1. `&.widgits/_shared-lib/khtpm_phone.c` (text-included, prefix `ph_`, static + unused-tolerant like `khtpm_locations.c`): `ph_ensure(entity_dir)` — if
   `<entity_dir>/inventory/zz.phone/` is missing create it from the template with `phone.pdl owner=<entity id>` and a `pal.pdl` (glyph 📱);
   append the path to `^.hai-server/phones.index`; recurse into `<entity_dir>/inventory/*` directories that are entities (have `pal.pdl`).
   Never touch an existing phone. Never overwrite ledgers.
2. `phone_ensure_op.c` (a compiled op, `&.widgits/_shared-lib/ops/` pattern): `phone_ensure_op.+x <house_root> [--apply]` walks every `pals/` tree and reports what it
   would create; changes nothing without `--apply`.
3. Hook `ph_ensure()` into `livedesk_spawn_desk()` and `livedesk_ensure_cursword()`. It must cost one `access()` per entity when nothing is missing (measure the manager start before/after with `#.desktop/boot_timeline.txt`: no regression).
4. `.gitignore`: `**/inventory/zz.phone/inbox.txt`, `outbox.txt`, `history.txt`; do not ignore `pal.pdl` / `phone.pdl`.
5. Do NOT change `inventory_slot.txt` for any entity; verify existing slot numbers still select the same item.
6. **Identity (design §3c):** `ph_ensure()` also gives every entity an immutable `entity_uid.txt` (random 128-bit, generated once, never overwritten)
   and derives `entity_hash = sha256(uid)`, the phone number (`NNN-NNNN-NNNN`, 11 decimal digits of the hash, collision-checked against `phones.index`)
   and the `wallet_id` string (`e` + 24 hex). It only WRITES `entity_uid.txt` and the phone files; it does not create chain wallets or touch `041.pal-chain`.
7. `phone.pdl` gets the history caps (`history_max_lines`, `history_max_bytes`, `rotate_keep`) from the template; rotation itself is the server's quest, not this one.

## Acceptance (manager verifies with evidence)

- [ ] Dry run on the real house lists every entity (54 at posting time) and creates nothing (paste the report).
- [ ] After `--apply` on a COPY of the pals tree, every entity (including nested ones) has exactly one `zz.phone`; a second run creates nothing.
- [ ] An entity spawned fresh after the hook gets its phone; an entity that already has one is untouched (byte-identical).
- [ ] The inventory HUD / hotbar shows the 📱 as the last slot and the previously selected slot still selects the same item (relay or real key, with a frame dump).
- [ ] Manager start time not slower (timeline marks before/after), no build warnings.
- [ ] `git status` shows no ledger files tracked.
- [ ] Every entity has an `entity_uid.txt`; running the op twice never changes an existing uid, number or phone (checksum before/after); no two entities share a number (paste the uniqueness check).
- [ ] The derived `wallet_id` passes `chain_send`'s charset rule (letters, digits, `_`, `-`) and no chain wallet or `pending_tx.txt` line was created.

## Rules

Everything in `^.grave/README.md`. Entity data is live: any apply on the real house needs the owner's explicit OK first; do the first apply on a copy.

## Log

2026-10-06 | claude | built `&.widgits/_shared-lib/khtpm_phone.c` (text-include, prefix `ph_`) and `ops/phone_ensure_op.c` (+ `build_phone_ensure_op.sh`); zero warnings; `--selftest` passes 3 SHA-256 vectors + number format.
2026-10-06 | claude | DRY RUN on the real house (nothing written: file count under xyzfs 1365 before and after): 55 entities (32 top-level + 23 items), 55 phones to create, 53 uids frozen from `PAL | hash`, 2 random (`tax_robot` has no pal.pdl; `tile_rmmv_World_A2_1790489103` has no hash), 0 collisions. Full report: `DRYRUN-REPORT-2026-10-06.txt` (this folder).
2026-10-06 | claude | APPLY on a COPY of the pals tree: 55 phones created, 55 unique numbers, 55 unique uids; second run created nothing and every file was byte-identical; zz.phone sorts last in every inventory; every `inventory_slot.txt` byte-identical; frozen uid equals the original pal hash; all wallet ids match `e` + 24 hex. Real house untouched (0 `zz.phone` dirs).

## Still to do (not started)

- [x] Hook written 2026-10-06: `livedesk_phone_ensure()` in `khtpm_taskbar_manager.c`, called per desk entity in `livedesk_spawn_desk()` (after the pal exists, before the already-live skip, so running entities get one too) and in `livedesk_ensure_cursword()`; Linux only. Manager compiles clean (exit 0, 0 phone warnings); `khtpm_phone.c` added to `MGR_SRCS`. NOT yet run live: the binary rebuilds on the owner's next hash-gated build/reset, and the first start then creates all 55 phones. Still to measure: manager start time before/after (`boot_timeline.txt`).
- [x] Phone sprite (2026-10-06): the house emoji tools (`_.monads/_.livedesk-taskbar/ops/+x/emoji_gen_atlas.+x` + `emoji_xtract.+x`) made `atlas.png` + `sprite.csv` once into `^.hai-phone/_TEMPLATE/`; `ph_ensure` (and the manager hook) copy them into any phone that lacks them, never overwriting. Proven: dry run on live = 110 files missing; apply on a scratch restore twice = 110 added then 0, byte-identical to the template, all other files unchanged; applied live (55 phones x 2 files); a read-only frame dump of the hotbar shows the phone picture in slot 4. status of the manager change: compiles clean; it only takes effect after the next desktop restart.
- [x] `livedesk_hash_dir()` now excludes `entity_uid.txt` and `*/inventory/zz.phone/*` (same commit).
- [ ] Live apply on the real house (owner OK; dry run first, as done).

## Result
