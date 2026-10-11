# Note for the Mac house (MMEST.3000) from claude (the Linux house), 2026-10-07

**Who/what:** written by claude on the Linux desktop (10.0.0.238) for whoever (agent or the owner) works in `~/Desktop/MMEST.3000/...` on this Mac. The owner chose MMEST.3000 as the Mac's working house and the `claude` branch as what it follows.

## What already happened here
1. **xyzfs/users was installed** into this house: `xyzfs/users` now has 24 users, **2,425 files**, from archive `xyzfs-users-20261007-155128.tar.gz` (sha256 `acb5aa00a7b36348f0eb3f65fef324c14eba332e55574b290b5f930ed940bd56`, matched on both machines; the installer checked checksum, safe paths and case-collisions, and the file count after unpacking). The archive, its `.archive.sha256` and `install-xyzfs-users.sh` are in `~/Downloads/`. `xyzfs/users` is NOT tracked by git (git-ignored), so a `git pull` never brings or removes it.
2. The `claude` branch on origin now contains the week's work (merge `8dc911beb`): clock (recurrence/commands/chaining), `game_snapshot_op`, chain escrow/faucet, physics nodes, digipet, DSR step 1, ring board, Footrace Fu, **Eden** (Start/Stop game, 100 save slots), music daemon, `book_page_op`, delegation pilots, Groq robot chat backend, hq-ftp spec, LAN hosts doc. **None of it has been run on this Mac yet.**

## Do this, in order (nothing starts by itself)
1. `cd ~/Desktop/MMEST.3000/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00` and `git status` (tell the owner about any local edits BEFORE pulling). Then `git pull origin claude`. Do not use `git reset --hard`, `git clean` or `git stash`.
2. Build the compiled programs (they are in no branch): `sh '$.crypts/button.sh' build` (this runs `$.crypts/compile-runner.sh`; read `REPORT.md` it writes under `$.crypts/build-reports/`). macOS differences may break some builds (for example `MSG_NOSIGNAL`, `-luuid`, X11 paths): report failures, do not patch around them silently.
3. **Look at `xyzfs/users`** (24 folders; the owner's is `0a9558a7-7c74-4358-833c-2d5b21edc421`) before starting the desktop. APFS is case-insensitive and may normalize Unicode in names; the installer found no collisions and the count matched (2,425), so names should be intact.
4. **Keys are not here on purpose**: `&.widgits/open-hai/state/raw_groq.txt` and `openrouter_api_key.txt` were NOT copied (git-ignored secrets). The robot's Chat-groq row needs `raw_groq.txt` (one line, mode 600); the owner must create it.
5. Start the desktop only when the owner says so (`sh '$.crypts/button.sh'` ...). The page `pre-design:eden-test` exists in the owner's book on the Linux house (robot `eden_robot`; the Eden button is not placed yet).

## Rules that apply here (from AGENTS.md)
Commit only your own paths (`git add <path>`, never `git add -A`), on your own branch, never merge/push/force without the owner. Never `git add` anything under `xyzfs/users`. Never push `user/*` branches. Never report "fixed" without a fresh build + fresh run + real evidence.

## Reaching the Linux house
ssh keys: the Linux desktop can ssh here (`lfs.master@10.0.0.144`, key login set up today). The Mac can reach the Linux desktop at `10.0.0.238` only if its public key is added there; the owner decides. Host list: `09-appendix/LAN-HOSTS.md`. Longer term: `HQ-FTP-LAN-SYNC-SPEC.md` (LAN sync without GitHub; not built).

## Message the owner can paste to the Mac's agent
"Read ~/Downloads/MAC-NOTE-FROM-CLAUDE-2026-10-07.md (or 13.agent-coms/MAC-NOTE-FROM-CLAUDE-2026-10-07.md after the pull). xyzfs/users is already installed (2,425 files). Pull claude, run the build, report the build report and any failures, and do not start the desktop yet."
