# Network-browser wasm3 and the eden_loop expectation (2026-11-23)

## Job 1: the worker could not be rebuilt
Cause: `&.hq-apps/network/build.sh` links `yz.muchiverse/third_party/wasm3/source/libwasm3.a`; that folder was never in git (no commit, no deleted path, no other branch) and not on disk. The compiled `nb_js_worker.+x` was the 2026-10-05 binary, older than crypto.randomUUID, TextEncoder/Blob and element methods.

Fix: vendored upstream wasm3 (github.com/wasm3/wasm3, MIT, licence file kept) at main commit 28ecb9af6d2040e474a70f7cb7f43666740141fb. v0.5.0 was tried first and does NOT compile with `nb_host.h` (m3type_t, M3Module.memories, 3-argument ResizeMemory missing). Tracked: `third_party/wasm3/source/` (1.3 MB, C sources and headers only), `LICENSE`, `PINNED.txt`, `.gitignore` (`*.a`). `build.sh` now checks the source is present and builds `libwasm3.a` (500 KB, ignored) from a fixed file list (no WASI, uvwasi, tracer). Rebuilt both binaries to a scratch dir with the same compile lines, then copied over the stale ones (copies of the stale ones kept in the session scratchpad). Worker 1,399,432 -> 1,898,008 bytes; eval 1,074,584 -> 1,402,840.

Counts (passed/failed), stale binary then rebuilt:
| case | before | after |
|---|---|---|
| nb_crypto_random | 2/3 | 5/0 |
| nb_element_methods | 2/2 | 4/0 |
| nb_text_blob | 2/2 | 4/0 |
| nb_media | 12/8 | 20/0 |
| nb_atob | 4/0 | 4/0 |
| nb_probe | 4/0 | 4/0 |

The binaries are gitignored; a fresh clone runs `build.sh`.

## Job 2: eden_loop 304/305
Eden (plain words): a small farm-life game inside the desk. Participants (Asa, Ava, ...) live in a game folder, hens lay eggs that evolve, they dig finds, sell for coin, keep house, share a garden, save into 100 slots, and talk to each other; every fact is a ledger row, the screen is a projection.

Verdict: the EXPECTATION was stale, the code did not regress. Evidence: building the tomom source (`chatbot_moe_v1.c`, `curriculum_bank.txt`, `curriculum/`) from commit 513efbe94 (the case's commit, 2026-10-07) with srand(7) answers "shining cosmos millions the"; the same build from HEAD answers "We machines their biology polynomial academic skills learners skills learners the their". Between them `chatbot_moe_v1.c` changed (ce391169e cross-expert generation, 2026-10-19, among others) and the curriculum changed (1000 files). `path=tomom` always passed. I changed line 353 of `cases/eden_loop.pdl` to expect the current answer, with a NOTE row naming the evidence: eden_loop now 305/0. This pin will go stale again when tomom is retrained; a durable fix would compare the phone line with the `ask_log.txt` response (owner decision whether to do that).

## Regression
spawn_lint 19, vox 361, rta_walk 77 observed green mech_new 43, token_follow 23 (all observed).
