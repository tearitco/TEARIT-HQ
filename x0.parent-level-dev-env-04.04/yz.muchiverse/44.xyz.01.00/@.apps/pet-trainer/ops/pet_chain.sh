#!/bin/sh
# pet_chain.sh - the pets' blockchain adapter. A pet's purse is its CONFIRMED chain balance minus its own pending outgoing.
# Verbs (pet_dir = a pet's state dir holding entity_uid.txt):
#   init                  make the chain if absent (idempotent)
#   wallet <pet_dir>      make the pet's wallet if absent; prints the wallet id (e + first 24 hex of entity_uid)
#   balance <pet_dir>     confirmed millicones
#   spendable <pet_dir>   confirmed - this wallet's pending outgoing
#   pay <pet_dir> <to_wallet_id> <mc>   refuses above spendable; else chain_send (pending until a block includes it)
#   mine <pet_dir>        ONE real block, niced, one miner at a time (flock), the chain enforces the daily cap; prints mined|capped|busy|failed
# Env: PET_CHAIN_ROOT (default <app>/state/chain), PET_CHAIN_DIFF / PET_CHAIN_CAP override mining.pdl (harness uses a tiny difficulty).
# The wallet secret lives in <pet_dir>/wallet_secret.txt (mode 600) and is NEVER printed or logged.
HERE="$(cd "$(dirname "$0")/.." && pwd)"; HOUSE="$(cd "$HERE/../.." && pwd)"
pdl() { awk -F'|' -v s="$1" -v k="$2" '{a=$1;b=$2;gsub(/^ +| +$/,"",a);gsub(/^ +| +$/,"",b)} a==s&&b==k{v=$3;sub(/#.*/,"",v);gsub(/^ +| +$/,"",v);print v;exit}' "$HERE/mining.pdl"; }
CID=$(pdl CHAIN id); BINDIR="$HOUSE/$(pdl CHAIN chain_bin)"
DIFF="${PET_CHAIN_DIFF:-$(pdl MINING difficulty_hex_zeros)}"; CAP="${PET_CHAIN_CAP:-$(pdl MINING daily_cap_blocks)}"
TMO=$(pdl MINING block_timeout_s); NC=$(pdl MINING nice)
ROOT="${PET_CHAIN_ROOT:-$HERE/state/chain}"; CH="$ROOT/chains/$CID"
op() { n="$1"; shift; PRISC_PROJECT_ROOT="$CH" "$BINDIR/ops/+x/$n.+x" "$@"; }
wid() { u=$(head -c 24 "$1/entity_uid.txt" 2>/dev/null); [ -n "$u" ] && echo "e$u"; }
V="$1"; PD="$2"
case "$V" in
 init) link() { mkdir -p "$ROOT"; [ -e "$ROOT/ops" ] || ln -s "$BINDIR/ops" "$ROOT/ops"; }   # chain_new makes <chain>/ops -> ../../ops; chain_send then finds chain_balance there
       link; [ -f "$CH/chain.pdl" ] && { echo "chain $CID ready"; exit 0; }
       PRISC_PROJECT_ROOT="$ROOT" "$BINDIR/ops/+x/chain_new.+x" "$CID" --kind user --difficulty "$DIFF" --cap "$CAP" --faucet-amount 0 >/dev/null 2>&1 && echo "chain $CID created" || { echo "chain $CID FAILED"; exit 1; } ;;
 wallet) sh "$0" init >/dev/null || exit 1; w=$(wid "$PD") || exit 1; [ -n "$w" ] || { echo "no entity_uid" >&2; exit 1; }
       if [ ! -d "$CH/wallets/$w" ]; then pw=$(head -c 18 /dev/urandom | od -An -tx1 | tr -d ' \n'); ( umask 077; printf '%s\n' "$pw" > "$PD/wallet_secret.txt" )
          op chain_create_wallet "$w" "$pw" >/dev/null 2>&1 || { echo "wallet create failed" >&2; exit 1; }; fi; echo "$w" ;;
 balance) w=$(wid "$PD") || exit 1; op chain_balance "$w" 2>/dev/null | head -1 ;;
 spendable) w=$(wid "$PD") || exit 1; b=$(op chain_balance "$w" 2>/dev/null | head -1); b=${b:-0}
       p=$(awk -F'|' -v w="$w" '$1=="TX"&&$2==w{s+=$4} END{print s+0}' "$CH/data/pending_tx.txt" 2>/dev/null); echo $(( b - p )) ;;
 pay) w=$(wid "$PD") || exit 1; to="$3"; mc="$4"; case "$mc" in ''|*[!0-9]*) echo "bad amount" >&2; exit 1;; esac
       sp=$(sh "$0" spendable "$PD"); [ "$sp" -ge "$mc" ] || { echo "insufficient: spendable $sp < $mc"; exit 2; }
       op chain_send "$w" "$to" "$mc" >/dev/null 2>&1 && echo "paid $mc to $to (pending)" || { echo "send failed"; exit 1; } ;;
 mine) w=$(wid "$PD") || exit 1; mkdir -p "$ROOT"
       out=$(flock -n "$ROOT/mine.lock" nice -n "${NC:-15}" timeout "${TMO:-180}" env PRISC_PROJECT_ROOT="$CH" "$BINDIR/ops/+x/chain_miner.+x" "$w" --blocks 1 2>&1); rc=$?
       case $rc in 0) echo mined;; 3) echo capped;; 1) [ -z "$out" ] && echo busy || echo failed;; 124) echo failed;; *) echo failed;; esac ;;
 *) echo "verbs: init wallet balance spendable pay mine" >&2; exit 1 ;;
esac
