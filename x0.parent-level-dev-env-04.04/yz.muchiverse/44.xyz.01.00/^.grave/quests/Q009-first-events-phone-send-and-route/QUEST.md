# Q009 — first events: `phone.send` + `server.route`, with tunables and a ledger

| field | value |
|---|---|
| status | built and scored by `verify.sh` (PASS 14/14, selftest PASS); NOT run against live phones, not wired to a loop; steps below done by claude 2026-10-06 |
| tier | worker / outside-agent (good second quest: small, verifiable, teaches the pattern) |
| size | M |
| assignee | claude (built; a worker can extend it: rotation, a router loop, `.pal` wiring) |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | none to start; the tunables defaults are commented, hand-tuned later |

## Mission (one sentence)

Make one message travel phone -> server -> phone as two reusable ops, with every decision number in a tunables file and every routing decision logged as a ledger row, proven by a verifier script.

## Why it matters

It is the smallest slice that exercises the whole pattern in `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3f (events, tunable joints, ledger rows, single-writer server) and the phones that now exist in all 55 entities (Q005).

## Read first

1. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3, §3b, §3f, §4 (message line format `<epoch_ms>|<from>|<to>|<kind>|<ref>|<text>`; inbox = server-only writer, outbox = owner-only writer)
2. `#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/PRISC-OPS-ARCHITECTURE.md` and `&.widgits/_shared-lib/khtpm_phone.c` (how a phone folder is laid out; text-include style)
3. `%.harnesses/harnecient-fsm/tunables.conf` (the joint file format to copy) and `#.ref/menu/event_commands.registry.pdl` (how a reusable command is declared)
4. `^.grave/quests/Q003-build-gate-include-list/verify.sh` (how a deterministic scorer is written: selftest first, stable `VERDICT|...` output)
5. `AGENTS.md` (incl. the new user-data rules: phones live under `xyzfs/users`, which is NOT in git; test on a COPY)

## Do

1. `phone_send_op.c` (compiled op, `&.widgits/_shared-lib/ops/`): `phone_send_op.+x <sender_phone_dir> <to_number> <kind> <ref> <text>`: validates the kind against the allowed list, rejects `|` and newlines in the text, appends ONE line to the sender's `outbox.txt` (owner-only writer), enforces `history_max_*` from the sender's `phone.pdl` by refusing with a clear error (rotation is a later quest).
2. `server_route_op.c`: one pass of the router: for each phone listed in `^.hai-server/phones.index`, read its `outbox.txt` from a saved byte cursor (size growth marker, never mtime), resolve `<to_number>` through the index, append to the recipient's `inbox.txt` and both `history.txt` files, append a ledger line to `^.hai-server/ledger.txt`, advance the cursor. Single writer of every inbox. Unknown number -> a `fail` line back to the sender, never a crash.
3. `^.hai-server/tunables.conf` (sourced `name=value`, defaults commented like the FSM file) with at least `route_max_msgs_per_min` and `route_batch_max`; the router reads it each pass. Each pass appends one row per decision to `^.hai-server/observations.log` (`<epoch_ms>|route|<joint>=<value>|<inputs>|<verdict>`).
4. Declare both as commands in the event registry (a `phone_send` COMMAND block) so an events-hq script can call it with no C change.
5. `verify.sh` in this quest folder (selftest first): on a COPY of two entities' phones (never the live ones): send A->B, run the router twice; assert B's inbox has exactly one new line, A's and B's history have it, the ledger has one `route` row, a second router pass adds nothing (cursor), an unknown number yields a `fail` line, and a rate-capped burst is throttled per `route_max_msgs_per_min`.

## Acceptance

- [ ] `bash verify.sh --selftest` passes; `bash verify.sh` prints PASS (paste output in `## Result`).
- [ ] Live phones untouched (checksum of two real `zz.phone` folders before/after; the test used copies).
- [ ] Build clean with `-Wall -Wextra`, zero warnings.
- [ ] Changing a number in `tunables.conf` changes the router's behavior with no recompile (show before/after).

## Rules

Everything in `^.grave/README.md`. Never write a live phone's `inbox.txt` from anything but the router. Do not push; do not run the router against live data without the owner's OK.

## Log

2026-10-06 | claude | Built: `&.widgits/_shared-lib/ops/phone_send_op.c` (the phone.send event), `server_route_op.c` (the server.route event: single writer of inboxes, byte cursors by size growth, spoof check, per-sender rate cap + per-pass batch cap from `^.hai-server/tunables.conf`, ledger + observations rows, rotation resync), `build_phone_ensure_op.sh` builds all three (`-Wall -Wextra`, 0 warnings). Registered `phone_send` in `#.ref/menu/event_commands.registry.pdl` (not exercised through events-hq yet). `^.hai-server/tunables.conf` (all defaults commented), runtime files `observations.log` / `cursors.txt` gitignored.
2026-10-06 | claude | `verify.sh`: 14 checks on a SANDBOX (two entities with phones made by the real `phone_ensure_op`; live phones compared by checksum before/after, untouched). PASS 14/14. `--selftest`: a router that routes nothing fails 10 of 14, the real router passes. Bugs found while building: a misleading-indentation warning and unbounded `%s` in ledger rows (fixed: fields bounded, verbatim copy written from the line buffer).

## Not done (extend here)

- [ ] A router loop: `server_route_op` is one pass; nothing calls it repeatedly yet (a `.pal` loop with a sleep, or the manager tick). Do NOT run it against live phones without the owner's OK.
- [ ] History/inbox rotation (the caps are enforced on send only; the router does not rotate yet).
- [ ] `phone_send` exercised through an events-hq script (the registry block is written, not run).
- [ ] The router writing `kind=ask-human` messages to the human's phone; leases (`command|lease|release|result` are accepted as kinds, not acted on).

## Result
