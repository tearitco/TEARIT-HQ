#!/bin/sh
# hash_gate.sh - real, hash-based incremental-compile gate, per
# EVENT-MODULARITY-AND-BUILD-SPEED.md §2 (2026-09-28, direct
# instruction: "surely could track last changed files thru a stored
# hash and only compile those... its not like it can break anything").
#
# Resolves that doc's own open question 3 in favor of a real, shared,
# SOURCED file: unlike this house's compiled-binary duplication
# convention (AI-caller ops, connect_op.c, etc. - see any of those
# files' own header comments for why THAT rule exists), a build
# script is never linked into a runtime binary, so there's no
# per-project drift risk to guard against by duplicating this logic -
# it's the same real shape as sourcing khtpm_css_parser.c via -I
# (SHARED-SOURCE-COMPILE-IN-PLACE.md), just for the build layer
# instead of the render layer.
#
# Granularity (resolves open question 1): one combined hash PER OUTPUT
# BINARY, over ALL of its real source inputs (including any shared
# -I'd files). This correctly handles SHARED-SOURCE-COMPILE-IN-PLACE.md's
# fan-out case for free - a shared file changing changes the combined
# hash of every binary that lists it as an input, so every affected
# binary rebuilds, with no separate dependency graph to maintain.
#
# Manifest (resolves open question 2): one plain house .pdl file,
# `.build_hashes.pdl`, living NEXT TO the build script that owns it
# (per-project, matching this house's own per-project-state
# convention) - never a house-wide single file, so two projects'
# build scripts can never collide on the same manifest.
#
# Usage, inside any build_*.sh, right before a slow compile step:
#
#   MANIFEST="$(dirname "$0")/.build_hashes.pdl"
#   . "$SHARED/hash_gate.sh"   # $SHARED = the _shared-lib dir, same
#                              # var every build_*.sh touching shared
#                              # source already computes
#
#   if hash_gate_stale "$MANIFEST" +x/some_binary.+x some_binary.c "$SHARED/some_shared.c"; then
#       $CC ... -o +x/some_binary.+x some_binary.c "$SHARED/some_shared.c" ...
#       hash_gate_commit "$MANIFEST" +x/some_binary.+x some_binary.c "$SHARED/some_shared.c"
#   else
#       echo "-- some_binary.+x up to date, skipping compile"
#   fi
#
# hash_gate_stale: exit 0 (shell "true") if a rebuild is needed
#   (binary missing, no manifest entry yet, or any source's combined
#   hash changed) - exit 1 if the existing binary is already correct
#   and the compile can be skipped.
# hash_gate_commit: call ONLY after a successful compile - records
#   the current combined hash so the NEXT run can skip it.
#
# Real, deliberate non-feature: no dependency tracking beyond "did any
# of these bytes change" - a source file's own #include of something
# NOT passed as an argument here is invisible to this gate (same real
# limitation `make` has without a real dep-scanner). Pass every real
# input file (including shared -I'd ones) explicitly, same as this
# house's own $CC invocations already list them explicitly today.

hash_gate_stale() {
    manifest="$1"; shift
    out_bin="$1"; shift
    [ -x "$out_bin" ] || return 0
    combined=$(cat "$@" 2>/dev/null | sha256sum | cut -d' ' -f1)
    [ -n "$combined" ] || return 0
    [ -f "$manifest" ] || return 0
    prev=$(awk -F' \\| ' -v k="$out_bin" '$1=="HASH" && $2==k {print $3}' "$manifest" | tail -1)
    [ "$prev" = "$combined" ] && return 1
    return 0
}

hash_gate_commit() {
    manifest="$1"; shift
    out_bin="$1"; shift
    combined=$(cat "$@" 2>/dev/null | sha256sum | cut -d' ' -f1)
    [ -n "$combined" ] || return 1
    tmp="$manifest.tmp.$$"
    if [ -f "$manifest" ]; then
        grep -v "^HASH | $out_bin | " "$manifest" > "$tmp" 2>/dev/null || : > "$tmp"
    else
        : > "$tmp"
    fi
    echo "HASH | $out_bin | $combined" >> "$tmp"
    mv "$tmp" "$manifest"
}
