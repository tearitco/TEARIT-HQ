# LAN hosts that run (or can run) the house, for agents (written 2026-10-07 at the owner's request)

Source of the addresses: the owner's note `/home/no/Desktop/🧩️Piecemark-IT/#.QWEN-2025/!.scp/scp-ssh.txt` (not in this repo). **Credentials are deliberately NOT copied here**: this file is tracked and pushed. Read the note for the login details, or better, set up ssh keys (below). Owner statement (2026-10-07): "all ssh endpoints (debilu, jb) are all running house; could use ssh to go in".

| Name | Login | Address | Role / notes | Checked 2026-10-07 |
|---|---|---|---|---|
| this machine ("no") | local | 10.0.0.238 | main Linux dev desktop; the live house | n/a |
| **mac** | `lfs.master@10.0.0.144` | 10.0.0.144 | the Mac (macOS, Darwin x86_64, APFS ~41 GiB free). Note's label: "mac". Target of the xyzfs/users copy | reachable; ssh **password login only** (key login refused: `publickey,password`); `~/Downloads` exists; `shasum` and `tar` present |
| debil | `debil@10.0.0.16` (the note said `debilu@`; **`debil` is the working login**) | 10.0.0.16 | Linux x86_64, runs the house. Houses: `~/Desktop/github/TEARIT-HQ` (branch `opencode`) and `~/Desktop/github/TEARIT-HQ-merge`, each with the SAME 24 user ids but an OLDER xyzfs/users (1,749 files vs our 2,425) | key login OK (host key changed earlier, accepted by the owner after checking, 2026-10-07); archive + installer copied to `~/Downloads`, sha256 matched, install **dry-run passed for both houses with `--replace`, not applied** |
| jb desktop | `jb@10.0.0.187` | 10.0.0.187 | listed `{desktop}`; owner says it runs the house | key login OK (2026-10-07); houses not surveyed |
| starfive | `user@10.0.0.31` | 10.0.0.31 | listed `{starfive}` (board) | not tested |

## How agents should use them
- Prefer **key-based ssh**. On this machine: `ssh-keygen -t ed25519` once, then the owner runs `ssh-copy-id <login>@<host>` for each host (typing the password once). After that nothing needs a password in a file. `sshpass` is installed here (`/usr/bin/sshpass`) but passing a password is a stopgap.
- Never put a password on a command line or in a tracked file; use a key, or an environment variable for a single command, and unset it.
- Moving user data between machines (`xyzfs/users`) is the owner's decision each time; the session permission layer may block it. Procedure and verification: `HQ-FTP-LAN-SYNC-SPEC.md` sections 9, 10 and 16 (backup first, new empty target folder, count + sha256 comparison, nothing started until a person looks).
- A Mac shares APFS hazards (case-insensitive names, Unicode normalization); see the spec section 9.
- The co-lab room (`&.hq-apps/co-lab-hai`) is a local transcript on one machine with human approval; it does not cross the LAN. Cross-machine agent talk needs ssh or the future hq-ftp/palnet path.
- Update this table when a host is tested; keep the notes column factual.

## Mac houses and the xyzfs/users install (2026-10-07)
Key login to the Mac (`lfs.master@10.0.0.144`) and to `jb@10.0.0.187` was set up by the owner with `~/ssh-setup-lan.sh`; `10.0.0.16` first failed with a CHANGED HOST KEY warning; the owner verified and ran `~/ssh-setup-debil.sh`, login is `debil@`; starfive not tried.
Houses found on the Mac (folders containing `$.crypts`): `~/Desktop/MMEST.3000/.../44.xyz.01.00` (**no xyzfs/users**), `~/Desktop/OPEN_CODE/NNEST-11.17/.../44.xyz.01.00` and `.../44.xyz❤️‍🔥️00.17` (24 users each).
**Install procedure** (script: `$.crypts/install-xyzfs-users.sh`, POSIX sh, works on macOS and Linux, dry run by default, never overwrites existing users, `--replace` moves aside to `users.bak-<time>`): on the Linux house make `tar -C <house>/xyzfs -czf xyzfs-users-<time>.tar.gz users` plus `<archive>.archive.sha256`; copy the archive, its `.archive.sha256` and the script to the target (`scp`); on the target run `sh install-xyzfs-users.sh <archive> <house_root>` (dry run: checksum, safe paths, case-collisions, plan), then the same with `--apply`; it checks the file count after unpacking; then `sh '$.crypts/button.sh' build` (compiled programs are in no branch); start the desktop only after looking. Transfer of 2026-10-07 15:51 archive: 2,425 files, sha256 `acb5aa00...bd56` matched on both machines; dry run against `MMEST.3000` passed; **not applied** (owner picks the house).

