#!/bin/sh
# nb_write_autofill.sh - the autofill jar for the network browser.
#
# Two jobs, one script, so the on-disk format lives in exactly one
# place (the same reason nb_write_field.sh owns network_browser_fields.txt):
#
# ARGV CONVENTION (identical to nb_write_field.sh, and for the same
# reason - the renderer appends its own tail to EVERY action):
#   default_cli_io_run_action() runs  "<action> '<dir>' '<house>' '<typed>'"
#   so the action's OWN words come first, then dir, house_root, typed.
#
#   remember <name> <origin>   -> argv: 1=remember 2=name 3=origin
#                                    4=dir 5=house_root 6=typed
#       Called by a content <cli_io> row's action on Enter, right after
#       the live field commit. Appends "origin<TAB>name<TAB>value" to
#       #.desktop/network_browser_autofill.txt. LAST line per
#       (origin, name) wins, matching the fields-file convention.
#
#   autofill <name> <origin>    -> argv: 1=autofill 2=name 3=origin
#                                    4=dir 5=house_root
#       Read-back: prints the remembered value for this exact
#       (origin, name) pair on stdout, or prints nothing and exits 1
#       when there is none. The manager calls this per text-ish input
#       row to fill the field's initial content=.
#
# WHY PER-ORIGIN AND NOT PER-NAME
#   A field named "email" or "q" means different things on different
#   sites. Keying on the name alone would offer one site's address on
#   another - a real privacy leak, not a feature. The origin is
#   scheme://host[:port] (see the manager's autofill_origin()), so
#   http and https on one host are still separate jars.
#
# NEVER STORED: password fields. The manager simply does not call
# remember for type=password (see autofill_origin()'s call site), so
# nothing sensitive can land here even if a page asks.
#
# The jar is a plain text file in the house, readable and deletable by
# the user; the manager also clears it on an explicit "clearautofill:"
# request (surfaced as a sidebar row, same shape as Clear All History).
set -u
TAB="$(printf '\t')"
# NB: the house desktop dir is literally "#.desktop" (hash-dot), the same
# name network_browser_fields.txt and network_browser_checks.txt use. Get
# this wrong and every write silently lands in a sibling ".desktop" that
# nothing ever reads - which is exactly what the first test run caught.
JAR_REL='/#.desktop/network_browser_autofill.txt'

if [ $# -lt 5 ]; then
    echo "nb_write_autofill.sh: unexpected argc ($#), expected >= 5 (verb name origin dir house [typed])" >&2
    exit 1
fi
VERB="$1"
NAME="$2"
ORIGIN="$3"
# $4 is the renderer's per-instance dir, which we do not need; $5 is the
# house root. Named so the positional convention reads the same as
# nb_write_field.sh's own header.
ARG_DIR="$4"
HOUSE_ROOT="$5"

if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "nb_write_autofill.sh: bad house_root" >&2
    exit 1
fi
JAR="$HOUSE_ROOT$JAR_REL"

case "$VERB" in
    remember)
        if [ $# -ne 6 ]; then
            echo "nb_write_autofill.sh: remember needs the typed value (argc 6)" >&2
            exit 1
        fi
        if [ -z "$NAME" ] || [ -z "$ORIGIN" ]; then
            echo "nb_write_autofill.sh: remember needs a name and an origin" >&2
            exit 1
        fi
        # One line, tabs only as separators: a value containing a tab
        # would forge extra fields, so reject rather than write it.
        case "$6" in
            *"$TAB"*)
                echo "nb_write_autofill.sh: value contains a tab, not storing" >&2
                exit 1
                ;;
        esac
        printf '%s%s%s%s%s\n' "$ORIGIN" "$TAB" "$NAME" "$TAB" "$6" >> "$JAR"
        ;;
    autofill)
        [ -f "$JAR" ] || exit 1
        [ -n "$NAME" ] && [ -n "$ORIGIN" ] || exit 1
        # LAST match wins - walk the whole file and keep the last hit,
        # so a re-visited site gets the most recent value.
        found=""
        while IFS="$TAB" read -r o n v; do
            if [ "$o" = "$ORIGIN" ] && [ "$n" = "$NAME" ]; then
                found="$v"
            fi
        done < "$JAR"
        [ -n "$found" ] || exit 1
        printf '%s\n' "$found"
        ;;
    *)
        echo "nb_write_autofill.sh: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac
exit 0