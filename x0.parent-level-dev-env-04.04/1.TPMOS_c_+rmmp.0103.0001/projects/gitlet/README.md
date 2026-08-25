# GITLET — TPM-Compliant Local Version Control
# ==============================================
# Location: projects/gitlet/
# Purpose: Local, file-based version control with trunk+delta storage.
#          No git. No remote. Just local, auditable, TPM-compliant commits.

---

## QUICK START

```bash
# 1. Initialize a repo (stores at ~/gitlet/<project-hash>/.gitlet/)
./projects/gitlet/ops/+x/gitlet_init.+x my-project-001

# 2. Stage files
./projects/gitlet/ops/+x/gitlet_add.+x my-project-001 file1.c
./projects/gitlet/ops/+x/gitlet_add.+x my-project-001 file2.txt

# 3. Commit
./projects/gitlet/ops/+x/gitlet_commit.+x my-project-001 "Initial commit"

# 4. Check status
./projects/gitlet/ops/+x/gitlet_status.+x my-project-001

# 5. View history
./projects/gitlet/ops/+x/gitlet_log.+x my-project-001

# 6. See changes
./projects/gitlet/ops/+x/gitlet_diff.+x my-project-001 file1.c

# 7. Create branch
./projects/gitlet/ops/+x/gitlet_branch.+x my-project-001 feature

# 8. Switch branch / restore files
./projects/gitlet/ops/+x/gitlet_checkout.+x my-project-001 feature
```

---

## STORAGE MODEL: TRUNK + DELTA

Gitlet does NOT store full file copies on every commit. It uses **trunk + delta** storage:

| Commit | What's Stored | Location |
|--------|--------------|----------|
| 1st commit of a file | Full content (trunk) | `~/gitlet/<hash>/.gitlet/objects/<hash>` |
| 2nd commit of same file | Line-by-line diff (delta) | `~/gitlet/<hash>/.gitlet/deltas/<hash>` |
| 3rd+ commit of same file | Line-by-line diff (delta) | `~/gitlet/<hash>/.gitlet/deltas/<hash>` |

**Tree entry format:** `filename hash type trunk_hash`
- Trunk entries: `file.txt ABC12345 trunk`
- Delta entries: `file.txt DEF67890 delta ABC12345` (references its trunk)

**Checkout reconstruction:**
- Trunk: reads object directly
- Delta: reads trunk + applies diff line-by-line (`-` delete, `+` insert)

**Storage savings:** A file changed 100 times = 1 trunk + 99 deltas. Much smaller than 100 full copies.

---

## DIRECTORY STRUCTURE

```
projects/gitlet/
├── project.pdl                    # Project metadata
├── ops/
│   ├── gitlet_common.h            # Shared utilities (for development reference only)
│   └── +x/
│       ├── gitlet_init.c          # Initialize repo
│       ├── gitlet_add.c           # Stage files (trunk+delta)
│       ├── gitlet_commit.c        # Create commit
│       ├── gitlet_log.c           # Show history
│       ├── gitlet_status.c        # Show working tree status
│       ├── gitlet_branch.c        # Create/list branches
│       ├── gitlet_checkout.c      # Switch branches, restore files
│       └── gitlet_diff.c          # Diff working tree vs last commit
├── layouts/                       # (Future: CHTPM UI layouts)
└── pieces/                        # (Future: TPM pieces for gitlet state)
```

**Storage location (outside project dir):**
```
~/gitlet/<project-hash>/
└── .gitlet/
    ├── HEAD                       # Current branch ref
    ├── index                      # Staging area (cleared after commit)
    ├── objects/                   # Trunks, commits, trees
    ├── deltas/                    # Delta diffs
    └── refs/
        └── heads/                 # Branch refs
```

---

## FOR USERS

### Typical Workflow

```bash
# Before making changes — snapshot current state
./projects/gitlet/ops/+x/gitlet_add.+x my-project button.sh README.md
./projects/gitlet/ops/+x/gitlet_commit.+x my-project "Snapshot before editing"

# Make your changes...
echo "new content" > button.sh

# See what changed
./projects/gitlet/ops/+x/gitlet_diff.+x my-project button.sh

# Commit changes
./projects/gitlet/ops/+x/gitlet_add.+x my-project button.sh
./projects/gitlet/ops/+x/gitlet_commit.+x my-project "Updated button.sh"

# If something breaks — restore last good commit
./projects/gitlet/ops/+x/gitlet_log.+x my-project  # find commit hash
./projects/gitlet/ops/+x/gitlet_checkout.+x my-project <commit-hash>
```

### Branching

```bash
# Create a feature branch
./projects/gitlet/ops/+x/gitlet_branch.+x my-project my-feature

# List branches
./projects/gitlet/ops/+x/gitlet_branch.+x my-project

# Switch to feature branch
./projects/gitlet/ops/+x/gitlet_checkout.+x my-project my-feature

# Switch back to master
./projects/gitlet/ops/+x/gitlet_checkout.+x my-project master
```

