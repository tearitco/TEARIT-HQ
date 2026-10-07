# 04 - WIN32 PORT HANDOFF

Where the HORN_CHAT Windows port stands, what is actually proved, what is
not, and what will bite whoever picks this up next.

Branch `attrition-win`, 6 commits ahead of `origin/attrition` (`153397a11`).
Everything below was verified on MSYS2 MinGW64 on this machine.

## WHAT THE PORT IS

HORN_CHAT is the piece the taskbar-launched `.chtpm` manager drives: a text
composer, a frame the renderer paints, and a turn loop that hands the
conversation to a provider, lets it call tools, and folds the results back
in. On Linux it is three cooperating processes reading each other through
files under `pieces/`.

The port target is win32 without losing the POSIX path. Not a rewrite, not a
shim layer - the same C, compiled both ways, with `#ifdef _WIN32` only where
the two platforms genuinely disagree about an operating-system fact.

## THE COMMITS

| Commit | What it establishes |
|---|---|
| `bcde71b14` | Survey of what the house already has for a tweakable `.pdl` sim manager. Read-only, but it is the map the rest of this follows. |
| `3f3dff18f` | `scripts/build.ps1`, and an explicit list of what does **not** build yet. |
| `905117cb7` | `exec` works in the shared PAL interpreter on win32, without a shell. |
| `3fc14d69d` | `horn_turn` runs on win32 without losing the POSIX path. |
| `a382896c2` | A real Windows sandbox for `horn_tool_exec`, plus a plain statement of what it cannot do. |
| `6d8ef2afd` | `write_file` can overwrite again; the e2e that proves it. |

## WHAT IS PROVED

```
powershell -File scripts/build.ps1          # all targets build, 0 warnings in ported sources
powershell -File scripts/e2e.ps1 -NoApi     # 35 passed, 0 failed
```

`e2e.ps1` boots the real three-process stack and drives real keystrokes
through the same `pieces/keyboard/history.txt` the app writes. Nothing is
mocked. It asserts on the real frame.

Green, from a fresh build and a fresh run:

- The stack boots; the parser forks the pal module; the first frame renders
  the box and its approval buttons.
- Typed characters accumulate in the composer.
- The tool allowlist holds: `list_dir` / `read_file` / `grep_files` work; an
  unregistered tool is refused; the session transcript is not searchable; a
  protected file errors instead of crashing.
- Write containment: `edit_file` refuses an ambiguous match and a missing
  one, honours `replace_all`, and `write_file` overwrites an existing file -
  while refusing the allowlist itself, `..` escapes, absolute paths,
  key-shaped files, and inventing directories.
- The Windows sandbox refuses by default and names all three of its gaps.
- With `HORN_WIN_SANDBOX=ack`, containment that genuinely exists is enforced:
  pinned working directory, restricted `PATH`, exit status reported honestly,
  a timeout that fires in 3.1s on a 30s command, no orphaned process after
  it, and a grandchild that cannot outlive its parent.
- Teardown leaves nothing running.

## THE FOUR BUGS THE E2E FOUND

All four were invisible from reading the code and obvious from driving it.
That is the argument for the harness, not for the author's patience.

**1. Every write was refused as an escape.** `resolve_write_path`
normalises `.` and `..` by prefixing every segment with `/`, the first one
included. On Linux that accidentally agrees with `project_root`, which
already begins with `/`. On Windows `project_root` begins with a drive
letter, so the result was `/C:/...` and the prefix test never matched. The
model could not write *anything* that existed. Fixed by dropping the leading
separator when the root has none, and comparing case-insensitively, because
Windows path comparison is.

**2. `write_file` could only ever create new files.** The whole design is
write-a-sibling-then-swap-it-in. MSVCRT's `rename` - which is what MinGW's
`<stdio.h>` binds to - fails with `EEXIST` when the destination exists, so
the swap silently did not replace. Every overwrite died with
`error: failed to write`, including editing a file the model had just
created itself. Now `MoveFileExW` + `MOVEFILE_REPLACE_EXISTING` on
`_WIN32`, plain `rename()` elsewhere.

**3. A drive-absolute path was not recognised as absolute.**
`"C:/Windows/Temp/x"` does not start with `/`, so the existing refusal
missed it and it was joined onto the root as
`C:/<project>/C:/Windows/Temp/x`. Contained either way - this was never an
escape - but reported as `does not exist`, which points the model at the
wrong problem entirely.

