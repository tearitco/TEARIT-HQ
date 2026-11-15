#!/bin/sh
# builds the office example harnz (the pre-design book op). Run once after a clone.
cd "$(dirname "$0")" && mkdir -p +x && echo "-- office_stamp -> +x/" && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/office_stamp.+x office_stamp.c && echo "OK office_stamp"
