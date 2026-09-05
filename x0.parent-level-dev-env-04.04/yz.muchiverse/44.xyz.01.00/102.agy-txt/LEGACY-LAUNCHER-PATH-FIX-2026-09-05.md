# Legacy launcher path-bug fix — 2026-09-05

Direct live report: `sh button.sh r` in this directory failed with
`./system/renderer: not found` / `./system/keyboard_input: not found`.
Root cause (full writeup:
`#.#.calendar-dox/!.HQ-IQ-BOOK/11.brainstorm/2026-09-05/PDL-READER-AND-FILE-EXPLORER-WIDGET.md`
§4): `button.sh` `cd`s into a per-session directory before invoking
its own binaries, but several `./system/*`/`./ops/+x/*` calls were
still relative — left over from before a real refactor removed the
old symlink-into-session approach. The binaries themselves were never
broken.

## Every project checked (44.xyz.01.00/ root, house-level projects with their own `button.sh`)

| Project | Has this bug pattern? | Action |
|---|---|---|
| `101.drag-drop-test=ON🀄️` | No — doesn't use the session-dir + relative-exec pattern | none needed |
| `101.ledger-player-npc-simple+3` | No | none needed |
| `101.lpns+map+4` | No | none needed |
| `101.mutaclsym🧟‍♂️️+18.0G` | No — always `cd "$SCRIPT_DIR"` before its own relative `./system/gl_mirror` / `./ops/+x/generate_map.+x` calls, so they already resolve correctly | none needed |
| `101.mutaclsym🧟‍♂️️19.00` | No — defines `SESSION_DIR` but never `cd`s into it before a relative exec | none needed |
| **`102.agy-txt`** | **Yes** | **Fixed** — see below |
| **`102.editor-📄️00.00`** | **Yes** | **Fixed** — see below |
| `103.media-studio` | No `button.sh` present | n/a |
| `150.gl-canvas` | No | none needed |
| `151.screen-rec+01.02` | No | none needed |

## What was fixed

Both `button.sh` scripts: every `./system/renderer`, `./system/
keyboard_input`, `./system/gl_mirror`, `./system/chtpm_rgb_render`,
and `./ops/+x/*.+x` binary invocation (including inside the exit
`trap` and the `[ -x ... ]` existence checks guarding them) changed
from a bare relative path to `"$SCRIPT_DIR/..."`. `102.editor-📄️00.00`
additionally had a dead relative-then-absolute fallback pair collapsed
to just the (now always-correct) absolute check.

**Deliberately left relative**: the `pieces/chtpm/layouts/editor.chtpm`
argument passed to `chtpm_parser_pal` in both scripts. That's a
data-file path read by the C program itself, which resolves it via
the already-exported `PRISC_PROJECT_ROOT` env var, not by shell/cwd
lookup — changing it wasn't part of this bug and risked breaking a
resolution path that was already correct.

## Verified live, both projects (2026-09-05)

`DISPLAY=:0 NO_GL=1 RUN_PROFILE=app sh button.sh r`, backgrounded,
checked after 2s:

- **`102.agy-txt`**: `chtpm_parser_pal` and `keyboard_input` launch and
  stay resident with zero "not found" errors (empty log). `renderer`
  itself launches but exits immediately on its own — confirmed via a
  direct, isolated run (`timeout 2 .../system/renderer`, same env)
  that this is the binary's own behavior, not a path/launch failure;
  a separate, pre-existing question from the bug reported here, not
  chased further this pass.
- **`102.editor-📄️00.00`**: `renderer`, `chtpm_parser_pal`, and
  `keyboard_input` all launch AND stay resident, zero errors.

Both cleaned up after testing (`pkill` the spawned processes,
`rm -rf pieces/sessions/*`) — no leftover state from this verification
pass.
