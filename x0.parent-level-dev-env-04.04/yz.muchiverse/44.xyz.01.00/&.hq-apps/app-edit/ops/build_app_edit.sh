#!/bin/sh
# builds app-edit (the projector, the verbs and the window opener are ONE op)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/app_edit_op.+x app_edit_op.c && echo "OK +x/app_edit_op.+x"
