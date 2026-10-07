# LAN hosts that run (or can run) the house, for agents (written 2026-10-07 at the owner's request)

Source of the addresses: the owner's note `/home/no/Desktop/🧩️Piecemark-IT/#.QWEN-2025/!.scp/scp-ssh.txt` (not in this repo). **Credentials are deliberately NOT copied here**: this file is tracked and pushed. Read the note for the login details, or better, set up ssh keys (below). Owner statement (2026-10-07): "all ssh endpoints (debilu, jb) are all running house; could use ssh to go in".

| Name | Login | Address | Role / notes | Checked 2026-10-07 |
|---|---|---|---|---|
| this machine ("no") | local | 10.0.0.238 | main Linux dev desktop; the live house | n/a |
| **mac** | `lfs.master@10.0.0.144` | 10.0.0.144 | the Mac (macOS, Darwin x86_64, APFS ~41 GiB free). Note's label: "mac". Target of the xyzfs/users copy | reachable; ssh **password login only** (key login refused: `publickey,password`); `~/Downloads` exists; `shasum` and `tar` present |
| debilu | `debilu@10.0.0.16` | 10.0.0.16 | listed in the note as an scp target (`~/Downloads/`); owner says it runs the house | not tested |
| jb desktop | `jb@10.0.0.187` | 10.0.0.187 | listed `{desktop}`; scp target `~/Downloads/`; owner says it runs the house | not tested |
| starfive | `user@10.0.0.31` | 10.0.0.31 | listed `{starfive}` (board) | not tested |

## How agents should use them
- Prefer **key-based ssh**. On this machine: `ssh-keygen -t ed25519` once, then the owner runs `ssh-copy-id <login>@<host>` for each host (typing the password once). After that nothing needs a password in a file. `sshpass` is installed here (`/usr/bin/sshpass`) but passing a password is a stopgap.
- Never put a password on a command line or in a tracked file; use a key, or an environment variable for a single command, and unset it.
- Moving user data between machines (`xyzfs/users`) is the owner's decision each time; the session permission layer may block it. Procedure and verification: `HQ-FTP-LAN-SYNC-SPEC.md` sections 9, 10 and 16 (backup first, new empty target folder, count + sha256 comparison, nothing started until a person looks).
- A Mac shares APFS hazards (case-insensitive names, Unicode normalization); see the spec section 9.
- The co-lab room (`&.hq-apps/co-lab-hai`) is a local transcript on one machine with human approval; it does not cross the LAN. Cross-machine agent talk needs ssh or the future hq-ftp/palnet path.
- Update this table when a host is tested; keep the notes column factual.
