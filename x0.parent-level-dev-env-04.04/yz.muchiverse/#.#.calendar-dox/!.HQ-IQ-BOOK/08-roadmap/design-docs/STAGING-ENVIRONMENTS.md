# Staging environments: alpha, beta, live

Status: IN USE since 2026-10-07 (beta exists; alpha is the per-task scratch worktree). Written by claude after the owner asked "can't you do this on staging? alpha/beta? something?" about a user-data import that had been done on the live tree (it was backed up and verified, but it should have been rehearsed first).

## Tiers
- **alpha**: where new work is done. Local branch `claude-alpha` (cut from the verified staging branch, `aaecd89b4`), worktree `/home/no/staging/alpha`. Develop and test here; nothing is pushed or applied to live until it has been checked in beta. Throwaway experiments (a merge trial, a risky data step) still use a short-lived `alpha-<task>` worktree under the scratchpad.
- **beta**: the persistent staging (integration + verification) house, `/home/no/staging/beta`, a worktree on the local branch `claude-staging-opencode`. Holds the next integration (currently `origin/opencode` merged code-only: merge `01fcb2530` + fix `96ccd4dcf`) and a **copy** of the user data seeded from the latest backup, so merges, imports and checks can run against realistic data without touching live. Local only, never pushed.
- **live**: the owner's checkout (`/home/no/Desktop/github/work/NNEST-12.00`, branch `claude`) and its real `xyzfs/users`. Nothing lands here until it has been rehearsed in alpha or beta and verified.

## Rules (data and merges)
1. **Rehearse first.** Any merge, cherry-pick, data import, or script that rewrites files runs in alpha/beta before live.
2. **Data:** take the backup first (tarball + sha256 list outside the repo, count verified; AGENTS.md rule), seed beta's `xyzfs/users` from that tarball, run the operation there, compare the result with what live would get (sha256 list), and only then apply to live, **additively** (never overwrite a longer or diverged live file; "retain the good data").
3. A code merge never stages `xyzfs/users` files; the foreign copy is preserved on a local orphan branch (`user-import/<source>-<date>`, plumbing `commit-tree`, never pushed).
4. Promotion of beta to live (moving the owner's checkout onto the staging branch) is the **owner's decision**, through the throwaway-worktree routine in `USER-DATA-BRANCHES-DESIGN.md`.
5. Limits: beta has **no compiled programs** (they are in no branch; run `sh '$.crypts/button.sh' build` there if a build is needed) and it has not been tested running a desk **next to** the live one (shared display, per-window state); until that is tested, use beta for merges, data and headless checks, not for opening windows.

## Worked example (2026-10-07)
Live import done first, with backup `xyzfs-users-20261007T032848.tar.gz`: 3 files extended (live content kept as prefix) + 2 added in `0a9558a7...`. Rehearsal then replayed from that same backup into beta: all 5 files and the **whole 2033-file tree** matched live byte for byte (0 differing paths), which shows the procedure is reproducible. Next time the order is reversed: beta first, live second.

## Beta verification of the opencode merge (2026-10-07)
House-wide `button.sh build` in beta (low priority): 94 PASS, 2 FAIL, 0 timeout. (1) `^.hai-horn` link failure: `irl_bootstrap_fsm` and `obs_feedback_write` use libuuid and the script only linked `halo_chat_describe` with `-luuid`; fixed on the staging branch (`aaecd89b4`, now links `-luuid` for any source including `uuid/uuid.h`), horn rebuild exit 0, all ops built. (2) `_.monads/_.livedesk-taskbar/ops/build_db_hq.sh`: **stale on `claude` too, not caused by the merge**: it compiles `khtpm_hq_render.c`, which no longer exists (the renderer is `khtpm_core_render`); left untouched, candidate for removal. Tests: `^.hai-horn/scripts/halo_offline_test.sh` ALL PASS (tier-1 valid edit QUEUED_FOR_REVIEW; master_phd edit PROMOTED with no ledger consult, confirming the auto-promote behavior noted in HARNESS-BEHAVIOR-BANK-DESIGN 4e). NOT run: `network/tests/nb_layout_test.sh` (needs a running network-browser window; not run beside the live desktop), the `worker_*` C tests (no documented runner found), `irl_pipeline.sh` (needs the model endpoint).
