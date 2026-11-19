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
