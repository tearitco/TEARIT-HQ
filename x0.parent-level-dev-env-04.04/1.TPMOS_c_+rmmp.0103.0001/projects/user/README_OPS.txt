================================================================================
USER PROJECT - Operations Dictionary
================================================================================
Location: projects/user/
Purpose: User profile management and authentication ops

================================================================================
AVAILABLE OPS (Callable from PAL scripts or other projects)
================================================================================

┌─────────────────────────────────────────────────────────────────────────────┐
│ create_profile.+x                                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│ Description: Creates a new user profile                                     │
│ Usage: OP user::create_profile <username>                                   │
│ Args: username (string, alphanumeric + underscore only)                     │
│                                                                             │
│ What it does:                                                               │
│   1. Validates username (alphanumeric + _ only)                             │
│   2. Creates directory: pieces/profiles/<username>/                         │
│   3. Creates state.txt with:                                                │
│      - username=<name>                                                      │
│      - created_at=now                                                       │
│      - session_count=0                                                      │
│      - last_login=never                                                     │
│                                                                             │
│ Example PAL script:                                                         │
│   OP user::create_profile "player1"                                         │
│                                                                             │
│ Output: "Profile 'player1' created successfully"                            │
│ Error: "Profile 'player1' already exists" (if duplicate)                    │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ auth_user.+x                                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│ Description: Authenticates a user and creates a session                     │
│ Usage: OP user::auth_user <username>                                        │
│ Args: username (string, must have existing profile)                         │
│                                                                             │
│ What it does:                                                               │
│   1. Checks if profile exists in pieces/profiles/<username>/state.txt       │
│   2. Creates session file: pieces/session/<username>.session                │
│   3. Updates profile session_count                                          │
│   4. Records login timestamp                                                │
│                                                                             │
│ Session file contains:                                                      │
│   - username=<name>                                                         │
│   - login_time=<unix_timestamp>                                             │
│   - status=active                                                           │
│                                                                             │
│ Example PAL script:                                                         │
│   OP user::auth_user "player1"                                              │
│                                                                             │
│ Output: "User 'player1' authenticated (session #1)"                         │
│ Error: "Profile 'player1' not found"                                        │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ get_session.+x                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│ Description: Retrieves current session data for a user                      │
│ Usage: OP user::get_session <username>                                      │
│ Args: username (string, must have active session)                           │
│                                                                             │
│ What it does:                                                               │
│   1. Reads session file: pieces/session/<username>.session                  │
│   2. Prints all session data to stdout                                      │
│                                                                             │
│ Example PAL script:                                                         │
│   OP user::get_session "player1"                                            │
│                                                                             │
│ Output:                                                                     │
│   Session data for 'player1':                                               │
│     username=player1                                                        │
│     login_time=1775106606                                                   │
│     status=active                                                           │
│                                                                             │
│ Error: "NOT_AUTHENTICATED: No active session for 'player1'"                 │
└─────────────────────────────────────────────────────────────────────────────┘

================================================================================
FILE STRUCTURE
================================================================================

projects/user/
├── project.pdl              # Project descriptor (exposes_ops = true)
├── layouts/
│   └── user.chtpm           # User management UI
├── manager/
│   └── +x/
│       └── user_manager.+x  # Interactive manager (polls input)
├── ops/
│   ├── +x/
│   │   ├── create_profile.+x
│   │   ├── auth_user.+x
│   │   └── get_session.+x
│   └── ops_manifest.txt     # Registered ops (for Fondu)
└── pieces/
    ├── profiles/            # User profiles (created by create_profile)
    │   └── <username>/
    │       └── state.txt
    └── session/             # Active sessions (created by auth_user)
        └── <username>.session

================================================================================
HOW TO USE IN OTHER PROJECTS (PAL Script Examples)
================================================================================

Example 1: Simple user registration and login
─────────────────────────────────────────────
# In projects/fuzz-op/scripts/register.asm

OP user::create_profile "hero_player"
OP user::auth_user "hero_player"
OP user::get_session "hero_player"

Example 2: Multi-player game setup
─────────────────────────────────────────────
# In projects/lsr/scripts/multiplayer.asm

# Register both players
OP user::create_profile "player1"
OP user::create_profile "player2"

# Authenticate both
OP user::auth_user "player1"
OP user::auth_user "player2"

# Verify sessions
OP user::get_session "player1"
OP user::get_session "player2"

Example 3: Session-aware gameplay
─────────────────────────────────────────────
# In projects/your-game/scripts/gameplay.asm

# At game start
OP user::auth_user "mycharacter"

# ... game logic happens ...

# Check session status periodically
OP user::get_session "mycharacter"

================================================================================
INSTALLATION VIA FONDU
================================================================================

To make user ops available to all projects:

  ./pieces/system/fondu/fondu.+x --install user

This will:
  1. Copy to pieces/apps/installed/user/
  2. Register ops in pieces/os/ops_registry/user.txt
  3. Add to ops catalog (view with: fondu --list-ops)

To check if user ops are installed:

  ./pieces/system/fondu/fondu.+x --list-ops | grep "user::"

================================================================================
TESTING THE OPS
================================================================================

Standalone test (no PAL script needed):

  # Create a profile
  ./projects/user/ops/+x/create_profile.+x projects/user testuser

  # Authenticate
  ./projects/user/ops/+x/auth_user.+x projects/user testuser

  # Get session info
  ./projects/user/ops/+x/get_session.+x projects/user testuser

Verify files created:

  ls projects/user/pieces/profiles/testuser/
  cat projects/user/pieces/profiles/testuser/state.txt
  cat projects/user/pieces/session/testuser.session

================================================================================
ADDING NEW USER OPS
================================================================================

To add a new user op:

1. Create C file in projects/user/ops/:
   - Follow CPU-safe pattern (fork/exec/waitpid)
   - Take project_path as first argument
   - Read/write only in projects/user/pieces/

2. Add to ops_manifest.txt:
   op_name|Description|args_format

3. Recompile:
   gcc -o projects/user/ops/+x/new_op.+x projects/user/ops/new_op.c

4. Reinstall via Fondu:
   ./pieces/system/fondu/fondu.+x --install user

================================================================================
