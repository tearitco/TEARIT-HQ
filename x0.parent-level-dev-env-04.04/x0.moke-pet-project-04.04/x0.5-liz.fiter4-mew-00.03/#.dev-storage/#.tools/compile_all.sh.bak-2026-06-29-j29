#!/bin/bash
# compile_all.sh - Compile ALL CHTPM binaries (projects + system)
# Usage: ./#.dev-storage/#.tools/compile_all.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

echo "=== CHTPM COMPILE ALL ==="
echo "Date: $(date)"
echo "Project Root: $PROJECT_ROOT"
echo ""

# macOS / Linux Detection
OS_TYPE="$(uname -s)"
WFLAGS="-Wno-format-extra-args"
GL_LIBS="-lglut -lGLU -lGL -lm"

if [ "$OS_TYPE" == "Darwin" ]; then
    echo "  [MAC] macOS detected"
    # Clang on Mac doesn't like -Wno-format-truncation
    GL_LIBS="-framework GLUT -framework OpenGL -lm"
else
    # Linux-specific warnings
    WFLAGS="$WFLAGS -Wno-format-truncation"
fi

# Resolve FreeType (Universal)
FT_CFLAGS=""
FT_LIBS="-lfreetype"

if command -v pkg-config >/dev/null 2>&1; then
    FT_CFLAGS=$(pkg-config --cflags freetype2)
    FT_LIBS=$(pkg-config --libs freetype2)
fi

# Fallbacks if pkg-config failed or missed paths
if [ -z "$FT_CFLAGS" ]; then
    if [ "$OS_TYPE" == "Darwin" ]; then
        FT_CFLAGS="-I/usr/local/opt/freetype/include/freetype2 -I/opt/homebrew/opt/freetype/include/freetype2"
    else
        # Standard Linux path
        FT_CFLAGS="-I/usr/include/freetype2"
    fi
fi

COMPILED=0
FAILED=0

compile_op() {
    local src="$1"
    local dst="$2"

    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        if gcc -o "$dst" "$src" -pthread -lm $WFLAGS; then
            if [ -f "$dst" ]; then
                echo "  ✓ $dst"
                ((COMPILED++))
            else
                echo "  ✗ FAILED (No binary): $src"
                ((FAILED++))
            fi
        else
            echo "  ✗ FAILED: $src"
            ((FAILED++))
        fi
    fi
}

compile_op_freetype() {
    local src="$1"
    local dst="$2"

    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        if gcc -o "$dst" "$src" -pthread -lm $FT_CFLAGS $FT_LIBS $WFLAGS; then
            if [ -f "$dst" ]; then
                echo "  ✓ $dst"
                ((COMPILED++))
            else
                echo "  ✗ FAILED (No binary): $src"
                ((FAILED++))
            fi
        else
            echo "  ✗ FAILED: $src"
            ((FAILED++))
        fi
    fi
}

compile_source() {
    local src="$1"
    local dst="$2"

    if grep -Eq '#include <(GLUT/glut.h|GL/glut.h|GL/gl.h|OpenGL/gl.h)>|glutInit|glutMainLoop|glBindTexture|glTexImage2D|glClear|glEnable|glBegin|glEnd|glTexCoord2f|glVertex2f|glGenTextures|glTexParameteri' "$src" 2>/dev/null; then
        if grep -q "ft2build.h" "$src" 2>/dev/null; then
            compile_gl "$src" "$dst" "$GL_LIBS $FT_CFLAGS $FT_LIBS"
        else
            compile_gl "$src" "$dst" "$GL_LIBS"
        fi
    elif grep -q "ft2build.h" "$src" 2>/dev/null; then
        compile_op_freetype "$src" "$dst"
    else
        compile_op "$src" "$dst"
    fi
}

