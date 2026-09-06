#!/bin/sh
# open_forum_cli.sh - thin wrapper so the network cell's forum row has
# the uniform `sh <launcher> <house_root>` contract every other
# launcher_network_* row uses. Forum is still the legacy CLI app in a
# terminal tab (open_network_app.sh) until forum-hq lands; this just
# supplies its fixed <key> <title> args.
HERE="$(cd "$(dirname "$0")" && pwd)"
exec sh "$HERE/open_network_app.sh" "${1:?house_root}" forum Forum
