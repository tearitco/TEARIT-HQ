# kill_all.ps1 - Surgical TPM Component Cleanup for Windows
# Targets specifically binaries in pieces\ and projects\ directories with .+x extension.

$ErrorActionPreference = "SilentlyContinue"

function Surgical-Kill {
    param([string]$name)
    try {
        Get-Process -ErrorAction SilentlyContinue | Where-Object {
            $_.Name -eq $name -or
            $_.Path -like "*\+x\$name.+" -or
            $_.Path -like "*\system\*$name*"
        } | ForEach-Object {
            Write-Host "Killing $($_.Name) (PID: $($_.Id))..." -ForegroundColor Yellow
            Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
        }
    } catch {
        # Ignore errors
    }
}

Write-Host "Cleaning environment (Windows)..." -ForegroundColor Cyan

$components = @(
    "orchestrator", "chtpm_parser", "chtpm_player", "renderer", "gl_renderer",
    "clock_daemon", "response_handler", "keyboard_input", "joystick_input",
    "input_capture", "fuzzpet_manager", "fuzz_legacy_manager", "playrm_module",
    "fuzzpet_v2_module", "man-ops_module", "man-pal_module", "man-add_module",
    "op-ed_module", "user_module", "loader_module", "editor_module",
    "db_editor_module", "pal_editor_module", "move_player", "move_z",
    "interact", "render_map", "menu_op", "project_loader", "piece_manager",
    "proc_manager", "pdl_reader", "ai_manager", "player_manager",
    "player_render", "prisc", "gl_os", "gl_os_manager", "gl_os_renderer"
)

foreach ($comp in $components) {
    Surgical-Kill $comp
}

# Kill Watchdog Job if running as a job
Get-Job -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*watchdog*" } | Stop-Job -ErrorAction SilentlyContinue

# Nuclear Option: Kill ANY residual .+x process in the workspace
try {
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*\+x\*.+x*" } | Stop-Process -Force -ErrorAction SilentlyContinue
} catch {
    # Ignore errors
}

# Reset input focus locks
if (Test-Path "pieces\apps\gl_os\session\input_focus.lock") {
    Remove-Item "pieces\apps\gl_os\session\input_focus.lock" -Force -ErrorAction SilentlyContinue
}

Write-Host "Cleanup complete." -ForegroundColor Green
