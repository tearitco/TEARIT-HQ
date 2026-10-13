## Dwarf Fortress (TPMOS Mode)

A book:page, events-driven roguelike built on the TPMOS framework.

### Project Structure

| Path | Description |
|------|-------------|
| `project.pdl` | Project definition (metadata, state, responses) |
| `pieces/hero/piece.pdl` | Player dwarf entity (Durin Stonecutter) |
| `pieces/monster/piece.pdl` | Enemy goblin entity |
| `scripts/*.asm` | Game scripts (movement, mining, combat, rest, look, build, death) |
| `concept-bank/` | Gemma-evolving concept weights (mining, combat) |
| `layouts/` | Map layouts (.chtpm format) |
| `ops/` | External operation binaries (rng_op, dungeon_gen) |

### Entities

- **Hero**: Durin Stonecutter, a miner-dwarf with skills in mining (10), fighting (5), crafting (3)
- **Monster**: Goblin Scout, aggression 60, hp 15, str 10

### Scripts

| Script | Trigger | Description |
|--------|---------|-------------|
| `game_loop.asm` | on_start | Main turn loop, reads relay for input |
| `move.asm` | on_move | Cardinal direction movement |
| `mine.asm` | on_mining | Mining action with skill checks |
| `combat.asm` | on_combat | Dwarf-goblin combat with dodge/attack |
| `rest.asm` | on_rest | Rest to restore HP (costs hunger/thirst) |
| `look.asm` | on_look | Scan surroundings for threats |
| `build.asm` | on_build | Construct fortifications |
| `death.asm` | on_death | Death sequence with stats |

### State Files

- `.pdl` files use book:page format (`SECTION | KEY | VALUE`)
- `state.txt` for quick key=value lookups
- Both formats are written on every state mutation