**4. The child's read-end pipe handle was inheritable.** The *write* end
must stay inheritable or the child's stdout is discarded in silence: the
command runs, exits 0, and every byte it printed vanishes. That much was
already right. The read end has no such need and was being leaked into
every child.

## WHAT IS NOT PROVED - READ THIS BEFORE TRUSTING A GREEN RUN

Three guarantees `e2e.sh` asserts on Linux **do not exist on Windows**, and
the harness deliberately SKIPs them with a printed reason rather than
translating them into weaker versions that would report green while the
guarantee is absent:

- **No read-only filesystem.** No equivalent of bwrap `--ro-bind / /`. A
  command can write anywhere the user can. The pinned cwd helps; it is not
  containment.
- **No network isolation.** No equivalent of `--unshare-net`.
- **The provider keys are reachable.** No equivalent of the tmpfs blanking.
  This is the single most important difference on this platform.

`run_script` therefore **refuses outright on Windows** unless
`HORN_WIN_SANDBOX=ack` is set, and the refusal names all three gaps. That
default is the mitigation. A test that quietly downgraded its own
expectations would undo it.

Also unproved: every **live** provider round-trip. `-NoApi` was used for the
recorded run. The harness judges provider availability by exit code, never
by grepping output - an earlier version grepped for `quota` while the probe
prompt itself contained that word, so the model's own reply tripped the
check and the live path was skipped while the summary still printed green.

## LANDMINES - EACH ONE COST REAL TIME

- **A leaked process presents as a compiler fault.** The pal module's
  process name is `prisc+x`; the `.pal` file is an argument to it, never a
  process name. The first teardown checked for `horn_main_loop.pal`, matched
  nothing, and reported `no leaked processes` with two `prisc+x` instances
  alive. They then held `system\prisc+x.exe` open, and the *next* build
  failed with `cannot open output file: Permission denied`. Kill by captured
  handle **and** by name.
- **`& op.+x` does not work from PowerShell.** It fails with
  `CantActivateDocumentInPipeline`. `Start-Process` is the only working
  route for an op on this platform. MinGW emits `+x`, not `.exe`.
- **Never wait on a constant for an async drain.** The first
  `Type-Str` waited on the `horn_prompt=` prefix, which is present *before*
  the first character lands, so the per-key sync returned instantly and the
  race ate characters: a run typed `What is 6 ties 7?`. Wait on length.
- **`$history` is an alias for `Get-History` in PowerShell**; assigning to it
  fails. Every path variable in `e2e.ps1` is `f`-prefixed for this reason.
- **`-Wformat-truncation` fires on every `snprintf` that appends to
  `PRISC_PROJECT_ROOT`**, because these checkouts sit under a very long
  OneDrive path. The build script reports warnings in ported sources as a
  failure and prints warnings in untouched sources as a note.
- **Matching on the word `warning` matches `0 warning(s)`.** The build gate
  failed a perfectly clean build while displaying only zero-warning lines. It
  now matches the count.

## OPEN - NOT ATTEMPTED

- **Live provider round-trip.** Untested here for want of quota, not because
  of a known fault.
- **`resolve_root` has no length guard.** If `PRISC_PROJECT_ROOT` exceeds
  `PATH_BUF` it truncates silently, and a truncated root points every path at
  the wrong file - a containment hole rather than a crash. `main` has already
  added exactly this guard in the WSR_PAL tree; it should be ported here.
  Not done because it is a security-relevant change to code the port does not
  otherwise touch.
- **`horn_completions.c` carries a pre-existing `-Wformat-truncation`
  warning** under MinGW, dating from the original v0.1 commit. Left alone
  rather than edited to make the harness green.
- **The three bwrap guarantees** above are permanent on this platform, not
  backlog. If a real Windows sandbox is ever wanted, it is a different piece
  of work, and `run_script`'s refusal is the correct state until then.

## REPRODUCING

```
cd .kilo/worktrees/attrition-win/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/^.hai-horn
powershell -File scripts/build.ps1
powershell -File scripts/e2e.ps1 -NoApi      # offline: 35/35
powershell -File scripts/e2e.ps1            # adds live provider assertions
```

MinGW64 must be reachable; `build.ps1` prepends `C:\msys64\mingw64\bin`
itself. Before diagnosing *any* build failure here, check for a live
`prisc+x` - see the landmines above.
