#!/bin/bash
# ensure_book_mount.sh - reliable auto-mount of the shared book-asset
# drive, source-able by any book-stack reader branch AND the openall
# launcher. Replaces the old one-shot ensure_mount() in
# $.crypts/scrypts/openall/run.sh, which failed at login because it
# called udisksctl exactly once while the polkit local session was still
# coming up as Active (=> silent deny), then checked only after a fixed
# 1s sleep and never retried.
#
# Source, don't exec: each caller needs this to return into its own
# context while defining the functions below. Usage:
#     . /path/to/ensure_book_mount.sh
#     ensure_book_mount
#
# The book assets live on an internal partition (UUID below), mounted by
# udisks2 at /media/no/<uuid>. findmnt -S resolves the real mount target
# by device, so we never hardcode a mountpoint udisks might place
# elsewhere.

BOOK_ASSET_UUID="b7ced73c-5231-4462-b98d-64e38fe2df9e"

# Real mount target of the book-asset device ("" if not mounted).
book_asset_mountpoint() {
    findmnt -n -o TARGET -S "/dev/disk/by-uuid/$BOOK_ASSET_UUID" 2>/dev/null | head -1
}

book_asset_mounted() {
    local mp
    mp="$(book_asset_mountpoint)"
    [ -n "$mp" ] || return 1
    mountpoint -q "$mp" 2>/dev/null
}

# Attempt to mount the book-asset drive. Idempotent + silent on success.
# Prints a BOOK_MOUNT_OK line; on failure prints BOOK_MOUNT_FAIL to
# stderr and returns 1 (callers decide whether to hard-fail or warn).
ensure_book_mount() {
    local i j tries mp
    if book_asset_mounted; then
        echo "BOOK_MOUNT_OK $(book_asset_mountpoint) (already mounted)"
        return 0
    fi
    tries=8
    for i in $(seq 1 "$tries"); do
        udisksctl mount -b "/dev/disk/by-uuid/$BOOK_ASSET_UUID" >/dev/null 2>&1
        # Mount may lag behind the command returning; poll briefly.
        for j in 1 2 3 4 5; do
            if book_asset_mounted; then
                echo "BOOK_MOUNT_OK $(book_asset_mountpoint) (attempt $i)"
                return 0
            fi
            sleep 0.5
        done
    done
    echo "BOOK_MOUNT_FAIL: could not mount $BOOK_ASSET_UUID (book assets unavailable)" >&2
    return 1
}
