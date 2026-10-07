# ^.hai-phone — entity phones (📱)

How entities talk and how we see their history. A phone is a pal owned by one entity (or a human). Design:
`#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3 (read first). Working name.

Files in one phone (copy `_TEMPLATE/` to `<owner-id>/`):

| file | written by | meaning |
|---|---|---|
| `phone.pdl` | server | owner, glyph, state (idle / ringing), created |
| `inbox.txt` | **the server only** | messages TO the owner; the owner reads new bytes by size growth |
| `outbox.txt` | **the owner only** | messages FROM the owner; the server reads new bytes by size growth |
| `history.txt` | the server | merged readable conversation (what the viewer shows) |

Message line (append-only, one write per line): `<epoch_ms>|<from>|<to>|<kind>|<ref>|<text>`; kinds: `say task ask-human answer status spawn done fail`.
Never edit or delete lines. No secrets in messages. Nothing here runs yet (design phase 0).
