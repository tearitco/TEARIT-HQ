# vm-rv-li Deployment Guide

## Repository Structure

This project spans two independent GitHub repositories:

```
/home/debil/Desktop/github/
├── TEARIT-HQ/                    (main repo)
│   └── x0.parent-level-dev-env-04.04/yz.muchiverse/.../
│       └── @.apps/vm-rv-li/      (this app: manager + layout + docs)
│
└── RVLV_EMUL/                    (separate repo, standalone)
    ├── src/                      (emulator source)
    ├── build/
    │   └── main                  (compiled binary)
    └── images/
        ├── xv6-kernel.bin        (bootable kernel)
        └── xv6-fs.img            (filesystem)
```

## Local Setup (Development)

Both repos must be cloned as **sibling directories**:

```bash
cd /home/debil/Desktop/github

# Clone main repo
git clone https://github.com/tearitco/TEARIT-HQ.git

# Clone emulator repo separately (NOT as submodule)
git clone https://github.com/tearitco/RVLV_EMUL.git

# Both exist now:
ls -d TEARIT-HQ RVLV_EMUL
```

The config hardcodes the absolute path to RVLV_EMUL:
```
emu_root | /home/debil/Desktop/github/RVLV_EMUL
```

This works because both repos are checked out to the same location.

## Remote Deployment (Production)

When deploying to a different machine, **clone both repos to the same parent directory**:

```bash
# On target machine:
mkdir ~/projects/tearitco
cd ~/projects/tearitco

git clone https://github.com/tearitco/TEARIT-HQ.git
git clone https://github.com/tearitco/RVLV_EMUL.git

# Adjust config path as needed:
RVLV_EMUL_PATH=$(realpath RVLV_EMUL)
sed -i "s|emu_root .*|emu_root | $RVLV_EMUL_PATH|" \
  TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/.../vm_rv_li/config.pdl
```

## Why Not a Submodule?

Tried this initially, but issues arose:
- RVLV_EMUL is a large repo (500+ MB) with significant history
- It's maintained independently by tearitco/RVLV_EMUL
- Most use cases need both repos independently (different branches, separate test cycles)
- Submodule adds deployment complexity (must build binary in submodule)

**Alternative**: Future versions could use environment variables or a configuration pointer file (see `resolve_emu_root()` in manager code) for fully flexible asset location.

## Build & Launch

```bash
# Emulator (in RVLV_EMUL, once)
cd RVLV_EMUL && ./build.sh

# vm-rv-li (in TEARIT-HQ, on changes)
cd TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/.../vm_rv_li
sh build.sh   # compiles manager binary

# Launch window
HOUSE=$(pwd | sed 's|/@.apps.*||')
sh button.sh "$HOUSE"
```

## Config Adjustment Pattern

If RVLV_EMUL is cloned to a non-standard path, update `#.desktop/vm_rv_li/config.pdl`:

```bash
# Find the path
EMULATOR_PATH=$(realpath /path/to/RVLV_EMUL)

# Update config (line 1 of config.pdl)
sed -i "1s|emu_root .*|emu_root | $EMULATOR_PATH|" config.pdl

# Restart window (live config reload works for next spawn)
```

## Troubleshooting

| Issue | Check |
|-------|-------|
| "emulator exited (127)" | RVLV_EMUL not at expected path; verify config.pdl `emu_root` |
| Binary not found | Run `build.sh` in RVLV_EMUL root to generate `build/main` |
| xv6 boot hangs | Check `RVLV_EMUL/images/` for xv6-kernel.bin + xv6-fs.img (xv6 is default) |

## Future: Flexible Asset Location

The manager code includes `resolve_emu_root()` function to read a pointer file
(`shared/RVLV-ASSET-SOURCE.pdl`) if needed. This allows config to use a
placeholder (`emu_root | RVLV_EMUL_PATH`) that resolves at runtime. Not
currently active but available if deployment flexibility becomes critical.