compile_gl() {
    local src="$1"
    local dst="$2"
    local extra_libs="$3"
    
    local final_libs="$GL_LIBS"
    if [ -n "$extra_libs" ]; then
        # If extra_libs starts with -lglut or -lGL, we might want to replace them
        # but for simplicity we'll just append and hope the linker handles it,
        # or better: we only use extra_libs if we didn't set a Mac-specific GL_LIBS.
        if [ "$OS_TYPE" == "Darwin" ]; then
            # Filter out linux-specific -lGL/GLUT from extra_libs if passed
            local filtered_libs=$(echo "$extra_libs" | sed 's/-lglut//g' | sed 's/-lGLU//g' | sed 's/-lGL//g')
            final_libs="$final_libs $filtered_libs"
        else
            final_libs="$extra_libs"
        fi
    fi

    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        if gcc -o "$dst" "$src" $final_libs $WFLAGS; then
            if [ -f "$dst" ]; then
                echo "  ✓ $dst"
                ((COMPILED++))
            else
                echo "  ✗ FAILED (No binary): $src"
                ((FAILED++))
            fi
        else
            echo "  ✗ FAILED: $src"
            ((FAILED++))
        fi
    fi
}

echo "=== 1. SYSTEM COMPONENTS ==="
compile_op "pieces/keyboard/src/keyboard_input_linux.c" "pieces/keyboard/plugins/+x/keyboard_input.+x"
compile_op "pieces/joystick/plugins/joystick_input.c" "pieces/joystick/plugins/+x/joystick_input.+x"
compile_op "pieces/chtpm/plugins/chtpm_parser.c" "pieces/chtpm/plugins/+x/chtpm_parser.+x"
compile_op "pieces/chtpm/plugins/orchestrator.c" "pieces/chtpm/plugins/+x/orchestrator.+x"
compile_op "pieces/display/renderer.c" "pieces/display/plugins/+x/renderer.+x"
compile_gl "pieces/display/gl_renderer.c" "pieces/display/plugins/+x/gl_renderer.+x" "$GL_LIBS $FT_CFLAGS $FT_LIBS"
compile_op "pieces/system/pdl/pdl_reader.c" "pieces/system/pdl/+x/pdl_reader.+x"

