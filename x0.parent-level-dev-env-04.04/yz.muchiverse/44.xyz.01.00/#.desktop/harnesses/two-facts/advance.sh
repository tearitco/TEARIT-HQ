#!/bin/sh
# Advance one fact for one id. The weight file chooses which.
#   which=page     increments page_value
#   which=numeric  increments numeric_value
# The other value is left unchanged. A missing or unknown which
# exits 1 and writes nothing.
set -u
facts="${1:-}"
weight="${2:-}"
[ -n "$facts" ] && [ -n "$weight" ] && [ -f "$facts" ] && [ -f "$weight" ] || {
  echo "Usage: advance.sh <facts> <weight>" >&2
  exit 1
}
which=$(sed -n 's/^which=//p' "$weight" | head -1)
case "$which" in
  page|numeric) ;;
  *) echo "advance: which must be page or numeric" >&2; exit 1 ;;
esac
page=$(sed -n 's/^page_value=//p' "$facts" | head -1)
num=$(sed -n 's/^numeric_value=//p' "$facts" | head -1)
id=$(sed -n 's/^id=//p' "$facts" | head -1)
case "$page" in *[!0-9]*|"") echo "advance: page_value not decimal" >&2; exit 1 ;; esac
case "$num" in *[!0-9]*|"") echo "advance: numeric_value not decimal" >&2; exit 1 ;; esac
if [ "$which" = page ]; then page=$((page + 1)); else num=$((num + 1)); fi
{
  printf 'id=%s\n' "$id"
  printf 'page_value=%s\n' "$page"
  printf 'numeric_value=%s\n' "$num"
  printf 'advanced=%s\n' "$which"
} > "$facts"
echo "ADVANCED $which id=$id page=$page numeric=$num"
