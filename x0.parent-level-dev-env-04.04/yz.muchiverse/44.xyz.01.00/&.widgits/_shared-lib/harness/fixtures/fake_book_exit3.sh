#!/bin/sh
# harness fixture: stands in for bp_book_op, prints the ok line but exits 3.
echo "bp_book: ok  events=0 pages=0 triggers=0 segments=0 shims=0 files=0  -> /nowhere"
exit 3
