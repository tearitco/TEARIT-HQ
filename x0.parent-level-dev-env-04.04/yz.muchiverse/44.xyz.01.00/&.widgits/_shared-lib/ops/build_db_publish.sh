#!/bin/sh
# build_db_publish.sh - builds db_publish into +x/ (publishes the open book's db to the db window mirror files).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/db_publish.+x db_publish.c
echo "OK +x/db_publish.+x"
