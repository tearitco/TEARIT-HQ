# button.ps1 - Windows launcher for muchi-pals (native; Linux keeps button.sh)
# Usage: powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
param([Parameter(Position=0)][string]$Action = "help")
$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR
$MSYS = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS) { if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" } }

function Get-Bin([string]$rel) {
  $exe = Join-Path $SCRIPT_DIR ($rel + ".exe")
  if (Test-Path -LiteralPath $exe) { return $exe }
  $plain = Join-Path $SCRIPT_DIR $rel
  if (Test-Path -LiteralPath $plain) { return $plain }
  return $null
}

function Invoke-Compile {
  Write-Host "=== muchi-pals compile (Win) ==="
  $sys = Join-Path $SCRIPT_DIR "system"
  Push-Location -LiteralPath $sys
  $jobs = @(
    @{o="prisc+x.exe"; s="prisc+x.c"; f=@()},
    @{o="keyboard_input.exe"; s="keyboard_input.c"; f=@()},
    @{o="renderer.exe"; s="renderer.c"; f=@()},
    @{o="egg_window.exe"; s="egg_window.c"; f=@("-lopengl32","-lgdi32","-luser32","-lm")},
    @{o="emoji_xtract.exe"; s="emoji_xtract.c"; f=@("-lm")}
  )
  $rc = 0
  foreach ($j in $jobs) {
    if (-not (Test-Path -LiteralPath $j.s)) { Write-Host "SKIP $($j.s)"; continue }
    Write-Host "gcc $($j.s) -> $($j.o)"
    & gcc -std=c11 -Wall -O2 -o $j.o $j.s @($j.f) 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "FAIL $($j.o)"; $rc = 1 } else { Write-Host "OK $($j.o)" }
  }
  # emoji_gen needs freetype - best effort
  if (Test-Path "emoji_gen_atlas.c") {
    $ft = & pkg-config --cflags --libs freetype2 2>$null
    if ($LASTEXITCODE -eq 0) {
      $args = $ft -split "\s+"
      & gcc -std=c11 -Wall -O2 -municode -o emoji_gen_atlas.exe emoji_gen_atlas.c @args -lm 2>&1 | Out-Null
      if ($LASTEXITCODE -eq 0) { Write-Host "OK emoji_gen_atlas.exe" } else { Write-Host "FAIL emoji_gen_atlas (optional)" }
    } else { Write-Host "SKIP emoji_gen_atlas (no freetype pkg-config)" }
  }
  Pop-Location
  # ops with .exe names where possible
  $ops = Join-Path $SCRIPT_DIR "ops"
  if (Test-Path -LiteralPath $ops) {
    New-Item -ItemType Directory -Path (Join-Path $ops "+x") -Force | Out-Null
    Get-ChildItem -LiteralPath $ops -Filter "*.c" | ForEach-Object {
      $base = $_.BaseName
      $out = Join-Path $ops "+x\$base.exe"
      & gcc -std=c11 -Wall -O2 -o $out $_.FullName 2>$null
      if ($LASTEXITCODE -eq 0) { Write-Host "OK ops/+x/$base.exe" }
    }
  }
  return $rc
}

function Invoke-Kill {
  $names = @("keyboard_input","renderer","prisc+x","egg_window","orchestrator","chtpm_parser_pal")
  foreach ($n in $names) {
    Get-Process -EA SilentlyContinue | Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
      ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
    & taskkill /F /IM "$n.exe" 2>$null | Out-Null
  }
  Write-Host "kill done"
}

function Invoke-Run {
  $env:PRISC_PROJECT_ROOT = "."
  $need = @("system\renderer.exe","system\prisc+x.exe","system\keyboard_input.exe")
  $miss = @($need | Where-Object { -not (Test-Path -LiteralPath (Join-Path $SCRIPT_DIR $_)) })
  if ($miss.Count -gt 0) { $null = Invoke-Compile }
  Invoke-Kill
  New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\system") -Force | Out-Null
  New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\display") -Force | Out-Null
  $r = Get-Bin "system\renderer"
  $p = Get-Bin "system\prisc+x"
  $k = Get-Bin "system\keyboard_input"
  if (-not $r -or -not $p -or -not $k) { Write-Host "MISSING bins"; return 1 }
  Start-Process -FilePath $r -WorkingDirectory $SCRIPT_DIR -WindowStyle Hidden
  $pal = "pal\main_loop.pal"
  if (-not (Test-Path -LiteralPath (Join-Path $SCRIPT_DIR $pal))) { $pal = "pal\main_loop_chtpm.pal" }
  Start-Process -FilePath $p -ArgumentList $pal -WorkingDirectory $SCRIPT_DIR -WindowStyle Hidden
  & $k
  Invoke-Kill
  return 0
}

$a = $Action.ToLowerInvariant()
if ($a -in @("compile","c","build")) { exit (Invoke-Compile) }
elseif ($a -in @("run","r","start")) { exit (Invoke-Run) }
elseif ($a -in @("kill","k","stop")) { Invoke-Kill; exit 0 }
elseif ($a -eq "check") {
  foreach ($b in @("system\renderer.exe","system\prisc+x.exe","system\keyboard_input.exe","system\egg_window.exe")) {
    if (Test-Path (Join-Path $SCRIPT_DIR $b)) { Write-Host "OK $b" } else { Write-Host "MISSING $b" }
  }
  exit 0
}
else {
  Write-Host "muchi-pals button.ps1: compile | run | kill | check"
  exit 0
}
