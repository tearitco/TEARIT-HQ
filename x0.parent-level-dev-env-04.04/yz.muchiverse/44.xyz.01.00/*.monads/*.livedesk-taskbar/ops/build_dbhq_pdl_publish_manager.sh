#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p +x
echo "-- dbhq_pdl_publish_manager -> +x/dbhq_pdl_publish_manager.+x"
gcc -std=c11 -Wall -O2 -o +x/dbhq_pdl_publish_manager.+x dbhq_pdl_publish_manager.c
echo "OK +x/dbhq_pdl_publish_manager.+x"
