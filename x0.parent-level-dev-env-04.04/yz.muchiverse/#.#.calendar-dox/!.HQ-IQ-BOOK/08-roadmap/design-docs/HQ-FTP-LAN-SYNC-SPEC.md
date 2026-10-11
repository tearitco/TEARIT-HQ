# hq-ftp LAN sync: share files and house data between instances with no GitHub and no other program (spec, 2026-10-07)

Owner (2026-10-07), after the `xyzfs/users` tracking problem: "we need to finish the network hq-ftp application so instances on LAN can share data without github or any other program. Can we spec doc that and transfer xyzuser fs data to mac?"
Status: **SPEC, not built.** It extends `HQ-FTP.md` (placeholder window `&.hq-apps/hq-ftp/` exists; that doc says the peer carries lines, not file bytes, and "the cut is not designed here"; this doc designs it). Related: `CROSS-MACHINE-NETWORKING-PLAN.md` (`palnet_peer.c` hardcodes 127.0.0.1), `CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md`, `STAGING-ENVIRONMENTS.md`, `game_snapshot_op` (content-addressed store and the safe-restore rules reused here), the Joint contract in `XO/13.phymoji-engine/02-ROADMAP-TECHNICAL-SPECIFICATION.md` section 0.

## 1. Goals and non-goals
Goals: (1) two or more houses on one LAN exchange files and whole trees (including `xyzfs/users/`) directly; (2) no central server, no account, no git remote, no external tool at run time (no rsync/ssh/scp/nc); (3) private data is protected: nothing moves without a human-approved pairing and an explicit share; (4) resumable, verified, atomic, never silently overwrites; (5) works Linux <-> macOS; (6) every action leaves an append-only ledger row; (7) self-healing and tunable under the Joint contract.
Non-goals v1: internet traversal, NAT hole punching, multi-GB streaming optimizations, live two-way database merge of running games, Windows.

## 2. Reuse (do not reinvent)
- **Hashing and the object store idea** from `game_snapshot_op`: SHA-256 (`ph_sha_hex`), `objects/<sha[0:2]>/<sha>`, manifests, all-or-nothing publish by rename, dry-run-by-default apply, and its path-safety checks (no `..`, no absolute path, no symlink in any destination component, never record setuid/setgid/sticky).
- **UI shape**: an X11-HQ window = layout `.chtpm` + css + one compiled manager that owns state and publishes a projection; redraw by an append-only marker file that GROWS (never mtime); input via the relay; nav-numbered rows (`khtpm-house-standards`).
- **Discovery idea** from `palnet_peer`/the presence directory, but NOT its line transport: bytes need a stream.

## 3. Components (all compiled C, self-contained, no header+link sharing; shared logic is text-included only if pure)
| Program | Verb |
|---|---|
| `ftp_beacon` | announce this house and listen for others (UDP) |
| `ftp_serve` | TCP listener: authenticate a peer, answer manifest/object requests, accept pushes into a quarantine |
| `ftp_manifest` | walk a share, apply deny-list and excludes, write `manifest.txt` (path, size, mode, sha256) and the manifest hash |
| `ftp_pull` / `ftp_push` | diff manifests, fetch/send only missing objects, resume, verify |
| `ftp_apply` | validate the quarantine and publish into the share (dry run by default, `--apply`) |
| `hq_ftp_manager.c` + `hq-ftp.chtpm/.css` | the window: peers, shares, queue, progress, history |
Fork + exec + waitpid with a watchdog for every child; no `system()`; no `pkill -f`; flag-file kill switch.

## 4. Discovery
UDP beacon on port **47931** (data TCP port **47932**, both in `ftp.pdl`), every `beacon_interval_s` (default 5): `HQFTP|1|<node_id>|<house_name>|<tcp_port>|<key_fingerprint8>`. Subnet-directed broadcast plus multicast `239.255.47.93`; peers are written to `peers_seen.txt` (append-only). If broadcast is blocked, `peers.pdl` rows `PEER | name | host:port` are used (manual). The beacon never carries file names or paths.

## 5. Pairing and trust (human in the loop)
First contact shows a **6-digit short authentication code** derived from both nodes' ephemeral public values on BOTH windows; a person compares and clicks Accept on each side. Result: `trusted_peers.pdl` row `TRUST | node_id | name | fingerprint | since | scopes`. No pairing = connections are dropped after the hello. Unpair = delete the row (ledgered). Crypto (honest scope): v1 uses an X25519-style key agreement is NOT hand-rolled; **decision needed**: (a) v1 = pre-shared pairing secret from the 6-digit code stretched with PBKDF2-HMAC-SHA256 (house has SHA-256), every frame authenticated with HMAC-SHA256 (integrity and peer authenticity, replay protection by a per-session counter), **no confidentiality** (documented; LAN-only; use on trusted networks or inside an SSH/WireGuard tunnel), (b) v2 = add a stream cipher (ChaCha20) with the derived key, written and tested against RFC 8439 vectors. Do not ship homemade key exchange. Recommended: (a) now, (b) before any untrusted network.

