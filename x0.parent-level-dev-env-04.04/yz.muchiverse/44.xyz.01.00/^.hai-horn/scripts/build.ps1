# scripts/build.ps1 - compile HORN_CHAT on win32.
#
# Port of scripts/build.sh; the two must stay behaviourally identical. Every
# difference between them is a platform difference and must be commented with
# WHY, or it is a drift bug waiting to happen (the reason scripts/build.sh in
# this house insists the .sh and .ps1 stay in step).
#
# WHAT IS ALREADY WIN32-CLEAN, verified by direct gcc on MSYS2 MinGW64 here:
#   system/keyboard_input.c, system/renderer.c  (this project's own sources)
#   _shared-lib/system/chtpm_parser_pal.c       (compiles; 3 warnings)
#   ops/horn_chat_backend, horn_completions, horn_decide, horn_publish,
#   horn_publish_pending  (5 of 7 ops)
#
# WHAT DOES NOT COMPILE ON WIN32, and why - this is the actual port surface:
#   _shared-lib/system/prisc+x.c    fork()/waitpid() at :1316 and :1346
#   ops/horn_turn.c                 fork()/waitpid/dup2/setsid, ~30 sites
#   ops/horn_tool_exec.c            fork()/waitpid/dup2/execv, ~35 sites,
#                                   AND it sandboxes via /usr/bin/bwrap
#
# MinGW supplies <unistd.h>, <dirent.h> and <sys/stat.h> so those includes are
# NOT blockers on their own. The blockers are the functions those headers
# declare: fork(), dup2(), setsid(), pipe() and sys/wait.h have no Windows
# implementation. NOTE the OneDrive trap that cost this port real time
# elsewhere in the house: MinGW's opendir/readdir returns 0 entries on some
# OneDrive paths, so any dir scan must carry a _findfirst fallback or it will
# silently report "nothing found" rather than failing.
$ErrorActionPreference = "Continue"
$PROJECT = Split-Path -Parent $PSScriptRoot

# Walk up to the house root that owns &.widgits/_shared-lib - same discovery
# scripts/build.sh does. From ^.hai-horn the parent 44.xyz.01.00 IS the root.
$pc = $PROJECT
while ($pc -and -not (Test-Path (Join-Path $pc "&.widgits\_shared-lib"))) {
    $parent = Split-Path -Parent $pc
    if ($parent -eq $pc) { $pc = $null } else { $pc = $parent }
}
if (-not $pc) {
    Write-Host "build: cannot find &.widgits/_shared-lib above this project" -ForegroundColor Red
    exit 1
}
$SHARED = Join-Path $pc "&.widgits\_shared-lib"
if (-not (Test-Path (Join-Path $SHARED "system"))) {
    Write-Host "build: $SHARED has no system/ directory" -ForegroundColor Red
    exit 1
}

Set-Location $PROJECT
foreach ($d in @("ops\+x","pieces\horn","pieces\display","pieces\keyboard",
                 "pieces\system","pieces\os","pieces\apps\player_app\manager",
                 "chats\HORN_SESSIONS")) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}

$env:Path = "C:\msys64\mingw64\bin;$env:Path"
$CFLAGS = @("-Wall", "-Wextra", "-O2", "-I$SHARED")

$FONT_SUPP = @("-Wno-unused-result", "-Wno-stringop-truncation")
$FAILED = @()
# Writes status and records failures in $script:FAILED. Deliberately returns
# NOTHING: a PowerShell function that returns a value leaks it to the caller's
# pipeline, and this is invoked as a statement, so a bare `return $true` would
# print a stray "True" between every build line.
function Try-Compile($src, $out, $extra) {
    $args = @($CFLAGS)
    if ($extra) { $args += $extra }
    $args += @($src, "-o", $out)
    $outText = & gcc @args 2>&1
    $code = $LASTEXITCODE
    # gcc's stderr arrives as ErrorRecord objects, not strings, so every line
    # is cast before it is inspected or printed.
    $lines = @($outText | ForEach-Object { [string]$_ })
    $errs = @($lines | Where-Object { $_ -match 'error:' })
    $warns = @($lines | Where-Object { $_ -match 'warning:' })
    if ($code -ne 0 -or $errs.Count -gt 0) {
        Write-Host ("  FAIL  {0,-22} {1} error(s)" -f (Split-Path $out -Leaf), $errs.Count) -ForegroundColor Red
        foreach ($e in ($errs | Select-Object -First 3)) {
            Write-Host ("          {0}" -f $e.Trim()) -ForegroundColor DarkRed
        }
        $script:FAILED += (Split-Path $out -Leaf)
        return
    }
    Write-Host ("  ok    {0,-22} {1} warning(s)" -f (Split-Path $out -Leaf), $warns.Count) -ForegroundColor DarkGreen
}

Write-Host "--- system processes ---" -ForegroundColor Cyan
# prisc+x is the pal interpreter that runs pal/horn_main_loop.pal. Compiled in
# place from _shared-lib per the 2026-09-09 house convention (never copied -
# earlier copies drifted, which is why the convention exists).
Try-Compile (Join-Path $SHARED "system\prisc+x.c") "system\prisc+x" $null
# These two are this project's OWN local sources - there is no canonical
# version of either in _shared-lib, so there is nothing to compile in place.
Try-Compile "system\keyboard_input.c" "system\keyboard_input" $null
Try-Compile "system\renderer.c" "system\renderer" $null
Try-Compile (Join-Path $SHARED "system\chtpm_parser_pal.c") "system\chtpm_parser_pal" $FONT_SUPP

Write-Host "--- ops ---" -ForegroundColor Cyan
Get-ChildItem "ops" -Filter *.c | Sort-Object Name | ForEach-Object {
    $name = $_.BaseName
    Try-Compile $_.FullName "ops\+x\$name.+x" $null
}

# Drop binaries whose source is gone. A stale horn_chat_openrouter.+x sat in
# ops/+x after the transport was renamed to horn_chat_backend, nothing
# referenced it, and it just made ops/ lie about what this project runs.
Get-ChildItem "ops\+x" -Filter *.+x -EA SilentlyContinue | ForEach-Object {
    $name = $_.BaseName
    if (-not (Test-Path "ops\$name.c")) {
        Write-Host "  removing stale $($_.Name) (no ops\$name.c)" -ForegroundColor Yellow
        Remove-Item $_.FullName -Force
    }
}

Write-Host "--- done ---" -ForegroundColor Cyan
if ($FAILED.Count -gt 0) {
    Write-Host ("{0} target(s) do NOT build on win32 yet: {1}" -f $FAILED.Count, ($FAILED -join ", ")) -ForegroundColor Yellow
    Write-Host "See scripts/build.ps1 header for the per-file port surface." -ForegroundColor Yellow
    exit 1
}
Write-Host "all targets built." -ForegroundColor Green
exit 0