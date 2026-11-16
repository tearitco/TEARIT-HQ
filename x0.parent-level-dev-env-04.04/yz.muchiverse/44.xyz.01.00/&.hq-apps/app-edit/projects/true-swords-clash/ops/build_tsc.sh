#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/tsc_op.+x tsc_op.c -lm && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/palnet_peer.+x palnet_peer.c && echo "OK +x/tsc_op.+x +x/palnet_peer.+x"
