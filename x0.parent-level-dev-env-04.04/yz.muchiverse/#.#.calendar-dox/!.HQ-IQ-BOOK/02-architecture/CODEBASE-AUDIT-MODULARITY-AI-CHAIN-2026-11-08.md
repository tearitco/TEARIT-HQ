# Codebase audit 2026-11-08: modularity, scriptability, AI synergy, and the AI / social / chain pipeline

Owner ask: audit major functions, ops and branches against the goals (modular, scriptable, AI-synergistic, teachable, clean reusable event-driven snippets and their blueprints), reviewing the AI / social / blockchain pipeline first; correct red flags in docs or code, then return to Teropoly.
**Method and honesty:** every number below was measured today (command class named in the row). Word matches and byte-identical hashes are crude: they find smells, they do not prove defects. `OBS` = I ran it, `DOC` = reasoning, not run. Nothing here changes game rules; fixes made today are listed in section 5.

## 1. The pipeline: designed vs built vs tested (this is the part to read first)
| Layer | Built (OBS 2026-11-08) | Tested | Gap |
|---|---|---|---|
| **Chain** (wallet, send, miner, escrow, Ed25519 `sig_op`, exchange, pet chain) | yes: `041.pal-chain/ops/*` (13 ops), `_shared-lib/ops/{hash_op,piece_mint,sig_op}.c` | `chain_signed` 30, `ev_chain_signed` 55, `chain_escrow_signed` 45, `pet_chain` 18, `exchange_hq` 34, all PASS today | wallet auth is an **unsalted SHA-256 password hash**; two such hashes are in PUBLIC git history (owner action to rotate, `16.game/WALLET-CONVENTIONS...` section 2); rotation not verifiable by me |
| **Social / network** (IRC, two machines, friends pane) | yes | `irc_two_machine` 19, `irc_friends_pane`, `palnet_peer_net` | `palnet_peer` binds 127.0.0.1 by default; `PALNET_BIND` opens it to a LAN (docs said "same-machine": corrected) |
| **AI** (the stated goal) | **mostly design.** `ai_describe.c` exists; `ai_fsm_transition` and `ai_goap_plan` (named in `17.ai/00-INDEX.md`) do **not**; no task router, no behavior bank, no `DATA` rows; tomom dormant; judges exist (`c_op_judge.c`, `entity_grade.c`, harness verdicts) and **no code outside the harnesses calls them** | judge-side cases pass | the flywheel has no first caller. `17.ai/AI-ROADMAP...` step A0 (a table of observed numbers) is not done, A1 (rated-bot move picker as the first caller) not started |
| **AI <-> chain link** (receipts: hash of a model pack or dataset, graduation records) | nothing | none | decision D22 is PROPOSED only; correct to keep it that way until A1 exists |
**Reading:** the chain layer is real, signed and tested; the social layer works across machines; the AI layer, the stated north star, is a stack of good designs with zero callers. More AI design docs would be the wrong next step; a first caller with a judge gate is (A0, then A1).

