# physics-nodes - gravity, orbit, escape velocity as swappable linked nodes

Design: `SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md` sections 3 and 8. Pipe rows, `#` comments, SI units (kg, m, s). Files: `system.pdl` (sample Sun/Earth/Moon/Mars, sources in comments), `physics_tunables.pdl` (default joints), `ops/phys_node_eval.c`.

| Row | Meaning |
|---|---|
| `CONST \| G \| 6.6743e-11` | the one place G lives |
| `BODY \| name \| parent \| mass \| radius \| a \| rotation_period` | `-` = no parent; `a` = semi-major axis around the parent |
| `NODE \| id \| kind \| in=a,b \| out=x,y \| impl=newton` | house default wiring. Kinds/impls: `gravity` newton, scaled; `orbit` kepler; `escape_velocity` standard (sqrt(2 g R), so it follows whatever gravity gives); any kind: `constant` (tunable `value`). Unknown impl = error |
| `LINK \| gravity.g -> escape.g` | `from_node.out -> to_node.in`. An input with no LINK is read from the body (`mass radius a rotation_period star_mass`) |
| `TUNABLE \| node \| key \| value` | named joint (`factor` for scaled, `value` for constant); `system.pdl` rows win over `physics_tunables.pdl` |
| `OVERRIDE \| scope=page:<page>\|planet:<body>\|system \| node \| impl=... \| key=value` | resolution order page > planet > system > house default; extra `key=value` are tunables for that override |

`ops/+x/phys_node_eval.+x <dir_with_system.pdl> <body> <node_id> [page]` (build: `gcc -Wall -Wextra -O2 -o ops/+x/phys_node_eval.+x ops/phys_node_eval.c -lm`). It pulls upstream nodes through LINK rows, so a changed tunable or swapped impl recomputes every downstream node. It prints and appends `OUT | body.node | key | value | impl=.. | h=<hash>` to `<dir>/data/phys_out.txt` (append-only, gitignored); a row is skipped when the latest row for that (body.node, key) has the same input hash (cache by input change, not mtime). Errors (unknown impl, node, body) exit 2 and append nothing. Harness: `_shared-lib/harness/phys_nodes.pal`.
