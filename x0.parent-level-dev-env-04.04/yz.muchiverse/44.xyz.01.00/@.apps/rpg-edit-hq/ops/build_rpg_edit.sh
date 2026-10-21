#!/bin/sh
cd "$(dirname "$0")"
mkdir -p +x
gcc -std=gnu11 -O2 -Wall -o +x/rpg_edit.+x rpg_edit.c -lm
echo OK "$PWD/+x/rpg_edit.+x"
