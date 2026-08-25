# Moke-Pet Living Handoff: Survival Evolution Track (v1.0)

## 1. Project State
The project has been upgraded to **v1.0** and has successfully implemented the core biological engine (Phases 1-5). 

### Current Features:
- **Metabolism Engine**: Complete cycle of `breathe`, `eat`, `rest`, and mortality.
- **Dynamic Ecosystem**: Perceptual `scan` Op + dynamic target eating.
- **Predation & Mortality**: Dead entities convert to `food`, enabling cannibalism/resource scavenging.
- **Mating Mechanics**: `mate` Op enables sovereign population growth.
- **RL Training**: `train` Op audits logs and updates `weights.txt`.

## 2. Structural Proofs
Everything is verified and passing.

## 3. Immediate Next Steps (For GUI Integration Agent)
1.  **Switch Context**: Use the current XO-PET bundle under `x0.moke-pet-project-2.2/x0.5-liz.fiter4-mew-00.02/`.
2.  **Manager Projection**: Implement the 'Thin Theater' pattern to publish `gui_state.txt`.
3.  **Input Routing**: Hook `keyboard_muscle.c` into `pieces/apps/player_app/history.txt`.
4.  **Bot5 Synergy**: Test the GUI against autonomous `bot5` piloting.
5.  **Snapshot/Replay**: Persist a frame/state snapshot every step so the world can rewind.
6.  **Control Semantics**: AI should advance one step by default unless `resume` is active.
7.  **Enclosure Ownership**: Keep the project root owning the enclosure, with controllers and pets living inside that enclosure as drop-in occupants.
8.  **Launcher Front Door**: Use `x0.moke-pet-project-2.2/button.sh` as the public entrypoint and keep the nested `button.sh` only as a compatibility wrapper.
9.  **Launcher Split**: Treat `run_xo-pet.sh` as the legacy PoC runtime and `xo-pet-launch.sh auto` as the descriptive launcher path to the same PoC.
10. **Runtime Reset**: Default the PoC launcher to `reset` so it clears keyboard/display scratch files on launch; allow `resume` to preserve them.
11. **Launcher Flags**: `button.sh` should route into `xo-pet-launch.sh auto` or `xo-pet-launch.sh controller <uid_pair>` without requiring the user to type an executable name.
12. **Controller Discovery**: Allow controllers to live at the project top level or under `controllers/`, but keep the executable itself in `+x/`.
13. **UID Fallback**: Auto-generate the controller uid pair when `controller` is used without a supplied pair.
14. **Autonomous State Logging**: Autonomous runs must log entity health/state fields, not just action names, so the world can be inspected without opening the controller UI.
15. **CHTPM Contract**: Preserve CHTPM-style layouts for both manual and autonomous projections so existing tools and test rigs keep working.

## 4. Usage
- **Init**: `./xo-pet-init.sh`
- **Build**: `./xo-pet-build-ops.sh`
- **Run**: `./button.sh auto`

*"Geography is destiny. The lizard corpse is just another piece of food in the directory tree."*
