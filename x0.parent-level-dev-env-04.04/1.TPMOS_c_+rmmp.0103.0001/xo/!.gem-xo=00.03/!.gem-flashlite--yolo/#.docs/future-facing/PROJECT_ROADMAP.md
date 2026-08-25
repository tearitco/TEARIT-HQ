================================================================================
PROJECT_ROADMAP.md - Editor + Player Roadmap
================================================================================
Date: 2026-03-09
Status: HISTORICAL BASELINE (Superseded by Apr 1 Fondu roadmap)

## 2026-04-01 UPDATE (CURRENT DIRECTION - FONDU CLARIFIED)

Authoritative near-term sequence:
1. Step 1: `op-ed` save-to-file + save/load roundtrip + local play boot + `pal`/op-event no-code create/bind/test.
2. Step 2: AI ops layer (FSM/RL/small LLM adapters with scalable profiles).
3. Step 3: Pet expansion (chat, self-management, self-supervised training, multi-pet management, generator).
4. Step 4: Voice accessibility (voice control + voice readback; same canonical action path as joystick/keyboard).
5. Step 5: Foundation simulation game (turn-based civ/market/war/space/chem-physics).
6. Step 6+: networking, gl-os surfaces, PMOS install/versioning/remote-use, then long-horizon kernel portability.
7. **Step 7: Fondu Installer & Ops Registry** - Implementation of automated compilation, non-editable production deployments, and ops discovery.

### STEP 7: FONDU DETAILS (Apr 1 Clarified Spec)

**What Fondu Is:**
- Project lifecycle manager (active ↔ archived ↔ installed)
- Ops registry system (discoverable via `fondu --list-ops`)
- Automated deployment tool (compile + deploy + register)
- Modular C utility (dogfooding the architecture)

**What Fondu Is NOT (Yet):**
- Package manager with dependency resolution
- Remote repository system
- Versioning/rollback system
- App store UI

**Fondu Architecture:**
```
Project States:
  projects/                → Active (editable, in compile)
  projects/trunk/          → Archived (source only, not compiled)
  pieces/apps/installed/   → Installed (compiled, read-only)

Ops Registry:
  pieces/os/ops_registry/
  ├── user.txt            (ops from user project)
  ├── fuzz-op.txt         (ops from fuzz-op project)
  └── ops_catalog.txt     (human-readable catalog)
```

**Key Commands:**
```bash
fondu --install <project>     # Compile + deploy + register ops
fondu --uninstall <app>       # Remove + unregister ops
fondu --archive <project>     # Move to trunk (source only)
fondu --restore <project>     # Restore from trunk + recompile
fondu --list                  # Show all projects and states
fondu --list-ops              # Show all available ops
```

**The Ops Ecosystem:**
```
projects/user/ops/+x/         projects/fuzz-op/
├── create_profile.+x  ──────>│ manager/
├── auth_user.+x              │   └── PAL scripts call user ops
└── get_session.+x            │
```

**Implementation Priority:**
1. Directory structure (`trunk/`, `installed/`, `ops_registry/`)
2. Manifest files (`compiled_projects.txt`, `ops_registry/*.txt`)
3. fondu.c skeleton (command parsing, file ops, registry mgmt)
4. Test with dummy project
5. Implement user project ops (`create_profile`, `auth_user`, `get_session`)

Architecture assumptions to follow:
- Canonical containment: `projects/<project>/pieces/world_<id>/map_<id>/<piece_id>/`
- No extra `/pieces/` under `map_<id>/`
- Pieces may contain pieces (inventory/interior nesting)
- **Everything is a project** (location determines launch method, not type)
- **Ops are reusable** (projects expose ops, other projects call them via PAL)
