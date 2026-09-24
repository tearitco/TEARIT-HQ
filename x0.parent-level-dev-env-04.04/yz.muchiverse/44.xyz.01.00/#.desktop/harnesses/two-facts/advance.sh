#!/bin/sh
# Advance one fact for one actor id. weight.txt which=page|numeric
# chooses. facts.txt is a manifest:
#   id, page_file, page_mirror, page_key, numeric_file, numeric_key
# page_value is one line inside that actor's ACTOR record. The mirror
# (actors.pdl) gets the same line so the fallback copy stays even.
# numeric_value is numeric_key=N in the pal's numeric file.
# Unknown which exits 1 and writes nothing.
set -u
facts="${1:-}"
weight="${2:-}"
[ -n "$facts" ] && [ -n "$weight" ] && [ -f "$facts" ] && [ -f "$weight" ] || {
  echo "Usage: advance.sh <facts> <weight>" >&2
  exit 1
}
kv() { sed -n "s/^$1=//p" "$facts" | head -1; }
which=$(sed -n 's/^which=//p' "$weight" | head -1)
case "$which" in
  page|numeric) ;;
  *) echo "advance: which must be page or numeric" >&2; exit 1 ;;
esac
id=$(kv id)
page_file=$(kv page_file)
page_mirror=$(kv page_mirror)
page_key=$(kv page_key)
numeric_file=$(kv numeric_file)
numeric_key=$(kv numeric_key)
[ -n "$id" ] && [ -f "$page_file" ] && [ -f "$numeric_file" ] || {
  echo "advance: manifest paths missing" >&2
  exit 1
}

actor_get() {
  awk -F'|' -v id="$id" -v key="$page_key" '
    { gsub(/^[ \t]+|[ \t]+$/, "", $1); gsub(/^[ \t]+|[ \t]+$/, "", $2); gsub(/^[ \t]+|[ \t]+$/, "", $3) }
    $1=="ACTOR" && $2=="id" { inrec=($3==id); next }
    inrec && $1=="ACTOR" && $2==key { print $3; exit }
  ' "$1"
}
num_get() { sed -n "s/^${numeric_key}=//p" "$numeric_file" | head -1; }

page=$(actor_get "$page_file")
num=$(num_get)
case "$page" in *[!0-9]*|"") echo "advance: page fact missing" >&2; exit 1 ;; esac
case "$num" in *[!0-9]*|"") echo "advance: numeric fact missing" >&2; exit 1 ;; esac
if [ "$which" = page ]; then page=$((page + 1)); else num=$((num + 1)); fi

set_actor() {
  local file="$1" val="$2"
  [ -f "$file" ] || return 0
  local tmp
  tmp=$(mktemp)
  awk -F'|' -v id="$id" -v key="$page_key" -v val="$val" '
    {
      line=$0
      a=$1; b=$2; c=$3
      gsub(/^[ \t]+|[ \t]+$/, "", a)
      gsub(/^[ \t]+|[ \t]+$/, "", b)
      gsub(/^[ \t]+|[ \t]+$/, "", c)
      if (a=="ACTOR" && b=="id") inrec=(c==id)
      else if (inrec && a=="ACTOR" && b==key) {
        printf "%-12s | %-18s | %s\n", "ACTOR", key, val
        next
      }
      print line
    }
  ' "$file" > "$tmp"
  mv "$tmp" "$file"
}
set_actor "$page_file" "$page"
set_actor "$page_mirror" "$page"
{
  printf 'id=%s\n' "$id"
  printf '%s=%s\n' "$numeric_key" "$num"
} > "$numeric_file"
echo "ADVANCED $which id=$id page=$page numeric=$num"
