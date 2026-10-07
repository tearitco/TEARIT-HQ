# Commit Discipline: Separate by Type

**Rule**: Always commit code and documentation in separate commits. Group related files logically.

## Pattern

When working on a feature with both code and docs:

1. **Code Commit** — all .c, .h, .js, .xhtpm, .css, .sh, .pdl (scripts/config)
   ```bash
   git add path/to/*.c path/to/*.xhtpm path/to/*.sh ...
   git commit -m "scoped feature: [description]"
   ```

2. **Documentation Commit** — all .md files
   ```bash
   git add path/to/*.md
   git commit -m "docs: [description]"
   ```

3. **Optional: Test Commit** — if adding significant tests
   ```bash
   git add path/to/tests/*
   git commit -m "tests: [description]"
   ```

## Why

- **Reviewability**: Code changes are reviewable separately from doc updates
- **Rollback safety**: Can revert docs without touching code, or vice versa
- **Clarity**: Commit message accurately describes what changed
- **History**: Future `git blame` and `git log` searches are cleaner
- **House standard**: Matches the pattern used in CENTROID_GOLD_STD and other apps

## Example: vm-rv-li (Oct 5, 2026)

```
914bb467f scoped feature: vm-rv-li emulator integration (khtpm window + manager)
          7 files: manager.c, xhtpm, css, build.sh, button.sh, toy.pdl, input.sh

2807b0cc4 docs: vm-rv-li guides (architecture, testing, deployment, assets)
          4 files: README.md, ASSET_STATUS.md, DEBUG_QUICKREF.md, DEPLOYMENT.md
```

## What NOT to Commit

- Runtime state files: `ui.txt`, `*.txt` in `#.desktop/`, `cli_io_active.txt`, etc.
- Compiled binaries: `*.+x`, `*.exe`, `build/main` (use .gitignore)
- Temporary logs: `*.log`, `emu.log`, etc.
- Editor temp files: `.swp`, `.swo`, `*~`, etc.

See AGENTS.md §Commit Discipline for full rules (no uncommitted work at session end).

## Verification Before Committing

```bash
# See exactly what will be committed
git diff --cached --name-only

# Verify it's ONLY the files you intended
# (no stray .log, .txt state files)

# Then commit
git commit -m "..."
```