## 2. Red flags, ranked (each with evidence and the smallest fix)
| # | Flag | Evidence | Smallest fix | Owner call? |
|---|---|---|---|---|
| P0-1 | **New Python in the tree, `py_lint` red.** `&.hq-apps/network/tests/nb_probe.py` (227 lines) arrived with the opencode merge (commit `690cbdf61`); five `.sh` browser tests call it; it is not in `py_baseline.txt` and must not be added (rule: baseline only shrinks) | `py_lint` case: `NEW_PY ...nb_probe.py` (OBS) | port to a small C op (`nb_probe.c`) and the five `.sh` tests to one pal case; until then the red is honest. Do not delete it (five tests break) | no |
| P0-2 | **Wallet password hashes in public history; scheme is unsalted SHA-256** | `WALLET-CONVENTIONS` section 2 (RAN by an earlier session), origin is PUBLIC | rotate the `house` and `jb` passwords; before real value moves, a salted KDF in `chain_create_wallet.c` | **yes: rotate** |
| P1-1 | **AI pipeline has no caller** (section 1) | scans: 0 files for `ai_fsm_transition`, `ai_goap_plan`, `behavior_bank`, `task_router`, `micro_task`; judge callers outside harness: none | do A0 (observed-numbers table) then A1; keep the free-quota rule | scope |
| P1-2 | **Event pages fork one shell per step**: 3,821 tracked `cmd_N.sh` are compiled event-page commands (`exec cmd_1.sh`). Lead (DOC): this is why a daily phase page costs ~1.3 s of CPU while a shipped-OFF page costs ~6 ms. It also sits against the "no new .sh" rule | counts: 4,554 tracked `.sh`, 3,821 of them `cmd_N.sh`; earlier measured event-run costs | measure one page run with `strace -c`-style counts of exec; if confirmed, a C/pal event runner op executes the page rows without a shell per step (design-choice test first) | design |
| P1-3 | **Copy-paste instead of the house sharing rule** (1 consumer inline, 2+ text-include or op) | 83 groups / 248 byte-identical source files, 4.6 MB wasted: `stb_image.h` x10, `stb_image_write.h` x8, `yahoo-broker/system/{renderer,keyboard_input,orchestrator,gl_mirror}.c` copied into `@.apps/yahoo-app` and others (x9), `tk_type_text.c`/`tk_focus_item.c`/`tk_inject_key.c` x8-9 across `test-harn-*`, `my-biotech/system/chtpm_parser_pal.c` x3 | one canonical copy + text-include or op per group; add a `dup_lint` case with a baseline that only shrinks (like `cpu_lint`) so the number can only fall | no |
| P1-4 | **70% of ops are not named by any shared harness case** | 477 of 681 op basenames absent from `harness/**/*.pdl,*.pal` (crude: project-local `test-harn-*` are not counted) | a real coverage report (`ops_without_case`), baseline that only shrinks; start with `_shared-lib` (16 untested) and the taskbar (28) | no |
| P2-1 | Shared renderer is one file of 14,600+ lines (`khtpm_core_render.c`); both lanes edit the same functions (today: `handle_key`, `redraw_body`, `poll_agent_history`) | merge of opencode today | keep (text-include design, perf), add a function-group map to its header; any split goes through the design-choice test | design |
| P2-2 | **Runtime files without caps** in user data: `eden_history.txt` 18 MB and `event_page_out.txt` 13 MB, each duplicated again under `eden_game/store/objects` (37 MB raw, 2 MB xz); `events.txt` 4 MiB silent drop (known) | archive listing, `RUNTIME-FILE-GROWTH-STANDARD.md` | cap + second guard + a row in the standard; snapshot-then-rotate for `events.txt` as already specified | no |
| P2-3 | Branch and worktree sprawl | 15 worktrees under `/home/no/staging` and one in `.kilo`, ~40 refs, `.git` 643 MB | list is the finding; owner decides what to retire (nothing deleted by me) | **yes** |
| P3-1 | `yt_resolve.c` embeds the public YouTube InnerTube ANDROID client key (the one `yt-dlp` ships) | OBS: prefix `AIza`, 39 chars, comment says innerTube client | not an account secret; ToS stance on impersonating that client is the owner's | owner |
| P3-2 | Secret scan false positives | `sk-` matches inside `desk-copy`, `task-...` in 2 build reports and 1 case (OBS, values never printed) | none | no |

## 3. What is healthy (so the audit does not read as only bad news)
- Ledger-as-authority rule is enforced by cases that can fail (Teropoly: replay, ghost, audit); chain, escrow, exchange and pet-chain pass today.
- `spawn_lint` PASS (19); the harness suite is large and named per feature; every fix today carried a negative test.
- Real secrets: none found in tracked files besides the public client key above; wallets are gitignored and absent from `origin`.
- `main`, `grok`, `claude` are one fast-forward line now (`d06e0940c`); opencode's 24 browser commits merged with one auto-merged shared file.

## 4. Backup size: is `rezip-house.sh` zipping the smallest reasonable file? (OBS, archive of 2026-11-08 01:46, 77.8 MB, 722 MB raw, 96,073 entries)
The script is sound (excludes `.git`, binaries, assets, mp3, TSOTS paint, live logs; refuses wiped trees). The growth from ~60 MB is **new data it was never told about**, and PNG does not compress:
| Group | Raw | In git? | Compressed (xz-3 proxy) | Verdict |
|---|---|---|---|---|
| `&.widgits/concept-bank/data/img/draft/**` (78 AI draft PNGs, seeds in the names) | 15 MB | 6 of 78 tracked | ~15 MB (PNG) | exclude: regenerable, not source |
| `&.hq-apps/img-hai/state/**` (522 generated PNGs) | 5.5 MB | no, ignored | ~5 MB | exclude: generated state |
| `xyzfs/users/**` | 92 MB | no, ignored | ~8.4 MB (the 2 eden logs + store copies 3.9 MB of it) | owner call: this archive is the only off-git copy besides local `user/*` branches |
| `&.widgits/palettes/**/*.csv` (10,347 files) | 223 MB | tracked | ~15 MB | keep (source data) or regenerate; owner call |
| `pet-trainer/state/pets/*/art/*.obj` | 39 MB | no | 0.6 MB | harmless |
| `*/maps/*/cells.rgba` (non-TSOTS atlases) | 7 MB now | mostly tracked | 0.2 MB | harmless |
Proposed two lines (NOT applied: the live `rezip-house.sh` carries another agent's uncommitted edit and a commit would sweep it in): add `-xr'!*/concept-bank/data/img/draft'` and `-xr'!*/img-hai/state'` to `zip_once`. Expected saving ~20 MB (back near 60 MB). The archive is a snapshot; regeneration of drafts costs free-model calls, so say so in the script comment.

## 5. Corrected today (docs) and what remains
- `WALLET-CONVENTIONS` signing row: planned -> built (`sig_op`, 3 cases). `ROADMAP-2026-10-27` social row: `palnet_peer` is configurable (`PALNET_BIND`), two-machine IRC proven.
- Remains (owner/next agent): P0-1 port, P0-2 rotate, P1-1 A0/A1, P1-2 measure, P1-3 and P1-4 lints with shrinking baselines, P2-2 caps, the two rezip lines.
