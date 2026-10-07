#!/bin/sh
# open_vm_rv_li.sh - HQ toys-menu launch entry point for vm-rv-li.
# The toys menu dispatch runs this via `sh -c` from CWD=house_root, and a
# leading '&' in a path is shell job-control, so the app under @.apps can't
# be named directly in the .pdl launch row (house rule §13). This wrapper
# lives at a glob-safe path, resolves the house root from $0, and execs the
# toy's button.sh. Mirrors open_mon.sh.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/@.apps/vm-rv-li/button.sh" "$HOUSE_ROOT"