## 6. Shares (what may move) and the deny-list
`shares.pdl`: `SHARE | name | local_path | mode=ro|rw | peers=<node ids or *> | enabled=0|1`. Defaults: `drop` (a folder, rw, enabled), everything else **disabled until a person enables it per peer**: `users-data` (`xyzfs/users`, off), `design-docs`, `harness-results`. A share root is validated at load (absolute, inside the house, no symlink escape).
**Hard deny-list, enforced at BOTH ends and not overridable by a share row** (compiled in, plus `deny.pdl` for additions): `**/state/raw_*.txt`, `openrouter_api_key.txt`, `tokenrouter_api_key.txt`, `.env*`, `*.key`, `*.pem`, `id_*`, `*secret*`, `module_parent.pid`, `interact_relay.txt`, `cli_io_state.txt`, `.hq_manager/`, `*.lock`, `/tmp` and runtime sockets. Wallet files (`wallet*`) are in a **sensitive class**: they move only when the share row says `sensitive=1` AND the receiving person confirms the list.

## 7. Protocol HQFTP/1 (TCP, framed)
Frame: `u32 length | u8 type | payload | 32-byte HMAC` (HMAC over a session counter + type + payload). Types: HELLO(version, node_id, nonce), AUTH, LIST_SHARES, MANIFEST_REQ(share, since_hash), MANIFEST(chunked), OBJ_REQ(sha list), OBJ_DATA(sha, offset, bytes), OBJ_DONE, PUSH_OFFER(share, manifest_hash, count, bytes), PUSH_ACCEPT/REJECT(reason), ERROR(code). Limits: max frame 1 MiB, objects streamed in `chunk_kb` (default 256) pieces, `max_file_mb` (default 2048), `max_files_per_share` (default 200000), idle timeout 30 s, one active transfer per peer per share. Every received object is **re-hashed**; a mismatch refetches up to 3 times, then quarantines the peer's connection and writes a `HASHFAIL` row.
**Resume**: partial objects are kept as `<sha>.part` with a verified offset (a sparse/append file) and continue after a dropped connection or restart; a received object is published by rename only after the full hash matches.

## 8. Sync semantics (the hard part, decided up front)
- **One-way pull is the default** and the first sync of a new machine: target tree empty or receiving into a new directory; result is verified by the manifest hash (both sides compute it; equal = identical tree).
- **Two-way sync** is explicit per share (`mode=rw` on both sides) and never deletes by default (`--prune` is a separate, ledgered, dry-run-first action).
- **Conflicts** (same path, different content on both sides since the last common manifest): never overwrite; keep both (`<name>.conflict-<node>-<ms>`) and list them in the window.
- **Append-only ledgers** named in `append_only.pdl` (`history.txt`, `*_ledger.txt`, `chat_history.txt`, `eden_history.txt` ...) are merged by **line union preserving order** (a line is never rewritten; duplicates collapsed by exact text), because they are the house's own append-only format.
- **Running games and live entity state**: syncing a tree whose entities are running is unsafe; the window refuses a `rw` apply into a share while its owner house reports running windows for that share (`proc ledger`), and a snapshot-first rule applies (`game_snapshot_op save` before an apply that touches game files).

## 9. Mac (and cross-platform) hazards, because the first real use is Linux -> macOS
- **Case-insensitive filesystem (APFS default)**: two paths that differ only in case would collide. The manifest builder rejects a share containing case-fold collisions and the receiver refuses to write one (`REFUSE|case-collision`).
- **Unicode normalization**: macOS may store names as NFD while Linux uses NFC. This house has emoji, `&`, `^`, `$`, `#`, `@`, `%`, `⛓️`, `🦓️`, `💸️`, `🤖️` in folder names. Rule: the protocol carries **byte-exact UTF-8 paths in NFC**; the receiver compares by byte AND by normalized form and refuses ambiguous pairs; a path-mapping table `pathmap.txt` records any rewritten name so it can be reversed.
- **Illegal/awkward characters** (`:`, trailing dots/spaces, reserved names): refused with a clear row, never silently renamed.
- **Sockets/APIs**: `MSG_NOSIGNAL` is absent on macOS (use `SO_NOSIGPIPE`); `sha256sum` is `shasum -a 256` there (irrelevant: hashing is in-house); build with `clang`; endianness handled by explicit network byte order.
- **Permissions/ownership**: only the executable bit and 0644/0755 are carried; no uid/gid.
- Compiled programs (`+x/`) are never shared: each machine builds its own (`sh '$.crypts/button.sh' build`).

