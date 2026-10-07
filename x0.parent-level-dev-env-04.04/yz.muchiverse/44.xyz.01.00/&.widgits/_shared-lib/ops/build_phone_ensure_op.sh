#!/bin/sh
# build_phone_ensure_op.sh - builds +x/phone_ensure_op.+x (the text-included khtpm_phone.c is the real code).
# Check it first:  ./+x/phone_ensure_op.+x --selftest      Dry run:  ./+x/phone_ensure_op.+x <house_root> --report FILE
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=c11 -Wall -Wextra -O2 -o +x/phone_ensure_op.+x phone_ensure_op.c
echo "OK +x/phone_ensure_op.+x"
