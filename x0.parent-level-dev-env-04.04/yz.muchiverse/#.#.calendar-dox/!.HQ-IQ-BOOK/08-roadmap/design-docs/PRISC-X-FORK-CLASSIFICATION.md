# prisc+x.c fork classification (Phase A)

**2026-09-09. Read-only pass** for `PRISC-X-FORK-CONSOLIDATION.md` §5
Phase A. Every claim below was checked against the actual source
(`diff -u` of each variant vs `&.widgits/_shared-lib/system/prisc+x.c`,
kept in `/tmp/prisc_classify/` during the pass).

## Verdict up front

**The `_shared-lib/system/prisc+x.c` canonical (1408 L) is genuinely
the newest, most complete VM. Every project fork is *behind* it, not
divergent from it.** The consolidation is **upgrade-everyone**, as the
parent doc first assessed. Two forks (`wsr`, `egg`) additionally carry
`#ifdef _WIN32` shims that must be folded into the canonical first.

> Note for the record: a crude "count fork-side `+` lines that aren't
> comments" heuristic briefly suggested the forks had unique code. That
> was wrong — those `+` lines are the diff realigning around larger
> canonical rewrites (e.g. fork `char literal_arg[256]` vs canonical
> `char literal_arg[1024]` shows as a `+`/`-` pair for the *same*
> field). Reading the hunks settles it: the canonical is ahead.

## What the canonical has that the forks lack

From `cluster10.diff` (representative; the 10-project fork):

| canonical feature | forks | risk to adopt |
|---|---|---|
| **string-register subsystem** — `NUM_SREGS`, `sregs[]`, `sreg_idx()`, ~17 `OP_S*` opcodes (`OP_SLIT/SCPY/SFMT/SREAD/SWRITE/...`) | absent | **none** — additive by construction; a `.pal` that never uses an `s*` mnemonic runs byte-for-byte as before (canonical's own header + `string-ops.md`) |
| `OP_BNE` | absent | none — appended enum value |
| **`original[1024]` (was `[128]`)** | `[128]` — a real, known bug: 128 truncates *every* emoji-heavy absolute path in this house, so `exec <path>` / any literal-path op silently failed | **strict fix** — forks are the broken side |
| quote-aware trailing `#` comment stripping (line-annotated `.pal`) | absent | none — only strips an unquoted `#` after real content |
| `g_pal_dir` — relative `exec` targets resolve against the program-file dir (merged from mutaclysm) | absent | none — only affects relative exec targets, which the forks couldn't resolve anyway |
| `rs1`/`rs2` default-0 exec fix (shorter `exec` forms left a source reg at hardwired-zero) | absent | strict fix |
| 3-arg `read_history <path> xD, xS` (real position register; the 1/2-arg forms unchanged) | absent (cluster10) / named `read_history_str` (`wsr`) | none for existing 2-arg scripts; `wsr` needs a mnemonic rename |

## Per-fork table

| project(s) | variant md5 | verdict |
|---|---|---|
| `041.pal-chain`, `041.pal-forum`, `044.pal-chat-irc`, `102.editor-📄️00.00`, `0.user-pal/00.login-signup`, `0.user-pal/01.avatar-creation`, `@.apps/my-biotech`, `@.apps/my-lawyer`, `@.apps/myne-qrypto/qtc`, `*.START_BUTTON`, `002.zoo/…INK…PEN` | `30ab13606d3c` (1094 L) | **clean upgrade.** Behind + carries the `original[128]` path-trunc bug. |
| `101.mutaclsym…19.00` (muta-neo), `@.apps/my-chara-txt` | `a2c7e1c2675a` (1176 L) | **clean upgrade.** Closer to canonical (has `original[512]`) but still no string ops / `g_pal_dir`. |
| `045.muchi-pal-agent`, `101.ledger-player-npc-simple` | `d333b84ff495` (1119 L) | **clean upgrade.** |
| `101.lpns+map+4` | `cd840d0777a2` (1130 L) | **clean upgrade.** |
| `101.mutaclsym…+18.0G` (muta-old) | `580b1196c144` (1228 L) | **clean upgrade** — this dir is a stale duplicate of muta anyway (`19.00` is current). |
| `014.wsr-pal💸️📌️+2` | `d40e3b9e292a` (1255 L) | upgrade **after**: (1) fold its `#ifdef _WIN32` `wsr_asprintf` shim (`vsnprintf`-based `asprintf` for MSVC) into canonical; (2) its `read_history_str` mnemonic → canonical's 3-arg `read_history <path> xD, xS` (same feature). |
| `01.muchi-pals-🥚️-13.01` (egg) | `676f5a72e0b1` (1133 L) | upgrade **after** folding its `#ifdef _WIN32` `win_quote_arg()` (MSVC command-line arg quoting) + `PROJ_MAX_PATH` path-buffer widening into canonical. |
| `&.widgits/board-viewer/system/prisc+x` | binary only, no `.c` | rebuild from canonical once the above land (Phase D). |

## Blockers before any Phase B

1. **The canonical has never been built or run by any project** — no
   `build_*.sh` compiles `_shared-lib/system/prisc+x.c`. So the first
   project switch is also the first real exercise of ~250 lines of
   canonical-only VM code in a live pal context. Phase B step 1 is:
   build the canonical standalone, run it against a corpus of real
   `.pal` files from several projects, `diff` the output vs each
   project's current binary — **before** editing a single `build.sh`.
2. Fold the `wsr` + `egg` `#ifdef _WIN32` shims into the canonical
   (Linux behaviour must stay byte-identical — they are all
   `_WIN32`-guarded, so this is low-risk, but it must be a reviewed
   commit with a Linux A/B).
3. The `prisc-x-popen-custom-op-freeze` fork+exec/watchdog fix must
   land in the canonical (it was reverted at `4370fc13`; re-do it here,
   once).

## Recommended Phase B order (lowest stakes first)

`102.editor-📄️00.00` → `041.pal-forum` (CLI) → `@.apps/my-lawyer` /
`my-biotech` / `myne-qrypto` → `044.pal-chat-irc` → `041.pal-chain` →
`0.user-pal` ×2 → `muta-neo` → `lpns` / `ledger` / `agent` → then
`wsr`, `egg`, board-viewer (after the fold-ins). One project per
commit; KPI set = `PRISC-X-FORK-CONSOLIDATION.md` §6 A–D.