## 10. The `xyzfs/users` transfer to the Mac (what the first real test does)
1. On the Linux house: back up (tarball + sha256 list outside the repo; file count recorded). 2. Both houses run hq-ftp, pair (6-digit code), enable share `users-data` for that one peer, `sensitive=1` only if wallets are meant to move. 3. The Mac **pulls** into an empty `xyzfs/users`. 4. Verify: manifest hashes equal, file counts equal, spot-check a pal folder, then start nothing until a person looks. 5. Ledger rows on both sides. 6. Disable the share again. Proof is the manifest-hash equality plus the drop-folder round trip from `HQ-FTP.md`'s first test, not a screenshot.

## 11. Window (X11-HQ) and events
Layout: header (house name, node id fingerprint, LAN address), **Peers** (seen, trusted, pair/unpair), **Shares** (enable/disable per peer, mode, sensitive flag), **Queue** (offer/pull/push with progress and speed), **History** (ledger tail), **Conflicts**. State in `state/` files, redraw by marker growth; actions arrive as relay lines. Optional registry command `offer_file` so an RPG-Maker-style page can offer a file (added only when a page needs it). Ledger `ftp_ledger.txt`: `<ms>|<peer>|<share>|<verb>|<path or hash>|<bytes>|<result>`.

## 12. Security model (threats considered)
Rogue LAN host (pairing code + HMAC; unauthenticated connections dropped, rate-limited); path traversal and symlink tricks (same validation as snapshot restore, plus a deny-list); resource exhaustion (frame/file/count limits, one transfer per peer-share, disk-space check before accept); replay (session counter in the HMAC); data leakage by accident (shares default off, deny-list at both ends, `sensitive` class, human confirmation of the file list for sensitive shares, nothing ever leaves via the beacon); tampering (every object re-hashed, manifest hash compared). No confidentiality in v1 (see section 5): **do not run `users-data` over an untrusted network until v2**.

## 13. Harness (pal, loopback, two scratch houses; proves everything except real two-machine networking)
Cases: beacon seen on loopback; pairing with matching/mismatching code (mismatch = no trust row); unpaired connection dropped; manifest equality after pull; tampered object refused and refetched; kill the client mid-transfer (own pid file, no `pkill -f`) and resume to a byte-identical tree; deny-list files never listed or sent; a share with `..`/symlink escape refused; case-collision and NFC/NFD pair refused; conflict keeps both; append-only ledger merge by line union; `--prune` dry-run first; rate and size limits; ledger rows complete; second pull is a no-op (zero bytes); timing for a 50 MB tree; a case shown able to fail. Manual two-machine procedure (Linux <-> Mac) documented with exact commands and the proof to collect.

## 14. Joints (Joint contract; defaults autonomy 0)
`ftp.chunk_kb`, `ftp.window` (outstanding requests), `ftp.beacon_interval_s`, `ftp.retry_backoff`, `ftp.bandwidth_cap_kbps`, `ftp.max_file_mb`, `ftp.pair_code_digits`. **Grow**: a per-peer grade from verified transfers `(ok+1)/(ok+hashfail+2)`; a peer's grade below a threshold disables auto-accept and requires a human click (never raises permissions by itself). **Learn/Train**: a sandbox trainer on a loopback pair picks `chunk_kb`/`window` for throughput within bounds; proposals reviewed, not auto-applied. **Heal**: resumable parts, refetch on hash mismatch, periodic `verify` of a share against the last common manifest, quarantine and a visible notice on repeated failure; never edits file content to "fix" a mismatch.

## 15. Build order
1. `ftp_manifest` + `ftp_apply` on scratch trees (reuses snapshot rules) with the harness cases for deny-list, path safety, Mac hazards. 2. `ftp_serve`/`ftp_pull`/`ftp_push` over loopback with HMAC framing, resume and tamper tests. 3. `ftp_beacon` + pairing. 4. The window and ledger. 5. Linux <-> Mac manual test with a small tree, then the `users-data` pull. 6. v2 cipher. **Not verified yet: everything above is a design.** Open questions for the owner: confirm v1 without encryption on trusted LAN; is `users-data` allowed to carry wallets (`sensitive=1`) or should wallets stay local; the Mac's address and whether it runs the house (needs a build).

## 16. Stopgap today (no code, run by the owner)
Until hq-ftp exists, a one-off copy to the Mac needs only tools already installed (ssh on both): `tar -C <house>/xyzfs -czf - users | ssh <mac-user>@<mac-host> 'cat > ~/users.tgz'`, then on both sides `tar -tzf users.tgz | wc -l` (counts must match) and a `sha256` list comparison. This moves private data, so the owner runs it; I do not.
