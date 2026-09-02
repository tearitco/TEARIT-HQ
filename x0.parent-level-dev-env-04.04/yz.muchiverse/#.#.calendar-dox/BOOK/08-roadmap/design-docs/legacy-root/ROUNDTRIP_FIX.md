# ROUNDTRIP_FIX.md — Linux return-leg fix log

**Applies to:** 2026-08-23 Linux return from macOS 15.1.1 (24B91) leg.
**Symptom:** db-hq opens empty terminal; mutaclysm halfway opens then empty terminal; entity menus dead; book-stack Read chain dead; treetRace/neo dead.

## Root cause

The macOS leg rebuilt ~75 binaries as **Mach-O**. On Linux return these cannot execute. Every build script guards with `[ -x binary ] || gcc...`; the Mach-O files kept their exec bits, so rebuilds silently skipped them.

## Fix applied (already executed)

1. **Quarantined 76 Mach-O executables** → `/tmp/opencode/macho-quarantine/` (preserving relative paths, recoverable).
2. **House-wide recompile:** `$.crypts/compile-runner.sh` → 44/44 PASS.
3. **Manual rebuilds for exceptions outside compile-runner scope:**
   - `apply_theme_op.c` (no build script) → `gcc -Wall -O2 -o ops/+x/apply_theme_op.+x ops/apply_theme_op.c $(pkg-config --cflags --libs freetype2 x11)`
   - `Mar$.$treetRace.wsr]Q]k32` (outside house root) → compiled all `.c` files per source dir into local `+x/` output dirs.
   - `%.harnesses/file-menu+editor/ops/hm_assert_{file,kv}.c` (no build script) → `gcc -Wall -O2` directly.
4. **Verification:** `find | file | grep Mach-O` → 0 remain. Spot-checked all critical paths (ELF confirmed).
5. **Smoke test:** `sh $.crypts/button.sh reset` → 1 strip parser + 1 taskbar manager + 6 entities spawned (rc=0 each).

## Windows compile coverage (2026-08-23)

- **`$.crypts/compile-runner.ps1`** — PowerShell twin of the bash runner. Finds every `build.ps1` / `scripts/build.ps1` and runs each from its own directory. Writes `build-reports/<timestamp>/REPORT.md`.
- **33 `build.ps1` generated** — auto-translated from the corresponding `build.sh` scripts for projects that were missing them. 5 projects already had native `.ps1` (wsr-pal, mutaclsym 19.00, aomorai-editor, piececraft-xyz, board-viewer).
- **Syntax fixes applied post-generation:** `${CC:-gcc}` → PowerShell default, `@PKG_` placeholders expanded, broken Darwin guards removed.
- **Needs Windows/MSYS2 verification:** these are untested translations. Known risk areas: `pkg-config` array-splat in PowerShell, path separator handling in copy loops, `$(pkg-config ...)` subexpression syntax. Verify on a real Windows machine before relying on them for production builds.

## Why the house-wide recompile script is the right tool

- `$.crypts/compile-runner.sh` finds every `scripts/build.sh` and `ops/build_*.sh` in the house and runs each from its own directory.
- **Critical:** it must be run AFTER purging/quarantining Mach-O files. If run before, the `[ -x ]` guards see Mach-O as present+executable and skip silently — the exact trap that caused the breakage.
- It does NOT cover:
  - Projects outside the house root (e.g., `Mar$.$treetRace.wsr]Q]k32` at `yz.muchiverse/` level).
  - Sources with no build script (e.g., `hm_assert_file.c`, `hm_assert_kv.c`, `apply_theme_op.c`).
- Report lands at `$.crypts/build-reports/<timestamp>/REPORT.md`.

## Re-run instructions (future roundtrips)

```bash
# 1. Quarantine any non-ELF executables
H="<house_root>"
find "$H" -type f -perm /111 ! -path '*/pieces/sessions/*' ! -path '*_BACKUP*' ! -path '*.backup*' -print0 \
  | xargs -0 file 2>/dev/null | grep -v 'ELF\|PE32\|C source\|text\|script' | cut -d: -f1 \
  | while read -r f; do mkdir -p "/tmp/opencode/macho-quarantine/$(dirname "$f")"; mv "$f" "/tmp/opencode/macho-quarantine/$f"; done

# 2. House-wide rebuild
BUILD_TIMEOUT=600 sh "$H/$.crypts/compile-runner.sh"

# 3. Manual extras (if any)
# treetRace:
cd "$H/../Mar\$.\$treetRace.wsr]Q]k32" && for d in . ai dev xdb '+/' '$.m$rr.🔘️.®™]x2]ON!/'; do (cd "$d" && for f in *.c; do gcc "$f" -o "+x/${f%.c}.+x" -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lGLEW -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 -lassimp -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL; done); done

# hm_assert pair:
gcc -Wall -O2 -o "$H/%.harnesses/file-menu+editor/ops/+x/hm_assert_file.+x" "$H/%.harnesses/file-menu+editor/ops/hm_assert_file.c"
gcc -Wall -O2 -o "$H/%.harnesses/file-menu+editor/ops/+x/hm_assert_kv.+x" "$H/%.harnesses/file-menu+editor/ops/hm_assert_kv.c"

# apply_theme_op (if missing):
gcc -Wall -O2 -o "$H/*.monads/*.livedesk-taskbar/ops/+x/apply_theme_op.+x" "$H/*.monads/*.livedesk-taskbar/ops/apply_theme_op.c" $(pkg-config --cflags --libs freetype2 x11)

# khtpm_show_text (if missing — book-stack Read chain):
gcc -Wall -O2 -o "$H/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x" "$H/&.widgits/tile-picker/ops/khtpm_show_text.c"

# 4. Verify
find "$H" -type f -perm /111 ! -path '*/pieces/sessions/*' -print0 | xargs -0 file 2>/dev/null | grep -c 'Mach-O' || echo "0 Mach-O — clean"

# 5. Relaunch
sh "$H/$.crypts/button.sh" reset
```

## Rollback

If anything breaks after rebuild, the quarantine is recoverable:
```bash
# restore (rarely needed — only if sources were lost)
cp -r /tmp/opencode/macho-quarantine/* <house_root>/
```

Delete quarantine once confidence is high:
```bash
rm -rf /tmp/opencode/macho-quarantine
```
