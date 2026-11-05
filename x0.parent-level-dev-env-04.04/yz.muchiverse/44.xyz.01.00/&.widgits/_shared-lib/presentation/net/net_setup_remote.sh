#!/bin/sh
# net_setup_remote.sh <user@host> - copy ONLY the three self-contained 044.pal-chat-irc ops sources to ~/phase2-net/src on another machine and compile them to ~/phase2-net/bin
# (no house, no xyzfs/users, no wallets). Works on Linux and macOS (cc). A password login: export SSHPASS=... first and pass --password as the 2nd argument (nothing is stored).
set -e
R="$1"; PW="$2"
SRC="$(cd "$(dirname "$0")/../../../.." && pwd)/044.pal-chat-irc"*"+2/ops"
S="ssh -o BatchMode=yes -o ConnectTimeout=8"; [ "$PW" = "--password" ] && S="sshpass -e ssh -o PubkeyAuthentication=no -o ConnectTimeout=8"
cd $SRC && tar cf - palnet_peer.c chat_post_message.c chat_inbox_watcher.c | $S "$R" 'mkdir -p ~/phase2-net/src ~/phase2-net/bin && cd ~/phase2-net/src && tar xf - && for f in palnet_peer chat_post_message chat_inbox_watcher; do cc -O2 -w -o ../bin/$f.+x $f.c && echo built $f; done'