echo "=== 1c. CHTPM OPS ==="
mkdir -p pieces/chtpm/ops/+x
for src in pieces/chtpm/ops/*.c; do
    if [ -f "$src" ]; then
        op_name=$(basename "$src" .c)
        if grep -q "ft2build.h" "$src" 2>/dev/null; then
            compile_op_freetype "$src" "pieces/chtpm/ops/+x/${op_name}.+x"
        elif grep -Eq '#include <(GLUT/glut.h|GL/glut.h|GL/gl.h|OpenGL/gl.h)>|glutInit|glutMainLoop|glBindTexture|glTexImage2D|glClear|glEnable|glBegin|glEnd|glTexCoord2f|glVertex2f|glGenTextures|glTexParameteri' "$src" 2>/dev/null; then
            compile_gl "$src" "pieces/chtpm/ops/+x/${op_name}.+x" "$GL_LIBS"
        else
            compile_op "$src" "pieces/chtpm/ops/+x/${op_name}.+x"
        fi
    fi
done

# Process management (proc_manager CLI tool)
compile_op "pieces/os/plugins/proc_manager.c" "pieces/os/plugins/+x/proc_manager.+x"
echo ""

echo "=== 1b. WINDOWS-SPECIFIC COMPONENTS (if on Windows) ==="
# These are compiled separately on Windows via compile_all.ps1
# Included here for documentation and MSYS2/MinGW cross-compilation
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    echo "  Windows detected - use pieces/buttons/windows/legacy/compile_all.ps1 instead"
    echo "  Compiles: keyboard_input_win.c, joystick_input_win.c (XInput), windows_renderer.c"
fi
echo ""

echo "=== 2. SYSTEM APPS ==="
# op-ed
compile_op "pieces/apps/op-ed/plugins/op-ed_module.c" "pieces/apps/op-ed/plugins/+x/op-ed_module.+x"

# player_app
compile_op "pieces/apps/player_app/manager/player_manager.c" "pieces/apps/player_app/manager/plugins/+x/player_manager.+x"

# player_app world plugins (modern loader, etc.)
mkdir -p pieces/apps/player_app/world/plugins/+x
for src in pieces/apps/player_app/world/plugins/*.c; do
    if [ -f "$src" ]; then
        op_name=$(basename "$src" .c)
        compile_op "$src" "pieces/apps/player_app/world/plugins/+x/${op_name}.+x"
    fi
done
echo ""

echo "=== 2b. GL-OS APP (OpenGL) ==="
compile_gl "pieces/apps/gl_os/plugins/gl_desktop.c" "pieces/apps/gl_os/plugins/+x/gl_desktop.+x" "$GL_LIBS"
compile_op "pieces/apps/gl_os/plugins/gl_os_audit_frame.c" "pieces/apps/gl_os/plugins/+x/gl_os_audit_frame.+x"
compile_op "pieces/apps/gl_os/plugins/gl_os_project_scan.c" "pieces/apps/gl_os/plugins/+x/gl_os_project_scan.+x"
compile_gl "pieces/apps/gl_os/plugins/gl_os_renderer.c" "pieces/apps/gl_os/plugins/+x/gl_os_renderer.+x" "$GL_LIBS"
compile_op "pieces/apps/gl_os/plugins/gl_os_session.c" "pieces/apps/gl_os/plugins/+x/gl_os_session.+x"
compile_op "pieces/apps/gl_os/plugins/gl_os_loader.c" "pieces/apps/gl_os/plugins/+x/gl_os_loader.+x"
compile_op "pieces/apps/gl_os/plugins/gl_os.c" "pieces/apps/gl_os/plugins/+x/gl_os.+x"
echo ""

echo "=== 3. PLAYRM OPS (SHARED) ==="
mkdir -p pieces/apps/playrm/ops/+x
for src in pieces/apps/playrm/ops/src/*.c; do
    if [ -f "$src" ]; then
        op_name=$(basename "$src" .c)
        compile_op "$src" "pieces/apps/playrm/ops/+x/${op_name}.+x"
    fi
done
echo ""

echo "=== 3b. PLAYRM PLUGINS ==="
mkdir -p pieces/apps/playrm/plugins/+x
compile_op "pieces/apps/playrm/plugins/playrm_module.c" "pieces/apps/playrm/plugins/+x/playrm_module.+x"
echo ""

echo "=== 3c. PLAYRM LOADER (Project Scanner Daemon) ==="
mkdir -p pieces/apps/playrm/loader/plugins/+x
compile_op "pieces/apps/playrm/loader/loader_module.c" "pieces/apps/playrm/loader/plugins/+x/loader_module.+x"
echo ""

echo "=== 4. PROJECT MANAGERS ==="
for proj_dir in projects/*/; do
    proj_name=$(basename "$proj_dir")

    # Skip trunk and non-project dirs
    if [ "$proj_name" = "trunk" ]; then continue; fi

    # Check for manager source
    if [ -f "${proj_dir}manager/${proj_name}_manager.c" ]; then
        if [ "$proj_name" = "user" ]; then
            # user_manager requires libcrypto for password hashing
            mkdir -p "${proj_dir}manager/+x"
            if gcc -o "${proj_dir}manager/+x/${proj_name}_manager.+x" "${proj_dir}manager/${proj_name}_manager.c" -pthread -lcrypto -Wno-format-truncation -Wno-format-extra-args 2>/dev/null; then
                echo "  ✓ ${proj_dir}manager/+x/${proj_name}_manager.+x"
                ((COMPILED++))
            else
                echo "  ✗ FAILED: ${proj_dir}manager/${proj_name}_manager.c"
                ((FAILED++))
            fi
        else
            compile_op "${proj_dir}manager/${proj_name}_manager.c" "${proj_dir}manager/+x/${proj_name}_manager.+x"
        fi
    fi

    # Special case: pal_editor_module
    if ([ "$proj_name" = "op-ed" ] || [ "$proj_name" = "slop-ed-dev" ]) && [ -f "${proj_dir}manager/pal_editor_module.c" ]; then
        compile_op "${proj_dir}manager/pal_editor_module.c" "${proj_dir}manager/+x/pal_editor_module.+x"
    fi

    # Special case: gl-os uses underscore instead of hyphen
    if [ "$proj_name" = "gl-os" ] && [ -f "${proj_dir}manager/gl_os_manager.c" ]; then
        compile_op "${proj_dir}manager/gl_os_manager.c" "${proj_dir}manager/+x/gl_os_manager.+x"
    fi

    # GL-Op-Ed Manager
    if [ "$proj_name" = "op-ed-gl" ] && [ -f "${proj_dir}manager/op-ed-gl_manager.c" ]; then
        compile_op "${proj_dir}manager/op-ed-gl_manager.c" "${proj_dir}manager/+x/op-ed-gl_manager.+x"
    fi
done
echo ""

