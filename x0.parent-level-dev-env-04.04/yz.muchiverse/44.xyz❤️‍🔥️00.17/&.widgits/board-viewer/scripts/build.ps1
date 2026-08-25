# build.ps1 - Windows build for board-viewer (parity with build.sh)
$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS) {
    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }
}

$HOUSE = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent
New-Item -ItemType Directory -Path "ops\+x","system" -Force | Out-Null
$CFLAGS = @("-Wall","-Wextra","-O2")
$rc = 0

function Build-One($Src, $Out, $Extra = @()) {
    if (-not (Test-Path $Src)) { Write-Host "SKIP $Src"; return }
    Write-Host "gcc $Src -> $Out"
    & gcc @CFLAGS $Src -o $Out @Extra
    if ($LASTEXITCODE -ne 0) { $script:rc = 1; Write-Host "  FAIL" } else { Write-Host "  OK" }
}

Write-Host "--- board-viewer ops ---"
Build-One "ops\bv_compose_frame.c" "ops\+x\bv_compose_frame.+x"
Build-One "ops\bv_menu_input.c" "ops\+x\bv_menu_input.+x" @("-lm")
Build-One "ops\bv_render_3d.c" "ops\+x\bv_render_3d.+x" @("-lm")
Build-One "ops\ledger_append.c" "ops\+x\ledger_append.+x"
Build-One "ops\ledger_peers.c" "ops\+x\ledger_peers.+x"
# bv_set_wm_pid needs X11 — skip on Win
Write-Host "  SKIP bv_set_wm_pid (X11-only)"

Write-Host "--- system bins from 014.wsr ---"
$wsr = Get-ChildItem -LiteralPath $HOUSE -Directory -EA SilentlyContinue |
    Where-Object { $_.Name -like "014.wsr*" } | Select-Object -First 1
if ($wsr) {
    foreach ($b in @("prisc+x","keyboard_input","renderer","chtpm_parser_pal","chtpm_rgb_render","gl_mirror")) {
        $copied = $false
        foreach ($ext in @(".exe","")) {
            $src = Join-Path $wsr.FullName "system\$b$ext"
            if (Test-Path -LiteralPath $src) {
                Copy-Item -LiteralPath $src -Destination "system\$b.exe" -Force
                Write-Host "  OK $b.exe"
                $copied = $true
                break
            }
        }
        if (-not $copied) { Write-Host "  MISS $b" }
    }
} else {
    Write-Host "WARN: no 014.wsr for system copy"
}

foreach ($glut in @("C:\msys64\mingw64\bin\libfreeglut.dll","C:\msys64\mingw64\bin\freeglut.dll")) {
    if (Test-Path $glut) {
        Copy-Item $glut "system\$(Split-Path $glut -Leaf)" -Force
        Write-Host "  OK $(Split-Path $glut -Leaf)"
        break
    }
}

Write-Host "build ok (rc=$rc)"
exit $rc
