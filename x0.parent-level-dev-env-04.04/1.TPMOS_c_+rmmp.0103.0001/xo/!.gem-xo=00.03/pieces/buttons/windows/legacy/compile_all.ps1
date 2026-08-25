# compile_all.ps1 - Compile all CHTPM v0.01 executables for Windows
# TPM-Compliant: Binaries go to their respective piece directories.

Write-Host "=== Compiling CHTPM v0.01 (Windows PowerShell) ===" -ForegroundColor Cyan
Write-Host "Ensure MinGW-w64 is in your PATH." -ForegroundColor Yellow

# KILL EXISTING PROCESSES FIRST (prevent "Permission denied" errors)
Write-Host "Killing existing TPM processes..." -ForegroundColor Gray
Get-Process | Where-Object {$_.Path -like "*TPMOS*"} | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

$GL_FLAGS = "-LC:/msys64/mingw64/lib -lopengl32 -lglu32 -lfreeglut -lwinmm -lgdi32 -luser32"
$FREETYPE_FLAGS = "-LC:/msys64/mingw64/lib -lfreetype"
$THREAD_FLAGS = "-lpthread"
# -D_WIN32 is typically auto-defined by MinGW, but we include it explicitly
$CFLAGS = "-D_WIN32 -std=gnu11"

function Compile-Piece {
    param([string]$source, [string]$output, [string]$extra_flags = "")

    if (-not (Test-Path $source)) {
        Write-Warning "Source file not found: $source"
        return
    }

    $out_dir = [System.IO.Path]::GetDirectoryName($output)
    if (-not (Test-Path $out_dir)) {
        New-Item -ItemType Directory $out_dir -Force | Out-Null
    }

    Write-Host "Compiling $source -> $output"
    # MinGW GCC requires libraries AFTER source files
    & gcc -D_WIN32 -std=gnu11 $source -o $output $extra_flags $THREAD_FLAGS
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to compile $source"
    }
}

# --- Core Apps (from Projects) ---
Write-Host "Compiling project managers..." -ForegroundColor Gray
Compile-Piece "projects\op-ed\manager\op-ed_manager.c" "projects\op-ed\manager\+x\op-ed_manager.+x"
Compile-Piece "projects\fuzz-op\manager\fuzz-op_manager.c" "projects\fuzz-op\manager\+x\fuzz-op_manager.+x"
Compile-Piece "projects\user\manager\user_manager.c" "projects\user\manager\+x\user_manager.+x"
Compile-Piece "projects\man-pal\manager\man-pal_module.c" "projects\man-pal\manager\+x\man-pal_module.+x"
Compile-Piece "projects\man-ops\manager\man-ops_module.c" "projects\man-ops\manager\+x\man-ops_module.+x"

# --- Keyboard & Joystick ---
Compile-Piece "pieces\keyboard\src\keyboard_input_win.c" "pieces\keyboard\plugins\+x\keyboard_input.+x"
# Windows: Use XInput for Xbox controllers
Write-Host "Compiling joystick_input (Windows/XInput)..." -ForegroundColor Gray
if (Test-Path "pieces\joystick\plugins\joystick_input_win.c") {
    & gcc -D_WIN32 -std=gnu11 "pieces\joystick\plugins\joystick_input_win.c" -o "pieces\joystick\plugins\+x\joystick_input.+x" -lxinput -lpthread
}

# --- CHTPM Core ---
Compile-Piece "pieces\chtpm\plugins\chtpm_parser.c" "pieces\chtpm\plugins\+x\chtpm_parser.+x"
Compile-Piece "pieces\chtpm\plugins\chtpm_player.c" "pieces\chtpm\plugins\+x\chtpm_player.+x"
Compile-Piece "pieces\chtpm\plugins\orchestrator.c" "pieces\chtpm\plugins\+x\orchestrator.+x"

# --- Display ---
Write-Host "Compiling display/windows_renderer (Windows text renderer)..." -ForegroundColor Gray
Compile-Piece "pieces\display\windows_renderer.c" "pieces\display\plugins\+x\renderer.+x"

