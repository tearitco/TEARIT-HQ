#!/bin/bash
# fix_old_pieces.sh - Add map_id to existing pieces without it
#
# This script scans all project pieces and adds map_id based on:
# 1. If piece position matches entities in a map file, use that map
# 2. Otherwise, default to the first map in the project
#
# Usage: ./fix_old_pieces.sh [project_name]
# If no project specified, fixes all projects

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."

PROJECT_ROOT="$(pwd)"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

fix_project_pieces() {
    local project="$1"
    local project_dir="$PROJECT_ROOT/projects/$project"
    
    if [ ! -d "$project_dir" ]; then
        echo -e "${YELLOW}Project '$project' not found, skipping.${NC}"
        return
    fi
    
    local pieces_dir="$project_dir/pieces"
    local maps_dir="$project_dir/maps"
    
    if [ ! -d "$pieces_dir" ]; then
        echo -e "${YELLOW}No pieces directory in '$project', skipping.${NC}"
        return
    fi
    
    if [ ! -d "$maps_dir" ]; then
        echo -e "${YELLOW}No maps directory in '$project', skipping.${NC}"
        return
    fi
    
    # Get the first map as default
    local default_map=""
    default_map=$(ls "$maps_dir"/*.txt 2>/dev/null | head -1 | xargs basename)
    
    if [ -z "$default_map" ]; then
        echo -e "${YELLOW}No maps found in '$project', skipping.${NC}"
        return
    fi
    
    echo -e "${GREEN}Fixing pieces in project: $project${NC}"
    echo "  Default map: $default_map"
    echo ""
    
    # Scan all piece directories
    for piece_dir in "$pieces_dir"/*/; do
        if [ ! -d "$piece_dir" ]; then continue; fi
        
        local piece_name=$(basename "$piece_dir")
        local state_file="$piece_dir/state.txt"
        
        # Skip selector (it's global)
        if [ "$piece_name" = "selector" ]; then continue; fi
        
        if [ ! -f "$state_file" ]; then continue; fi
        
        # Check if map_id already exists
        if grep -q "^map_id=" "$state_file" 2>/dev/null; then
            echo "  ✓ $piece_name: Already has map_id"
            continue
        fi
        
        # Read position from state
        local pos_x=$(grep "^pos_x=" "$state_file" | cut -d'=' -f2)
        local pos_y=$(grep "^pos_y=" "$state_file" | cut -d'=' -f2)
        
        if [ -z "$pos_x" ] || [ -z "$pos_y" ]; then
            # No position, use default map
            echo "  $piece_name: No position, using default map"
            echo "map_id=$default_map" >> "$state_file"
            continue
        fi
        
        # Try to find which map this piece belongs to by scanning map files
        local found_map=""
        for map_file in "$maps_dir"/*.txt; do
            if [ ! -f "$map_file" ]; then continue; fi
            
            local map_name=$(basename "$map_file")
            local row_num=0
            local found_in_map=0
            
            while IFS= read -r line; do
                # Check if position matches a non-empty tile in this map
                local col_num=0
                local i=0
                while [ $i -lt ${#line} ]; do
                    local char="${line:$i:1}"
                    if [ $col_num -eq $pos_x ] && [ $row_num -eq $pos_y ]; then
                        if [ "$char" != "." ] && [ -n "$char" ]; then
                            found_map="$map_name"
                            found_in_map=1
                            break 2
                        fi
                    fi
                    # Handle UTF-8 multi-byte characters
                    if [[ "$char" =~ [^\x00-\x7F] ]]; then
                        i=$((i + 3))  # Skip remaining bytes of UTF-8 char
                    else
                        i=$((i + 1))
                    fi
                    col_num=$((col_num + 1))
                done
                row_num=$((row_num + 1))
            done < "$map_file"
            
            if [ $found_in_map -eq 1 ]; then break; fi
        done
        
        # Add map_id
        if [ -n "$found_map" ]; then
            echo "  $piece_name: Found map $found_map by position ($pos_x,$pos_y)"
            echo "map_id=$found_map" >> "$state_file"
        else
            echo "  $piece_name: Using default map $default_map"
            echo "map_id=$default_map" >> "$state_file"
        fi
    done
    
    echo ""
}

# Main
echo "=== PMO Piece map_id Migration Script ==="
echo ""

if [ -n "$1" ]; then
    # Fix specific project
    fix_project_pieces "$1"
else
    # Fix all projects
    for project_dir in "$PROJECT_ROOT/projects/"*/; do
        if [ -d "$project_dir" ]; then
            project=$(basename "$project_dir")
            fix_project_pieces "$project"
        fi
    done
fi

echo -e "${GREEN}Migration complete!${NC}"
echo ""
echo "To verify, check a piece state file:"
echo "  cat projects/template/pieces/npc_7_5/state.txt"
