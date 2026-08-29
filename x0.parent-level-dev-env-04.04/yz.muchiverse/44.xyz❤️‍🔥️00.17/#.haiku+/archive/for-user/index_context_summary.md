# yz.muchiverse/2.muchi-verse-0.0 — Quick Reference Index

## Directory Structure & Projects

### Priority 0: Foundation (Don't Move, Don't Touch Unless Fixing)
- **shared-ops/** — Canonical copies of all shared code (prisc+x.c, chtpm_parser_pal.c, keyboard_input.c, palnet_peer.c)
- **shared-ops-manifest.txt** — Which project synced which file; "don't sync from here" exceptions
- **!.xyzos-standards.txt** — THE running rulebook; every real gotcha found across every project lives here; READ THIS FIRST, ALWAYS
- **#.dox/** — Family-wide design docs (USER-PAL-STANDARD.txt, PAL-CHAT-IRC-STANDARD.txt, PAL-FORUM-STANDARD.txt, PAL-CHAIN-STANDARD.txt, PAL-NET-STANDARD.txt)
- **#.haiku/** — Haiku agent context (user.txt, agent.txt, sonnet-handoff.txt, this file)
- **pal-scripts/** — Meta-orchestration layer (pal-chat-agent.pal, launch_irc_agent.sh); session-isolated instance launching

### Priority 1: Active Core Projects (The Now)
- **1.muchi-pal-agent🤖️** — Real chtpm chat agent; provider_kind: ollama/gemini/llamacpp/iqabod/script; owns CHAT-INTEGRATION-ARCHITECTURE.txt, MPC.txt, ROADMAP-models.txt
- **1.pal-chat-irc** — IRC-style chatroom (rooms/users/messages); real P2P via palnet_peer; has live irc-agent-0000 bot from this session
- **1.pal-chain⛓️** — Blockchain/wallet/mining pal project; already using palnet_peer for P2P networking
- **1.wsr-pal** — ASCII-to-RGB rendering (chtpm_rgb_render); future home of humans/organisms/animals/plants (per SCALE_BANK notes)
- **1.mutaclsym🧟‍♂️️** — Reference game project; has its own AI that section 4.4 wants modularized into registry+switch+ops pattern
- **0.gemma-only♦️** — Dedicated workspace for gemma-specific work; section 4.1 scaffolding lives here

### Priority 2: Secondary Projects (The Soon)
- **2.zoo_0000** — Drag-and-drop pet game (egg_window.c, zoo_window.c, gl_mirror.c); waiting on tasks #7/#8/#9 (palnet_peer wiring)
- **1.muchi-pals-🥚️-13.00** — Egg/zoo pet project (same as 2.zoo_0000, different naming era)
- **0.user-pal👤️** — Shared identity/login registry project
- **2.muchipal-editor-0.0** — Editor project (unclear current status)
- **2.avatar-creation👤️** — Avatar generation project (new this session)

### Priority 3: Future / Exploratory
- **1.pal-forum** — Forum project (posts/follows/DMs); foundational scaffolding still pending
- **3.piececraft-3d-pal** — 3D project
- **3.muchi-chatai-desk-pals** — Desktop pals
- **4.muchi-evo-pal, 4.muchi-civ-pal, 4.muchimon-pal, 5.yahoo-pal** — Various future projects
- **+.pal-platformer, +-.plats-8.01** — Platformer variants

## Key Files (Always, First)

### If working on chat agent:
- 1.muchi-pal-agent🤖️/CHAT-INTEGRATION-ARCHITECTURE.txt (design doc for everything about agents, providers, LLM-to-LLM distillation)
- 1.muchi-pal-agent🤖️/ROADMAP-models.txt (provider roadmap, training pipeline, corpus management)
- 1.muchi-pal-agent🤖️/MPC.txt (multi-provider configuration)

### If working on IRC:
- 1.pal-chat-irc/PAL-CHAT-IRC-STANDARD.txt (in #.dox/)
- 1.pal-chat-irc/ rooms/ subdirectory (real directories for each room, messages.txt inside)

### If working on P2P networking:
- shared-ops/palnet_peer.c (READ the header comment, don't guess what it does)
- #.dox/PAL-NET-STANDARD.txt

### If working on drag-and-drop:
- #.dox/ pal-net presence/discovery design doc (from task #5)
- 2.zoo_0000/egg_window.c, zoo_window.c, gl_mirror.c
- shared-ops/palnet_peer.c (will be called by drag wiring)

### If working on anything involving models/providers:
- 1.muchi-pal-agent🤖️/model_list.txt (registry of available providers)
- 1.muchi-pal-agent🤖️/system/prisc+x.c (the VM all .pal scripts run on)
- 1.muchi-pal-agent🤖️/pal/main_loop.pal (the main control script)

## The Core Tech Stack (One Paragraph)
prisc+x is a tiny custom VM (RISC-V-style registers, li/beq/j/custom-op bytecode) that runs .pal scripts. chtpm_parser_pal is the shared UI engine — real declarative <cli_io>/<button>/${piece_methods} layouts, drives BOTH terminal rendering and (via chtpm_rgb_render) real RGB pixel rendering from the SAME layout. Every project is: a chtpm layout (pieces/chtpm/layouts/*.chtpm) + a .pal script (the main loop, calls custom ops each tick) + a pile of small, self-contained C "ops" (each does ONE thing, owns its own root-resolution/constants, never includes shared headers) + plain pipe-delimited or key=value text files as ALL state (state.txt, context_log.txt, registry files) — no databases, no JSON-as-state, no sockets unless a project genuinely needs real networking (palnet_peer.c). [!user: prefer .pal to .c when possible when designing ops going forward]

## Standing Conventions (Violate = Reintroduced Bugs)

### Session Isolation (CRITICAL)
- Never launch directly against project root (breaks isolation)
- Always use button.sh `run` or replicate exactly (pal-scripts/launch_irc_agent.sh is worked example)
- Session dirs: pieces/sessions/<id>/ (created on launch, deleted on exit)

### Process Management (CRITICAL)
- KILL by /proc/$pid/cwd, never by argv (processes have identical command text across sessions)
- After directory move/rename: sweep for orphaned processes TWICE (once often isn't enough)
- PRISC_PROJECT_ROOT is cached at startup, not live (stale processes keep writing to old paths)

### Frame Rendering (CRITICAL)
- ONE visible frame writer: only chtpm_parser_pal writes to display/current_frame.txt (or project's own view.txt)
- Ops write their OWN view.txt, then ping frame_changed.txt

### VM Bug Class (CRITICAL)
- Several prisc+x opcodes had register-sentinel bug: unset fields defaulted to 0 (valid register) instead of -1 sentinel
- If adding new opcodes/forms, check this exact bug class before trusting it
- Affected opcodes: OP_READ_HISTORY, OP_EXEC (both fixed this session)

### Syncing Shared Code (CRITICAL)
- Always diff before overwriting (check shared-ops-manifest.txt for "don't sync" notes)
- NOT yet re-synced to all projects: prisc+x.c fixes (safe to sync whenever those projects are next touched)

### Design-Doc-First (NON-NEGOTIABLE)
- Every real feature gets a design doc written BEFORE code
- Read design docs IN FULL before building (don't skim, don't guess)
- Examples this session: CHAT-INTEGRATION-ARCHITECTURE.txt (whole system), pal-scripts/README.txt (meta-orchestration)

## The ONE Pattern Used Everywhere
**REGISTRY + SWITCH + SELF-CONTAINED OPS**

Examples:
- Provider selection: model_list.txt (registry) + switch_model.c (switch) + send_message.c (dispatcher to ops)
- Script routing: trigger_list.txt (registry) + script provider (switch/parse) + execute_tool.c (dispatcher)
- NPC behavior: npc_bank.txt (registry) + switch command (activate) + per-NPC script/fsm/rl (ops)
- Terrain generation: terrain_registry.txt (future, registry) + selector op (switch) + generator ops (do work)
- Scale fidelity: SCALE_BANK.txt (registry, entity_type|domain|scale_level|module_id) + config file (switch) + actual modules (ops)

When adding new swappable behavior ANYWHERE, use this pattern. Don't invent new patterns.

## Recent Changes (This Session, 2026-07-20)
- Fixed two VM bugs in prisc+x.c (register-sentinel defaults on OP_READ_HISTORY, OP_EXEC)
- Built irc-agent-0000 bridge from muchi-pal-agent into pal-chat-irc (ops/irc_agent_poll.c, ops/set_irc_agent.c)
- Added script provider_kind with trigger-word routing (pieces/registry/triggers/trigger_list.txt)
- Created pal-scripts/ meta-orchestration layer (pal-chat-agent.pal + launch_irc_agent.sh)
- Moved 4 projects into yz.muchiverse/2.muchi-verse-0.0/
- Renamed muchi-pal-llm → muchi-pal-agent
- Reorganized projects with priority prefixes (0/1/2/3) and emoji labels

## What's Designed But Not Yet Built (Section 2 of zest-er-summary)
- CHAT-INTEGRATION-ARCHITECTURE.txt sec. 2: generic ui_drive connector ops (session launch/inject-key/read-frame/kill)
- CHAT-INTEGRATION-ARCHITECTURE.txt sec. 4: multi-instance LLM-to-LLM distillation (irc-agent used simpler direct_ops path)
- Section 4.2: IQABELLA training loop (teacher/student chat, harvest, train, iterate)
- Section 4.4: mutaclsym AI modularization (registry+switch pattern)
- Section 4.5: terrain/leveling/evolution patterns
- Section 4.6: SCALE_BANK (entity_type|domain|scale_level fidelity configuration)

## Next Priority (Section 4.1)
**Gemma Tool-Use Scaffolding** — Teach gemma plain-text keyword convention instead of native JSON tools
1. Write keyword parser (reuse text_to_pal_prompt.c pattern)
2. Add tts_speak.c wrapping edge-tts
3. Reuse existing tool ops (file_ops, exec, list_dir, search_in_files, web_search)
4. New provider_kind: ollama_keyword (or extend ollama with sub-flag)
5. This unlocks IQABELLA training, API budget tiering, everything downstream
