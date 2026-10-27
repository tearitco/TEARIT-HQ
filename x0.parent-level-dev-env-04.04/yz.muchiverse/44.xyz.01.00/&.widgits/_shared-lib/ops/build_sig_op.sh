#!/bin/sh
# build_sig_op.sh - builds sig_op into +x/ (Ed25519 keygen/sign/verify via OpenSSL libcrypto).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/sig_op.+x sig_op.c -lcrypto
echo "OK +x/sig_op.+x"
