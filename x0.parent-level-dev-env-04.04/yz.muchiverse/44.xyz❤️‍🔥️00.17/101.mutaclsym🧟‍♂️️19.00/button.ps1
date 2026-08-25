# button.ps1 - Windows launcher for Mutaclysm (parity with button.sh run).

# Taskbar toys cell calls: powershell -File button.ps1 run

# ASCII only in this file. Linux stays on button.sh.

param(

    [Parameter(Position = 0)]

    [string]$Action = "help"

)

$ErrorActionPreference = "Continue"

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

Set-Location -LiteralPath $SCRIPT_DIR

$HOUSE_DIR = Split-Path $SCRIPT_DIR -Parent

$MSYS = "C:\msys64\mingw64\bin"

if (Test-Path $MSYS) {

    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }

}



function Get-Bin([string]$rel) {

    foreach ($c in @(($rel + ".exe"), $rel)) {

        $p = Join-Path $SCRIPT_DIR $c

        if (Test-Path -LiteralPath $p) { return $p }

    }

    return $null

}



function Invoke-Kill {

    $names = @(

        "keyboard_input", "renderer", "prisc+x", "chtpm_parser_pal",

        "chtpm_rgb_render", "gl_mirror", "orchestrator"

    )

    foreach ($n in $names) {

        Get-Process -ErrorAction SilentlyContinue |

            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |

            ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }

    }

}



function Invoke-Run {

    Write-Host "=== Mutaclysm Windows launcher ==="

    $orch = Get-Bin "system\orchestrator"

    $gl = Get-Bin "system\gl_mirror"

    $rgb = Get-Bin "system\chtpm_rgb_render"

    $kb = Get-Bin "system\keyboard_input"

    $rend = Get-Bin "system\renderer"

    if (-not $orch) { Write-Error "MISSING system\orchestrator.exe"; return 1 }



    Invoke-Kill



    $sid = "{0}-{1}" -f [DateTimeOffset]::Now.ToUnixTimeSeconds(), $PID

    $SESSION = Join-Path $SCRIPT_DIR ("pieces\sessions\" + $sid)

    foreach ($d in @(

        "pieces\system", "pieces\display", "pieces\apps\player_app",

        "pieces\keyboard", "pieces\os", "projects\mutaclysm\manager"

    )) {

        New-Item -ItemType Directory -Path (Join-Path $SESSION $d) -Force | Out-Null

    }

    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\apps\player_app") -Force | Out-Null

    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\display") -Force | Out-Null

    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\hero_01\inventory") -Force | Out-Null

    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "data") -Force | Out-Null

    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\system\widget_cmds") -Force | Out-Null



    $utf8 = New-Object System.Text.UTF8Encoding $false

    function Write-NoBom([string]$Path, [string]$Text) {

        [System.IO.File]::WriteAllText($Path, $Text, $utf8)

    }



    foreach ($f in @(

        (Join-Path $SESSION "pieces\apps\player_app\interact_relay.txt"),

        (Join-Path $SESSION "pieces\keyboard\history.txt"),

        (Join-Path $SESSION "pieces\display\pc_screen_changed.txt"),

        (Join-Path $SESSION "pieces\display\frame_changed.txt"),

        (Join-Path $SESSION "projects\mutaclysm\manager\gui_state.txt"),

        (Join-Path $SCRIPT_DIR "pieces\system\quit_flag.txt"),

        (Join-Path $SCRIPT_DIR "pieces\system\board.txt"),

        (Join-Path $SCRIPT_DIR "pieces\system\entities.txt"),

        (Join-Path $SCRIPT_DIR "pieces\system\widget_cmds\inbox.txt")

    )) {

        $dir = Split-Path $f -Parent

        if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }

        if (-not (Test-Path -LiteralPath $f)) { New-Item -ItemType File -Path $f -Force | Out-Null }

    }



    Write-NoBom (Join-Path $SESSION "pieces\system\house_root.txt") ($HOUSE_DIR + "`n")

    Write-NoBom (Join-Path $SESSION "pieces\system\real_project_root.txt") ($SCRIPT_DIR + "`n")

    Write-NoBom (Join-Path $SESSION "pieces\apps\player_app\state.txt") @"

module_path=system/prisc+x pal/game_module.pal

project_id=mutaclysm

active_target_id=main

"@

    Write-NoBom (Join-Path $SCRIPT_DIR "pieces\system\board_widget_bridge.txt") @"

inbox_path=pieces/system/widget_cmds/inbox.txt

kind=board_game

project_id=mutaclysm

display_name=Mutaclysm

