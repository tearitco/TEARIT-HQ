# TPMOS Project Progress Matrix
Date: 2026-06-24

This is a vision-based estimate of how far each major project has progressed toward its intended role in the TPMOS ecosystem.

Important:
- the percentages are subjective
- they are relative to the current vision, not just whether the code compiles
- they assume the project is "working" unless there is an obvious blocker or known mismatch
- they are intended to help future agents prioritize, not to serve as a release audit

## Topline Read

- Visual shell lane: promising but still early-to-mid stage
- Simulation / pets lane: farthest along in mechanics depth
- Editor / PAL lane: meaningful scaffolding, but still not the reusable core it wants to become
- Local LLM lane: useful and real, but still needs a cleaner boundary and tighter curriculum memory loop
- Network / LSR lane: the most aspirational and the least complete, by design

## Progress Table

| Project | Est. % | Current Role | Why It Scores Here |
|---|---:|---|---|
| `wraith-alpha` | 35% | Visual shell bridge | Parser -> RGB -> GL bridge exists, but the host/runtime contract is still alpha and not the final unified shell. |
| `chtmgl-alpha` | 60% | Current GL milestone family | This is the clearest current GL-facing milestone, with actual layouts and integration direction. |
| `chtmgl-beta` | 55% | GL sibling / follow-on lane | Close to alpha, but still positioned as a related milestone family rather than the anchor. |
| `fuzz-op` | 70% | Mechanics benchmark for pets / sim | Deep selector, z-level, turn, op, and AI-like behavior already exist; it still needs the cleaner reusable contract and broader network/editor convergence. |
| `op-ed` | 65% | Reusable editor core in progress | Strong architecture intent, current file baseline, thin-engine player, and event-editor roadmap; not yet the finished reusable editor. |
| `op-ed-gl` | 55% | GL editor variant | Clear GL editor direction, but still a variant lane rather than the canonical reusable editor core. |
| `bot-editor` | 35% | AI bot authoring lane | Important conceptually, but still relatively early compared with the main editor and simulation lanes. |
| `gem-dev` | 50% | Local agent shell | Real local-LLM interface and structured TPMOS integration are present, but the richer curriculum / distillation lane is still forming. |
| `cpp-llm` | 55% | Local agent with stronger interface contracts | More developed local agent integration and performance/cpu-safety language than the early shells, but still not the full vision. |
| `groq-ollama` | 50% | Alternate local-agent backend | Similar maturity to `cpp-llm`, with a different backend path and similar “useful but not finished” status. |
| `p2p-net` | 25% | Network / identity / multiplayer infrastructure | The roadmap is substantial, but the core ops and shared network contract are still early. |
| `lsr` | 30% | Civilization / economy / learning framework | Strong long-term roadmap and piece-based ambition, but the learning/network/map stack is still largely aspirational. |
| `chat-op` | 35% | Chat / communication lane | Clear project identity and direction, but not yet a major unified platform layer. |
| `quiz-engine` | 40% | Training / learning testbed | Useful for curriculum-style agent work, but still a supporting lane rather than a complete platform. |
| `man-pal` | 45% | PAL orchestration demo | Important for reusable scripting, but still narrower than the long-term PAL + editor vision. |
| `media-editor` | 35% | Media-side editor lane | Relevant as a shell/editor adjacent project, but not central to the core architecture right now. |
| `gltpm-demo` | 50% | Demonstration world | A decent proof-of-concept surface for GLTPM ideas, but still a demo lane. |
| `slop-ed-dev` | 40% | Editor / export experimentation | The roadmap is present, but this is still more of an experimentation lane than a finished platform. |
| `agy-text-editor` | 50% | Text editing baseline | Useful as a baseline dependency and interaction model, but not the final reusable editor story. |

## Lane Summary

### 1. Wraith / Visual Shell

Average progress: about 48%

This lane has visible architecture and a live bridge pattern, but it is still not the finished host shell that can cleanly absorb all future `chtmgl`-style work.

### 2. Simulation / Pets

Average progress: about 68%

This is the strongest mechanics lane. `fuzz-op` already proves a lot of the behavior stack, and `xo-pets` should become the cleaner reusable version of that idea.

### 3. Editor / PAL

Average progress: about 52%

There is real scaffolding and real intent, but the reusable editor core is still not fully settled. This is the main place where duplication must be avoided.

### 4. Local LLM / Curriculum

Average progress: about 52%

The local agent interfaces are real, but the “distill themes into curriculum pieces with weights” vision is still early. This lane is valuable because it can grow independently.

### 5. Network / LSR / Multiplayer

Average progress: about 28%

This is the largest long-horizon lane and the least complete. The strategic shape is clear, but the concrete implementation is still mostly roadmap.

## Practical Priorities Implied By The Scores

1. Keep the Wraith / `chtmgl` visual shell contract stable enough to host future work.
2. Use `fuzz-op` as the proven mechanics benchmark, but build `xo-pets` as the cleaner reusable contract.
3. Finish one reusable editor/PAL path before making more project-specific editors.
4. Keep the local FSM / mini-LLM chatbot lane separate and useful.
5. Delay heavy multiplayer / LSR coupling until the core state contract stops moving.

## Notes On Obvious Risks

- `op-ed` and `slop-ed-dev` are both clearly in-progress and should not be treated as finished editor products.
- The local LLM projects are useful, but they still need a tighter boundary around what is UI shell, what is model interface, and what is curriculum memory.
- The network lane is the most susceptible to premature complexity, because once identity and shared state harden, later refactors become expensive.

## Next Update Trigger

Revisit this matrix when one of these happens:

- Wraith hosts a `chtmgl` project cleanly and consistently
- `xo-pets` has a stable controller/editor contract
- the local chatbot can produce meaningful curriculum pieces
- `p2p-net` has a real small-network loop
