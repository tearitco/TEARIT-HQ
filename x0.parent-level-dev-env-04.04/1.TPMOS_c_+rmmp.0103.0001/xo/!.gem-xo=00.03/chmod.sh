#!/bin/bash

# Define the root directory to start from
# Change "." to your desired path, e.g., "/var/www/html"
ROOT_DIR="."

# Define permissions
DIR_PERMS=755 # rwxr-xr-x
FILE_PERMS=644 # rw-r--r--

echo "Setting directory permissions to $DIR_PERMS and file permissions to $FILE_PERMS in $ROOT_DIR"

# Set permissions for all directories and subdirectories
find "$ROOT_DIR" -type d -exec chmod "$DIR_PERMS" {} +
echo "Directory permissions set."

# Set permissions for all files and subfiles
find "$ROOT_DIR" -type f -exec chmod "$FILE_PERMS" {} +
echo "File permissions set."

echo "Permissions change complete."

