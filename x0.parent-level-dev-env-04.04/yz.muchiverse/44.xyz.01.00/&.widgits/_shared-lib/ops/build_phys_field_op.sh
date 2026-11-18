#!/bin/sh
# build_phys_field_op.sh - build the gravity-field physics driver (phys_field_op.c over ../phys_field.c, the shared body step)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/phys_field_op.+x phys_field_op.c && echo "OK +x/phys_field_op.+x"
