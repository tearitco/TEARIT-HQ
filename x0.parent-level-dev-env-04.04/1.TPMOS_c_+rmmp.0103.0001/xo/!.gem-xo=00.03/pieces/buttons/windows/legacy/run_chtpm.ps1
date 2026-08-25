# run_chtpm.ps1 - Run the CHTPM v0.01 system on Windows
# Responsibility: Clean environment, set location, launch orchestrator.
# NOTE: Watchdog disabled until basic runtime is working

Write-Host "=== Launching TPM Pipeline (Windows) ===" -ForegroundColor Cyan
Write-Host "NOTE: Watchdog disabled - Press Ctrl+C to exit cleanly" -ForegroundColor Yellow

# 0. NUCLEAR CLEANUP
Write-Host "Cleaning environment..." -ForegroundColor Cyan
try {
    .\kill_all.ps1
} catch {
    Write-Host "Warning: kill_all.ps1 had issues (continuing anyway)" -ForegroundColor Yellow
}

Write-Host "Kill completed, continuing..." -ForegroundColor Cyan

# DYNAMIC PATH RESOLUTION
$SCRIPT_DIR = (Get-Item .).FullName
Write-Host "Script directory: $SCRIPT_DIR" -ForegroundColor Gray

# 1. Set up location_kvp (SMF-Compliant)
Write-Host "Setting up location_kvp..." -ForegroundColor Gray

# 1. Set up location_kvp (SMF-Compliant)
if (-not (Test-Path "pieces\locations")) { New-Item -ItemType Directory "pieces\locations" -Force }
$location_content = @"
project_root=$SCRIPT_DIR
pieces_dir=$SCRIPT_DIR\pieces
fuzzpet_app_dir=$SCRIPT_DIR\pieces\apps\fuzzpet_app
fuzzpet_dir=$SCRIPT_DIR\pieces\apps\fuzzpet_app\fuzzpet
editor_dir=$SCRIPT_DIR\pieces\apps\editor
system_dir=$SCRIPT_DIR\pieces\system
pet_01_dir=$SCRIPT_DIR\pieces\world\map_01\pet_01
pet_02_dir=$SCRIPT_DIR\pieces\world\map_01\pet_02
selector_dir=$SCRIPT_DIR\pieces\world\map_01\selector
os_procs_dir=$SCRIPT_DIR\pieces\os\procs
clock_daemon_dir=$SCRIPT_DIR\pieces\system\clock_daemon
manager_dir=$SCRIPT_DIR\pieces\apps\fuzzpet_app\manager
"@
Set-Content -Path "pieces\locations\location_kvp" -Value $location_content

# 2. Reset all entities to known good state
Write-Host "Resetting entities..."

# Create project-based piece structure for op-ed
New-Item -ItemType Directory "projects\op-ed\pieces\selector" -Force | Out-Null
Set-Content -Path "projects\op-ed\pieces\selector\state.txt" -Value "name=Selector`ntype=selector`npos_x=5`npos_y=5`npos_z=0`non_map=1"

# 3. Reset Manager State
New-Item -ItemType Directory "pieces\apps\fuzzpet_app\manager" -Force | Out-Null
Set-Content -Path "pieces\apps\fuzzpet_app\manager\state.txt" -Value "active_target_id=selector`nlast_key=None"

New-Item -ItemType Directory "projects\op-ed\manager" -Force | Out-Null
Set-Content -Path "projects\op-ed\manager\state.txt" -Value "project_id=op-ed`nactive_target_id=selector`ncurrent_z=0`nlast_key=None"
Clear-Content -Path "projects\op-ed\history.txt" -ErrorAction SilentlyContinue

# User App State
New-Item -ItemType Directory "pieces\apps\user\pieces\session" -Force | Out-Null
Set-Content -Path "pieces\apps\user\pieces\session\state.txt" -Value "session_status=auth_required`ncurrent_user=guest"

# template project (for player_app default)
New-Item -ItemType Directory "projects\template\manager" -Force | Out-Null
Set-Content -Path "projects\template\manager\state.txt" -Value "project_id=template`nactive_target_id=hero`ncurrent_z=0`nlast_key=None"
Clear-Content -Path "projects\template\history.txt" -ErrorAction SilentlyContinue

New-Item -ItemType Directory "projects\template\pieces\hero" -Force | Out-Null
Set-Content -Path "projects\template\pieces\hero\state.txt" -Value "name=Hero`ntype=hero`npos_x=5`npos_y=5`npos_z=0`non_map=1"

New-Item -ItemType Directory "pieces\apps\player_app\manager" -Force | Out-Null
Set-Content -Path "pieces\apps\player_app\manager\state.txt" -Value "project_id=template`nactive_target_id=hero`ncurrent_z=0`nlast_key=None"

# 4.1 Reset Editor State
New-Item -ItemType Directory "pieces\apps\editor" -Force | Out-Null
Set-Content -Path "pieces\apps\editor\state.txt" -Value "gui_focus=1`nemoji_mode=0`ncursor_x=5`ncursor_y=5`ncursor_z=0`nlast_key=None`nproject_id=template`nmap_idx=0`nglyph_idx=0"

New-Item -ItemType Directory "pieces\apps\editor\manager" -Force | Out-Null
Set-Content -Path "pieces\apps\editor\manager\state.txt" -Value "project_id=template`nactive_target_id=selector`ncurrent_z=0`nlast_key=None"

