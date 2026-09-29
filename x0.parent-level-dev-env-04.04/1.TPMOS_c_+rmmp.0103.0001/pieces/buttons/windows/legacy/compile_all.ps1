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
# agy-text-editor: was MISSING from this list entirely, so
# agy-text-editor_manager.+x was never built on Windows and
# editor.chtpm's <module> tag pointed at a nonexistent binary. That is why
# INTERACT mode forwarded keys into player_app/history.txt with nothing
# consuming them. Evidence: FRAME_REPORT_20260926-1540_agy-interact-inject.txt
Compile-Piece "projects\agy-text-editor\manager\agy-text-editor_manager.c" "projects\agy-text-editor\manager\+x\agy-text-editor_manager.+x"

# Shared file Ops. Also MISSING from this list, and agy-text-editor invokes
# all four at runtime through run_op() -- without them the manager builds but
# every keystroke silently no-ops because its Op binary is absent.
$file_ops = @("text_edit_key", "text_editor_view", "file_copy", "dir_browse")
foreach ($op in $file_ops) {
    Compile-Piece "pieces\system\file_ops\$op.c" "pieces\system\file_ops\+x\$op.+x"
}

# --- cpp-llm ---
# Was MISSING entirely, so cpp-llm did not run on Windows at all. Its manager
# needs -lws2_32: the MinGW toolchain has no ifaddrs.h, so resolve_my_ip() uses
# gethostbyname() instead, which pulls in ws2_32. MinGW's own gcc rejects
# -lwinsock2 in this sysroot; ws2_32 is the library name it actually ships.
Compile-Piece "projects\cpp-llm\manager\cpp-llm_manager.c" "projects\cpp-llm\manager\+x\cpp-llm_manager.+x" "-lws2_32"
# Its Ops. run_tool() resolves these under projects/cpp-llm/ops/+x at runtime,
# so without them every tool call returns NULL and the manager no-ops. 10 of the
# 13 compiled unmodified; cmd_exec, connect_op and search_in_files needed
# Windows spawn/pipe/getline equivalents.
$cpp_llm_ops = @("cmd_exec", "complete_path", "connect_op", "cpp-llm_bridge",
                 "edit_file", "file_ops", "json_escaper", "json_parser",
                 "json_state", "list_dir", "search_in_files",
                 "text_to_llama3", "web_search")
foreach ($op in $cpp_llm_ops) {
    Compile-Piece "projects\cpp-llm\ops\src\$op.c" "projects\cpp-llm\ops\+x\$op.+x"
}

# --- groq-ollama ---
# Also MISSING entirely, the same omission as cpp-llm. Needs -lws2_32 for the
# same reason only if it resolves addresses; it does not (no ifaddrs.h use), so
# the flag is omitted here rather than added on a guess. 9 of its 11 Ops
# compiled unmodified; cmd_exec and search_in_files needed the same
# Windows pipe-spawn and getline equivalents as cpp-llm's copies, which are
# byte-for-byte the same source apart from a header comment.
Compile-Piece "projects\groq-ollama\manager\groq-ollama_manager.c" "projects\groq-ollama\manager\+x\groq-ollama_manager.+x"
$groq_ops = @("cmd_exec", "complete_path", "edit_file", "file_ops",
              "gemini_payload_builder", "groq-ollama_bridge", "json_parser",
              "json_state", "list_dir", "search_in_files", "web_search")
foreach ($op in $groq_ops) {
    Compile-Piece "projects\groq-ollama\ops\src\$op.c" "projects\groq-ollama\ops\+x\$op.+x"
}

# --- gem-dev ---
# Third of the same family, third of the same omission. 9 of 13 Ops compiled
# unmodified; four needed work. cmd_exec and search_in_files are the shared
# duplicated ops again (byte-identical to the cpp-llm copies apart from a
# header comment). startup_reset_op only needed the one-argument MinGW mkdir
# shim in ensure_dir(). web_search was the only genuinely new one: it
# fork/execs curl through a pipe to read the DuckDuckGo response, so it took
# the full CreatePipe/CreateProcess port plus its own PATH walk for curl.
Compile-Piece "projects\gem-dev\manager\gem-dev_manager.c" "projects\gem-dev\manager\+x\gem-dev_manager.+x"
$gem_ops = @("cmd_exec", "complete_path", "edit_file", "file_ops", "gem-dev",
             "gemini_payload_builder", "get_completion_methods_op", "json_parser",
             "json_state", "list_dir", "search_in_files", "startup_reset_op",
             "web_search")
foreach ($op in $gem_ops) {
    Compile-Piece "projects\gem-dev\ops\src\$op.c" "projects\gem-dev\ops\+x\$op.+x"
}

# --- slop-ed-dev ---
# The last of the four, and the only one that COMPILED on Windows the whole
# time. Unlike the LLM trio it was already partly ported in-place -- sys/wait.h
# guarded, windows.h, a mkdir shim, a usleep shim and its own Windows asprintf
# -- so it needed no compile fixes and was not portable in the sense that
# mattered. Its run_command() had "#else return system(cmd)", which hands the
# string to cmd.exe, and every one of its nine call sites was POSIX shell:
# "mkdir -p", "cp -r", single-quoted paths, "> /dev/null 2>&1", "VAR=x cmd".
# cmd.exe has none of those, so the whole app compiled clean and did nothing.
# A green build was the only thing wrong with it, and it is the reason this
# project's port could not be judged by whether it compiled.
# No Ops of its own -- it calls the shared pieces/system/file_ops ones, already
# ported for agy-text-editor.
Compile-Piece "projects\slop-ed-dev\manager\slop-ed-dev_manager.c" "projects\slop-ed-dev\manager\+x\slop-ed-dev_manager.+x"
# pal_editor.chtpm points at a SECOND module in the same directory, so building
# only the manager would leave one of the six layouts pointing at a binary that
# does not exist.
Compile-Piece "projects\slop-ed-dev\manager\pal_editor_module.c" "projects\slop-ed-dev\manager\+x\pal_editor_module.+x"



# --- Keyboard & Joystick ---
Compile-Piece "pieces\keyboard\src\keyboard_input_win.c" "pieces\keyboard\plugins\+x\keyboard_input.+x"
# Windows: Use XInput for Xbox controllers
Write-Host "Compiling joystick_input (Windows/XInput)..." -ForegroundColor Gray
if (Test-Path "pieces\joystick\plugins\joystick_input_win.c") {
    # Was a raw gcc call, so unlike every Compile-Piece target it never got
    # its +x\ directory created and failed to link on a clean tree. Compile-Piece
    # creates the output dir (see its New-Item above) and adds -lpthread.
    Compile-Piece "pieces\joystick\plugins\joystick_input_win.c" "pieces\joystick\plugins\+x\joystick_input.+x" "-lxinput"
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