echo "=== 4b. WRAITH ALPHA RUNTIME ==="
mkdir -p "projects/wraith-alpha/manager/+x"
mkdir -p "projects/wraith-alpha/ops/+x"
mkdir -p "projects/wraith-alpha/plugins/+x"
compile_op "projects/wraith-alpha/manager/wraith-alpha_manager.c" "projects/wraith-alpha/manager/+x/wraith-alpha_manager.+x"
compile_source "projects/wraith-alpha/ops/font-gen-op.c" "projects/wraith-alpha/ops/+x/font-gen-op.+x"
compile_op "projects/wraith-alpha/ops/wraith_parser_alpha.c" "projects/wraith-alpha/ops/+x/wraith_parser_alpha.+x"
compile_gl "projects/wraith-alpha/ops/wraith_gl.c" "projects/wraith-alpha/ops/+x/wraith_gl.+x" "$GL_LIBS"
compile_op "projects/wraith-alpha/plugins/wraith_rgb_daemon.c" "projects/wraith-alpha/plugins/+x/wraith_rgb_daemon.+x"
echo ""

echo "=== 4c. NESTED WRAITH PROJECTS ==="
for nested_dir in projects/wraith/wraith-projects/*/; do
    [ -d "$nested_dir" ] || continue

    if [ -d "${nested_dir}manager/" ]; then
        mkdir -p "${nested_dir}manager/+x"
        for src in "${nested_dir}manager/"*.c; do
            [ -f "$src" ] || continue
            bin_name=$(basename "$src" .c)
            compile_source "$src" "${nested_dir}manager/+x/${bin_name}.+x"
        done
    fi

    for subdir in "ops" "ops/src" "plugins"; do
        if [ -d "${nested_dir}${subdir}/" ]; then
            mkdir -p "${nested_dir}${subdir}/+x"
            for src in "${nested_dir}${subdir}/"*.c; do
                [ -f "$src" ] || continue
                bin_name=$(basename "$src" .c)
                compile_source "$src" "${nested_dir}${subdir}/+x/${bin_name}.+x"
            done
        fi
    done
done
echo ""

echo "=== 5. PROJECT OPS ==="
for proj_dir in projects/*/; do
    proj_name=$(basename "$proj_dir")
    
    # Skip trunk
    if [ "$proj_name" = "trunk" ]; then continue; fi
    
    # Check for ops directory (both root and src/ subdirectory)
    for op_subdir in "ops" "ops/src" "plugins"; do
        if [ -d "${proj_dir}${op_subdir}/" ]; then
            for src in "${proj_dir}${op_subdir}/"*.c; do
                if [ -f "$src" ]; then
                    op_name=$(basename "$src" .c)
                    mkdir -p "${proj_dir}${op_subdir}/+x"
                    compile_source "$src" "${proj_dir}${op_subdir}/+x/${op_name}.+x"
                fi
            done
        fi
    done
done
echo ""

echo "=== 6. LEGACY APPS (TRUNK) ==="
if [ -d "projects/trunk/legacy_archive/" ]; then
    for app_dir in projects/trunk/legacy_archive/*/; do
        app_name=$(basename "$app_dir")

        # Compile managers
        if [ -f "${app_dir}manager/${app_name}_manager.c" ]; then
            compile_op "${app_dir}manager/${app_name}_manager.c" "${app_dir}manager/+x/${app_name}_manager.+x"
        fi

        # Compile ops/traits
        for subdir in ops traits; do
            if [ -d "${app_dir}${subdir}/" ]; then
                for src in "${app_dir}${subdir}/"*.c; do
                    if [ -f "$src" ]; then
                        op_name=$(basename "$src" .c)
                        compile_op "$src" "${app_dir}${subdir}/+x/${op_name}.+x"
                    fi
                done
            fi
        done
    done
fi
echo ""

echo "=== 7. SHARED APP TRAITS (FUZZPET_APP) ==="
# zombie_ai - Zombie chase AI for fuzz-op project
compile_op "pieces/apps/fuzzpet_app/traits/zombie_ai.c" "pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x"
echo ""

echo "=== COMPILE SUMMARY ==="
echo "  Compiled: $COMPILED binaries"
echo "  Failed:   $FAILED binaries"
echo ""

if [ $FAILED -gt 0 ]; then
    echo "⚠ WARNING: $FAILED binaries failed to compile!"
    echo "Check error messages above."
    exit 1
else
    echo "✓ All binaries compiled successfully!"
    exit 0
fi
