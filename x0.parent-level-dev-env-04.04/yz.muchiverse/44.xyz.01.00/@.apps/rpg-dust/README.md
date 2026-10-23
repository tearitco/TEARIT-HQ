# rpg-dust (R0 only, 2026-10-23)

A planned pets-like board that simulates a planet from a seed, solar system down to molecules. Design (paper): BOOK `16.game/RPG-DUST-DESIGN.md`. **There is no board, daemon or window yet.** What exists:

| File | What | Proof |
|---|---|---|
| `scale.pdl` | the scale ladder (10 levels), the universe `SEED` and the tweakable `PARAM` rows | `ops/+x/dust_check.+x ladder scale.pdl` -> `ladder ok: 10 levels, root system, seed 20261023, 5 params` |
| `reactions.pdl` | four hand-authored reactions (water, methane, photosynthesis, rusting) with public enthalpies | `ops/+x/dust_check.+x react ../../&.widgits/concept-bank/data/chem/elements.pdl reactions.pdl` -> `reactions=4 balanced=4 bad=0` |
| `ops/dust_check.c` | the two checks (pure: read, print, exit code) | harness cases below |
| `../../&.widgits/_shared-lib/chem_formula.c` | per-element atom counts from a formula (text-include; same grammar as `chem_label.c`, which is untouched) | used by `dust_check` |

Build: `sh ops/build_dust_check.sh` (`-Wall -Wextra -Werror`).
Harness (pal + locked cases, `&.widgits/_shared-lib/harness/`): `dust_ladder.pal` 61 checks, `dust_react.pal` 39 checks. Run with a many-args prisc VM (harness README). Negative tests are part of the cases; two deliberate code mutants (react detects only lost atoms; ladder allows an equal-size child) were each caught by the harness on 2026-10-23.

Prior art to read before building more: `&.widgits/physics-nodes/`, `&.widgits/solar-sandbox/`, `SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md`, `REAL-3D-PLANET-EXPLORATION-AND-REALTIME-MODE-DESIGN.md` (design section 2b lists them).
Not built: the seed hash (R1), any generator, zoom, the overrides ledger, anatomy, the board skin, the address grammar.
