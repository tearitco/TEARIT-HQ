# Q005 — a phone in every entity's inventory (from now on and retroactively)

| field | value |
|---|---|
| status | open — waiting for owner OK to touch the manager spawn path and live entity data |
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

## Result