"@



    $cfg = Join-Path $SCRIPT_DIR "pieces\system\config.txt"

    if (-not (Test-Path -LiteralPath $cfg)) {

        Write-NoBom $cfg @"

game_id=mutaclysm-001

turn=1

hunger=100

thirst=100

stamina=100

hp=100

max_hp=100

evasion=0

defense=0

level=1

exp=0

gold=0

game_state=title

"@

    }



    $hero = Join-Path $SCRIPT_DIR "pieces\hero_01\state.txt"

    if (Test-Path -LiteralPath $hero) {

        $txt = [IO.File]::ReadAllText($hero)

        if ($txt -match "(?m)^interact_mode=") {

            $txt = [regex]::Replace($txt, "(?m)^interact_mode=.*$", "interact_mode=0")

        } else {

            $txt = $txt.TrimEnd() + "`ninteract_mode=0`n"

        }

        if ($txt -notmatch "(?m)^last_possessed_id=") {

            $txt = $txt.TrimEnd() + "`nlast_possessed_id=hero`n"

        }

        Write-NoBom $hero $txt

    }



    # Relative root: MinGW fopen breaks on emoji abs paths. Session cwd

    # is pieces\sessions\<sid>, so ..\..\.. is the project dir.

    $env:PRISC_PROJECT_ROOT = "..\..\.."

    $env:PRISC_PROJECT_ID = "mutaclysm"

    $env:NO_NET = "1"

    $pal = Join-Path $SCRIPT_DIR "pieces\chtpm\layouts\main.chtpm"

    if (Test-Path -LiteralPath $pal) { $env:PAL_LAYOUT = "..\..\..\pieces\chtpm\layouts\main.chtpm" }



    $glut = "C:\msys64\mingw64\bin\libfreeglut.dll"

    if (Test-Path $glut) {

        $dest = Join-Path $SCRIPT_DIR "system\libfreeglut.dll"

        if (-not (Test-Path -LiteralPath $dest)) {

            try { Copy-Item -LiteralPath $glut -Destination $dest -Force } catch {}

        }

    }



    $procs = @()

    function Start-Hid([string]$exe) {

        if (-not $exe) { return $null }

        $p = Start-Process -FilePath $exe -WorkingDirectory $SESSION -WindowStyle Hidden -PassThru

        return $p

    }



    $pOrch = Start-Hid $orch

    if ($pOrch) { $procs += $pOrch; Write-Host "orchestrator pid=$($pOrch.Id)" }



    $frame = Join-Path $SESSION "pieces\display\current_frame.txt"

    for ($i = 0; $i -lt 40; $i++) {

        if ((Test-Path -LiteralPath $frame) -and ((Get-Item -LiteralPath $frame).Length -gt 0)) { break }

        Start-Sleep -Milliseconds 100

    }



    if ($rgb) {

        $pRgb = Start-Hid $rgb

        if ($pRgb) { $procs += $pRgb; Write-Host "chtpm_rgb_render pid=$($pRgb.Id)" }

        $raw = Join-Path $SESSION "pieces\display\rgb_frame.raw"

        for ($i = 0; $i -lt 50; $i++) {

            if ((Test-Path -LiteralPath $raw) -and ((Get-Item -LiteralPath $raw).Length -gt 10000)) { break }

            Start-Sleep -Milliseconds 100

        }

    }



    if ($gl) {

        $pGl = Start-Process -FilePath $gl -WorkingDirectory $SESSION -WindowStyle Normal -PassThru

        if ($pGl) {

            $procs += $pGl

            Write-Host "gl_mirror pid=$($pGl.Id)"

            Start-Sleep -Milliseconds 400

            if ($pGl.HasExited) {

                Write-Host "FAIL: gl_mirror exited code=$($pGl.ExitCode) (freeglut/DLL?)"

            }

        }

    } else {

        Write-Host "MISSING gl_mirror.exe"

    }



    if ($rend) {

        $pRend = Start-Hid $rend

        if ($pRend) { $procs += $pRend }

    }

    if ($kb) {

        $pKb = Start-Hid $kb

        if ($pKb) { $procs += $pKb; Write-Host "keyboard_input pid=$($pKb.Id)" }

    }



    Write-Host "Mutaclysm launched (session=$sid). Close the GL window / toys again to stop."

    return 0

}



$a = $Action.ToLowerInvariant()

if ($a -in @("run", "r", "start")) { exit (Invoke-Run) }

elseif ($a -in @("kill", "k", "stop")) { Invoke-Kill; exit 0 }

elseif ($a -in @("help", "-h", "--help", "")) {

    Write-Host "Usage: .\button.ps1 run|kill"

    exit 0

} else {

    Write-Host "unknown action $Action"

    exit 1

}