---

## FOR AGENTS / DEVELOPERS

### Building Ops

Each op is **standalone C** — no headers, no shared build step:

```bash
cd projects/gitlet/ops/+x
gcc -o gitlet_init.+x gitlet_init.c
gcc -o gitlet_add.+x gitlet_add.c
# ... etc
```

### Adding a New Op

1. Create `projects/gitlet/ops/+x/gitlet_<name>.c`
2. Include all utilities inline (no `#include "gitlet_common.h"`)
3. Required utilities to inline:
   - `resolve_gitlet_root(project_hash)` — sets `GITLET_ROOT` to `~/gitlet/<hash>/.gitlet`
   - `hash_contents(data, size, hash_output)` — djb2 hash → 8-char alphanumeric
   - `read_file_contents(path, &size)` — reads file into malloc'd buffer
   - `get_current_commit(commit, size)` — reads HEAD → branch ref → commit hash
4. Compile: `gcc -o gitlet_<name>.+x gitlet_<name>.c`
5. Test in `#.test/` directory

### Key Design Decisions

1. **No headers** — Each op is self-contained. Duplicated code is acceptable for independence.
2. **Trunk + delta** — First commit stores full content. Subsequent commits store only diffs.
3. **Tree stores trunk reference** — Delta entries include `trunk_hash` so checkout knows which trunk to reconstruct from.
4. **Index cleared after commit** — The staging area is emptied after each commit. Previous versions are found in the commit tree, not the index.
5. **Storage at `~/gitlet/`** — Not in the project directory. Each project gets its own hash directory.

### Testing

```bash
cd #.test/
rm -rf ~/gitlet/test-xxx
../projects/gitlet/ops/+x/gitlet_init.+x test-xxx
# ... test your op ...
```

### Known Limitations & Future Improvements

1. **Diff algorithm** — Simple line-by-line. Not Myers algorithm. Works for small text changes.
   - **Why Myers matters:** Myers diff finds the *shortest edit script* — the minimum number of insertions and deletions to transform one file into another. Our simple line-by-line approach can produce bloated diffs when lines are reordered or moved. Myers handles moved blocks, reordered sections, and complex edits efficiently. For large files or complex refactors, Myers produces smaller, cleaner deltas — saving storage and making checkout reconstruction more reliable.

2. **No merge** — Branch switching works, but merging branches is not implemented.
   - **Why merge matters:** Merge allows two divergent histories to be combined. Without it, branches are isolated — you can switch between them, but you can't combine work done on both. A merge algorithm (three-way merge using a common ancestor) would let developers work on features independently, then integrate them. For TPMOS, this means multiple agents could edit different files on different branches, then merge their work into master without overwriting each other.

3. **No remote** — Purely local. No push/pull/fetch.
   - **Why remote matters:** Remote support enables collaboration across machines. Push sends your commits to another gitlet repo (on another machine, a server, or a shared drive). Pull fetches commits from a remote and merges them. For TPMOS, this means: (a) backup — push your work to a safe location, (b) collaboration — multiple developers can share commits, (c) CI/CD — automated systems can pull changes, build, and push results back.

4. **No .gitignore** — All files must be staged explicitly.
   - **Why .gitignore matters:** Projects have generated files (compiled binaries, temp files, build artifacts) that shouldn't be versioned. .gitignore patterns automatically exclude these, keeping commits clean and focused on source code. Without it, agents must manually exclude build artifacts, which is error-prone.

5. **Delta chain** — Deltas reference trunks directly, not previous deltas. No delta chains.
   - **Why delta chains matter:** Currently, every delta references the original trunk. This means checkout always reads the trunk + one delta — fast and simple. But it also means every delta stores the full diff from the trunk, which can be large if the file has changed significantly. Delta chains store each delta as the diff from the *previous version*, not the trunk. This makes each delta smaller (only the incremental change), saving more storage. The tradeoff: checkout must apply the entire chain (trunk → delta1 → delta2 → ... → deltaN), which is slower. For files that change frequently, delta chains save significant space. For files that rarely change, direct trunk references are faster. A hybrid approach (delta chains with periodic "re-base" to create new trunks) gives the best of both.

---

## HASH ALGORITHM

djb2 hash with 62-character charset (`0-9A-Za-z`). Output: 8 characters.
Collision probability is low for typical project sizes (< 1000 files).

---

## TPM COMPLIANCE

- Each op is a standalone `.+x` binary (Muscle)
- Storage uses file-based state (Mirror)
- No shared libraries or headers
- Can be called from PAL scripts: `OP gitlet::commit "message"`
- Future: CHTPM UI layouts in `projects/gitlet/layouts/`