New-Item -ItemType Directory "pieces\apps\editor\pieces\selector" -Force | Out-Null
Set-Content -Path "pieces\apps\editor\pieces\selector\state.txt" -Value "pos_x=5`npos_y=5`npos_z=0`nactive=1`nicon=X"

# 4.2 Reset op-ed App State
New-Item -ItemType Directory "pieces\apps\op-ed" -Force | Out-Null
Set-Content -Path "pieces\apps\op-ed\state.txt" -Value "gui_focus=1`nemoji_mode=0`ncursor_x=5`ncursor_y=5`ncursor_z=0`nlast_key=None`nproject_id=op-ed`nmap_idx=0`nglyph_idx=0"

New-Item -ItemType Directory "pieces\apps\op-ed\manager" -Force | Out-Null
Set-Content -Path "pieces\apps\op-ed\manager\state.txt" -Value "project_id=op-ed`nactive_target_id=selector`ncurrent_z=0`nlast_key=None"

New-Item -ItemType Directory "pieces\apps\op-ed\pieces\selector" -Force | Out-Null
Set-Content -Path "pieces\apps\op-ed\pieces\selector\state.txt" -Value "pos_x=5`npos_y=5`npos_z=0`nactive=1`nicon=X"

Clear-Content -Path "pieces\apps\op-ed\history.txt" -ErrorAction SilentlyContinue
Clear-Content -Path "pieces\apps\op-ed\view.txt" -ErrorAction SilentlyContinue
Clear-Content -Path "pieces\apps\op-ed\view_changed.txt" -ErrorAction SilentlyContinue
Clear-Content -Path "pieces\apps\op-ed\response.txt" -ErrorAction SilentlyContinue

# 4. Reset Clock Daemon
New-Item -ItemType Directory "pieces\system\clock_daemon" -Force | Out-Null
Set-Content -Path "pieces\system\clock_daemon\state.txt" -Value "turn=0`ntime=08:00:00`nmode=turn`ntick_rate=1"

# 5. Reset FuzzPet State
New-Item -ItemType Directory "pieces\apps\fuzzpet_app\fuzzpet" -Force | Out-Null
Set-Content -Path "pieces\apps\fuzzpet_app\fuzzpet\state.txt" -Value "name=Fuzzball`nhunger=50`nhappiness=55`nenergy=100`nlevel=1`npos_x=5`npos_y=2`npos_z=0`nstatus=active`nemoji_mode=0"

New-Item -ItemType Directory "pieces\world\map_01\selector" -Force | Out-Null
Set-Content -Path "pieces\world\map_01\selector\state.txt" -Value "name=Selector`ntype=selector`npos_x=5`npos_y=5`npos_z=0`non_map=1`nemoji=X"

# 6. Clear Sessions and Historical Data
$clear_files = @(
    "pieces\keyboard\history.txt", "pieces\keyboard\ledger.txt",
    "pieces\apps\player_app\history.txt", "pieces\apps\player_app\state.txt",
    "pieces\apps\editor\history.txt", "pieces\apps\editor\view.txt",
    "pieces\apps\editor\view_changed.txt", "pieces\apps\editor\response.txt",
    "pieces\apps\fuzzpet_app\fuzzpet\history.txt", "pieces\apps\fuzzpet_app\fuzzpet\ledger.txt",
    "pieces\apps\fuzzpet_app\fuzzpet\view_changed.txt", "pieces\apps\fuzzpet_app\fuzzpet\last_response.txt",
    "pieces\display\frame_changed.txt", "pieces\display\current_frame.txt",
    "pieces\display\ledger.txt", "pieces\chtpm\ledger.txt",
    "pieces\master_ledger\master_ledger.txt", "pieces\master_ledger\ledger.txt",
    "pieces\master_ledger\frame_changed.txt", "pieces\os\proc_list.txt",
    "pieces\apps\fuzzpet_app\manager\debug_log.txt", "pieces\apps\fuzzpet_app\world\map.txt",
    "pieces\apps\fuzzpet_app\world\ledger.txt", "pieces\apps\fuzzpet_app\clock\ledger.txt",
    "debug.txt"
)
foreach ($f in $clear_files) {
    if (Test-Path $f) { Clear-Content $f } else { New-Item -ItemType File $f -Force | Out-Null }
}

# Create debug frames directory
New-Item -ItemType Directory "pieces\debug\frames" -Force | Out-Null

# 7. Reset World Map (fuzzpet-specific)
$map_content = @"
####################
#  ...............T#
#  ...............T#
#  ....R..........T#
#  ....R..........T#
#  ....R..........T#
#  ....R..........T#
#  ................#
#                  #
####################
"@
Set-Content -Path "pieces\apps\fuzzpet_app\world\map.txt" -Value $map_content

Write-Host ""
Write-Host "Launching TPM Pipeline (Nuclear Cleanup Complete)..." -ForegroundColor Green
Write-Host "  - Editor: Isolated (pieces/apps/editor/)"
Write-Host "  - FuzzPet: Isolated (pieces/apps/fuzzpet_app/)"
Write-Host ""
Write-Host "Launching Orchestrator..." -ForegroundColor Green
Write-Host "Press Ctrl+C to exit and cleanup all processes"
Write-Host "----------------------------------------" -ForegroundColor Green

& ".\pieces\chtpm\plugins\+x\orchestrator.+x"
