# Q008 — retire the old HORN transport `horn_chat_openrouter` (two HORN generations live side by side)

| field | value |
|---|---|
| status | open |
| tier | worker / outside-agent (opencode owns most of the files; coordinate) |
| size | S–M |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | whether opencode or claude does it (the old scripts are opencode's) |

## Mission (one sentence)

Make `^.hai-horn` have ONE transport: move every caller of the old single-provider `ops/horn_chat_openrouter.c` onto the multi-provider `ops/horn_chat_backend.c`, then delete the old file.

## Why this exists (read, not assumed — 2026-10-06)

`claude` replaced `horn_chat_openrouter` with `horn_chat_backend` (commit `2a60cbab2`). The merge of `origin/opencode` into `claude` (`354abbb86`) brought the old file BACK together with opencode's older launchers. It is NOT dead: these still call it (found with `git grep horn_chat_openrouter`):

- `^.hai-horn/horn_chat.sh` (old launcher: builds with plain gcc, runs the old binary)
- `^.hai-horn/halo_test_harness.sh`
- `^.hai-horn/horn_chat_test.sh`
- `^.hai-horn/horn_chat_main.pal` (comment only)
- 5 docs mention it (`git grep -l horn_chat_openrouter -- '*.md'`)

Already ported (do not redo): `^.hai-horn/halo_chat.sh` calls `ops/+x/horn_chat_backend.+x` (branch `claude-halo-pull`, `9028e0706`, merged into `claude`).
`scripts/build.sh` compiles every `ops/*.c`, so the old file still gets built; its stale-binary sweep only removes binaries whose `.c` is gone.

## Do

1. For each caller above, decide: port to `horn_chat_backend` (prompt as argv[1] starts fresh; `HORN_REPLY_FILE` / `pieces/horn/last_reply.txt` carry the reply) or delete it as obsolete (HORN's own e2e is `scripts/e2e.sh`; the supported launcher is the one `README.md` documents).
2. Run `bash scripts/halo_offline_test.sh` and `timeout 900 bash scripts/e2e.sh` before and after; both must still pass.
3. `git rm ops/horn_chat_openrouter.c`; run `sh scripts/build.sh` (it will also delete the stale `ops/+x/horn_chat_openrouter.+x`).
4. Fix the 5 doc mentions.

## Acceptance

- [ ] `git grep horn_chat_openrouter` finds nothing except history notes (e.g. the comment in `scripts/build.sh` and `halo_chat.sh`).
- [ ] `sh scripts/build.sh` exits 0; `bash scripts/halo_offline_test.sh` prints ALL PASS; HORN e2e passes.
- [ ] No key, token or raw provider response committed.

## Rules

Everything in `^.grave/README.md`. Do not delete the old file before the callers are ported or removed: that was checked on 2026-10-06 and four scripts would break.

## Log

2026-10-06 | claude | posted after the owner asked "kill it if it's dead"; it was not dead, so nothing was deleted.

## Result
