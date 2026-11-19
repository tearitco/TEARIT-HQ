#!/bin/sh
# build_phys_field_op.sh - build the gravity-field physics driver (phys_field_op.c over ../phys_field.c, the shared body step)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/phys_field_op.+x phys_field_op.c && echo "OK +x/phys_field_op.+x"
# rule_patrol_op (2026-11-18, D93): the CLI face of the PATROL rule (rule_patrol.c over phys_field.c); one build line per rule op of the physics family, no new script
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_patrol_op.+x rule_patrol_op.c && echo "OK +x/rule_patrol_op.+x"
# rule_fireball_op (2026-11-18, D93): the CLI face of the FIREBALL rule (rule_fireball.c over rule_patrol.c over phys_field.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_fireball_op.+x rule_fireball_op.c && echo "OK +x/rule_fireball_op.+x"
# rule_stomp_op (2026-11-18, D93): the CLI face of the STOMP / HIT rule (rule_stomp.c over rule_patrol.c over phys_field.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_stomp_op.+x rule_stomp_op.c && echo "OK +x/rule_stomp_op.+x"
# rule_pickup_op (2026-11-18, D93): the CLI face of the pickup rule (rule_pickup.c over phys_field.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_pickup_op.+x rule_pickup_op.c && echo "OK +x/rule_pickup_op.+x"
# rule_goal_op (2026-11-18, D93): the CLI face of the goal rule (rule_goal.c over phys_field.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_goal_op.+x rule_goal_op.c && echo "OK +x/rule_goal_op.+x"
# rule_spawn_op (2026-11-19, D93): the CLI face of the spawn rule (rule_spawn.c over rule_patrol.c over phys_field.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_spawn_op.+x rule_spawn_op.c && echo "OK +x/rule_spawn_op.+x"
# rule_fall_op (2026-11-19, D93): the CLI face of the fall rule (rule_fall.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_fall_op.+x rule_fall_op.c && echo "OK +x/rule_fall_op.+x"
# rule_input_op (2026-11-19, D93): the CLI face of the input rule (rule_input.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_input_op.+x rule_input_op.c && echo "OK +x/rule_input_op.+x"
# rule_win_op (2026-11-19, D93): the CLI face of the win rule (rule_win.c over rule_goal.c)
cd "$(dirname "$0")" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/rule_win_op.+x rule_win_op.c && echo "OK +x/rule_win_op.+x"