# gl_renderer needs FreeType and OpenGL
Write-Host "Compiling display/gl_renderer (OpenGL/FreeType)..." -ForegroundColor Gray
if (Test-Path "pieces\display\gl_renderer.c") {
    & gcc -D_WIN32 -std=gnu11 -IC:/msys64/mingw64/include/freetype2 "pieces\display\gl_renderer.c" -o "pieces\display\plugins\+x\gl_renderer.+x" -LC:/msys64/mingw64/lib -lfreeglut -lglu32 -lopengl32 -lfreetype
}

# --- Master Ledger ---
Compile-Piece "pieces\master_ledger\plugins\piece_manager.c" "pieces\master_ledger\plugins\+x\piece_manager.+x"
Compile-Piece "pieces\master_ledger\plugins\response_handler.c" "pieces\master_ledger\plugins\+x\response_handler.+x"

# --- Clock ---
Compile-Piece "pieces\system\clock_daemon\plugins\clock_daemon.c" "pieces\system\clock_daemon\plugins\+x\clock_daemon.+x"

# --- Player App ---
Compile-Piece "pieces\apps\player_app\manager\player_manager.c" "pieces\apps\player_app\manager\plugins\+x\player_manager.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\player_render.c" "pieces\apps\player_app\world\plugins\+x\player_render.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\menu_op.c" "pieces\apps\player_app\world\plugins\+x\menu_op.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\project_loader.c" "pieces\apps\player_app\world\plugins\+x\project_loader.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\move_z.c" "pieces\apps\player_app\world\plugins\+x\move_z.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\interact.c" "pieces\apps\player_app\world\plugins\+x\interact.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\place_tile.c" "pieces\apps\player_app\world\plugins\+x\place_tile.+x"

# --- GL-OS ---
Write-Host "Compiling gl_os components..." -ForegroundColor Gray
& gcc -D_WIN32 -std=gnu11 "pieces\apps\gl_os\plugins\gl_desktop.c" -o "pieces\apps\gl_os\plugins\+x\gl_desktop.+x" -LC:/msys64/mingw64/lib -lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32 -luser32
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_session.c" "pieces\apps\gl_os\plugins\+x\gl_os_session.+x"
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_loader.c" "pieces\apps\gl_os\plugins\+x\gl_os_loader.+x"
& gcc -D_WIN32 -std=gnu11 "pieces\apps\gl_os\plugins\gl_os_renderer.c" -o "pieces\apps\gl_os\plugins\+x\gl_os_renderer.+x" -LC:/msys64/mingw64/lib -lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32 -luser32

# --- Shared Ops ---
$ops_src = "pieces\apps\playrm\ops\src"
$ops_dest = "pieces\apps\playrm\ops\+x"
if (Test-Path $ops_src) {
    $ops_list = @("move_player", "move_z", "move_selector", "interact", "render_map", "menu_op", "project_loader", "console_print", "place_tile", "create_piece", "undo_action", "move_entity", "fuzzpet_action", "stat_decay")
    foreach ($op in $ops_list) {
        Compile-Piece "$ops_src\$op.c" "$ops_dest\$op.+x"
    }
}

Compile-Piece "pieces\apps\playrm\plugins\playrm_module.c" "pieces\apps\playrm\plugins\+x\playrm_module.+x"
Compile-Piece "pieces\apps\playrm\loader\loader_module.c" "pieces\apps\playrm\loader\plugins\+x\loader_module.+x"

# --- Prisc & System ---
Compile-Piece "pieces\system\prisc\prisc+x.c" "pieces\system\prisc\prisc+x"
Compile-Piece "pieces\system\pdl\pdl_reader.c" "pieces\system\pdl\+x\pdl_reader.+x"

# --- Locations & OS ---
Compile-Piece "pieces\locations\path_utils.c" "pieces\locations\+x\path_utils.+x"
Compile-Piece "pieces\os\plugins\proc_manager.c" "pieces\os\plugins\+x\proc_manager.+x"

Write-Host "=== Compilation Complete ===" -ForegroundColor Green
